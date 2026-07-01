mod error;
mod ffi;
mod logger;
mod nft;
mod resolv;
mod proxy;

use crate::ffi::{start_c_proxy, stop_c_proxy, CProxyParams};
use crate::nft::cleanup_tproxy_rules;

use anyhow::{Context, Result};
use inotify::{EventMask, Inotify, WatchMask};
use serde::Deserialize;
use signal_hook::consts::{SIGINT, SIGTERM};
use signal_hook::low_level::pipe;
use std::collections::{HashMap, HashSet};
use std::env::args;
use std::fs;
use std::io::{Read, Write};
use std::net::{SocketAddr, TcpStream};
use std::os::raw::{c_int, c_uint};
use std::os::unix::io::AsRawFd;
use std::os::unix::net::UnixStream;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;
use std::thread;
use std::time::{Duration, Instant};

type SharedBool = Arc<AtomicBool>;

//代理配置
#[derive(Debug, Deserialize)]
struct Config {
    #[serde(default)]
    log: LogConfig,
    #[serde(default)]
    ssh: SshConfig,
    #[serde(default)]
    socks5: Socks5Config,
    #[serde(default)]
    tproxy: TproxyConfig,
    #[serde(default)]
    dns: DnsConfig,
    #[serde(default)]
    proxy: ProxyConfig,
    #[serde(default)]
    probe: ProbeConfig,
}

#[derive(Debug, Deserialize)]
#[serde(default)]
struct LogConfig {
    level: String,
}
impl Default for LogConfig {
    fn default() -> Self { Self { level: "info".to_string() } }
}

#[derive(Debug, Deserialize)]
#[serde(default)]
struct SshConfig {
    host: Option<String>,
    port: u16,
    user: Option<String>,
    key: Option<String>
}
impl Default for SshConfig {
    fn default() -> Self {
        Self { host: None, port: 22, user: Some("root".to_string()), key: None}
    }
}

#[derive(Debug, Deserialize)]
#[serde(default)]
struct Socks5Config {
    listen: Option<String>,
}
impl Default for Socks5Config {
    fn default() -> Self { Self { listen: Some("127.0.0.1:1080".to_string()) } }
}

#[derive(Debug, Deserialize)]
#[serde(default)]
struct TproxyConfig {
    port: u16,
}
impl Default for TproxyConfig {
    fn default() -> Self { Self { port: 1081 } }
}

#[derive(Debug, Deserialize)]
#[serde(default)]
struct DnsConfig {
    listen: Option<String>,
    client: u16,
    proxy: Option<String>,
    servers: Vec<DnsServerConfig>,
}
impl Default for DnsConfig {
    fn default() -> Self {
        Self { listen: Some("127.0.0.1:1053".to_string()), client: 1054, proxy: None, servers: Vec::new() }
    }
}

#[derive(Debug, Deserialize)]
#[serde(default)]
struct ProxyConfig {
    #[serde(rename = "static")]
    cidr: CidrConfig,
    auto: String,
}
impl Default for ProxyConfig {
    fn default() -> Self {
        Self {
            cidr: CidrConfig::default(),
            auto: "/etc/proxy/auto.txt".to_string(),
        }
    }
}

#[derive(Debug, Deserialize)]
#[serde(default)]
struct CidrConfig {
    goproxy: String,
    bypass: String,
}
impl Default for CidrConfig {
    fn default() -> Self {
        Self {
            goproxy: "/etc/proxy/goproxy.cidr.txt".to_string(),
            bypass: "/etc/proxy/bypass.cidr.txt".to_string(),
        }
    }
}

#[derive(Debug, Deserialize, Clone)]
struct DnsServerConfig {
    #[serde(rename = "match")]
    match_: String,
    server: String,
    #[serde(default)]
    proxy: bool,
}

