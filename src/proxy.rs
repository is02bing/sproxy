use std::collections::HashSet;
use std::fs;
use std::net::IpAddr;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{Arc, LazyLock, RwLock};

use anyhow::{Context, Result};
use crate::error::NftError;
use crate::nft::{add_set_element_ip, is_ip_or_cidr, replace_set_elements, setup_tproxy_rules, BYPASS_SET, PROXY_STATIC_SET, PROXY_AUTO_SET};
use crate::resolv;
use crate::{log_error, log_info};

type SharedBool = Arc<AtomicBool>;

/// 全局必须代理域名集合（must-proxy = goproxy - bypass）
static GOPROXY_DOMAINS: LazyLock<RwLock<HashSet<String>>> = LazyLock::new(|| RwLock::new(HashSet::new()));

/// 全局 bypass 域名集合（DNS 解析时即使 proxy=true 也不走代理）
static BYPASS_DOMAINS: LazyLock<RwLock<HashSet<String>>> = LazyLock::new(|| RwLock::new(HashSet::new()));

/// 全局 bypass CIDR 缓存（DNS 解析时检查 IP 是否落在 bypass 网段）
static BYPASS_CIDRS: LazyLock<RwLock<Vec<(IpAddr, u8)>>> = LazyLock::new(|| RwLock::new(Vec::new()));

//从文件加载 CIDR/IP地址列表（每行一个，`#` 注释，空行忽略）
pub fn load_cidr(path: &str) -> Result<Vec<String>, NftError> {
    let content = fs::read_to_string(path).map_err(|e| NftError::Other(format!("read {} failed: {}", path, e)))?;

    let addrs: Vec<String> = content.lines()
        .map(|line| line.trim())
        .filter(|line| !line.is_empty() && !line.starts_with('#'))
        .filter(|line| is_ip_or_cidr(line))
        .map(|s| s.to_string())
        .collect();

    Ok(addrs)
}

//加载/重载 CIDR goproxy 地址列表到 nft proxy_static set
//整体替换（flush + add）：首次注入时 set 为空无副作用，重载时清除旧元素。
pub fn reload_goproxy_cidr(path: &str) {
    match load_cidr(path) {
        Ok(addrs) => {
            if let Err(e) = replace_set_elements(PROXY_STATIC_SET, &addrs) {
                log_error!("reload proxy_static failed: {}", e);
            } else {
                log_info!("proxy_static reloaded: {} addresses", addrs.len());
            }
        }
        Err(e) => log_error!("reload goproxy cidr failed: {}", e),
    }
}

/// 添加单个 IP 到 proxy_dyn set（DNS 回调时调用，带 timeout 自动过期）
pub fn add_proxy_address(ip: &str) {
    if let Err(e) = add_set_element_ip(PROXY_AUTO_SET, ip) {
        log_error!("[dns-cb] add {} to nftables failed: {}", ip, e);
    }
}

// ── 域名列表 ──

/// 单行域名规则的分类
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum DomainRule {
    /// 走代理的域名（`|` / `||` 或无前缀）
    Proxy,
    /// bypass 域名（`@@` 前缀）
    Bypass,
}

/// 从一行文本中提取域名规则
///
/// 规则:
/// - `!` 或 `[` 或 `#` 开头: 注释，忽略
/// - `/regex/` 形式: 正则表达式，不支持，忽略
/// - `@@` 开头: bypass 域名，去掉 `@@` 后再剥离 `||` / `|` 前缀
/// - `||` 或 `|` 开头: proxy 域名
/// - 无前缀: 默认视为 proxy 域名
///
/// 提取域名时去除 `scheme://`、路径、端口；保留 `*` 等通配符字符。
pub fn extract_domain(line: &str) -> Option<(DomainRule, String)> {
    let s = line.trim();
    if s.is_empty() || s.starts_with('!') || s.starts_with('[') || s.starts_with('#') {
        return None;
    }
    // 形如 /pattern/ 的正则规则不支持，忽略
    if s.starts_with('/') && s.ends_with('/') && s.len() > 1 {
        return None;
    }

    let (rule, rest) = if let Some(rest) = s.strip_prefix("@@") {
        // @@ 后通常跟着 || 或 |，需要再剥离一次
        let rest = rest.strip_prefix("||").or_else(|| rest.strip_prefix('|')).unwrap_or(rest);
        (DomainRule::Bypass, rest)
    } else if let Some(rest) = s.strip_prefix("||") {
        (DomainRule::Proxy, rest)
    } else if let Some(rest) = s.strip_prefix('|') {
        (DomainRule::Proxy, rest)
    } else {
        (DomainRule::Proxy, s)
    };

    // 去除 scheme (http://, https://, ...)
    let rest = if let Some(pos) = rest.find("://") {
        &rest[pos + 3..]
    } else {
        rest
    };
    // 去除路径
    let rest = if let Some(pos) = rest.find('/') {
        &rest[..pos]
    } else {
        rest
    };
    // 去除端口
    let rest = if let Some(pos) = rest.rfind(':') {
        &rest[..pos]
    } else {
        rest
    };

    let domain = rest.trim().to_lowercase();
    if domain.is_empty() || domain.contains(' ') || is_ip_or_cidr(&domain) {
        return None;
    }
    Some((rule, domain))
}

