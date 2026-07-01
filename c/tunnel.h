#pragma once

#include <stdint.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <event.h>
#include <libssh2.h>
#include "list.h"
#include "utils.h"

#define SSH_CH_STATE_OPENING  1
#define SSH_CH_STATE_READY    2
#define SSH_CH_STATE_CLOSED   3

#define SSH_WRITE_BUF_SIZE    65536
#define SSH_READ_BUF_SIZE     8192

#define SSH_KEEPALIVE_INTERVAL  30   /* seconds */
#define SSH_KEEPALIVE_MAX_MISS   3
#define SSH_RECONNECT_MIN_DELAY  2   /* seconds */
#define SSH_RECONNECT_MAX_DELAY  60  /* seconds */

struct tunnel_t;

typedef struct {
	list_head              list;
	struct tunnel_t        *tunnel;
	LIBSSH2_CHANNEL        *libssh2_ch;

	/* open parameters (used while opening) */
	char                   dest_host[256];
	int                    dest_port;
	int                    state;

	/* Client socket (NOT owned by channel) */
	struct bufferevent    *client_bev;

	/* Write buffer for data pending to send to SSH channel */
	uint8_t               *write_buf;
	size_t                 write_buf_size;
	size_t                 write_len;

	/* Stats */
	uint64_t               tx_bytes;
	uint64_t               rx_bytes;
	struct timeval         create_time;
	struct timeval         last_time;

	/* Back-reference for callbacks */
	void                  *user_data;
} ssh_channel_t;

typedef struct tunnel_t {
	struct event_base *base;
	LIBSSH2_SESSION   *session;
	int                ssh_fd;

	/* server address */
	struct sockaddr_in server_addr;
	char               username[64];
	char               key_file[256];

	/* events */
	struct event       read_event;
	struct event       write_event;
	struct event       read_rearm_timer;  /* 无 channel 时延迟重新 arm read_event */
	int                write_registered;

	/* channel list */
	list_head          channels;
	uint32_t           channel_count;

	/* Callbacks */
	void (*on_channel_open)(ssh_channel_t *ch, void *ctx);
	void (*on_channel_close)(ssh_channel_t *ch, void *ctx);
	void (*on_channel_touch)(ssh_channel_t *ch, void *ctx);
	void  *cb_ctx;

	/* 隧道状态变化回调：连接成功(ready=1)/断开(ready=0)转换时调用。
	 * 由 tcp.c 在 ssh_tunnel_init 后设置；tunnel.c 在运行时转换点调用。 */
	void (*on_state_change)(int ready, void *ctx);

	/* keepalive */
	struct event       keepalive_timer;
	int                keepalive_interval;  /* seconds */
	int                keepalive_missed;

	/* reconnect */
	struct event       reconnect_timer;
	int                reconnect_delay;     /* current delay in seconds */
	int                reconnect_attempts;

	/* state */
	int connected;
} tunnel_t;

/**
 * initialize ssh tunnel: connect, handshake, authenticate (blocking).
 * after init, session is set to non-blocking for event loop.
 * also starts keepalive timer and initializes reconnect timer.
 */
int  ssh_tunnel_init(tunnel_t *tunnel, struct event_base *base,
                     const char *host, int port,
                     const char *username, const char *key_file);

/** disconnect and cleanup ssh tunnel (final shutdown, no reconnect) */
void ssh_tunnel_term(tunnel_t *tunnel);

/**
 * open a direct-tcpip SSH channel to dest_host:dest_port.
 * returns NULL if tunnel is not connected.
 * in non-blocking mode, channel may be in OPENING state initially.
 * on_channel_open callback fires when ready.
 */
ssh_channel_t *ssh_tunnel_open_channel(tunnel_t *tunnel,
                                        const char *dest_host, int dest_port,
                                        struct bufferevent *client_bev,
                                        void *user_data);

/** close and free an ssh channel */
void ssh_tunnel_close_channel(ssh_channel_t *ch);

/** write data to ssh channel (handles EAGAIN and buffering) */
int  ssh_channel_write_data(ssh_channel_t *ch, const void *data, size_t len);

/** process all channels: read from ssh, write to clients, handle opens */
void ssh_tunnel_process_channels(tunnel_t *tunnel);

/** check if SSH tunnel is connected */
int  ssh_tunnel_is_connected(tunnel_t *tunnel);
