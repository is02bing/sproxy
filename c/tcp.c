#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <assert.h>
#include <arpa/inet.h>
#include <event.h>
#include "list.h"
#include "tcp.h"
#include "tunnel.h"
#include "socks5.h"
#include "proxy.h"
#include "utils.h"
#include "trace.h"

static void proxy_tcp_clean_idle(proxy_tcp_t *instance);
static void proxy_tcp_list_add(proxy_tcp_t *self, tcp_t *relay);
static void proxy_tcp_list_del(tcp_t *relay);
static void proxy_tcp_bufferevent_free(struct bufferevent *buffev);
static void proxy_tcp_clean_socks5_idle(proxy_tcp_t *proxy);

static void proxy_tcp_drop_socks5(socks_t *conn);
static void proxy_tcp_touch_socks5(socks_t *conn);

//ssh tunnel callbacks (shared by tproxy & socks5)
static void on_ssh_channel_open(ssh_channel_t *ch, void *ctx)
{
	(void)ctx;
	if (!ch->user_data) {
		ERROR("on_ssh_channel_open: user_data is NULL, skipping");
		return;
	}

	int conn_type = *(int *)ch->user_data;
	if (conn_type == CONN_TYPE_TPROXY) {
		tcp_t *relay = (tcp_t *)ch->user_data;
		bufferevent_setwatermark(relay->client_event, EV_READ | EV_WRITE, 0, 4096);
		bufferevent_enable(relay->client_event, EV_READ | EV_WRITE);
		INFO("client: %n -> %s:%d, tunnel ready", &relay->client_addr, ch->dest_host, ch->dest_port);
	} else if (conn_type == CONN_TYPE_SOCKS5) {
		socks_t *conn = (socks_t *)ch->user_data;
		TRACE("on_ssh_channel_open: socks5 conn=%p state=%d client_event=%p",(void *)conn, conn->state, (void *)conn->client_event);
		//send socks success reply
		socks5_reply_t reply;
		reply.ver = socks5_version;
		reply.rep = socks5_reply_succeeded;
		reply.rsv = 0x00;
		reply.atyp = socks5_addrtype_ipv4;
		bufferevent_write(conn->client_event, &reply, sizeof(reply));

		socks5_addr_ipv4_t bind;
		memset(&bind, 0, sizeof(bind));
		bufferevent_write(conn->client_event, &bind, sizeof(bind));

		conn->state = socks5_state_relay;
		bufferevent_setwatermark(conn->client_event, EV_READ | EV_WRITE, 0, 4096);
		bufferevent_enable(conn->client_event, EV_READ | EV_WRITE);
		INFO("socks5: %n -> %s:%d, tunnel ready", &conn->client_addr, ch->dest_host, ch->dest_port);
	}
}

static void on_ssh_channel_close(ssh_channel_t *ch, void *ctx)
{
	(void)ctx;
	void *ud = ch->user_data;
	if (!ud) {
		ssh_tunnel_close_channel(ch);
		return;
	}

	int conn_type = *(int *)ud;
	ch->user_data = NULL;   /* prevent double callback */

	if (conn_type == CONN_TYPE_TPROXY) {
		tcp_t *relay = (tcp_t *)ud;
		/* Do NOT pre-null relay->ssh_ch: proxy_tcp_drop_relay() needs it
		   non-NULL to call ssh_tunnel_close_channel() (which list_del + free ch).
		   ssh_tunnel_close_channel() does not re-invoke on_channel_close, so
		   there is no double-callback risk. */
		proxy_tcp_drop_relay(relay);
	} else if (conn_type == CONN_TYPE_SOCKS5) {
		socks_t *conn = (socks_t *)ud;
		proxy_tcp_drop_socks5(conn);
	}
}

