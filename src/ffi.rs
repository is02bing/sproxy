use std::ffi::{c_void, CStr, CString};
use std::os::raw::{c_char, c_int};
use std::os::unix::io::IntoRawFd;
use std::os::unix::net::UnixStream;
use std::sync::OnceLock;
use std::thread;

use crate::proxy;
use crate::log_error;

//DNS 解析回调类型
type DnsResolveCb = extern "C" fn(*const c_char, *const c_char, c_int);

/// 隧道状态变化回调类型（C 侧在隧道连接/断开转换时调用）
type TunnelStateCb = extern "C" fn(c_int);

//C端DNS服务器条目（与 proxy_dns_server_t 对应）
#[repr(C)]
pub struct ProxyDnsServer {
    pub match_: *const c_char,
    pub server: *const c_char,
    pub proxy: bool,
}

//C端代理配置结构体（与 proxy_config_t 对应）
#[repr(C)]
pub struct ProxyConfig {
    pub log_level: c_int,
    pub fast_mode: c_int,
    pub ssh_host: *const c_char,
    pub ssh_port: u16,
    pub ssh_user: *const c_char,
    pub ssh_key: *const c_char,
    pub socks5_listen: *const c_char,
    pub tproxy_port: u16,
    pub dns_listen: *const c_char,
    pub dns_client_port: u16,
    pub dns_proxy: *const c_char,
    pub dns_server_count: c_int,
    pub dns_servers: *const ProxyDnsServer,
    pub dns_domain_count: c_int,
    pub dns_domains: *const *const c_char,
    pub tunnel_state_cb: TunnelStateCb,
    pub dns_resolve_cb: DnsResolveCb,
}

extern "C" {
    pub fn proxy_start(cfg: *const ProxyConfig) -> c_int;
    pub fn proxy_stop();
    pub fn proxy_tunnel_ready() -> c_int;
    pub fn proxy_set_default_relay(servers: *const *const c_char, count: c_int);
    pub fn proxy_dns_reconfigure(domain_count: c_int, domains: *const *const c_char);
}

extern "C" {
    fn read(fd: c_int, buf: *mut c_void, count: usize) -> isize;
    fn write(fd: c_int, buf: *const c_void, count: usize) -> isize;
}

//DNS回调入口：C侧DNS解析出IP时调用
//仅做参数检查和 C→Rust字符串转换，决策逻辑委托给 proxy::on_dns_resolve。
extern "C" fn on_dns_resolve(ip_ptr: *const c_char, domain_ptr: *const c_char, proxy: c_int) {
    let ip = unsafe { CStr::from_ptr(ip_ptr).to_string_lossy().into_owned() };
    let domain = unsafe { CStr::from_ptr(domain_ptr).to_string_lossy().into_owned() };
    proxy::on_dns_resolve(&ip, &domain, proxy != 0);
}

//设置rdns默认上游nameserver
pub fn set_default_relay(ips: &[String]) {
    let cstrs: Vec<CString> = ips.iter().filter_map(|s| CString::new(s.as_str()).ok()).collect();
    let ptrs: Vec<*const c_char> = cstrs.iter().map(|s| s.as_ptr()).collect();
    unsafe {
        proxy_set_default_relay(
            if ptrs.is_empty() { std::ptr::null() } else { ptrs.as_ptr() },
            ptrs.len() as c_int,
        );
    }
}

/// 热更新 @goproxy 域名到 C 端 proxy_relay.proxy_map（auto.txt 重载后调用）
///
/// C 侧 dns_reconfigure_servers 会拷贝域名串到自有存储，CString 仅需在调用期间存活。
pub fn reconfigure_dns_servers(domains: &[String]) {
    let cstrs: Vec<CString> = domains.iter()
        .filter_map(|s| CString::new(s.as_str()).ok())
        .collect();
    let ptrs: Vec<*const c_char> = cstrs.iter().map(|s| s.as_ptr()).collect();
    unsafe {
        proxy_dns_reconfigure(
            ptrs.len() as c_int,
            if ptrs.is_empty() { std::ptr::null() } else { ptrs.as_ptr() },
        );
    }
}

// Send 安全的配置参数（可跨线程传递，在目标线程内构建 C 结构体）
pub struct CProxyParams {
    pub log_level: c_int,
    pub ssh_host: Option<String>,
    pub ssh_port: u16,
    pub ssh_user: Option<String>,
    pub ssh_key: Option<String>,
    pub socks5_listen: Option<String>,
    pub tproxy_port: u16,
    pub dns_listen: Option<String>,
    pub dns_client_port: u16,
    pub dns_proxy: Option<String>,
    pub dns_servers: Vec<(String, String, bool)>,
    pub dns_domains: Vec<String>,
}