//补充探活配置：通过本地 socks5 经隧道连接 ssh_server:port，验证端到端可达
#[derive(Debug, Deserialize)]
#[serde(default)]
struct ProbeConfig {
    enable: bool,
    interval: u64,  // 秒，探活间隔
    timeout: u64,   // 秒，单次探活超时
}
impl Default for ProbeConfig {
    fn default() -> Self {
        Self { enable: false, interval: 60, timeout: 5 }
    }
}

/// 运行时共享状态
struct AppState {
    rules_active: SharedBool,
    tproxy_port: u16,
    ssh_server_ip: Option<String>,
    ssh_port: u16,
    socks5_listen: Option<String>,
    dns_listen: Option<String>,
    dns_client_port: u16,
    cidr_bypass_path: String,
    cidr_goproxy_path: String,
    auto_path: String,
    /// 原始 DNS 服务器配置（match, server, proxy），`@goproxy` 为字面占位符，
    /// 在 startup / reload 时展开为实际 goproxy 域名关键词
    dns_raw: Vec<(String, String, bool)>,
    /// 补充探活配置
    probe_enable: bool,
    probe_interval: Duration,
    probe_timeout: Duration,
}

impl AppState {
    /// 从 dns.listen（如 "127.0.0.1:1053"）解析出 (dns_enabled, dns_listen_port)
    fn dns_listen_info(&self) -> (bool, u16) {
        match &self.dns_listen {
            Some(s) => {
                let port = s.rsplit(':').next().and_then(|p| p.parse::<u16>().ok());
                match port {
                    Some(p) => (true, p),
                    None => (false, 0),
                }
            }
            None => (false, 0),
        }
    }

    /// 构建tproxy规则注入上下文
    fn tproxy_ctx(&self) -> proxy::TproxyContext<'_> {
        let ssh_ip = self.ssh_server_ip.as_deref().unwrap_or("0.0.0.0");
        let (dns_enabled, dns_listen_port) = self.dns_listen_info();
        proxy::TproxyContext {
            tproxy_port: self.tproxy_port,
            ssh_server_ip: ssh_ip,
            dns_enabled,
            dns_listen_port,
            dns_client_port: self.dns_client_port,
            bypass_path: &self.cidr_bypass_path,
            goproxy_path: &self.cidr_goproxy_path,
            rules_active: &self.rules_active,
        }
    }
}

fn main() -> Result<()> {
    let config = parse_args()?;

    //初始化日志级别
    if let Some(level) = logger::level_from_str(&config.log.level) {
        logger::set_log_level(level);
    }
    print_config(&config);

    let state = init_state(&config)?;

    //注册信号处理器：SIGINT/SIGTERM 通过管道唤醒主循环，走统一清理路径优雅退出
    let mut signal_rx = init_signal_pipe()?;

    //加载域名列表 (must-proxy = proxy - bypass，来自 proxy.auto 文件)
    if let Err(e) = proxy::load_proxy_domains(&state.auto_path) {
        log_error!("load proxy domains failed: {}", e);
    }

    //启动代理（dns.servers 非 @goproxy 条目作为关键词 relay；@goproxy 域名随 dns_init 一起加载）
    let dns_servers = state.dns_raw.clone();
    let goproxy_domains = if state.dns_listen.is_some() {
        proxy::snapshot_goproxy_domains()
    } else {
        Vec::new()
    };
    log_info!("dns proxy_map domains: {} goproxy", goproxy_domains.len());
    let c_params = build_c_params(&config, dns_servers, goproxy_domains);
    let c_handle = start_c_proxy(&c_params);

    //等待服务启动
    log_info!("waiting for tunnel and proxy services to start...");
    thread::sleep(Duration::from_secs(3));

    //运行监控循环：inotify 监听文件变化 + 隧道状态回调驱动健康处理（单线程）
    let result = event_loop(&state, &mut signal_rx);

    //退出前清理nft规则(幂等)，并停止C代理
    if state.rules_active.load(Ordering::SeqCst) {
        match cleanup_tproxy_rules() {
            Ok(_) => log_info!("nft ruleset sproxy cleaned"),
            Err(e) => log_error!("nft ruleset clean failed: {}", e),
        }
    }
    stop_c_proxy(c_handle);
    result
}