static void on_ssh_channel_touch(ssh_channel_t *ch, void *ctx)
{
	(void)ctx;
	if (!ch->user_data) return;
	int conn_type = *(int *)ch->user_data;
	if (conn_type == CONN_TYPE_TPROXY) {
		tcp_t *relay = (tcp_t *)ch->user_data;
		proxy_tcp_touch_relay(relay);
	} else if (conn_type == CONN_TYPE_SOCKS5) {
		socks_t *conn = (socks_t *)ch->user_data;
		proxy_tcp_touch_socks5(conn);
	}
}

/* ──────────────────────────────────────────
 *  tproxy relay management
 * ────────────────────────────────────────── */

static void proxy_tcp_clean_idle(proxy_tcp_t* instance) {
	struct timeval now;
	gettimeofday(&now, NULL);
	tcp_t *tmp, *relay;
	list_for_each_entry_safe(relay, tmp, &instance->tproxy_conn_list, list) {
		struct timeval idle;
		timersub(&now, &relay->last_time, &idle);
		if (idle.tv_sec >= instance->tproxy_conn_max_idle_sec) {
			proxy_tcp_drop_relay(relay);
		}
	}
}

static void proxy_tcp_list_add(proxy_tcp_t *self, tcp_t *relay) {
	assert(list_empty(&relay->list));
	list_add(&relay->list, &self->tproxy_conn_list);
	self->tproxy_conn_active_num++;
}

static void proxy_tcp_list_del(tcp_t *relay) {
	proxy_tcp_t* proxy = relay->proxy;
	if (!list_empty(&relay->list)) {
		proxy->tproxy_conn_active_num--;
		list_del(&relay->list);
	}
}

static void proxy_tcp_bufferevent_free(struct bufferevent *buffev)
{
	int fd = bufferevent_getfd(buffev);
	if (bufferevent_setfd(buffev, -1)) {
		ERROR("bufferevent_setfd");
	}
	bufferevent_free(buffev);
	if (fd != -1) {
		close(fd);
	}
}

//tproxy: client event callbacks
static void proxy_tcp_client_on_event(struct bufferevent *buffev, short what, void* _arg) {
	tcp_t *relay = _arg;
	assert(buffev == relay->client_event);
	if (what & BEV_EVENT_CONNECTED) {
		TRACE("client: %n, event: connected", &relay->client_addr);
	} else if (what & BEV_EVENT_ERROR) {
		TRACE("client: %n, event: error", &relay->client_addr);
		proxy_tcp_drop_relay(relay);
	} else if (what & BEV_EVENT_TIMEOUT) {
		TRACE("client: %n, event: timeout", &relay->client_addr);
		proxy_tcp_drop_relay(relay);
	} else if (what & BEV_EVENT_EOF) {
		TRACE("client: %n, event: eof", &relay->client_addr);
		proxy_tcp_drop_relay(relay);
	}
}

static void proxy_tcp_client_read_cb(struct bufferevent *buffev, void* _arg) {
	tcp_t *relay = _arg;
	assert(buffev == relay->client_event);

	char data[8192];
	int rlen = evbuffer_remove(relay->client_event->input, data, sizeof(data));
	if (rlen <= 0) {
		WARNING("client: %n, read empty", &relay->client_addr);
		return;
	}

	proxy_tcp_touch_relay(relay);

	if (!relay->ssh_ch || relay->ssh_ch->state == SSH_CH_STATE_CLOSED) {
		proxy_tcp_drop_relay(relay);
		return;
	}
	int wlen = ssh_channel_write_data(relay->ssh_ch, data, rlen);
	if (wlen < 0) {
		ERROR("client: %n, tunnel write error", &relay->client_addr);
		proxy_tcp_drop_relay(relay);
		return;
	}
	TRACE("client: %n -> %s:%d, data:%d byte", &relay->client_addr, relay->ssh_ch->dest_host, relay->ssh_ch->dest_port, rlen);

	relay->tx_bytes += rlen;
}

static void proxy_tcp_client_write_cb(struct bufferevent* buffev, void* _arg) {
	tcp_t *relay = _arg;
	assert(buffev == relay->client_event);
}

