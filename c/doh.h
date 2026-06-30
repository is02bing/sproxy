#pragma once
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/listener.h>
#include <event2/util.h>
#include <event2/http.h>
#include <event2/bufferevent_ssl.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "dns.h"

typedef enum {
	schema_https,
	schema_http,
} http_schema;

typedef enum  {
	HTTP_STATE_INIT,
	HTTP_RECV_BODY,
	HTTP_CALLBACK_DONE
} http_state;

typedef void (*http_callback_t)(void* data, size_t len, void* ctx, void* arg) ;
typedef struct {
	http_state               state;
	http_schema              schema;
	char                     host[32];
	uint16_t                 port;
	char                     uri[512];
	struct ssl_st*           ssl;
	struct bufferevent       *bev;
	void*                    ctx;
	void*                    arg;
	http_callback_t          callback;
} http_t;

// make doh request
void doh_request(struct event_base* base, const char* url, void* ctx, void* arg, http_callback_t cb);

//convert dns over http[s] to udp dns packet
uint16_t doh_convert(void* json, size_t len, uint16_t id, dns_packet_t* packet);
