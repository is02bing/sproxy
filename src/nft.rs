use std::ffi::CString;
use std::fmt::Write;
use std::net::{IpAddr, Ipv4Addr};
use std::os::raw::c_int;
use crate::error::NftError;
use crate::log_info;

pub const TPROXY_TABLE: &str = "sproxy";
// 静态代理集（从goproxy.txt加载，reload时整体替换）
pub const PROXY_STATIC_SET: &str = "goproxy";
// 动态代理集（由域名列表定义,按需要注入,自动过期）
pub const PROXY_AUTO_SET: &str = "auto";
// 绕过代理的IP地址集合（从bypass.txt加载）
pub const BYPASS_SET: &str = "bypass";
// resolv.conf 中的 DNS nameserver集合
pub const DNS_ADDRS_SET: &str = "dns";

// auto set 元素过期时间（秒）： 注入的IP 1小时后自动从set移除
pub const PROXY_AUTO_TIMEOUT: u64 = 3600;

//要处理的链
const TPROXY_PREROUTING_CHAIN: &str = "prerouting";
const TPROXY_OUTPUT_CHAIN: &str = "output";

#[repr(C)]
struct NftCtx {
    _opaque: [u8; 0],
}

extern "C" {
    fn nft_ctx_new(flags: u32) -> *mut NftCtx;
    fn nft_ctx_free(ctx: *mut NftCtx);
    fn nft_run_cmd_from_buffer(ctx: *mut NftCtx, buf: *const i8) -> c_int;
}

fn nft_run(cmd: &str) -> Result<(), NftError> {
    let ctx = unsafe { nft_ctx_new(0) };
    if ctx.is_null() {
        return Err(NftError::Other("nft context allocate failed".into()));
    }

    let c_cmd = CString::new(cmd).map_err(|e| NftError::Other(format!("nft invalid command: {}", e)))?;

    let ret = unsafe { nft_run_cmd_from_buffer(ctx, c_cmd.as_ptr()) };
    unsafe { nft_ctx_free(ctx) };

    if ret != 0 {
        Err(NftError::Other(format!("nft script failed:\n{}", cmd.trim())))
    } else {
        log_info!("nft script succeed:\n{}", cmd);
        Ok(())
    }
}

///判断字符串是否为合法的IP或 CIDR
pub fn is_ip_or_cidr(s: &str) -> bool {
    let ip_part = if let Some(pos) = s.find('/') {
        &s[..pos]
    } else {
        s
    };
    ip_part.parse::<IpAddr>().is_ok()
}

// ── CIDR合并: 消除重叠/包含区间(nft set不支持重叠)
// 将IPv4 CIDR或单IP解析为 (起始, 结束) 数值区间，无法解析返回 None
fn parse_cidr_v4(s: &str) -> Option<(u32, u32)> {
    let (ip_str, prefix) = match s.split_once('/') {
        Some((ip, p)) => {
            let prefix: u32 = p.parse().ok()?;
            if prefix > 32 { return None; }
            (ip, prefix)
        }
        None => (s, 32),
    };
    let ip: u32 = u32::from(ip_str.parse::<Ipv4Addr>().ok()?);
    let mask = if prefix == 0 { 0 } else { !0u32 << (32 - prefix) };
    let network = ip & mask;
    Some((network, network | !mask))
}

//将数值区间拆分为最少的CIDR列表
fn range_to_cidrs(start: u32, end: u32) -> Vec<String> {
    let mut result = Vec::new();
    let mut current = start as u64;
    let end = end as u64;
    while current <= end {
        let remaining = end - current + 1;
        // 满足 2^bits <= remaining 的最大 bits
        let max_size_bits = 63 - remaining.leading_zeros();
        // 满足 current 按 2^bits 对齐的最大 bits（current==0 时可对齐到 /0）
        let align_bits = if current == 0 { 32u32 } else { (current as u32).trailing_zeros() };
        let bits = max_size_bits.min(align_bits);
        let prefix = 32 - bits;
        let size = 1u64 << bits;
        result.push(format!("{}/{}", Ipv4Addr::from(current as u32), prefix));
        current += size;
    }
    result
}

//合并重叠/相邻的IPv4 CIDR；非IPv4条目原样保留
fn merge_cidrs(addrs: &[String]) -> Vec<String> {
    let mut ranges: Vec<(u32, u32)> = Vec::new();
    let mut others: Vec<String> = Vec::new();
    for a in addrs {
        if !is_ip_or_cidr(a) {
            continue;
        }
        match parse_cidr_v4(a) {
            Some(r) => ranges.push(r),
            None => others.push(a.clone()),
        }
    }
    if ranges.is_empty() {
        return others;
    }
    ranges.sort_by_key(|r| (r.0, r.1));
    let mut merged: Vec<(u32, u32)> = Vec::new();
    for (start, end) in ranges {
        if let Some(last) = merged.last_mut() {
            //重叠或相邻则合并
            if start <= last.1.saturating_add(1) {
                if end > last.1 {
                    last.1 = end;
                }
                continue;
            }
        }
        merged.push((start, end));
    }
    let mut result = others;
    for (start, end) in merged {
        result.extend(range_to_cidrs(start, end));
    }
    result
}