//tproxy: accept
static void proxy_tcp_accept(int listen_fd, short what, void *_arg) {
	(void)what;
	proxy_tcp_t* proxy = _arg;
	tcp_t *relay = NULL;

	if (!proxy->tunnel || !ssh_tunnel_is_connected(proxy->tunnel)) {
		struct sockaddr_in dummy;
		socklen_t dummy_len = sizeof(dummy);
		int fd = accept(listen_fd, (struct sockaddr *)&dummy, &dummy_len);
		if (fd >= 0) close(fd);
		return;
	}

	struct sockaddr_in src_addr, dst_addr;
	socklen_t addrlen = sizeof(src_addr);
	int fd = -1;
	fd = accept(listen_fd, (struct sockaddr *) &src_addr, &addrlen);
	if (fd == -1) {
		const int e = errno;
		if (e == ENFILE || e == EMFILE || e == ENOBUFS || e == ENOMEM) {
			proxy_tcp_clean_idle(proxy);
		}
		goto proxy_accept_fail;
	}

	relay = malloc(sizeof(tcp_t));
	if (!relay) {
		ERROR("malloc failed");
		goto proxy_accept_fail;
	}
	memset(relay, 0, sizeof(tcp_t));
	relay->conn_type = CONN_TYPE_TPROXY;
	relay->proxy = proxy;
	INIT_LIST_HEAD(&relay->list);

	addrlen = sizeof(src_addr);
	if (getsockname(fd, (struct sockaddr *) &src_addr, &addrlen)) {
		ERROR("getsockname");
		goto proxy_accept_fail;
	}
	if (get_dest_addr(fd, &dst_addr)) {
		goto proxy_accept_fail;
	}
	memcpy(&relay->client_addr, &src_addr, sizeof(src_addr));
	if (fcntl_nonblock(fd)) {
		ERROR("fcntl nonblock");
		goto proxy_accept_fail;
	}

	if (set_tcp_keepalive(fd)) {
		goto proxy_accept_fail;
	}

	gettimeofday(&relay->create_time, NULL);
	gettimeofday(&relay->last_time, NULL);

	relay->client_event = bufferevent_socket_new(proxy->base, fd, BEV_OPT_CLOSE_ON_FREE);
	if (!relay->client_event) {
		ERROR("bufferevent_new");
		goto proxy_accept_fail;
	}
	fd = -1;
	bufferevent_setcb(relay->client_event, proxy_tcp_client_read_cb, proxy_tcp_client_write_cb,
	                   proxy_tcp_client_on_event, relay);

	char dest_host[64];
	inet_ntop(AF_INET, &dst_addr.sin_addr, dest_host, sizeof(dest_host));
	int dest_port = ntohs(dst_addr.sin_port);

	relay->ssh_ch = ssh_tunnel_open_channel(proxy->tunnel, dest_host, dest_port,
	                                          relay->client_event, relay);
	if (!relay->ssh_ch) {
		ERROR("client: %n -> %s:%d, ssh channel open failed", &relay->client_addr, dest_host, dest_port);
		goto proxy_accept_fail;
	}

	proxy_tcp_list_add(proxy, relay);
	INFO("new client: %n -> %s:%d (tproxy)", &relay->client_addr, dest_host, dest_port);
	return;

proxy_accept_fail:
	if (relay) {
		proxy_tcp_drop_relay(relay);
	}
	if (fd != -1) {
		close(fd);
	}
}

void proxy_tcp_start_relay(tcp_t* relay) {
	TRACE("client: %n -> %s:%d, start relay (ssh)", &relay->client_addr,
	      relay->ssh_ch ? relay->ssh_ch->dest_host : "?",
	      relay->ssh_ch ? relay->ssh_ch->dest_port : 0);
	bufferevent_setwatermark(relay->client_event, EV_READ | EV_WRITE, 0, 4096);
	bufferevent_enable(relay->client_event, EV_READ | EV_WRITE);
}

void proxy_tcp_touch_relay(tcp_t *relay)
{
	gettimeofday(&relay->last_time, NULL);
}

