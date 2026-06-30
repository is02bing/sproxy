#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/**
 * DNS 解析回调：当代理域名被解析出 IP 时调用
 * @param ip      解析出的 IP 地址字符串，如 "1.2.3.4"
 * @param domain  原始域名，如 "github.com"
 */
typedef void (*dns_resolve_cb)(const char *ip, const char *domain, int proxy);

/**
 * 隧道状态变化回调类型
 * @param ready  1=隧道刚连通, 0=隧道刚断开
 *
 * 由 C 侧在隧道连接/断开转换时调用，Rust 侧据此唤醒主循环处理，
 * 无需 Rust 周期性轮询。回调运行在 C 事件线程，必须轻量且线程安全。
 */
typedef void (*tunnel_state_cb)(int ready);/**
 * 设置 rdns 默认上游 nameserver（由 Rust 侧 inotify 解析 resolv.conf 后调用）
 * @param servers  外部 nameserver IP 字符串数组（已跳过 127.x 本地 DNS）
 * @param count    servers 数组元素个数
 */
void proxy_set_default_relay(const char * const *servers, int count);

/* ── DNS 服务器条目 ── */
typedef struct {
    const char *match;     /* 匹配规则，如 "*" */
    const char *server;    /* DNS 服务器地址，如 "8.8.8.8:53" */
    bool        proxy;     /* 解析结果是否加入代理集合 */
} proxy_dns_server_t;

/**
 * 热更新 @goproxy 域名到 proxy_relay.proxy_map（auto.txt 重载时调用）
 * @param domain_count  域名串个数
 * @param domains       域名串数组（无前导点），C 侧会拷贝内容
 */
void proxy_dns_reconfigure(int domain_count, const char * const *domains);

/* ── 代理配置结构体（由 Rust 填充） ── */
typedef struct {
    /* log / misc */
    int         log_level;
    int         fast_mode;

    /* upstream: SSH */
    const char *ssh_host;
    uint16_t    ssh_port;
    const char *ssh_user;
    const char *ssh_key;

    /* local SOCKS5 server (for browsers, e.g. "127.0.0.1:1080") */
    const char *socks5_listen;       /* NULL to disable */

    /* tcp (tproxy) */
    uint16_t    tproxy_port;

    /* dns */
    const char *dns_listen;          /* 如 "127.0.0.1:1053"，NULL 则不启用 */
    uint16_t    dns_client_port;
    const char *dns_proxy;           /* 如 "8.218.215.3:1053"，NULL 则不启用 */
    int         dns_server_count;
    proxy_dns_server_t *dns_servers; /* 数组指针 */
    int         dns_domain_count;            /* 初始 @goproxy 域名个数 */
    const char * const *dns_domains;         /* 初始 @goproxy 域名串数组（无前导点） */

    /* 隧道状态变化回调（C 侧在隧道连接/断开转换时调用，运行在 C 事件线程） */
    tunnel_state_cb tunnel_state_cb;

    /* DNS 解析回调（C 侧解析出 IP 时调用） */
    dns_resolve_cb  dns_resolve_cb;
} proxy_config_t;

/**
 * 启动代理服务 — 直接传入配置结构体
 * socks5、tcp、dns 按配置条件启动
 */
int  proxy_start(const proxy_config_t *cfg);

/**
 * 请求事件循环退出（线程安全，可由任意线程调用）。
 * 不释放资源；实际清理由 C 事件线程在 event_dispatch 返回后内部完成。
 * 外部线程调用后应等待 proxy_start 返回（即 C 线程退出）再继续。
 */
void proxy_stop(void);

/**
 * 获取当前 DNS 回调（供 dns.c 内部使用）
 */
dns_resolve_cb proxy_get_dns_callback(void);

/**
 * 检查 SSH 隧道是否就绪（已连接）
 * @return 1=已连接, 0=未连接
 */
int proxy_tunnel_ready(void);

/**
 * 通知 Rust 侧隧道状态发生变化（连接/断开）。供 tunnel.c / tcp.c 在状态转换点调用。
 * @param ready  1=隧道刚连通, 0=隧道刚断开
 */
void proxy_notify_tunnel_state(int ready);