/// 从单一文件加载 proxy / bypass 域名集合
///
/// 文件中按行首规则区分:
/// - `@@` 开头: bypass 域名
/// - `|` / `||` 开头: proxy 域名
/// - 无前缀: 默认视为 proxy 域名
///
/// 计算 must-proxy = proxy - bypass，更新全局集合
pub fn load_proxy_domains(auto_path: &str) -> Result<()> {
    let content = fs::read_to_string(auto_path)
        .with_context(|| format!("读取 {} 失败", auto_path))?;

    let mut proxy_domains: HashSet<String> = HashSet::new();
    let mut bypass_domains: HashSet<String> = HashSet::new();

    for line in content.lines() {
        if let Some((rule, domain)) = extract_domain(line) {
            match rule {
                DomainRule::Proxy => { proxy_domains.insert(domain); }
                DomainRule::Bypass => { bypass_domains.insert(domain); }
            }
        }
    }

    let goproxy: HashSet<String> = proxy_domains.difference(&bypass_domains).cloned().collect();
    log_info!("goproxy: {} domains (proxy: {}, bypass: {})",
              goproxy.len(), proxy_domains.len(), bypass_domains.len());

    if let Ok(mut set) = GOPROXY_DOMAINS.write() {
        *set = goproxy;
    }
    if let Ok(mut set) = BYPASS_DOMAINS.write() {
        *set = bypass_domains;
    }
    Ok(())
}

/// 检查域名是否在 bypass 列表中（支持子域名匹配）
fn is_bypass_domain(domain: &str) -> bool {
    is_domain_in_set(domain, &BYPASS_DOMAINS)
}

/// 快照当前 must-proxy（goproxy）域名集合
pub fn snapshot_goproxy_domains() -> Vec<String> {
    if let Ok(set) = GOPROXY_DOMAINS.read() {
        set.iter().cloned().collect()
    } else {
        Vec::new()
    }
}

/// 检查域名是否在 must-proxy 列表中（支持子域名匹配）
///
/// 例如 must-proxy 中有 "google.com"，则 "www.google.com" 也匹配
fn is_goproxy_domain(domain: &str) -> bool {
    is_domain_in_set(domain, &GOPROXY_DOMAINS)
}

/// 通用子域名匹配：从完整域名逐级向上查找
fn is_domain_in_set(domain: &str, lock: &RwLock<HashSet<String>>) -> bool {
    let domain_lower = domain.to_lowercase();
    if let Ok(set) = lock.read() {
        let mut d: &str = &domain_lower;
        loop {
            if set.contains(d) {
                return true;
            }
            if let Some(pos) = d.find('.') {
                d = &d[pos + 1..];
            } else {
                break;
            }
        }
    }
    false
}

// ── bypass CIDR 缓存与 IP 检查 ──

/// 解析 "1.2.3.0/24" 或 "1.2.3.4" 为 (IpAddr, prefix_len)
fn parse_cidr(s: &str) -> Option<(IpAddr, u8)> {
    let (ip_str, prefix) = match s.split_once('/') {
        Some((ip, p)) => (ip, p.parse::<u8>().ok()?),
        None => (s, 32),
    };
    let ip: IpAddr = ip_str.parse().ok()?;
    Some((ip, prefix))
}

/// 更新全局 bypass CIDR 缓存（bypass 文件加载时调用）
fn update_bypass_cidrs(addrs: &[String]) {
    let cidrs: Vec<(IpAddr, u8)> = addrs.iter()
        .filter_map(|s| parse_cidr(s))
        .collect();
    if let Ok(mut set) = BYPASS_CIDRS.write() {
        *set = cidrs;
    }
}