void proxy_tcp_drop_relay(tcp_t *relay) {
	int fd = relay->client_event ? bufferevent_getfd(relay->client_event) : -1;

	if (relay->ssh_ch) {
		relay->ssh_ch->user_data = NULL;
		ssh_tunnel_close_channel(relay->ssh_ch);
		relay->ssh_ch = NULL;
	}
	INFO("close client: fd=%d, %n, tx:%l, rx:%l", fd, &relay->client_addr,
	     (long)relay->tx_bytes, (long)relay->rx_bytes);

	if (relay->client_event) {
		proxy_tcp_bufferevent_free(relay->client_event);
	}

	proxy_tcp_list_del(relay);
	free(relay);
}

/* ──────────────────────────────────────────
 *  socks5 server implementation
 * ────────────────────────────────────────── */

static void proxy_tcp_clean_socks5_idle(proxy_tcp_t *proxy) {
	struct timeval now;
	gettimeofday(&now, NULL);
	socks_t *tmp, *conn;
	list_for_each_entry_safe(conn, tmp, &proxy->socks5_conn_list, list) {
		struct timeval idle;
		timersub(&now, &conn->last_time, &idle);
		if (idle.tv_sec >= proxy->socks5_conn_max_idle_sec) {
			proxy_tcp_drop_socks5(conn);
		}
	}
}

static void proxy_tcp_touch_socks5(socks_t *conn) {
	gettimeofday(&conn->last_time, NULL);
}

static void proxy_tcp_drop_socks5(socks_t *conn) {
	proxy_tcp_t *proxy = conn->proxy;

	if (conn->ssh_ch) {
		conn->ssh_ch->user_data = NULL;
		ssh_tunnel_close_channel(conn->ssh_ch);
		conn->ssh_ch = NULL;
	}

	if (conn->client_event) {
		int fd = bufferevent_getfd(conn->client_event);
		proxy_tcp_bufferevent_free(conn->client_event);
		conn->client_event = NULL;
		INFO("socks5: close fd=%d, %n -> %s:%d, tx:%l, rx:%l", fd, &conn->client_addr,
		     conn->dest_host, conn->dest_port, (long)conn->tx_bytes, (long)conn->rx_bytes);
	}

	if (!list_empty(&conn->list)) {
		proxy->socks5_conn_active_num--;
		list_del(&conn->list);
	}

	free(conn);
}

// send a socks5 error reply and drop the connection
static void socks5_server_send_error(socks_t *conn, uint8_t rep) {
	socks5_reply_t reply;
	reply.ver = socks5_version;
	reply.rep = rep;
	reply.rsv = 0x00;
	reply.atyp = socks5_addrtype_ipv4;
	bufferevent_write(conn->client_event, &reply, sizeof(reply));

	socks5_addr_ipv4_t bind;
	memset(&bind, 0, sizeof(bind));
	bufferevent_write(conn->client_event, &bind, sizeof(bind));
}