fn build_c_params(config: &Config, dns_servers: Vec<(String, String, bool)>, dns_domains: Vec<String>) -> CProxyParams {
    let log_level = match config.log.level.as_str() {
        "trace" => 9,
        "debug" => 8,
        "info" => 4,
        "warn" => 3,
        "error" => 2,
        _ => 4,
    };
    CProxyParams {
        log_level,
        ssh_host: config.ssh.host.clone(),
        ssh_port: config.ssh.port,
        ssh_user: config.ssh.user.clone(),
        ssh_key: config.ssh.key.clone(),
        socks5_listen: config.socks5.listen.clone(),
        tproxy_port: config.tproxy.port,
        dns_listen: config.dns.listen.clone(),
        dns_client_port: config.dns.client,
        dns_proxy: config.dns.proxy.clone(),
        dns_servers,
        dns_domains,
    }
}

//读原始DNS服务器条目（@goproxy 由 proxy_map 单独处理，不进入关键词 relay）
fn collect_dns_raw(config: &Config) -> Vec<(String, String, bool)> {
    if config.dns.servers.is_empty() {
        return vec![("*".to_string(), "8.8.8.8:53".to_string(), true)];
    }
    config.dns.servers.iter()
        .filter(|s| s.match_ != "@goproxy")
        .map(|s| (s.match_.clone(), s.server.clone(), s.proxy))
        .collect()
}

// ── 初始化 ──
fn init_state(config: &Config) -> Result<AppState> {
    Ok(AppState {
        rules_active: Arc::new(AtomicBool::new(false)),
        tproxy_port: config.tproxy.port,
        ssh_server_ip: config.ssh.host.clone(),
        ssh_port: config.ssh.port,
        socks5_listen: config.socks5.listen.clone(),
        dns_listen: config.dns.listen.clone(),
        dns_client_port: config.dns.client,
        cidr_bypass_path: config.proxy.cidr.bypass.clone(),
        cidr_goproxy_path: config.proxy.cidr.goproxy.clone(),
        auto_path: config.proxy.auto.clone(),
        dns_raw: collect_dns_raw(config),
        probe_enable: config.probe.enable,
        probe_interval: Duration::from_secs(config.probe.interval),
        probe_timeout: Duration::from_secs(config.probe.timeout),
    })
}

#[repr(C)]
struct PollFd {
    fd: c_int,
    events: i16,
    revents: i16,
}

extern "C" {
    fn poll(fds: *mut PollFd, nfds: c_uint, timeout: c_int) -> c_int;
}

#[derive(Clone, Copy, PartialEq, Eq, Hash, Debug)]
enum WatchTarget {
    ResolvConf,
    BypassCidr,
    GoproxyCidr,
    AutoNames,
}

// 创建信号唤醒管道并注册 SIGINT/SIGTERM。
//信号到达时由signal-hook向管道写端写入，主循环poll读端被唤醒后优雅退出。
fn init_signal_pipe() -> Result<UnixStream> {
    let (rx, tx) = UnixStream::pair().context("create signal pipe failed")?;
    // 读端非阻塞：主循环被唤醒后 drain 管道，避免读完阻塞
    let _ = rx.set_nonblocking(true);
    // 为每个信号 clone 一个写端交给 signal-hook（register 接管 owned fd 的所有权，
    // 注销时各自关闭自己的副本；UnixStream 走 send 路径，写端无需预先设非阻塞）
    pipe::register(SIGINT, tx.try_clone()?).context("register SIGINT failed")?;
    pipe::register(SIGTERM, tx.try_clone()?).context("register SIGTERM failed")?;
    // 原始写端不再需要（已为每个信号各 clone 副本），drop 关闭
    drop(tx);
    Ok(rx)
}