// ── set 元素操作 ──

const SET_ELEM_BATCH_SIZE: usize = 16;

/// 批量添加元素到 nft set（自动合并重叠 CIDR）
pub fn add_set_elements(set_name: &str, addresses: &[String]) -> Result<(), NftError> {
    let merged = merge_cidrs(addresses);
    for chunk in merged.chunks(SET_ELEM_BATCH_SIZE) {
        if chunk.is_empty() {
            continue;
        }
        let cmd = format!("add element inet {} {} {{ {} }}", TPROXY_TABLE, set_name, chunk.join(", "));
        nft_run(cmd.as_str())?;
    }
    Ok(())
}

/// 添加单个 IP 到带 timeout 的 set（DNS 动态注入用）
///
/// 不做 CIDR 合并，直接写入 IP（继承 set 级 timeout 自动过期）。
pub fn add_set_element_ip(set_name: &str, ip: &str) -> Result<(), NftError> {
    let cmd = format!("add element inet {} {} {{ {} }}", TPROXY_TABLE, set_name, ip);
    nft_run(cmd.as_str())
}

/// 原子替换 set 元素：flush + add 在同一个 nft 脚本中执行
///
/// 自动合并重叠 CIDR。flush 错误忽略（set 不存在时保证可重入）。
/// 空列表等价于仅 flush。
pub fn replace_set_elements(set_name: &str, addresses: &[String]) -> Result<(), NftError> {
    // flush 单独执行并忽略错误（set 可能不存在）
    let _ = nft_run(format!("flush set inet {} {}", TPROXY_TABLE, set_name).as_str());

    let merged = merge_cidrs(addresses);
    if merged.is_empty() {
        return Ok(());
    }

    let mut script = String::new();
    for chunk in merged.chunks(SET_ELEM_BATCH_SIZE) {
        if chunk.is_empty() {
            continue;
        }
        writeln!(script, "add element inet {} {} {{ {} }}", TPROXY_TABLE, set_name, chunk.join(", ")).unwrap();
    }
    if script.is_empty() {
        return Ok(());
    }
    nft_run(&script)
}

//默认私有/保留地址段
//作为独立规则写入prerouting/output链，不依赖bypass集合
const PRIVATE_NETS: &[&str] = &[
    "10.0.0.0/8",
    "172.16.0.0/12",
    "192.168.0.0/16",
    "127.0.0.0/8",
    "169.254.0.0/16",
    "100.64.0.0/10",
    "0.0.0.0/8",
];

