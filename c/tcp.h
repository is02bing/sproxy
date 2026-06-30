#pragma once

#include <sys/time.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <assert.h>
#include <event.h>
#include <libconfig.h>
#include "list.h"
#include "utils.h"
#include "tunnel.h"
#include "socks5.h"

#define CONN_TYPE_TPROXY  0
#define CONN_TYPE_SOCKS5  1

/* Forward declaration (proxy_tcp_t and socks_t are mutually dependent) */
struct proxy_tcp_t;
typedef struct proxy_tcp_t proxy_tcp_t;

/* ── tproxy relay connection ── */

typedef struct {
	int                 conn_type;  /* CONN_TYPE_TPROXY */
	list_head           list;
	proxy_tcp_t         *proxy;

	/* client socket */
	struct sockaddr_in  client_addr;
	struct bufferevent *client_event;

	/* upstream: SSH channel */
	ssh_channel_t      *ssh_ch;

	/* stats */
	uint64_t            tx_bytes;
	uint64_t            rx_bytes;

	/* time */
	struct timeval      create_time;
	struct timeval      last_time;
} tcp_t;

/* ── SOCKS5 server connection ── */

typedef struct socks5_conn {
	int                 conn_type;  /* CONN_TYPE_SOCKS5 */
	list_head           list;
	proxy_tcp_t        *proxy;

	/* client socket */
	struct sockaddr_in  client_addr;
	struct bufferevent *client_event;

	/* SOCKS5 protocol state */
	socks5_state_t state;

	/* destination (from SOCKS5 CONNECT request) */
	char                dest_host[256];
	uint16_t            dest_port;

	/* SSH channel for forwarding */
	ssh_channel_t      *ssh_ch;

	/* stats */
	uint64_t            tx_bytes;
	uint64_t            rx_bytes;

	/* time */
	struct timeval      create_time;
	struct timeval      last_time;
} socks_t;

/* ── proxy instance ── */

struct proxy_tcp_t {
	struct event_base*  base;

	/* upstream: SSH tunnel (shared by tproxy and socks5) */
	tunnel_t*       tunnel;

	/* ── tproxy service ── */
	struct  sockaddr_in tproxy_listen_addr;
	int                 tproxy_listen_fd;
	uint16_t            tproxy_listen_backlog;
	struct event        tproxy_listen_event;
	uint32_t            tproxy_conn_max_idle_sec;
	uint32_t            tproxy_conn_active_num;
	list_head           tproxy_conn_list;

	/* ── socks5 service ── */
	struct sockaddr_in  socks5_listen_addr;
	int                 socks5_listen_fd;
	struct event        socks5_listen_event;
	uint32_t            socks5_conn_active_num;
	uint32_t            socks5_conn_max_idle_sec;
	list_head           socks5_conn_list;
};

/* ── tproxy functions ── */
void proxy_tcp_start_relay(tcp_t* relay);
void proxy_tcp_drop_relay(tcp_t *relay);
void proxy_tcp_touch_relay(tcp_t *relay);

/* ── common functions ── */
int  proxy_tcp_init(proxy_tcp_t* proxy,
                                const char *ssh_host, uint16_t ssh_port,
                                const char *ssh_user, const char *ssh_key,
                                uint16_t tproxy_port,
                                const char *socks5_listen);
void proxy_tcp_term(proxy_tcp_t* proxy);