fn event_loop(state: &AppState, signal_rx: &mut UnixStream) -> Result<()> {
    let mask = WatchMask::MODIFY | WatchMask::MOVE_SELF | WatchMask::DELETE_SELF | WatchMask::CREATE;
    let mut inotify = Inotify::init().context("inotify init failed")?;
    let mut watch_map: HashMap<inotify::WatchDescriptor, WatchTarget> = HashMap::new();

    // 注册CIDR/域名列表文件监听
    let watch_list: Vec<(&str, WatchTarget)> = vec![
        (state.cidr_bypass_path.as_str(), WatchTarget::BypassCidr),
        (state.cidr_goproxy_path.as_str(), WatchTarget::GoproxyCidr),
        (state.auto_path.as_str(), WatchTarget::AutoNames),
    ];

    for (path, target) in &watch_list {
        match inotify.watches().add(*path, mask) {
            Ok(wd) => {
                watch_map.insert(wd, *target);
                log_info!("watching {} ({:?})", path, target);
            }
            Err(e) => log_error!("watch {} failed: {}", path, e),
        }
    }

    const POLLIN: i16 = 0x0001;
    //resolv.conf仅在DNS启用时监听
    if state.dns_listen.is_some() {
        match inotify.watches().add(resolv::RESOLV_CONF, mask) {
            Ok(wd) => {
                watch_map.insert(wd, WatchTarget::ResolvConf);
                log_info!("watching {} (ResolvConf)", resolv::RESOLV_CONF);
            }
            Err(e) => log_error!("watch {} failed: {}", resolv::RESOLV_CONF, e),
        }
        //首次立即应用resolv.conf
        resolv::apply_resolv_change(&state.rules_active);
    }

    // 隧道状态唤醒管道读端：C 侧隧道连接/断开转换时写入，主循环据此唤醒处理
    let wake_fd = ffi::tunnel_wake_fd();
    // 信号唤醒 fd：SIGINT/SIGTERM 到达时可读，主循环据此优雅退出
    let signal_fd = signal_rx.as_raw_fd();
    let mut buffer = [0u8; 4096];

    // 补充探活调度：仅当 probe.enable 且 socks5 与 ssh host 均已配置时启用。
    // 启用后 poll 使用有限超时，到期触发一次 socks5 端到端探活。
    let probe_ready = state.probe_enable
        && state.socks5_listen.is_some()
        && state.ssh_server_ip.is_some();
    if state.probe_enable && !probe_ready {
        log_warn!("probe enabled but socks5/ssh.host missing, probe disabled");
    }
    let mut next_probe: Option<Instant> = if probe_ready {
        log_info!("probe scheduled: interval={}s timeout={}s via socks5 -> {}:{}",
                  state.probe_interval.as_secs(), state.probe_timeout.as_secs(),
                  state.ssh_server_ip.as_deref().unwrap_or("?"), state.ssh_port);
        Some(Instant::now() + state.probe_interval)
    } else {
        None
    };

    loop {
        // 计算 poll 超时：探活启用时取到下次探活的剩余毫秒数，否则无限阻塞
        let poll_timeout: c_int = match next_probe {
            Some(np) => {
                let now = Instant::now();
                if now >= np { 0 } else { np.duration_since(now).as_millis().min(i32::MAX as u128) as c_int }
            }
            None => -1,
        };

        // 同时 poll inotify、隧道唤醒管道与信号 fd
        let inotify_fd = inotify.as_raw_fd();
        let mut pfds = [
            PollFd { fd: inotify_fd, events: POLLIN, revents: 0 },
            PollFd { fd: wake_fd, events: POLLIN, revents: 0 },
            PollFd { fd: signal_fd, events: POLLIN, revents: 0 },
        ];
        let ret = unsafe { poll(pfds.as_mut_ptr(), pfds.len() as c_uint, poll_timeout) };

        if ret < 0 {
            let err = std::io::Error::last_os_error();
            if err.kind() == std::io::ErrorKind::Interrupted {
                continue;
            }
            return Err(anyhow::Error::new(err).context("poll failed"));
        }

        // 检查所有 fd 的 revents，非 POLLIN 的异常标志（POLLERR/POLLHUP/POLLNVAL 等）记录 warning
        for (i, pfd) in pfds.iter().enumerate() {
            let extra = pfd.revents & !POLLIN;
            if extra != 0 {
                log_warn!("poll fd[{}] (fd={}) non-POLLIN revents: 0x{:x}", i, pfd.fd, extra);
            }
        }

        // 补充探活：到期时执行一次 socks5 端到端探活，据此管理 nft 规则
        if let Some(np) = next_probe {
            if Instant::now() >= np {
                run_probe(state);
                next_probe = Some(Instant::now() + state.probe_interval);
            }
        }

        // 信号到达：drain 管道后优雅退出，由 main 统一清理
        if (pfds[2].revents & POLLIN) != 0 {
            let mut buf = [0u8; 64];
            loop {
                match signal_rx.read(&mut buf) {
                    Ok(0) | Err(_) => break,
                    Ok(_) => {}
                }
            }
            log_info!("received termination signal (SIGINT/SIGTERM), shutting down gracefully");
            return Ok(());
        }

        // 隧道状态变化：排空唤醒管道，查询权威状态后直接处理 nft 规则
        // （处理始终在主线程进行，与 C 事件线程隔离）
        if (pfds[1].revents & POLLIN) != 0 {
            ffi::drain_tunnel_wake();
            if unsafe { ffi::proxy_tunnel_ready() } != 0 {
                // 隧道连通：若规则未激活则注入
                if !state.rules_active.load(Ordering::SeqCst) {
                    let ctx = state.tproxy_ctx();
                    proxy::apply_tproxy_rules(&ctx);
                }
            } else {
                // 隧道断开：清理 nft 规则
                if state.rules_active.load(Ordering::SeqCst) {
                    log_warn!("tunnel disconnected, cleaning nftables rules");
                    match cleanup_tproxy_rules() {
                        Ok(_) => {
                            log_info!("nft ruleset cleaned");
                            state.rules_active.store(false, Ordering::SeqCst);
                        }
                        Err(e) => log_error!("nft ruleset sproxy failed: {}", e),
                    }
                }
            }
        }

        if (pfds[0].revents & POLLIN) != 0 {
            //读取inotify事件
            let mut changed: HashSet<WatchTarget> = HashSet::new();
            let mut rewatch: Vec<WatchTarget> = Vec::new();

            for event in inotify.read_events(&mut buffer)? {
                if let Some(target) = watch_map.get(&event.wd).copied() {
                    if event.mask.contains(EventMask::IGNORED) {
                        // watch 因文件被替换/删除而失效，需重新添加
                        watch_map.remove(&event.wd);
                        if !rewatch.contains(&target) {
                            rewatch.push(target);
                        }
                    } else {
                        changed.insert(target);
                    }
                }
            }

            //重新添加失效的watch(文件可能被原子替换，如resolv.conf)
            for target in &rewatch {
                if let Some(path) = path_for_target(*target, state) {
                    match inotify.watches().add(&path, mask) {
                        Ok(wd) => {
                            watch_map.insert(wd, *target);
                            changed.insert(*target); // 文件已被替换，触发一次重新加载
                        }
                        Err(e) => log_error!("rewatch {} failed: {}", path, e),
                    }
                }
            }

            //防抖:短暂等待写入完成，再读取剩余事件
            if !changed.is_empty() {
                thread::sleep(Duration::from_millis(100));
                let _ = inotify.read_events(&mut buffer);
            }

            //处理变化的文件
            for target in &changed {
                handle_file_change(*target, state);
            }
        }
    }
}