// socks5 server: read from client (browser)
static void socks5_server_client_read_cb(struct bufferevent *be, void *arg) {
	socks_t *conn = (socks_t *)arg;
	struct evbuffer *input = bufferevent_get_input(be);

	proxy_tcp_touch_socks5(conn);

	if (conn->state == socks5_state_method) {
		size_t available = evbuffer_get_length(input);
		if (available < 2) return;

		uint8_t ver;
		evbuffer_remove(input, &ver, 1);
		if (ver != socks5_version) {
			proxy_tcp_drop_socks5(conn);
			return;
		}

		uint8_t nmethods;
		evbuffer_remove(input, &nmethods, 1);
		if (available < (size_t)(2 + nmethods)) return;

		uint8_t methods[256];
		evbuffer_remove(input, methods, nmethods);

		uint8_t chosen = 0xff;
		for (int i = 0; i < nmethods; i++) {
			if (methods[i] == socks5_auth_method_none) {
				chosen = socks5_auth_method_none;
				break;
			}
			if (methods[i] == socks5_auth_method_pass) {
				chosen = socks5_auth_method_pass;
			}
		}

		socks5_method_reply_t reply;
		reply.version = socks5_version;
		reply.method = chosen;
		bufferevent_write(be, &reply, sizeof(reply));

		if (chosen == 0xff) {
			conn->state = socks5_state_error;
			proxy_tcp_drop_socks5(conn);
			return;
		}

		if (chosen == socks5_auth_method_none) {
			conn->state = socks5_state_request;
			bufferevent_setwatermark(be, EV_READ | EV_WRITE, sizeof(socks5_req_t), sizeof(socks5_req_t) + 256);
		} else {
			conn->state = socks5_state_auth;
			bufferevent_setwatermark(be, EV_READ | EV_WRITE, 2, 515);
		}
		return;
	}

	if (conn->state == socks5_state_auth) {
		size_t available = evbuffer_get_length(input);
		if (available < 2) return;

		uint8_t auth_ver;
		evbuffer_remove(input, &auth_ver, 1);
		if (auth_ver != socks5_password_version) {
			socks5_auth_reply_t reply = { .version = socks5_password_version, .status = 0xff };
			bufferevent_write(be, &reply, sizeof(reply));
			conn->state = socks5_state_error;
			proxy_tcp_drop_socks5(conn);
			return;
		}

		uint8_t ulen;
		evbuffer_remove(input, &ulen, 1);
		if (available < (size_t)(2 + ulen)) return;

		uint8_t username[256];
		evbuffer_remove(input, username, ulen);

		uint8_t plen;
		if (evbuffer_get_length(input) < 1) return;
		evbuffer_remove(input, &plen, 1);
		if (evbuffer_get_length(input) < plen) return;
		uint8_t password[256];
		evbuffer_remove(input, password, plen);

		/* TODO: actual auth check — for now accept all */
		socks5_auth_reply_t reply = { .version = socks5_password_version, .status = socks5_password_passed };
		bufferevent_write(be, &reply, sizeof(reply));

		conn->state = socks5_state_request;
		bufferevent_setwatermark(be, EV_READ | EV_WRITE, sizeof(socks5_req_t), sizeof(socks5_req_t) + 256);
		return;
	}

	if (conn->state == socks5_state_request) {
		size_t available = evbuffer_get_length(input);
		if (available < sizeof(socks5_req_t)) return;

		socks5_req_t req;
		evbuffer_remove(input, &req, sizeof(req));

		if (req.version != socks5_version) {
			proxy_tcp_drop_socks5(conn);
			return;
		}

		if (req.command != socks5_command_connect) {
			socks5_server_send_error(conn, socks5_reply_command_not_supported);
			conn->state = socks5_state_error;
			proxy_tcp_drop_socks5(conn);
			return;
		}

		if (req.addrtype == socks5_addrtype_ipv4) {
			socks5_addr_ipv4_t ipv4;
			if (evbuffer_get_length(input) < sizeof(ipv4)) return;
			evbuffer_remove(input, &ipv4, sizeof(ipv4));
			inet_ntop(AF_INET, &ipv4.addr, conn->dest_host, sizeof(conn->dest_host));
			conn->dest_port = ntohs(ipv4.port);
		} else if (req.addrtype == socks5_addrtype_domain) {
			uint8_t dlen;
			if (evbuffer_get_length(input) < 1) return;
			evbuffer_remove(input, &dlen, 1);
			if (evbuffer_get_length(input) < (size_t)(dlen + 2)) return;
			evbuffer_remove(input, conn->dest_host, dlen);
			conn->dest_host[dlen] = '\0';
			uint16_t port;
			evbuffer_remove(input, &port, sizeof(port));
			conn->dest_port = ntohs(port);
		} else {
			socks5_server_send_error(conn, socks5_reply_address_type_not_supported);
			conn->state = socks5_state_error;
			proxy_tcp_drop_socks5(conn);
			return;
		}

		INFO("socks5: %n -> %s:%d, connect request", &conn->client_addr, conn->dest_host, conn->dest_port);

		conn->ssh_ch = ssh_tunnel_open_channel(conn->proxy->tunnel, conn->dest_host, conn->dest_port, conn->client_event, conn);
		if (!conn->ssh_ch) {
			socks5_server_send_error(conn, socks5_reply_server_failure);
			conn->state = socks5_state_error;
			proxy_tcp_drop_socks5(conn);
			return;
		}

		//Don't enable client read yet — wait for SSH channel to open
		bufferevent_disable(conn->client_event, EV_READ);
		return;
	}

	if (conn->state == socks5_state_relay) {
		char data[8192];
		int rlen = evbuffer_remove(input, data, sizeof(data));
		if (rlen <= 0) return;

		if (!conn->ssh_ch || conn->ssh_ch->state == SSH_CH_STATE_CLOSED) {
			proxy_tcp_drop_socks5(conn);
			return;
		}

		int wlen = ssh_channel_write_data(conn->ssh_ch, data, rlen);
		if (wlen < 0) {
			proxy_tcp_drop_socks5(conn);
			return;
		}

		conn->tx_bytes += rlen;
		proxy_tcp_touch_socks5(conn);
		TRACE("socks5: %n -> %s:%d, data:%d byte", &conn->client_addr, conn->dest_host, conn->dest_port, rlen);
	}
}

