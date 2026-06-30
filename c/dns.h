#pragma once
#include <sys/time.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <assert.h>
#include <stdatomic.h>
#include <pthread.h>
#include <event.h>
#include <libconfig.h>
#include "utils.h"
#include "hashmap.h"

/*
QR	   Indicates if the message is a query (0) or a reply (1)	1bit
OPCODE The type can be QUERY (standard query, 0), IQUERY (inverse query, 1), or STATUS (server status request, 2)	4bit
AA	   Authoritative Answer, in a response, indicates if the DNS server is authoritative for the queried hostname	1bit
TC	   TrunCation, indicates that this message was truncated due to excessive length	1bit
RD	   Recursion Desired, indicates if the client means a recursive query	1bit
RA	   Recursion Available, in a response, indicates if the replying DNS server supports recursion	1bit
Z	   Zero, reserved for future use	3bit
RCODE  Response code, can be NOERROR (0), FORMERR (1, Format error), SERVFAIL (2), NXDOMAIN (3, Nonexistent domain), etc.
 */
typedef struct {
	uint16_t id;

	// flags
	uint16_t rcode:4;
	uint16_t z:3;
	uint16_t ra:1;
	uint16_t rd:1;
	uint16_t tc:1;
	uint16_t aa:1;
	uint16_t opcode:4;
	uint16_t qr:1;

	// count
	uint16_t qdcount;
	uint16_t ancount;
	uint16_t nscount;
	uint16_t arcount;

	//data
	uint8_t  data[1024];
} PACKED dns_packet_t;

typedef struct {
	uint8_t  name[0];
	uint16_t type;
	uint16_t class;
	uint32_t ttl;
	uint16_t rdlength;
	uint8_t  rdata[0];
} PACKED dns_answer_t;

#define KEYWORD_CONTAIN 0
#define KEYWORD_SUFFIX  1
#define KEYWORD_PREFIX  2
#define KEYWORD_STRICT  3

typedef struct {
    const char* keyword;
    uint8_t     keyword_len;
    uint8_t     keyword_type;
} keyword_s;

typedef struct {
	void*              parent;           //point to dns_t
	bool               doh;
	bool               dot;              //标记：使用 DNS over TCP（持久连接复用）
	bool               proxy;           //标记：此 relay 解析的 IP 应加入代理集合
	union {
		struct sockaddr_in server;       //domain name server addr
		char               server_url[256];
	};
	uint16_t          keyword_count;
    char*             keyword_storage;
    keyword_s         keyword_list[0];
} dns_relay_t;

typedef struct {
	uint8_t            server_count;
	struct sockaddr_in servers[6];
} dns_resolv_t;

/* 默认上游 DNS 双缓冲：active 为当前在用槽，standby 为更新槽。
 * 更新流程：写 standby → 原子翻转 active 索引 → 读方切到新数据。 */
#define DNS_RELAY_SLOTS 2
typedef struct {
	dns_resolv_t       slots[DNS_RELAY_SLOTS];
	_Atomic uint8_t    active;   /* 当前在用槽索引（0 或 1） */
} dns_default_relay_t;

typedef struct {
    dns_relay_t*        relay;                       /* proxy DNS relay，命中 proxy_map 时返回（dns_init 按 dns.proxy 分配） */
    struct hashmap      proxy_map[DNS_RELAY_SLOTS];  /* 双缓冲 suffix hash：key=域名(无前导点)，value=relay 指针 */
    char*               key_storage[DNS_RELAY_SLOTS]; /* 每槽域名串存储（NUL 分隔），reconfigure 时在锁下释放旧 standby */
    _Atomic uint8_t     active;
} dns_proxy_relay_t;

typedef struct {
	struct sockaddr_in inaddr;
	uint64_t           create;
	uint16_t           length;
	dns_packet_t       packet;
	dns_relay_t*       relay;
	char               name[256];
	uint16_t           class;
	uint16_t           type;
	uint8_t            retry;
} dns_query_t;

#define MAX_NAME_SERVER_NUMBER 32
typedef struct {
	struct event_base* base;

	struct event       inotify_event;
	int                resolv_watch_fd;

    int                socket_raw_udp;
	struct sockaddr_in listen_addr;
	struct event       listen_event;

	struct sockaddr_in client_addr;
	struct event       client_event;

	struct event       timer_event;

	dns_default_relay_t default_relay;
    dns_proxy_relay_t   proxy_relay;
    dns_relay_t*        relays[MAX_NAME_SERVER_NUMBER];
    size_t              relay_count;
	struct hashmap      query_map;
} dns_t;

int dns_init(dns_t* dns,
             const char *listen_addr, uint16_t client_port,
             const char *proxy_addr,
             int server_count, void *servers,
             int domain_count, const char* const* domains);
void dns_term(dns_t* dns);

/* 解析resolv.conf后推送 nameserver，设置dns默认上游 */
void dns_set_default_relay(dns_t* dns, const char* const* ips, int count);

/* 热更新 @goproxy 域名到 proxy_relay.proxy_map（双缓冲，主从切换）。
 * domain_count/domains 为 goproxy 域名串数组（无前导点）；C 侧拷贝到自有存储。 */
void dns_reconfigure_servers(dns_t* dns, int domain_count, const char* const* domains);