/// 探活：通过本地socks5经SSH隧道连接 ssh_server:ssh_port，
/// 验证端到端可达。探活成功且规则未激活则注入；探活失败且规则已激活则清理。
fn run_probe(state: &AppState) {
    let (socks5_addr, target_ip) = match (&state.socks5_listen, &state.ssh_server_ip) {
        (Some(s), Some(ip)) => (s.as_str(), ip.as_str()),
        _ => return,
    };
    let ok = socks5_probe(socks5_addr, target_ip, state.ssh_port, state.probe_timeout);
    if ok {
        log_info!("probe: tunnel healthy (socks5 -> {}:{} via tunnel ok)", target_ip, state.ssh_port);
        if !state.rules_active.load(Ordering::SeqCst) {
            let ctx = state.tproxy_ctx();
            proxy::apply_tproxy_rules(&ctx);
        }
    } else {
        log_warn!("probe: tunnel unhealthy (socks5 -> {}:{} via tunnel failed)", target_ip, state.ssh_port);
        if state.rules_active.load(Ordering::SeqCst) {
            match cleanup_tproxy_rules() {
                Ok(_) => {
                    log_info!("probe: nft ruleset cleaned due to probe failure");
                    state.rules_active.store(false, Ordering::SeqCst);
                }
                Err(e) => log_error!("probe: nft ruleset clean failed: {}", e),
            }
        }
    }
}