static void socks5_server_client_event_cb(struct bufferevent *be, short what, void *arg) {
	(void)be;
	socks_t *conn = (socks_t *)arg;
	if (what & (BEV_EVENT_ERROR | BEV_EVENT_EOF | BEV_EVENT_TIMEOUT)) {
		proxy_tcp_drop_socks5(conn);
	}
}

// socks5 server: accept new connection
static void socks5_server_accept(int listen_fd, short what, void *_arg) {
	(void)what;
	proxy_tcp_t *proxy = _arg;

	if (!proxy->tunnel || !ssh_tunnel_is_connected(proxy->tunnel)) {
		struct sockaddr_in dummy;
		socklen_t dummy_len = sizeof(dummy);
		int fd = accept(listen_fd, (struct sockaddr *)&dummy, &dummy_len);
		if (fd >= 0) close(fd);
		return;
	}

	struct sockaddr_in client_addr;
	socklen_t addrlen = sizeof(client_addr);
	int fd = accept(listen_fd, (struct sockaddr *)&client_addr, &addrlen);
	if (fd < 0) {
		const int e = errno;
		if (e == ENFILE || e == EMFILE || e == ENOBUFS || e == ENOMEM) {
			proxy_tcp_clean_socks5_idle(proxy);
		}
		return;
	}

	if (fcntl_nonblock(fd)) {
		close(fd);
		return;
	}

	if (set_tcp_keepalive(fd)) {
		close(fd);
		return;
	}

	socks_t *conn = calloc(1, sizeof(socks_t));
	if (!conn) {
		close(fd);
		return;
	}

	conn->conn_type = CONN_TYPE_SOCKS5;
	conn->proxy = proxy;
	conn->state = socks5_state_method;
	memcpy(&conn->client_addr, &client_addr, sizeof(client_addr));
	INIT_LIST_HEAD(&conn->list);
	gettimeofday(&conn->create_time, NULL);
	gettimeofday(&conn->last_time, NULL);

	conn->client_event = bufferevent_socket_new(proxy->base, fd, BEV_OPT_CLOSE_ON_FREE);
	if (!conn->client_event) {
		close(fd);
		free(conn);
		return;
	}

	bufferevent_setcb(conn->client_event, socks5_server_client_read_cb, NULL, socks5_server_client_event_cb, conn);
	bufferevent_setwatermark(conn->client_event, EV_READ | EV_WRITE, 2, 257);
	bufferevent_enable(conn->client_event, EV_READ | EV_WRITE);

	list_add(&conn->list, &proxy->socks5_conn_list);
	proxy->socks5_conn_active_num++;

	TRACE("socks5: new connection from %n", &conn->client_addr);
}


