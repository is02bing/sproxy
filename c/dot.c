#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "dot.h"
#include "utils.h"
#include "trace.h"

/* ── pending 请求：按 DNS ID 在连接上排队，回包时按 id 取出 ── */
typedef struct dot_pending {
	uint16_t              id;
	void*                 ctx;
	void*                 arg;
	dot_callback_t        cb;
	struct dot_pending*   next;
} dot_pending_t;

/* ── 一条到上游 server 的持久 TCP 连接 ── */
typedef struct dot_conn {
	struct sockaddr_in    server;
	struct bufferevent*   bev;
	bool                  connected;
	bool                  connecting;
	dot_pending_t*        pending_head;
	dot_pending_t*        pending_tail;
	struct dot_conn*      next;
} dot_conn_t;

/* 全局连接池（按 server 地址去重，链表即可：上游数量少） */
static dot_conn_t* g_conn_list = NULL;

#define DOT_MAX_MSG 2048   /* DNS over TCP 单条消息上限 */

/* ─────────────── 连接查找 / 创建 ─────────────── */

static dot_conn_t* dot_find_conn(const struct sockaddr_in* server) {
	dot_conn_t* c = g_conn_list;
	while (c) {
		if (c->server.sin_port == server->sin_port
		 && c->server.sin_addr.s_addr == server->sin_addr.s_addr) {
			return c;
		}
		c = c->next;
	}
	return NULL;
}

static dot_conn_t* dot_new_conn(const struct sockaddr_in* server) {
	dot_conn_t* c = calloc(1, sizeof(dot_conn_t));
	if (!c) {
		ERRNO("calloc dot_conn");
		return NULL;
	}
	c->server = *server;
	c->next = g_conn_list;
	g_conn_list = c;
	return c;
}

/* ─────────────── pending 队列 ─────────────── */

static void dot_pending_push(dot_conn_t* c, dot_pending_t* p) {
	p->next = NULL;
	if (c->pending_tail) {
		c->pending_tail->next = p;
	} else {
		c->pending_head = p;
	}
	c->pending_tail = p;
}

static dot_pending_t* dot_pending_pop(dot_conn_t* c, uint16_t id) {
	dot_pending_t** pp = &c->pending_head;
	dot_pending_t*  prev = NULL;
	while (*pp) {
		if ((*pp)->id == id) {
			dot_pending_t* p = *pp;
			*pp = p->next;
			if (p == c->pending_tail) c->pending_tail = prev;
			return p;
		}
		prev = *pp;
		pp = &(*pp)->next;
	}
	return NULL;
}

/* 把连接上的所有 pending 以失败回调通知，并清空 */
static void dot_fail_all(dot_conn_t* c) {
	dot_pending_t* p = c->pending_head;
	c->pending_head = c->pending_tail = NULL;
	while (p) {
		dot_pending_t* next = p->next;
		p->cb(NULL, 0, p->ctx, p->arg);
		free(p);
		p = next;
	}
}

/* 关闭连接并失败所有 pending，但保留 dot_conn 条目以便下次重连复用。
 * 顺序：先复位标志 + 释放 bev，再回调 pending。
 * 这样即使回调中重试 dot_request，dot_get_conn 也会因 connected/connecting=false
 * 而新建 bev，不会复用即将释放的连接。 */
static void dot_close(dot_conn_t* c) {
	c->connected = false;
	c->connecting = false;
	if (c->bev) {
		bufferevent_free(c->bev);
		c->bev = NULL;
	}
	dot_fail_all(c);
}

/* ─────────────── libevent 回调 ─────────────── */

static void dot_read_cb(struct bufferevent* bev, void* arg) {
	dot_conn_t* c = arg;
	struct evbuffer* in = bufferevent_get_input(bev);

	for (;;) {
		size_t avail = evbuffer_get_length(in);
		if (avail < 2) break;  /* 长度前缀未到 */

		uint8_t hdr[2];
		evbuffer_copyout(in, hdr, 2);
		uint16_t mlen = ((uint16_t)hdr[0] << 8) | hdr[1];
		if (mlen < 12 || mlen > DOT_MAX_MSG) {
			ERROR("dot %n bad msg len %d, reset connection", &c->server, (int)mlen);
			dot_close(c);
			return;
		}
		if (avail < (size_t)2 + mlen) break;  /* 消息体未到齐 */

		evbuffer_drain(in, 2);
		uint8_t buf[DOT_MAX_MSG];
		evbuffer_remove(in, buf, mlen);

		uint16_t id = ((uint16_t)buf[0] << 8) | buf[1];
		dot_pending_t* p = dot_pending_pop(c, id);
		if (p) {
			p->cb(buf, mlen, p->ctx, p->arg);
			free(p);
		} else {
			WARNING("dot %n reply 0x%x no pending, drop", &c->server, (int)id);
		}
	}
}