// SOCKS5 探活：连接本地 socks5 代理，完成无认证握手后发起 CONNECT 到
// (target_ip, target_port)。C 侧会经 SSH 隧道开 direct-tcpip channel，
// 成功返回 true 表示隧道端到端可达。全程带超时，避免阻塞主循环过久。
fn socks5_probe(socks5_addr: &str, target_ip: &str, target_port: u16, timeout: Duration) -> bool {
    let socks5: SocketAddr = match socks5_addr.parse() {
        Ok(a) => a,
        Err(_) => return false,
    };
    let mut stream = match TcpStream::connect_timeout(&socks5, timeout) {
        Ok(s) => s,
        Err(e) => {
            log_warn!("probe: connect socks5 {} failed: {}", socks5_addr, e);
            return false;
        }
    };
    let _ = stream.set_read_timeout(Some(timeout));
    let _ = stream.set_write_timeout(Some(timeout));

    // 1) method negotiation: VER=5, NMETHODS=1, METHOD=0x00 (no auth)
    if let Err(e) = stream.write_all(&[0x05, 0x01, 0x00]) {
        log_warn!("probe: socks5 method write failed: {}", e);
        return false;
    }
    let mut hdr = [0u8; 2];
    if let Err(e) = stream.read_exact(&mut hdr) {
        log_warn!("probe: socks5 method read failed: {}", e);
        return false;
    }
    if hdr[0] != 0x05 || hdr[1] != 0x00 {
        log_warn!("probe: socks5 method rejected (ver={}, method={})", hdr[0], hdr[1]);
        return false;
    }

    // 2) CONNECT request: VER=5, CMD=1(CONNECT), RSV=0, ATYP, DST, PORT
    let mut req: Vec<u8> = vec![0x05, 0x01, 0x00];
    if let Ok(ip) = target_ip.parse::<std::net::Ipv4Addr>() {
        req.push(0x01); // ATYP=IPv4
        req.extend_from_slice(&ip.octets());
    } else {
        req.push(0x03); // ATYP=domain
        req.push(target_ip.len() as u8);
        req.extend_from_slice(target_ip.as_bytes());
    }
    req.push((target_port >> 8) as u8);
    req.push((target_port & 0xff) as u8);
    if let Err(e) = stream.write_all(&req) {
        log_warn!("probe: socks5 connect request write failed: {}", e);
        return false;
    }

    // 3) CONNECT response: 只需读前 4 字节 [VER, REP, RSV, ATYP]，REP=0 即成功
    let mut resp = [0u8; 4];
    if let Err(e) = stream.read_exact(&mut resp) {
        log_warn!("probe: socks5 connect response read failed: {}", e);
        return false;
    }
    if resp[0] != 0x05 {
        log_warn!("probe: socks5 response bad ver={}", resp[0]);
        return false;
    }
    if resp[1] != 0x00 {
        log_warn!("probe: socks5 connect failed (rep={})", resp[1]);
        return false;
    }
    true
}