/// 检查 IP 是否落在 bypass CIDR 网段中
fn is_bypass_ip(ip: &str) -> bool {
    let target: IpAddr = match ip.parse() {
        Ok(addr) => addr,
        Err(_) => return false,
    };
    if let Ok(cidrs) = BYPASS_CIDRS.read() {
        for (net_ip, prefix) in cidrs.iter() {
            if ip_in_cidr(&target, net_ip, *prefix) {
                return true;
            }
        }
    }
    false
}

/// 判断 target 是否在 (network, prefix) 网段内
fn ip_in_cidr(target: &IpAddr, network: &IpAddr, prefix: u8) -> bool {
    match (target, network) {
        (IpAddr::V4(t), IpAddr::V4(n)) => {
            if prefix > 32 { return false; }
            let mask = if prefix == 0 { 0 } else { !0u32 << (32 - prefix) };
            (u32::from(*t) & mask) == (u32::from(*n) & mask)
        }
        (IpAddr::V6(t), IpAddr::V6(n)) => {
            if prefix > 128 { return false; }
            let ta = u128::from(*t);
            let na = u128::from(*n);
            if prefix == 0 { return true; }
            let mask = !0u128 << (128 - prefix);
            (ta & mask) == (na & mask)
        }
        _ => false,
    }
}

/// DNS 回调决策：根据域名和 proxy 标志决定是否将 IP 加入代理集合
/// - bypass 域名优先级最高，即使 proxy=true 也不走代理
/// - IP 落在 bypass CIDR 网段也不走代理
/// - proxy=true 或域名在 goproxy 列表中，则加入 proxy_addrs set
pub fn on_dns_resolve(ip: &str, domain: &str, proxy: bool) {
    // DNS 协议中 query->name 为 FQDN，带尾点（根标记），去除后再与域名集合匹配
    let domain = domain.strip_suffix('.').unwrap_or(domain);

    if is_bypass_domain(domain) {
        log_info!("[dns-cb] {} -> {}, bypass domain", domain, ip);
        return;
    }

    if is_bypass_ip(ip) {
        log_info!("[dns-cb] {} -> {}, bypass ip", domain, ip);
        return;
    }

    let goproxy = is_goproxy_domain(domain);
    if goproxy {
        log_info!("[dns-cb] {} -> {}, goproxy", domain, ip);
        add_proxy_address(ip);
    } else if proxy {
        log_info!("[dns-cb] {} -> {}, proxy auto by dns", domain, ip);
        add_proxy_address(ip);
    } else {
        log_info!("[dns-cb] {} -> {}, bypass", domain, ip);
    }
}

/// tproxy 规则注入所需的上下文参数
pub struct TproxyContext<'a> {
    pub tproxy_port: u16,
    pub ssh_server_ip: &'a str,
    pub dns_enabled: bool,
    pub dns_listen_port: u16,
    pub dns_client_port: u16,
    pub bypass_path: &'a str,
    pub goproxy_path: &'a str,
    pub rules_active: &'a SharedBool,
}

/// 加载 bypass 地址并注入 tproxy 规则
///
/// 注入 tproxy 规则（首次建表）
///
/// 加载 bypass 地址 → setup_tproxy_rules 建表 → rules_active 置 true
/// → 填充 dns_addrs set + 加载 goproxy CIDR。
/// 返回是否成功。
pub fn apply_tproxy_rules(ctx: &TproxyContext) -> bool {
    // 加载 bypass 地址（即使为空也应用：默认私有地址段总是作为 bypass）
    let bypass_addrs = match load_cidr(ctx.bypass_path) {
        Ok(addrs) => addrs,
        Err(e) => {
            log_error!("load bypass addresses failed: {}", e);
            return false;
        }
    };
    // 同步更新 bypass CIDR 内存缓存（供 DNS 回调 IP 检查）
    update_bypass_cidrs(&bypass_addrs);

    match setup_tproxy_rules(
        ctx.tproxy_port,
        ctx.ssh_server_ip,
        ctx.dns_enabled,
        ctx.dns_listen_port,
        ctx.dns_client_port,
        &bypass_addrs,
    ) {
        Ok(_) => {
            log_info!("nftables tproxy ruleset injected (tproxy -> :{})", ctx.tproxy_port);
            ctx.rules_active.store(true, Ordering::SeqCst);
            // dns 启用时填充 dns_addrs set（初始 nameserver）
            if ctx.dns_enabled {
                resolv::apply_resolv_change(ctx.rules_active);
            }
            // 加载 CIDR goproxy 地址到 proxy_addrs set
            reload_goproxy_cidr(ctx.goproxy_path);
            true
        }
        Err(e) => {
            log_error!("setup nftables tproxy ruleset failed: {}", e);
            false
        }
    }
}

