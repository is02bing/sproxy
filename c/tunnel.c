#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <libssh2.h>
#include <event.h>
#include "tunnel.h"
#include "trace.h"
#include "utils.h"

static void ssh_tunnel_disconnect(tunnel_t *tunnel);

/* 无channel时read_event被暂时摘除，由此时器在100ms后重新arm。
 * 避免SSH socket上非 channel包(GLOBAL_REQUEST 等)未消费时
 * EV_PERSIST持续触发on_readable造成100% CPU忙等。 */
static void ssh_tunnel_rearm_read_cb(int fd, short what, void *arg)
{
	(void)fd; (void)what;
	tunnel_t *tunnel = arg;
	event_add(&tunnel->read_event, NULL);
}

static void ssh_tunnel_on_readable(int fd, short what, void *arg)
{
	(void)fd; (void)what;
	tunnel_t *tunnel = arg;

	if (!tunnel->connected) {
		//stale event — socket already closed during disconnect.
        //re-registering will happen in reconnect.
		return;
	}

	ssh_tunnel_process_channels(tunnel);
}

static void ssh_tunnel_on_writable(int fd, short what, void *arg)
{
	(void)fd; (void)what;
	tunnel_t *tunnel = arg;

	if (!tunnel->connected)
		return;

	ssh_tunnel_process_channels(tunnel);
}

static void ssh_tunnel_keepalive_cb(int fd, short what, void *arg)
{
	(void)fd; (void)what;
	tunnel_t *tunnel = arg;

	if (!tunnel->connected)
		return;

	// send keepalive packet
	int seconds_to_next = 0;
	int rc = libssh2_keepalive_send(tunnel->session, &seconds_to_next);

	if (rc == LIBSSH2_ERROR_EAGAIN) {
		//would block — try again next interval
		TRACE("ssh: keepalive send EAGAIN");
	} else if (rc < 0) {
		tunnel->keepalive_missed++;
		ERROR("ssh: keepalive send failed (missed %d/%d)", tunnel->keepalive_missed, SSH_KEEPALIVE_MAX_MISS);
		if (tunnel->keepalive_missed >= SSH_KEEPALIVE_MAX_MISS) {
			ERROR("ssh: too many keepalive misses, disconnecting");
			ssh_tunnel_disconnect(tunnel);
			return;
		}
	} else {
		//success
		tunnel->keepalive_missed = 0;
	}

	// reschedule
	int interval = seconds_to_next > 0 ? seconds_to_next : tunnel->keepalive_interval;
	struct timeval tv = { .tv_sec = interval, .tv_usec = 0 };
	evtimer_add(&tunnel->keepalive_timer, &tv);
}