static int init_listen_socket(struct sockaddr_in *addr, int *fd_out,
                              struct event_base *base, struct event *ev,
                              void (*cb)(int, short, void *), void *cb_arg,
                              int tproxy_mode)
{
	int on = 1;
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd == -1) {
		ERROR("socket");
		return -1;
	}
	*fd_out = fd;

	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on))) {
		ERROR("setsockopt");
		return -1;
	}

	/* tproxy 需要设置 IP_TRANSPARENT，允许 socket 接收目的地址非本机的连接 */
	if (tproxy_mode) {
		if (setsockopt(fd, SOL_IP, IP_TRANSPARENT, &on, sizeof(on))) {
			ERROR("setsockopt IP_TRANSPARENT");
			return -1;
		}
	}

	if (bind(fd, (struct sockaddr *)addr, sizeof(*addr))) {
		ERRNO("bind %n", addr);
		return -1;
	}

	if (fcntl_nonblock(fd)) {
		ERRNO("fcntl nonblock");
		return -1;
	}

	if (listen(fd, SOMAXCONN)) {
		ERRNO("listen %n", addr);
		return -1;
	}

	event_set(ev, fd, EV_READ | EV_PERSIST, cb, cb_arg);
	event_base_set(base, ev);
	if (event_add(ev, NULL)) {
		ERRNO("event_add");
		return -1;
	}

	return 0;
}

/* 隧道状态变化转发器：tunnel.c 在运行时连接/断开转换时回调，
 * 转发给 proxy.c 的全局通知函数（最终调用 Rust 注册的回调）。 */
static void on_tunnel_state_change(int ready, void *ctx) {
	(void)ctx;
	proxy_notify_tunnel_state(ready);
}