// 配置tproxy代理规则
pub fn setup_tproxy_rules(
    tproxy_port: u16,
    ssh_server_ip: &str,
    dns_enabled: bool,
    dns_listen_port: u16,
    dns_client_port: u16,
    bypass_addresses: &[String],
) -> Result<(), NftError> {
    let mut script = String::new();

    writeln!(script, "destroy table inet {}", TPROXY_TABLE).unwrap();
    //创建表
    writeln!(script, "add table inet {}", TPROXY_TABLE).unwrap();
    //创建 proxy_cidr set（静态：从 goproxy.txt 加载的 CIDR）
    writeln!(script, "add set inet {} {} {{ type ipv4_addr; flags interval; }}", TPROXY_TABLE, PROXY_STATIC_SET).unwrap();
    //创建 proxy_dyn set（动态：DNS 回调注入的单 IP，带 timeout 自动过期）
    writeln!(script, "add set inet {} {} {{ type ipv4_addr; flags timeout; timeout {}s; }}", TPROXY_TABLE, PROXY_AUTO_SET, PROXY_AUTO_TIMEOUT).unwrap();
    //创建 bypass_addrs set（静态：从 bypass.txt 加载，用户自定义）
    writeln!(script, "add set inet {} {} {{ type ipv4_addr; flags interval; }}", TPROXY_TABLE, BYPASS_SET).unwrap();
    //创建 dns_addrs set（resolv.conf 中的 nameserver）
    writeln!(script, "add set inet {} {} {{ type ipv4_addr; flags interval; }}", TPROXY_TABLE, DNS_ADDRS_SET).unwrap();

    //创建prerouting链（入站流量钩子）
    writeln!(script, "add chain inet {} {} {{ type filter hook prerouting priority -150; policy accept; }}", TPROXY_TABLE, TPROXY_PREROUTING_CHAIN).unwrap();
    //创建output链（出站流量钩子，nat型用于redirect）
    writeln!(script, "add chain inet {} {} {{ type nat hook output priority -150; policy accept; }}", TPROXY_TABLE, TPROXY_OUTPUT_CHAIN).unwrap();

    // Prerouting规则：
    //dns_addrs set的53端口 → tproxy到rdns, 必须放在 PRIVATE_NETS之前：若nameserver本身是私有IP，否则会被私有网段accept截胡而不转发到rdns
    if dns_enabled {
        writeln!(script, "add rule inet {} {} ip daddr @{} meta l4proto {{ tcp, udp }} th dport 53 tproxy ip to :{}", TPROXY_TABLE, TPROXY_PREROUTING_CHAIN, DNS_ADDRS_SET, dns_listen_port).unwrap();
    }

    //默认私有/保留地址仅TCP直接accept
    for cidr in PRIVATE_NETS {
        writeln!(script, "add rule inet {} {} meta l4proto tcp ip daddr {} accept", TPROXY_TABLE, TPROXY_PREROUTING_CHAIN, cidr).unwrap();
    }
    //匹配入站TCP → 透明代理（排除 bypass 地址）：静态集
    writeln!(script, "add rule inet {} {} meta l4proto tcp meta nfproto ipv4 ip daddr @{} ip daddr != @{} tcp dport != {} tproxy ip to :{}",
             TPROXY_TABLE, TPROXY_PREROUTING_CHAIN, PROXY_STATIC_SET, BYPASS_SET, tproxy_port, tproxy_port
    ).unwrap();
    //匹配入站TCP → 透明代理（排除 bypass 地址）：动态集
    //update 语句在匹配时刷新该元素的 timeout，实现"有连接则保活，无连接才过期"
    writeln!(script, "add rule inet {} {} meta l4proto tcp meta nfproto ipv4 ip daddr @{} ip daddr != @{} tcp dport != {} update @{} {{ ip daddr timeout {}s }} tproxy ip to :{}",
             TPROXY_TABLE, TPROXY_PREROUTING_CHAIN, PROXY_AUTO_SET, BYPASS_SET, tproxy_port, PROXY_AUTO_SET, PROXY_AUTO_TIMEOUT, tproxy_port
    ).unwrap();

    //Output规则：
    //排除SSH服务器
    if !ssh_server_ip.is_empty() {
        if let Ok(IpAddr::V4(_)) = ssh_server_ip.parse::<IpAddr>() {
            writeln!(script, "add rule inet {} {} ip daddr {} accept", TPROXY_TABLE, TPROXY_OUTPUT_CHAIN, ssh_server_ip).unwrap();
        }
    }

    //rdns自身向上游DNS查询放行（udp sport dns_client_port），不拦截, 须在DNS redirect之前，否则rdns向@dns_addrs中上游的查询会被redirect回自己形成死循环
    if dns_client_port > 0 {
        writeln!(script, "add rule inet {} {} meta l4proto udp udp sport {} accept", TPROXY_TABLE, TPROXY_OUTPUT_CHAIN, dns_client_port).unwrap();
    }

    //dns启用时，匹配dns_addrs set的53端口 → redirect到rdns
    //必须放在 PRIVATE_NETS之前：若 nameserver本身是私有IP，否则会被私有网段accept截胡而不转发到rdns）
    if dns_enabled {
        writeln!(script, "add rule inet {} {} ip daddr @{} meta l4proto {{ tcp, udp }} th dport 53 redirect to :{}", TPROXY_TABLE, TPROXY_OUTPUT_CHAIN, DNS_ADDRS_SET, dns_listen_port).unwrap();
    }

    //默认私有/保留地址仅TCP直接accept
    for cidr in PRIVATE_NETS {
        writeln!(script, "add rule inet {} {} meta l4proto tcp ip daddr {} accept", TPROXY_TABLE, TPROXY_OUTPUT_CHAIN, cidr).unwrap();
    }

    //redirect 规则：匹配出站TCP → 重定向到代理端口（排除 bypass）：静态 CIDR 集合
    writeln!(script, "add rule inet {} {} meta l4proto tcp meta nfproto ipv4 ip daddr @{} ip daddr != @{} tcp dport != {} redirect to :{}",
             TPROXY_TABLE, TPROXY_OUTPUT_CHAIN, PROXY_STATIC_SET, BYPASS_SET, tproxy_port, tproxy_port
    ).unwrap();
    //redirect 规则：匹配出站TCP → 重定向到代理端口（排除 bypass）：动态 DNS 集合
    //update 语句在匹配时刷新该元素的 timeout，实现"有连接则保活，无连接才过期"
    writeln!(script, "add rule inet {} {} meta l4proto tcp meta nfproto ipv4 ip daddr @{} ip daddr != @{} tcp dport != {} update @{} {{ ip daddr timeout {}s }} redirect to :{}",
             TPROXY_TABLE, TPROXY_OUTPUT_CHAIN, PROXY_AUTO_SET, BYPASS_SET, tproxy_port, PROXY_AUTO_SET, PROXY_AUTO_TIMEOUT, tproxy_port
    ).unwrap();

    nft_run(&script)?;

    //添加用户自定义 bypass 地址到 bypass_addrs set（可选，可为空）
    if !bypass_addresses.is_empty() {
        add_set_elements(BYPASS_SET, bypass_addresses)?;
    }

    Ok(())
}

//清理tproxy规则（幂等：表不存在不报错，与 setup 开头 destroy 一致）
pub fn cleanup_tproxy_rules() -> Result<(), NftError> {
    nft_run(format!("destroy table inet {}", TPROXY_TABLE).as_str())
}