static void ssh_tunnel_reconnect_cb(int fd, short what, void *arg)
{
	(void)fd; (void)what;
	tunnel_t *tunnel = arg;

	INFO("ssh: attempting reconnect (attempt %d, delay %ds)...", tunnel->reconnect_attempts + 1, tunnel->reconnect_delay);

	// create new socket
	tunnel->ssh_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (tunnel->ssh_fd < 0) {
		ERROR("ssh: reconnect socket() failed");
		goto reconnect_fail;
	}

	// set connect timeout
	struct timeval tv = { .tv_sec = 30, .tv_usec = 0 };
	setsockopt(tunnel->ssh_fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
	setsockopt(tunnel->ssh_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

	// connect (blocking with timeout)
	if (connect(tunnel->ssh_fd, (struct sockaddr *)&tunnel->server_addr, sizeof(tunnel->server_addr)) < 0) {
		ERROR("ssh: reconnect connect() failed: %s", strerror(errno));
		goto reconnect_fail;
	}

	// create new session
	tunnel->session = libssh2_session_init();
	if (!tunnel->session) {
		ERROR("ssh: reconnect session init failed");
		goto reconnect_fail;
	}

	// set blocking for handshake + auth
	libssh2_session_set_blocking(tunnel->session, 1);

	// handshake
	if (libssh2_session_handshake(tunnel->session, tunnel->ssh_fd)) {
		char *msg = NULL;
		libssh2_session_last_error(tunnel->session, &msg, NULL, 0);
		ERROR("ssh: reconnect handshake failed: %s", msg ? msg : "unknown");
		goto reconnect_fail;
	}

	// authenticate with public key
	if (libssh2_userauth_publickey_fromfile(tunnel->session, tunnel->username, NULL, tunnel->key_file, NULL)) {
		char *msg = NULL;
		libssh2_session_last_error(tunnel->session, &msg, NULL, 0);
		ERROR("ssh: reconnect auth failed: %s", msg ? msg : "unknown");
		goto reconnect_fail;
	}

	//success, configure keepalive
	/* want_reply=0：服务端不回包，避免无 channel 时 keepalive 回复滞留 socket
	 * 导致 read_event(EV_PERSIST) 忙等。断连检测仍由 keepalive_send 在 socket
	 * 断开时返回 SOCKET_SEND 触发 keepalive_missed，不依赖回复包。 */
	libssh2_keepalive_config(tunnel->session, 0, tunnel->keepalive_interval);

	// switch to non-blocking
	libssh2_session_set_blocking(tunnel->session, 0);
	if (fcntl_nonblock(tunnel->ssh_fd) != 0) {
		ERROR("ssh: reconnect fcntl_nonblock failed");
		goto reconnect_fail;
	}

	// register events
	event_set(&tunnel->read_event, tunnel->ssh_fd, EV_READ | EV_PERSIST, ssh_tunnel_on_readable, tunnel);
	event_base_set(tunnel->base, &tunnel->read_event);
	event_add(&tunnel->read_event, NULL);

	event_set(&tunnel->write_event, tunnel->ssh_fd, EV_WRITE, ssh_tunnel_on_writable, tunnel);
	event_base_set(tunnel->base, &tunnel->write_event);
	tunnel->write_registered = 0;

	tunnel->connected = 1;
	tunnel->keepalive_missed = 0;
	tunnel->reconnect_attempts = 0;

	INFO("ssh: reconnected to %s:%d",inet_ntoa(tunnel->server_addr.sin_addr), ntohs(tunnel->server_addr.sin_port));

	/* 通知 Rust 侧隧道已连通（运行时重连成功） */
	if (tunnel->on_state_change) tunnel->on_state_change(1, tunnel->cb_ctx);

	// start keepalive timer
	struct timeval ktv = { .tv_sec = tunnel->keepalive_interval, .tv_usec = 0 };
	evtimer_add(&tunnel->keepalive_timer, &ktv);

	// initialize read rearm timer (reconnect path)
	evtimer_set(&tunnel->read_rearm_timer, ssh_tunnel_rearm_read_cb, tunnel);
	event_base_set(tunnel->base, &tunnel->read_rearm_timer);

	return;

reconnect_fail:
	ERROR("ssh: reconnect attempt %d failed", tunnel->reconnect_attempts + 1);

	if (tunnel->session) {
		libssh2_session_free(tunnel->session);
		tunnel->session = NULL;
	}

	if (tunnel->ssh_fd >= 0) {
		close(tunnel->ssh_fd);
		tunnel->ssh_fd = -1;
	}

	// exponential backoff
	tunnel->reconnect_attempts++;
	if (tunnel->reconnect_delay < SSH_RECONNECT_MAX_DELAY)
		tunnel->reconnect_delay *= 2;

	struct timeval rtv = { .tv_sec = tunnel->reconnect_delay, .tv_usec = 0 };
	evtimer_add(&tunnel->reconnect_timer, &rtv);
}

static void ssh_tunnel_disconnect(tunnel_t *tunnel)
{
	if (!tunnel->connected)
		return;

	ERROR("ssh: tunnel disconnected from %s:%d", inet_ntoa(tunnel->server_addr.sin_addr), ntohs(tunnel->server_addr.sin_port));

	tunnel->connected = 0;

	/* 通知 Rust 侧隧道已断开（运行时断连） */
	if (tunnel->on_state_change) tunnel->on_state_change(0, tunnel->cb_ctx);

	//stop keepalive timer
	if (event_initialized(&tunnel->keepalive_timer)) {
        event_del(&tunnel->keepalive_timer);
    }

	/* close all channels — on_channel_close callback will:
	   1) drop the relay (close client socket, free tcp_t)
	   2) free the ssh_channel_t */
	ssh_channel_t *ch, *tmp;
	list_for_each_entry_safe(ch, tmp, &tunnel->channels, list) {
		ch->state = SSH_CH_STATE_CLOSED;
		if (tunnel->on_channel_close)
			tunnel->on_channel_close(ch, tunnel->cb_ctx);
	}

	// unregister I/O events
	if (event_initialized(&tunnel->read_event)) {
        event_del(&tunnel->read_event);
    }

	if (event_initialized(&tunnel->read_rearm_timer)) {
        event_del(&tunnel->read_rearm_timer);
    }

	if (tunnel->write_registered && event_initialized(&tunnel->write_event)) {
        event_del(&tunnel->write_event);
    }
	tunnel->write_registered = 0;

	// close SSH session and socket
	if (tunnel->session) {
		libssh2_session_set_blocking(tunnel->session, 1);
		libssh2_session_disconnect(tunnel->session, "connection lost");
		libssh2_session_free(tunnel->session);
		tunnel->session = NULL;
	}

	if (tunnel->ssh_fd >= 0) {
		close(tunnel->ssh_fd);
		tunnel->ssh_fd = -1;
	}

	// start reconnect timer with initial delay
	tunnel->reconnect_delay = SSH_RECONNECT_MIN_DELAY;
	tunnel->reconnect_attempts = 0;
	struct timeval rtv = { .tv_sec = tunnel->reconnect_delay, .tv_usec = 0 };
	evtimer_add(&tunnel->reconnect_timer, &rtv);

	INFO("ssh: reconnect scheduled in %ds", tunnel->reconnect_delay);
}

int ssh_tunnel_init(tunnel_t *tunnel, struct event_base *base,
        const char *host, int port, const char *username, const char *key_file)
{
	int rc;

	memset(tunnel, 0, sizeof(*tunnel));
	tunnel->ssh_fd = -1;
	INIT_LIST_HEAD(&tunnel->channels);
	tunnel->base = base;
	tunnel->keepalive_interval = SSH_KEEPALIVE_INTERVAL;

	//parse server address
	tunnel->server_addr.sin_family = AF_INET;
	tunnel->server_addr.sin_port = htons(port);
	if (inet_aton(host, &tunnel->server_addr.sin_addr) == 0) {
		ERROR("ssh: invalid host %s", host);
		return -1;
	}

	strncpy(tunnel->username, username, sizeof(tunnel->username) - 1);
	strncpy(tunnel->key_file, key_file, sizeof(tunnel->key_file) - 1);

	// create socket
	tunnel->ssh_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (tunnel->ssh_fd < 0) {
		ERROR("ssh: socket() failed");
		return -1;
	}

	//set connect timeout (300s)
	struct timeval tv = { .tv_sec = 300, .tv_usec = 0 };
	setsockopt(tunnel->ssh_fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
	setsockopt(tunnel->ssh_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

	//connect (blocking)
	rc = connect(tunnel->ssh_fd, (struct sockaddr *)&tunnel->server_addr, sizeof(tunnel->server_addr));
	if (rc < 0) {
		ERROR("ssh: connect to %s:%d failed: %s", host, port, strerror(errno));
		goto fail;
	}

	// init libssh2
	rc = libssh2_init(0);
	if (rc) {
		ERROR("ssh: libssh2_init() failed: %d", rc);
		goto fail;
	}

	// create session
	tunnel->session = libssh2_session_init();
	if (!tunnel->session) {
		ERROR("ssh: libssh2_session_init() failed");
		goto fail;
	}

	// set blocking for handshake + auth
	libssh2_session_set_blocking(tunnel->session, 1);

	// handshake
	rc = libssh2_session_handshake(tunnel->session, tunnel->ssh_fd);
	if (rc) {
		char *msg = NULL;
		libssh2_session_last_error(tunnel->session, &msg, NULL, 0);
		ERROR("ssh: handshake failed: %s", msg ? msg : "unknown");
		goto fail;
	}

	// verify host key (basic: accept first time, TODO: known_hosts check)
	// for now we skip strict verification, same as ssh -o StrictHostKeyChecking=no
	// authenticate with public key
	rc = libssh2_userauth_publickey_fromfile(tunnel->session, tunnel->username, NULL, tunnel->key_file,	NULL);
	if (rc) {
		char *msg = NULL;
		libssh2_session_last_error(tunnel->session, &msg, NULL, 0);
		ERROR("ssh: public key auth failed: %s", msg ? msg : "unknown");
		goto fail;
	}

	// configure keepalive
	libssh2_keepalive_config(tunnel->session, 0, tunnel->keepalive_interval);

	INFO("ssh: connected and authenticated to %s:%d as %s", host, port, username);

	// switch to non-blocking for event loop
	libssh2_session_set_blocking(tunnel->session, 0);

	TRACE("ssh: session switched to non-blocking mode, fd=%d", tunnel->ssh_fd);

	// set socket non-blocking
	if (fcntl_nonblock(tunnel->ssh_fd) != 0) {
		ERROR("ssh: fcntl_nonblock failed");
		goto fail;
	}

	// register ssh socket for read events (persistent)
	event_set(&tunnel->read_event, tunnel->ssh_fd, EV_READ | EV_PERSIST, ssh_tunnel_on_readable, tunnel);
	event_base_set(base, &tunnel->read_event);
	event_add(&tunnel->read_event, NULL);

	// write event (one-shot, initially not registered)
	event_set(&tunnel->write_event, tunnel->ssh_fd, EV_WRITE, ssh_tunnel_on_writable, tunnel);
	event_base_set(base, &tunnel->write_event);
	tunnel->write_registered = 0;

	// initialize read rearm timer (for deferring read_event when no channels)
	evtimer_set(&tunnel->read_rearm_timer, ssh_tunnel_rearm_read_cb, tunnel);
	event_base_set(base, &tunnel->read_rearm_timer);

	// initialize keepalive timer
	evtimer_set(&tunnel->keepalive_timer, ssh_tunnel_keepalive_cb, tunnel);
	event_base_set(base, &tunnel->keepalive_timer);
	struct timeval ktv = { .tv_sec = tunnel->keepalive_interval, .tv_usec = 0 };
	evtimer_add(&tunnel->keepalive_timer, &ktv);

	// initialize reconnect timer (not started yet)
	evtimer_set(&tunnel->reconnect_timer, ssh_tunnel_reconnect_cb, tunnel);
	event_base_set(base, &tunnel->reconnect_timer);

	tunnel->connected = 1;
	tunnel->keepalive_missed = 0;
	tunnel->reconnect_attempts = 0;
	return 0;

fail:
	if (tunnel->session) {
		libssh2_session_free(tunnel->session);
		tunnel->session = NULL;
	}

	if (tunnel->ssh_fd >= 0) {
		close(tunnel->ssh_fd);
		tunnel->ssh_fd = -1;
	}
	return -1;
}

void ssh_tunnel_term(tunnel_t *tunnel)
{
	if (!tunnel->connected && tunnel->ssh_fd < 0 && !tunnel->session) {
		// already fully terminated — just cancel timers
		if (event_initialized(&tunnel->keepalive_timer)) {
            event_del(&tunnel->keepalive_timer);
        }
		if (event_initialized(&tunnel->reconnect_timer)) {
            event_del(&tunnel->reconnect_timer);
        }
		if (event_initialized(&tunnel->read_rearm_timer)) {
            event_del(&tunnel->read_rearm_timer);
        }
		return;
	}

	// stop reconnect timer first (if running)
	if (event_initialized(&tunnel->reconnect_timer)) {
        event_del(&tunnel->reconnect_timer);
    }

	// stop keepalive timer
	if (event_initialized(&tunnel->keepalive_timer)) {
        event_del(&tunnel->keepalive_timer);
    }

	// stop read rearm timer
	if (event_initialized(&tunnel->read_rearm_timer)) {
        event_del(&tunnel->read_rearm_timer);
    }

	// close all channels
	ssh_channel_t *ch, *tmp;
	list_for_each_entry_safe(ch, tmp, &tunnel->channels, list) {
		if (ch->libssh2_ch) {
			libssh2_channel_close(ch->libssh2_ch);
			libssh2_channel_free(ch->libssh2_ch);
			ch->libssh2_ch = NULL;
		}
		if (ch->write_buf) {
			free(ch->write_buf);
			ch->write_buf = NULL;
		}
		ch->state = SSH_CH_STATE_CLOSED;
		list_del(&ch->list);
	}
	tunnel->channel_count = 0;

	if (event_initialized(&tunnel->read_event)) {
        event_del(&tunnel->read_event);
    }

	if (tunnel->write_registered && event_initialized(&tunnel->write_event)) {
        event_del(&tunnel->write_event);
    }

	if (tunnel->session) {
		libssh2_session_set_blocking(tunnel->session, 1);
		libssh2_session_disconnect(tunnel->session, "shutdown");
		libssh2_session_free(tunnel->session);
		tunnel->session = NULL;
	}

	if (tunnel->ssh_fd >= 0) {
		close(tunnel->ssh_fd);
		tunnel->ssh_fd = -1;
	}

	tunnel->connected = 0;
}

ssh_channel_t *ssh_tunnel_open_channel(tunnel_t *tunnel,
                                        const char *dest_host, int dest_port,
                                        struct bufferevent *client_bev,
                                        void *user_data)
{
	// reject if tunnel is not connected
	if (!tunnel->connected) {
		ERROR("ssh: cannot open channel to %s:%d — tunnel not connected", dest_host, dest_port);
		return NULL;
	}

	ssh_channel_t *ch = calloc(1, sizeof(ssh_channel_t));
	if (!ch) {
        return NULL;
    }

	ch->tunnel = tunnel;
	ch->client_bev = client_bev;
	ch->user_data = user_data;
	strncpy(ch->dest_host, dest_host, sizeof(ch->dest_host) - 1);
	ch->dest_port = dest_port;
	ch->state = SSH_CH_STATE_OPENING;

	ch->write_buf = malloc(SSH_WRITE_BUF_SIZE);
	if (!ch->write_buf) {
		free(ch);
		return NULL;
	}
	ch->write_buf_size = SSH_WRITE_BUF_SIZE;
	ch->write_len = 0;

	gettimeofday(&ch->create_time, NULL);
	gettimeofday(&ch->last_time, NULL);

	INIT_LIST_HEAD(&ch->list);
	list_add(&ch->list, &tunnel->channels);
	tunnel->channel_count++;

	TRACE("ssh: opening direct-tcpip to %s:%d (connected=%d, channels=%u, fd=%d, blocking=%d)",
	      dest_host, dest_port, tunnel->connected, tunnel->channel_count,
	      tunnel->ssh_fd, libssh2_session_get_blocking(tunnel->session));

	// try to open the channel immediately
	ch->libssh2_ch = libssh2_channel_direct_tcpip_ex(tunnel->session, dest_host, dest_port, "127.0.0.1", 0);

	if (ch->libssh2_ch) {
		ch->state = SSH_CH_STATE_READY;
		INFO("ssh: channel opened to %s:%d", dest_host, dest_port);
		/* notify: channel is ready */
		if (tunnel->on_channel_open) {
            tunnel->on_channel_open(ch, tunnel->cb_ctx);
        }
	} else {
		int err = libssh2_session_last_errno(tunnel->session);
		if (err == LIBSSH2_ERROR_EAGAIN) {
			TRACE("ssh: channel to %s:%d opening (EAGAIN)", dest_host, dest_port);
			/* register for write events so we get notified to retry */
			if (!tunnel->write_registered) {
				event_add(&tunnel->write_event, NULL);
				tunnel->write_registered = 1;
			}
		} else {
			char *msg = NULL;
			libssh2_session_last_error(tunnel->session, &msg, NULL, 0);
			ERROR("ssh: open channel to %s:%d failed (err=%d): %s", dest_host, dest_port, err, msg ? msg : "unknown");
			list_del(&ch->list);
			tunnel->channel_count--;
			free(ch->write_buf);
			free(ch);
			return NULL;
		}
	}

	return ch;
}

void ssh_tunnel_close_channel(ssh_channel_t *ch)
{
	if (!ch) return;

	tunnel_t *tunnel = ch->tunnel;

	if (ch->libssh2_ch) {
		libssh2_channel_close(ch->libssh2_ch);
		libssh2_channel_free(ch->libssh2_ch);
		ch->libssh2_ch = NULL;
	}

	if (ch->write_buf) {
		free(ch->write_buf);
		ch->write_buf = NULL;
	}

	ch->state = SSH_CH_STATE_CLOSED;

	if (!list_empty(&ch->list)) {
		list_del(&ch->list);
		tunnel->channel_count--;
	}

	free(ch);
}

int ssh_channel_write_data(ssh_channel_t *ch, const void *data, size_t len)
{
	if (!ch || ch->state == SSH_CH_STATE_CLOSED)
		return -1;

	if (len == 0) {
        return 0;
    }

	tunnel_t *tunnel = ch->tunnel;

	/* if channel is still opening, buffer the data */
	if (ch->state == SSH_CH_STATE_OPENING) {
		if (ch->write_len + len > ch->write_buf_size) {
			WARNING("ssh: write buffer overflow for %s:%d (opening), dropping %zu bytes", ch->dest_host, ch->dest_port, len);
			return -1;
		}
		memcpy(ch->write_buf + ch->write_len, data, len);
		ch->write_len += len;
		return (int)len;
	}

	/* If no buffered data, try to write directly first (common fast-path) */
	if (ch->write_len == 0) {
		ssize_t wn = libssh2_channel_write(ch->libssh2_ch, (const char *)data, len);
		if (wn > 0) {
			ch->tx_bytes += wn;
			if ((size_t)wn < len) {
				//partial write — buffer the remainder
				size_t remain = len - wn;
				memcpy(ch->write_buf, (const char *)data + wn, remain);
				ch->write_len = remain;
				if (!tunnel->write_registered) {
					event_add(&tunnel->write_event, NULL);
					tunnel->write_registered = 1;
				}
			}
			if (tunnel->on_channel_touch) {
                tunnel->on_channel_touch(ch, tunnel->cb_ctx);
            }
			return (int)len;
		} else if (wn == LIBSSH2_ERROR_EAGAIN) {
			// can't write now — buffer for later
			if (len > ch->write_buf_size) {
				WARNING("ssh: write buffer overflow for %s:%d, dropping %zu bytes", ch->dest_host, ch->dest_port, len);
				return -1;
			}
			memcpy(ch->write_buf, data, len);
			ch->write_len = len;
			if (!tunnel->write_registered) {
				event_add(&tunnel->write_event, NULL);
				tunnel->write_registered = 1;
			}
			if (tunnel->on_channel_touch) {
                tunnel->on_channel_touch(ch, tunnel->cb_ctx);
            }
			return (int)len;
		} else if (wn < 0) {
			ERROR("ssh: channel write error: %zd", wn);
			return -1;
		}
		/* wn == 0 shouldn't happen for channel_write, treat as EAGAIN */
	}

	//there's already buffered data — append new data, then flush
	if (ch->write_len > 0) {
		if (ch->write_len + len > ch->write_buf_size) {
			WARNING("ssh: write buffer overflow for %s:%d, dropping %zu bytes", ch->dest_host, ch->dest_port, len);
			return -1;
		}
		memcpy(ch->write_buf + ch->write_len, data, len);
		ch->write_len += len;

		// try to flush buffered data
		ssize_t wn = libssh2_channel_write(ch->libssh2_ch, (const char *)ch->write_buf, ch->write_len);
		if (wn > 0) {
			ch->tx_bytes += wn;
			if ((size_t)wn < ch->write_len) {
				memmove(ch->write_buf, ch->write_buf + wn, ch->write_len - wn);
			}
			ch->write_len -= wn;
		} else if (wn == LIBSSH2_ERROR_EAGAIN) {
			// can't write now, data is buffered
			if (!tunnel->write_registered) {
				event_add(&tunnel->write_event, NULL);
				tunnel->write_registered = 1;
			}
		} else if (wn < 0) {
			ERROR("ssh: channel write error: %zd", wn);
			return -1;
		}

		// if there's still buffered data, ensure write events are registered
		if (ch->write_len > 0 && !tunnel->write_registered) {
			event_add(&tunnel->write_event, NULL);
			tunnel->write_registered = 1;
		}
	}

	if (tunnel->on_channel_touch) {
        tunnel->on_channel_touch(ch, tunnel->cb_ctx);
    }

	return (int)len;
}

//helper: check if a session error is fatal (connection lost)
static int ssh_is_session_fatal_error(int err)
{
	return err == LIBSSH2_ERROR_SOCKET_DISCONNECT ||
	       err == LIBSSH2_ERROR_SOCKET_SEND ||
	       err == LIBSSH2_ERROR_SOCKET_RECV ||
	       err == LIBSSH2_ERROR_PROTO;
}

void ssh_tunnel_process_channels(tunnel_t *tunnel)
{
	if (!tunnel->connected || !tunnel->session) {
        return;
    }

	/* 无channel时SSH socket 上的非channel 包(GLOBAL_REQUEST、keepalive 回复等)
	 * 无法被channel_read消费，EV_PERSIST会持续触发on_readable造成忙等。
	 * 暂时摘除read_event，100ms后由 read_rearm_timer重新arm。
	 * 有channel时channel_read会顺带消费 transport，不进入此分支。
	 *
	 * 注意：libssh2 1.11.1公开API中只有channel_read会读transport层，
	 * keepalive_send/transport_send均为纯发送，不消费输入队列。
	 * 因此无channel时无法主动drain，只能靠低频rearm避免忙等；
	 * session致命错误检测由keepalive_timer的keepalive_send在发送失败时触发。 */
	if (list_empty(&tunnel->channels)) {
		event_del(&tunnel->read_event);
		struct timeval rv = { .tv_sec = 0, .tv_usec = 100000 }; /* 100ms */
		evtimer_add(&tunnel->read_rearm_timer, &rv);
		return;
	}

	ssh_channel_t *ch, *tmp;
	int any_progress;
	int still_have_pending = 0;
	int session_dead = 0;

	do {
		any_progress = 0;
		list_for_each_entry_safe(ch, tmp, &tunnel->channels, list) {
            // 1) try to open pending channels
            if (ch->state == SSH_CH_STATE_OPENING) {
                ch->libssh2_ch = libssh2_channel_direct_tcpip_ex(tunnel->session, ch->dest_host, ch->dest_port,	"127.0.0.1", 0);
                if (ch->libssh2_ch) {
                    ch->state = SSH_CH_STATE_READY;
                    any_progress = 1;
                    INFO("ssh: channel opened to %s:%d", ch->dest_host, ch->dest_port);
                    if (tunnel->on_channel_open) {
                        tunnel->on_channel_open(ch, tunnel->cb_ctx);
                    } else {
                        ERROR("ssh: on_channel_open callback is NULL!");
                    }
                } else {
                    int err = libssh2_session_last_errno(tunnel->session);
                    if (err == LIBSSH2_ERROR_EAGAIN) {
                        /* keep waiting */
                    } else if (ssh_is_session_fatal_error(err)) {
                        session_dead = 1;
                        break;
                    } else {
                        char *msg = NULL;
                        libssh2_session_last_error(tunnel->session, &msg, NULL, 0);
                        ERROR("ssh: open channel to %s:%d failed (err=%d): %s", ch->dest_host, ch->dest_port, err, msg ? msg : "unknown");
                        ch->state = SSH_CH_STATE_CLOSED;
                        if (tunnel->on_channel_close) {
                            tunnel->on_channel_close(ch, tunnel->cb_ctx);
                        }
                        /* ch is freed by on_channel_close → ssh_tunnel_close_channel;
                           must not touch ch beyond this point */
                        continue;
                    }
                }
            }

			// 2 read from ready channels → write to client */
			if (ch->state == SSH_CH_STATE_READY && ch->libssh2_ch) {
				char buf[SSH_READ_BUF_SIZE];
				ssize_t n;

				do {
					n = libssh2_channel_read(ch->libssh2_ch, buf, sizeof(buf));
					if (n > 0) {
						any_progress = 1;
						bufferevent_write(ch->client_bev, buf, n);
						ch->rx_bytes += n;
						if (tunnel->on_channel_touch) {
                            tunnel->on_channel_touch(ch, tunnel->cb_ctx);
                        }
					}
				} while (n > 0);

				if (n == 0 || (n < 0 && n != LIBSSH2_ERROR_EAGAIN)) {
					// check if it's a session-level fatal error
					if (n < 0) {
						int err = libssh2_session_last_errno(tunnel->session);
						if (ssh_is_session_fatal_error(err)) {
							session_dead = 1;
							break;
						}
					}
					if (n == 0 || libssh2_channel_eof(ch->libssh2_ch)) {
						INFO("ssh: channel to %s:%d closed by remote", ch->dest_host, ch->dest_port);
						ch->state = SSH_CH_STATE_CLOSED;
						if (tunnel->on_channel_close) {
                            tunnel->on_channel_close(ch, tunnel->cb_ctx);
                        }
						continue;
					}
				}

				// 3 try to flush pending writes to SSH channel
				if (ch->write_len > 0) {
					ssize_t wn = libssh2_channel_write(ch->libssh2_ch, (const char *)ch->write_buf, ch->write_len);
					if (wn > 0) {
						any_progress = 1;
						ch->tx_bytes += wn;
						if ((size_t)wn < ch->write_len) {
							memmove(ch->write_buf, ch->write_buf + wn,
							        ch->write_len - wn);
						}
						ch->write_len -= wn;
						if (tunnel->on_channel_touch)
							tunnel->on_channel_touch(ch, tunnel->cb_ctx);
					} else if (wn < 0 && wn != LIBSSH2_ERROR_EAGAIN) {
						int err = libssh2_session_last_errno(tunnel->session);
						if (ssh_is_session_fatal_error(err)) {
							session_dead = 1;
							break;
						}
					}
				}
			}
		}
	} while (any_progress && !session_dead);

	//if session is dead, disconnect the entire tunnel (which starts reconnect)
	if (session_dead) {
		ERROR("ssh: session-level error detected, disconnecting tunnel");
		ssh_tunnel_disconnect(tunnel);
		return;
	}

	// check if we still need write events
	list_for_each_entry(ch, &tunnel->channels, list) {
		if (ch->state != SSH_CH_STATE_CLOSED &&
		    (ch->write_len > 0 || ch->state == SSH_CH_STATE_OPENING)) {
			still_have_pending = 1;
			break;
		}
	}

	if (still_have_pending && !tunnel->write_registered) {
		event_add(&tunnel->write_event, NULL);
		tunnel->write_registered = 1;
	} else if (!still_have_pending && tunnel->write_registered) {
		event_del(&tunnel->write_event);
		tunnel->write_registered = 0;
	}
}

int ssh_tunnel_is_connected(tunnel_t *tunnel)
{
	return tunnel && tunnel->connected;
}