//启动C代理，返回 C 事件线程的 JoinHandle。
//调用方在退出前应调用 stop_c_proxy(handle) 请求停止并等待线程退出，
//确保 C 侧资源在事件线程内安全释放（libevent 非线程安全）。
//
//所有 CString / Vec 在闭包内构造并随线程存活，cfg 的裸指针始终指向有效内存；
//闭包只捕获 Send 类型（CString/Vec/原语/函数指针），无需 unsafe impl Send。
pub fn start_c_proxy(params: &CProxyParams) -> thread::JoinHandle<()> {
    let ssh_host = params.ssh_host.as_ref().map(|s| CString::new(s.as_str()).expect("invalid ssh_host"));
    let ssh_user = params.ssh_user.as_ref().map(|s| CString::new(s.as_str()).expect("invalid ssh_user"));
    let ssh_key = params.ssh_key.as_ref().map(|s| CString::new(s.as_str()).expect("invalid ssh_key"));
    let socks5_listen = params.socks5_listen.as_ref().map(|s| CString::new(s.as_str()).expect("invalid socks5_listen"));
    let dns_listen = params.dns_listen.as_ref().map(|s| CString::new(s.as_str()).expect("invalid dns_listen"));
    let dns_proxy = params.dns_proxy.as_ref().map(|s| CString::new(s.as_str()).expect("invalid dns_proxy"));

    let dns_cs: Vec<(CString, CString)> = params.dns_servers.iter()
        .map(|(m, s, _)| {
            (CString::new(m.as_str()).expect("invalid dns match"),
             CString::new(s.as_str()).expect("invalid dns server"))
        })
        .collect();
    let dns_proxy_flags: Vec<bool> = params.dns_servers.iter().map(|(_, _, p)| *p).collect();

    let domain_cs: Vec<CString> = params.dns_domains.iter()
        .filter_map(|s| CString::new(s.as_str()).ok())
        .collect();

    //标量值复制，避免在闭包内借用 params（params 不一定 'static）
    let log_level = params.log_level;
    let ssh_port = params.ssh_port;
    let tproxy_port = params.tproxy_port;
    let dns_client_port = params.dns_client_port;

    thread::spawn(move || {
        //在线程内构建 dns_entries / domain_ptrs / cfg，确保裸指针指向的数据
        //（dns_cs / domain_cs / 各 CString）在线程退出前一直有效。
        let dns_entries: Vec<ProxyDnsServer> = dns_cs.iter()
            .zip(dns_proxy_flags.iter())
            .map(|((cm, cs), proxy)| ProxyDnsServer {
                match_: cm.as_ptr(),
                server: cs.as_ptr(),
                proxy: *proxy,
            })
            .collect();
        let domain_ptrs: Vec<*const c_char> = domain_cs.iter().map(|s| s.as_ptr()).collect();

        let cfg = ProxyConfig {
            log_level,
            fast_mode: 0,
            ssh_host: ssh_host.as_ref().map_or(std::ptr::null(), |s| s.as_ptr()),
            ssh_port,
            ssh_user: ssh_user.as_ref().map_or(std::ptr::null(), |s| s.as_ptr()),
            ssh_key: ssh_key.as_ref().map_or(std::ptr::null(), |s| s.as_ptr()),
            socks5_listen: socks5_listen.as_ref().map_or(std::ptr::null(), |s| s.as_ptr()),
            tproxy_port,
            dns_listen: dns_listen.as_ref().map_or(std::ptr::null(), |s| s.as_ptr()),
            dns_client_port,
            dns_proxy: dns_proxy.as_ref().map_or(std::ptr::null(), |s| s.as_ptr()),
            dns_server_count: dns_entries.len() as c_int,
            dns_servers: if dns_entries.is_empty() { std::ptr::null() } else { dns_entries.as_ptr() },
            dns_domain_count: domain_ptrs.len() as c_int,
            dns_domains: if domain_ptrs.is_empty() { std::ptr::null() } else { domain_ptrs.as_ptr() },
            tunnel_state_cb: on_tunnel_state,
            dns_resolve_cb: on_dns_resolve,
        };

        let result = unsafe { proxy_start(&cfg) };
        if result != 0 {
            log_error!("proxy exited with error: {}", result);
        }
        //cfg / dns_entries / domain_ptrs 在此 drop，此时 proxy_start 已返回，安全。
    })
}

//停止代理：请求 C 事件循环退出，并等待 C 线程完成资源释放后返回。
//proxy_stop 仅 loopbreak（线程安全），实际清理由 C 事件线程在
//event_dispatch 返回后内部完成，所以 join 是必要的。
pub fn stop_c_proxy(handle: thread::JoinHandle<()>) {
    unsafe { proxy_stop() };
    if let Err(e) = handle.join() {
        log_error!("C proxy thread panicked: {:?}", e);
    }
}

/* ── 隧道状态变化唤醒机制 ──
 * C侧事件线程在隧道连接/断开转换时调用on_tunnel_state，向唤醒管道写 字节；
 * Rust主线程poll该管道读端，被唤醒后查询proxy_tunnel_ready()并执行处理。
 * 管道单写单读、写端非阻塞，确保C线程不阻塞、且与Rust主线程隔离。 */
static TUNNEL_WAKE: OnceLock<(c_int, c_int)> = OnceLock::new(); // (read_fd, write_fd)

fn tunnel_wake_fds() -> &'static (c_int, c_int) {
    TUNNEL_WAKE.get_or_init(|| {
        let (rx, tx) = UnixStream::pair().expect("tunnel wake pipe");
        // 写端非阻塞：管道满时丢弃（主循环已有待处理唤醒，查询权威状态即可）
        let _ = tx.set_nonblocking(true);
        let _ = rx.set_nonblocking(true);
        (rx.into_raw_fd(), tx.into_raw_fd())
    })
}

//唤醒管道读端fd（供主循环加入poll集合）
pub fn tunnel_wake_fd() -> c_int {
    tunnel_wake_fds().0
}

//排空唤醒管道（主循环被唤醒后调用，丢弃全部待处理字节）
pub fn drain_tunnel_wake() {
    let fd = tunnel_wake_fds().0;
    let mut buf = [0u8; 64];
    unsafe {
        loop {
            if read(fd, buf.as_mut_ptr() as *mut c_void, buf.len()) <= 0 {
                break;
            }
        }
    }
}

//侧隧道状态变化回调-运行在C端线程：仅向唤醒管道写1字节
extern "C" fn on_tunnel_state(ready: c_int) {
    let wfd = tunnel_wake_fds().1;
    let buf = [if ready != 0 { 1u8 } else { 0u8 }];
    unsafe {
        let _ = write(wfd, buf.as_ptr() as *const c_void, 1);
    }
}