/// bypass 文件变化时刷新 bypass_addrs set（仅当规则已激活）
///
/// 原子替换 set 元素（flush + add 在同一 nft 脚本），不重建 nft 表，
/// dns_addrs / proxy_addrs 不受影响。
pub fn reload_bypass(ctx: &TproxyContext) {
    if !ctx.rules_active.load(Ordering::SeqCst) {
        return;
    }

    let new_addrs = match load_cidr(ctx.bypass_path) {
        Ok(addrs) => addrs,
        Err(e) => {
            log_error!("reload bypass addresses failed: {}", e);
            return;
        }
    };

    if let Err(e) = replace_set_elements(BYPASS_SET, &new_addrs) {
        log_error!("reload bypass_addrs failed: {}", e);
        return;
    }
    // 同步更新 bypass CIDR 内存缓存
    update_bypass_cidrs(&new_addrs);
    log_info!("bypass_addrs reloaded: {} addresses", new_addrs.len());
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_extract_domain_bypass() {
        assert_eq!(
            extract_domain("@@||firebase-settings.crashlytics.com"),
            Some((DomainRule::Bypass, "firebase-settings.crashlytics.com".to_string()))
        );
        assert_eq!(
            extract_domain("@@||cn.investing.com"),
            Some((DomainRule::Bypass, "cn.investing.com".to_string()))
        );
        assert_eq!(
            extract_domain("@@||www.typepad.com"),
            Some((DomainRule::Bypass, "www.typepad.com".to_string()))
        );
    }

    #[test]
    fn test_extract_domain_proxy() {
        assert_eq!(
            extract_domain("||blogjav.net"),
            Some((DomainRule::Proxy, "blogjav.net".to_string()))
        );
        assert_eq!(
            extract_domain("||zoominfo.com"),
            Some((DomainRule::Proxy, "zoominfo.com".to_string()))
        );
        assert_eq!(
            extract_domain("||ptwxz.com"),
            Some((DomainRule::Proxy, "ptwxz.com".to_string()))
        );
        assert_eq!(
            extract_domain("||miuipolska.pl"),
            Some((DomainRule::Proxy, "miuipolska.pl".to_string()))
        );
    }

    #[test]
    fn test_extract_domain_strips_path_and_wildcard() {
        assert_eq!(
            extract_domain("||addons.mozilla.org/*-*/firefox/addon/ublock-origin/*"),
            Some((DomainRule::Proxy, "addons.mozilla.org".to_string()))
        );
        assert_eq!(
            extract_domain("||addons.mozilla.org/firefox/downloads/file/*/ublock_origin-*.xpi"),
            Some((DomainRule::Proxy, "addons.mozilla.org".to_string()))
        );
        assert_eq!(
            extract_domain("||msha.gov"),
            Some((DomainRule::Proxy, "msha.gov".to_string()))
        );
    }

    #[test]
    fn test_extract_domain_single_pipe_with_scheme() {
        assert_eq!(
            extract_domain("|http://cdn*.search.xxx/"),
            Some((DomainRule::Proxy, "cdn*.search.xxx".to_string()))
        );
    }

    #[test]
    fn test_extract_domain_comments_and_regex_ignored() {
        assert_eq!(extract_domain("! this is a comment"), None);
        assert_eq!(extract_domain("[Adblock Plus 2.0]"), None);
        assert_eq!(extract_domain("# hash comment"), None);
        assert_eq!(extract_domain(""), None);
        assert_eq!(extract_domain("   "), None);
        // 正则形式 /pattern/ 不支持
        assert_eq!(extract_domain("/.*\\.example\\.com/"), None);
    }

    #[test]
    fn test_extract_domain_plain_line_defaults_to_proxy() {
        assert_eq!(
            extract_domain("example.com"),
            Some((DomainRule::Proxy, "example.com".to_string()))
        );
        assert_eq!(
            extract_domain("http://foo.bar.com:8080/path"),
            Some((DomainRule::Proxy, "foo.bar.com".to_string()))
        );
    }

    #[test]
    fn test_extract_domain_at_at_single_pipe() {
        assert_eq!(
            extract_domain("@@|example.org"),
            Some((DomainRule::Bypass, "example.org".to_string()))
        );
    }
}