static void dot_event_cb(struct bufferevent* bev, short what, void* arg) {
	dot_conn_t* c = arg;
	(void)bev;

	if (what & BEV_EVENT_CONNECTED) {
		c->connected = true;
		c->connecting = false;
		TRACE("dot %n connected", &c->server);
		return;
	}

	if (what & BEV_EVENT_EOF) {
		/* 远端关闭连接（DoT 服务器空闲回收），属正常情况 */
		TRACE("dot %n closed by remote (event 0x%x)", &c->server, (int)what);
		dot_close(c);
	} else if (what & BEV_EVENT_TIMEOUT) {
		WARNING("dot %n timeout (event 0x%x)", &c->server, (int)what);
		dot_close(c);
	} else if (what & BEV_EVENT_ERROR) {
		ERROR("dot %n error (event 0x%x): %s", &c->server, (int)what,
		      evutil_socket_error_to_string(EVUTIL_SOCKET_ERROR()));
		dot_close(c);
	}
}

/* 取得一条可用连接（已连接 / 正在连接 / 新建） */
static dot_conn_t* dot_get_conn(struct event_base* base, const struct sockaddr_in* server) {
	dot_conn_t* c = dot_find_conn(server);
	if (c && (c->connected || c->connecting)) return c;
	if (!c) {
		c = dot_new_conn(server);
		if (!c) return NULL;
	}

	c->bev = bufferevent_socket_new(base, -1, BEV_OPT_CLOSE_ON_FREE);
	if (!c->bev) {
		ERROR("bufferevent_socket_new");
		return NULL;
	}
	bufferevent_setcb(c->bev, dot_read_cb, NULL, dot_event_cb, c);
	bufferevent_enable(c->bev, EV_READ | EV_WRITE);
	bufferevent_settimeout(c->bev, 5, 5);  /* 5s 读/写超时 */

	c->connecting = true;
	if (bufferevent_socket_connect(c->bev, (struct sockaddr*)&c->server, sizeof(c->server))
	    && errno != EINPROGRESS) {
		ERRNO("dot connect %n", &c->server);
		bufferevent_free(c->bev);
		c->bev = NULL;
		c->connecting = false;
		return NULL;
	}
	return c;
}

/* ─────────────── 对外接口 ─────────────── */

void dot_request(struct event_base* base, struct sockaddr_in* server,
                 const void* data, size_t len, uint16_t id,
                 void* ctx, void* arg, dot_callback_t cb) {
	if (!cb) return;
	if (len < 12 || len > DOT_MAX_MSG) {
		cb(NULL, 0, ctx, arg);
		return;
	}

	dot_conn_t* c = dot_get_conn(base, server);
	if (!c) {
		cb(NULL, 0, ctx, arg);
		return;
	}

	dot_pending_t* p = calloc(1, sizeof(dot_pending_t));
	if (!p) {
		ERRNO("calloc dot_pending");
		cb(NULL, 0, ctx, arg);
		return;
	}
	p->id = id;
	p->ctx = ctx;
	p->arg = arg;
	p->cb = cb;
	dot_pending_push(c, p);

	/* 2 字节大端长度前缀 + DNS 报文 */
	uint8_t hdr[2] = { (uint8_t)(len >> 8), (uint8_t)(len & 0xff) };
	bufferevent_write(c->bev, hdr, 2);
	bufferevent_write(c->bev, data, len);
}

void dot_term(void) {
	dot_conn_t* c = g_conn_list;
	g_conn_list = NULL;
	while (c) {
		dot_conn_t* next = c->next;
		dot_fail_all(c);
		if (c->bev) bufferevent_free(c->bev);
		free(c);
		c = next;
	}
}