int proxy_tcp_init(proxy_tcp_t* proxy,
                               const char *ssh_host, uint16_t ssh_port,
                               const char *ssh_user, const char *ssh_key,
                               uint16_t tproxy_port,
                               const char *socks5_listen)
{
	//initialize fields for safe cleanup
	proxy->tunnel = NULL;
	proxy->tproxy_listen_fd = -1;
	proxy->socks5_listen_fd = -1;
	INIT_LIST_HEAD(&proxy->tproxy_conn_list);
	INIT_LIST_HEAD(&proxy->socks5_conn_list);

	//initialize SSH tunnel (shared upstream)
	if (ssh_host && ssh_user && ssh_key) {
		proxy->tunnel = malloc(sizeof(tunnel_t));
		if (!proxy->tunnel) {
			ERROR("ssh tunnel alloc failed");
			goto proxy_init_fail;
		}

		if (ssh_tunnel_init(proxy->tunnel, proxy->base, ssh_host, ssh_port, ssh_user, ssh_key) != 0) {
			ERROR("ssh tunnel init failed");
			free(proxy->tunnel);
			proxy->tunnel = NULL;
			goto proxy_init_fail;
		}

		proxy->tunnel->on_channel_open  = on_ssh_channel_open;
		proxy->tunnel->on_channel_close = on_ssh_channel_close;
		proxy->tunnel->on_channel_touch = on_ssh_channel_touch;
		proxy->tunnel->on_state_change  = on_tunnel_state_change;
		proxy->tunnel->cb_ctx = NULL;

		/* ssh_tunnel_init 成功时已置 connected=1，但此时回调刚注册，
		 * 需显式通知 Rust 侧初始就绪状态（运行时重连/断连由 tunnel.c 通知）。 */
		on_tunnel_state_change(proxy->tunnel->connected, NULL);

		INFO("upstream: ssh://%s:%d", ssh_host, ssh_port);
	} else {
		ERROR("no upstream configured (need ssh_host/user/key)");
		goto proxy_init_fail;
	}

	// initialize tproxy listener
	if (tproxy_port > 0) {
		proxy->tproxy_listen_addr.sin_family = AF_INET;
		proxy->tproxy_listen_addr.sin_addr.s_addr = htonl(INADDR_ANY);
		proxy->tproxy_listen_addr.sin_port = htons(tproxy_port);
		proxy->tproxy_listen_backlog = SOMAXCONN;
		proxy->tproxy_conn_max_idle_sec = 600;

		if (init_listen_socket(&proxy->tproxy_listen_addr, &proxy->tproxy_listen_fd,
		                       proxy->base, &proxy->tproxy_listen_event,
		                       proxy_tcp_accept, proxy, 1) != 0) {
			goto proxy_init_fail;
		}
		INFO("tproxy: listening on %n", &proxy->tproxy_listen_addr);
	}

	//initialize socks5 server
	if (socks5_listen != NULL) {
		char listen_copy[256]={0};
		strncpy(listen_copy, socks5_listen, sizeof(listen_copy) - 1);
		listen_copy[sizeof(listen_copy) - 1] = '\0';

		char *ip_str = strtok(listen_copy, ":");
		char *port_str = strtok(NULL, ":");

		proxy->socks5_listen_addr.sin_family = AF_INET;
		if (ip_str && inet_aton(ip_str, &proxy->socks5_listen_addr.sin_addr)) {
			/* ok */
		} else {
			proxy->socks5_listen_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		}
		if (port_str) {
			int port = atoi(port_str);
			proxy->socks5_listen_addr.sin_port = htons(port);
		} else {
			proxy->socks5_listen_addr.sin_port = htons(1080);
		}

		proxy->socks5_conn_max_idle_sec = 600;

		if (init_listen_socket(&proxy->socks5_listen_addr, &proxy->socks5_listen_fd,
		                       proxy->base, &proxy->socks5_listen_event,
		                       socks5_server_accept, proxy, 0) != 0) {
			goto proxy_init_fail;
		}
		INFO("socks5: listening on %n", &proxy->socks5_listen_addr);
	}

	// ignore SIGPIPE
	struct sigaction sa = {}, sa_old = {};
	sa.sa_handler = SIG_IGN;
	sa.sa_flags = SA_RESTART;
	sigaction(SIGPIPE, &sa, &sa_old);

	return 0;

proxy_init_fail:
	if (proxy->tunnel) {
		ssh_tunnel_term(proxy->tunnel);
		free(proxy->tunnel);
		proxy->tunnel = NULL;
	}

	if (proxy->tproxy_listen_fd >= 0) {
		if (event_initialized(&proxy->tproxy_listen_event)) {
            event_del(&proxy->tproxy_listen_event);
        }
		close(proxy->tproxy_listen_fd);
		proxy->tproxy_listen_fd = -1;
	}

	if (proxy->socks5_listen_fd >= 0) {
		if (event_initialized(&proxy->socks5_listen_event)) {
            event_del(&proxy->socks5_listen_event);
        }
		close(proxy->socks5_listen_fd);
		proxy->socks5_listen_fd = -1;
	}
	return -1;
}

void proxy_tcp_term(proxy_tcp_t* proxy) {
	// close tproxy connections
	if (!list_empty(&proxy->tproxy_conn_list)) {
		tcp_t *tmp, *client = NULL;
		list_for_each_entry_safe(client, tmp, &proxy->tproxy_conn_list, list) {
			proxy_tcp_drop_relay(client);
		}
	}

	//close SOCKS5 connections
	if (!list_empty(&proxy->socks5_conn_list)) {
		socks_t *tmp, *conn = NULL;
		list_for_each_entry_safe(conn, tmp, &proxy->socks5_conn_list, list) {
			proxy_tcp_drop_socks5(conn);
		}
	}

	//close tproxy listener
	if (event_initialized(&proxy->tproxy_listen_event)) {
		event_del(&proxy->tproxy_listen_event);
	}

	//close socks5 listener
	if (event_initialized(&proxy->socks5_listen_event)) {
		event_del(&proxy->socks5_listen_event);
	}

	// close ssh tunnel
	if (proxy->tunnel) {
		ssh_tunnel_term(proxy->tunnel);
		free(proxy->tunnel);
		proxy->tunnel = NULL;
	}
}