fn path_for_target(target: WatchTarget, state: &AppState) -> Option<String> {
    match target {
        WatchTarget::ResolvConf => Some(resolv::RESOLV_CONF.to_string()),
        WatchTarget::BypassCidr => Some(state.cidr_bypass_path.clone()),
        WatchTarget::GoproxyCidr => Some(state.cidr_goproxy_path.clone()),
        WatchTarget::AutoNames => Some(state.auto_path.clone()),
    }
}

fn handle_file_change(target: WatchTarget, state: &AppState) {
    match target {
        WatchTarget::ResolvConf => {
            resolv::apply_resolv_change(&state.rules_active);
        }
        WatchTarget::BypassCidr => {
            let ctx = state.tproxy_ctx();
            proxy::reload_bypass(&ctx);
        }
        WatchTarget::GoproxyCidr => {
            if state.rules_active.load(Ordering::SeqCst) {
                proxy::reload_goproxy_cidr(&state.cidr_goproxy_path);
            }
        }
        WatchTarget::AutoNames => {
            if let Err(e) = proxy::load_proxy_domains(&state.auto_path) {
                log_error!("reload proxy domains failed: {}", e);
            } else if state.dns_listen.is_some() {
                //goproxy域名变化 → 热更新C端proxy_relay.proxy_map
                let goproxy_domains = proxy::snapshot_goproxy_domains();
                ffi::reconfigure_dns_servers(&goproxy_domains);
                log_info!("dns proxy_map reloaded: {} goproxy domains", goproxy_domains.len());
            }
        }
    }
}

fn print_config(config: &Config) {
    let upstream = if config.ssh.host.is_some() {
        format!("ssh {}@{}:{}", config.ssh.user.as_deref().unwrap_or(""), config.ssh.host.as_deref().unwrap_or(""), config.ssh.port)
    } else {
        "none".to_string()
    };

    log_info!("sproxy - transparent proxy");
    log_info!("  upstream: {}", upstream);
    match &config.socks5.listen {
        Some(listen) => log_info!("  socks5 server: {}", listen),
        None => log_info!("  socks5 server: disabled"),
    }
    log_info!("  transparent proxy port: {}", config.tproxy.port);
    log_info!("  proxy.static.goproxy: {}", config.proxy.cidr.goproxy);
    log_info!("  proxy.static.bypass: {}", config.proxy.cidr.bypass);
    log_info!("  proxy.auto: {}", config.proxy.auto);
    match &config.dns.listen {
        Some(listen) => {
            match &config.dns.proxy {
                Some(proxy) => log_info!("  dns listen: {} ({} servers), proxy: {}", listen, config.dns.servers.len(), proxy),
                None => log_info!("  dns listen: {} ({} servers)", listen, config.dns.servers.len()),
            }
        }
        None => log_info!("  dns: disabled"),
    }
    log_info!("  log level: {}", config.log.level);
    if config.probe.enable {
        log_info!("  probe: enabled (interval={}s, timeout={}s)", config.probe.interval, config.probe.timeout);
    } else {
        log_info!("  probe: disabled");
    }
}

fn parse_args() -> Result<Config> {
    let args: Vec<String> = args().collect();
    if args.len() < 2 {
        println!(
            "usage: {} <config.yaml>\n\
             example: {} config.yaml",
            args[0], args[0]
        );
        std::process::exit(1);
    }

    let config_path = &args[1];
    let content = fs::read_to_string(config_path).with_context(|| format!("unable to read config {}", config_path))?;
    let config: Config = serde_yaml::from_str(&content).with_context(|| format!("parse config {} failed", config_path))?;
    Ok(config)
}