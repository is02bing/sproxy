use std::fs;
use std::net::IpAddr;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;

use crate::ffi::set_default_relay;
use crate::nft::{is_ip_or_cidr, replace_set_elements, DNS_ADDRS_SET};
use crate::{log_error, log_info};

pub const RESOLV_CONF: &str = "/etc/resolv.conf";

type SharedBool = Arc<AtomicBool>;

/// 从 /etc/resolv.conf 解析 nameserver
///
/// 仅保留 IPv4 nameserver：nft dns_addrs set 类型为 ipv4_addr，DNS 劫持规则
/// 仅匹配 ipv4 流量（`ip daddr @dns_addrs`），IPv6 nameserver 无法写入 set
/// 也无法被劫持，故排除。同时跳过 127.x loopback 本地 DNS。
pub fn parse_resolv_nameservers(path: &str) -> Vec<String> {
    let content = match fs::read_to_string(path) {
        Ok(c) => c,
        Err(_) => return Vec::new(),
    };
    content
        .lines()
        .map(|l| {
            // 先按 '#' 截断行内注释，再 trim；纯注释行截断后为空会被过滤
            l.split('#').next().unwrap_or("").trim()
        })
        // 过滤空行与 ';' 开头的注释行（resolv.conf 合法注释符）
        .filter(|l| !l.is_empty() && !l.starts_with(';'))
        .filter_map(|l| {
            let mut it = l.split_whitespace();
            if it.next() != Some("nameserver") {
                return None;
            }
            it.next().map(|s| s.trim().to_string())
        })
        .filter(|ip| is_ip_or_cidr(ip))
        .filter_map(|ip| match ip.parse::<IpAddr>() {
            // 仅保留 IPv4 且非 loopback
            Ok(IpAddr::V4(v4)) if !v4.is_loopback() => Some(ip),
            _ => None,
        })
        .collect()
}

/// 更新 dns_addrs set（resolv.conf 变化时调用，只改 set 不重建表）
///
/// 原子替换 set 元素（flush + add 在同一 nft 脚本），表结构和规则不受影响。
fn update_dns_addrs(nameservers: &[String]) -> Result<(), crate::error::NftError> {
    replace_set_elements(DNS_ADDRS_SET, nameservers)
}

/// 解析 resolv.conf → 推送rdns默认上游 + 更新dns_addrs set(不重建nft表）
pub fn apply_resolv_change(rules_active: &SharedBool) {
    let nameservers = parse_resolv_nameservers(RESOLV_CONF);
    if nameservers.is_empty() {
        log_info!("[resolv] no external nameserver in {}", RESOLV_CONF);
        return;
    }

    // 1. 推送给 rdns 作为默认上游
    set_default_relay(&nameservers);
    log_info!("[resolv] default relay updated: {:?}", nameservers);

    // 2. 更新 dns_addrs set（仅当规则已激活，表存在时）
    if !rules_active.load(Ordering::SeqCst) {
        return;
    }
    if let Err(e) = update_dns_addrs(&nameservers) {
        log_error!("[resolv] update dns_addrs failed: {}", e);
    }
}
