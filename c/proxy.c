#include <stdlib.h>
#include <string.h>
#include <event.h>
#include "base.h"
#include "tcp.h"
#include "dns.h"
#include "trace.h"
#include "proxy.h"

/* 全局状态 */
static struct event_base *g_event_base = NULL;
static proxy_tcp_t       *g_tcp = NULL;
static dns_t             *g_dns = NULL;
static int                g_running = 0;

/* DNS 回调 */
static dns_resolve_cb g_dns_resolve_cb = NULL;

/* 隧道状态变化回调（由 Rust 侧注册，在隧道连接/断开转换时调用） */
static tunnel_state_cb g_tunnel_state_cb = NULL;

/* 前置声明：proxy_cleanup 定义在 proxy_start 之后 */
static void proxy_cleanup(void);

dns_resolve_cb proxy_get_dns_callback(void) {
	return g_dns_resolve_cb;
}

/* 通知 Rust 侧隧道状态发生变化（连接/断开）。供 tunnel.c / tcp.c 在状态转换点调用。
 * 运行在 C 事件线程，仅做一次函数指针调用，实际处理由 Rust 主线程异步完成。 */
void proxy_notify_tunnel_state(int ready) {
	if (g_tunnel_state_cb) {
		g_tunnel_state_cb(ready);
	}
}

/* 设置 rdns 默认上游 nameserver（由 Rust 侧 inotify 解析 resolv.conf 后调用） */
void proxy_set_default_relay(const char * const *servers, int count) {
	if (g_dns != NULL) {
		dns_set_default_relay(g_dns, servers, count);
	}
}

/* 热更新 @goproxy 域名到 proxy_relay.proxy_map（auto.txt 重载时由 Rust 侧调用） */
void proxy_dns_reconfigure(int domain_count, const char * const *domains) {
	if (g_dns != NULL) {
		dns_reconfigure_servers(g_dns, domain_count, domains);
	}
}

int proxy_start(const proxy_config_t *cfg) {
	g_dns_resolve_cb = cfg->dns_resolve_cb;
	g_tunnel_state_cb = cfg->tunnel_state_cb;

	evutil_secure_rng_init();
	if (event_get_struct_event_size() != sizeof(struct event)) {
		fprintf(stderr, "libevent event_get_struct_event_size() != sizeof(struct event)! check and recompile");
		return -1;
	}

	base_init(cfg->log_level, cfg->fast_mode);

	g_event_base = event_init();

	/* Create proxy instance if tproxy or socks5 server is configured */
	if ((cfg->tproxy_port > 0 || cfg->socks5_listen != NULL) && cfg->ssh_host != NULL) {
		g_tcp = malloc(sizeof(proxy_tcp_t));
		if (g_tcp == NULL) {
			ERROR("tcp alloc failed");
			goto shutdown;
		}
		memset(g_tcp, 0, sizeof(proxy_tcp_t));
		g_tcp->base = g_event_base;
		if (proxy_tcp_init(g_tcp,
                           cfg->ssh_host, cfg->ssh_port, cfg->ssh_user, cfg->ssh_key,
		                   cfg->tproxy_port,
		                   cfg->socks5_listen)) {
			ERROR("proxy init failed");
			goto shutdown;
		}
	}

	/* dns */
	if (cfg->dns_listen != NULL && cfg->dns_server_count > 0) {
		g_dns = malloc(sizeof(dns_t));
		if (g_dns == NULL) {
			ERROR("dns alloc failed");
			goto shutdown;
		}
		memset(g_dns, 0, sizeof(dns_t));
		g_dns->base = g_event_base;
		if (dns_init(g_dns, cfg->dns_listen, cfg->dns_client_port, cfg->dns_proxy, cfg->dns_server_count, cfg->dns_servers, cfg->dns_domain_count, cfg->dns_domains)) {
			ERROR("dns init failed");
			goto shutdown;
		}
	}

	if (g_tcp == NULL && g_dns == NULL) {
		ERROR("no service configured (need tproxy/socks5+ssh or dns)");
		goto shutdown;
	}

	g_running = 1;
	event_dispatch();

shutdown:
	/* event_dispatch 已返回（loopbreak 或无事件），此时事件线程空闲，
	 * 在本线程内清理是安全的——所有 libevent 调用都在同一线程。 */
	proxy_cleanup();
	return 0;
}

/* 完整释放所有资源。仅在 C 事件线程内调用（event_dispatch 返回后），
 * 因为 libevent 的 event_del/event_base_free 非线程安全。
 * 对应的对外停止接口为 proxy_stop()（仅请求退出）。 */
static void proxy_cleanup(void) {
	if (g_dns != NULL) {
		dns_term(g_dns);
		free(g_dns);
		g_dns = NULL;
	}

	if (g_tcp != NULL) {
		proxy_tcp_term(g_tcp);
		free(g_tcp);
		g_tcp = NULL;
	}

	base_term();

	if (g_event_base != NULL) {
		event_base_free(g_event_base);
		g_event_base = NULL;
	}

	g_running = 0;
}

/* 请求事件循环退出，可由任意线程调用（event_base_loopbreak 线程安全）。
 * 不释放任何资源；实际清理由 C 事件线程在 event_dispatch 返回后的
 * proxy_cleanup() 完成。外部线程调用后应等待 proxy_start 返回（即 C 线程
 * 退出）后再继续，以确保资源已释放。 */
void proxy_stop(void) {
	if (g_event_base != NULL) {
		event_base_loopbreak(g_event_base);
	}
}

int proxy_tunnel_ready(void) {
	if (g_tcp && g_tcp->tunnel) {
		return ssh_tunnel_is_connected(g_tcp->tunnel);
	}
	return 0;
}
