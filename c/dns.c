#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <assert.h>
#include <alloca.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <libconfig.h>
#include <event2/http.h>
#include "dns.h"
#include "doh.h"
#include "dot.h"
#include "proxy.h"
#include "utils.h"
#include "trace.h"

typedef struct  {
	uint16_t raw[6];
} PACKED dns_header_t;

static void dns_header_ntoh(dns_packet_t* packet) {
	dns_header_t* h = (dns_header_t*) packet;
	for (uint8_t idx=0; idx < 6; ++idx) {
		h->raw[idx] = ntohs(h->raw[idx]);
	}
}

static void dns_send_packet(int fd, dns_packet_t* packet, uint16_t len, struct sockaddr_in* addr) {
	//change header to network endian
	dns_header_t* h = (dns_header_t*) packet;
	for (uint8_t idx=0; idx < 6; ++idx) {
		h->raw[idx] = htons(h->raw[idx]);
	}

	ssize_t out_len = sendto(fd, packet, len, 0, addr, sizeof(*addr));
	if (out_len != len) {
		ERRNO("sending %d bytes, but only %d were sent.", (int)len, (int)out_len);
	}

	//change it back to host endian
	for (uint8_t idx=0; idx < 6; ++idx) {
		h->raw[idx] = ntohs(h->raw[idx]);
	}
}

static bool dns_read_qname(dns_query_t* request)
{
	dns_packet_t* packet = &request->packet;
	char*  name = request->name;
	size_t name_size = sizeof(request->name);

	uint8_t *body = (uint8_t *) packet->data;
	size_t body_len = (uint8_t*)packet + request->length - packet->data;

	uint8_t* qname_s = body;
	uint8_t* qname_e = body + body_len - 4;  //name部分结束
	uint8_t qname_len = 0;

	uint8_t nl = 0;
	// get the domain name
	while(qname_s < qname_e) {
		qname_len = *qname_s;
		if (qname_len >= 1) {
			if (qname_len > 64 || (size_t)(nl + qname_len + 1) >= name_size) {
				ERROR("qname invalid:\t", (void*)qname_s+1, (int)qname_len);
				return false;
			}

			memcpy(name + nl, qname_s + 1, qname_len);
			name[nl + qname_len] = '.';
			nl += (qname_len + 1);
			//next part;
			qname_s = qname_s + 1 + qname_len;
		} else {
			//name end
			qname_s = qname_s + 1;
			break;
		}
	}

	if (nl >= 1 && nl <= (name_size-1)) {
		name[nl] = 0;
	}

	uint16_t* qptr = (uint16_t*)qname_s;
	request->type  = ntohs(*qptr);
	request->class = ntohs(*(qptr + 1));

	/*
    TYPE fields are used in resource records.  Note that these types are a subset of QTYPEs.
	TYPE            value and meaning
	A               1 a host address
	NS              2 an authoritative name server
	MD              3 a mail destination (Obsolete - use MX)
	MF              4 a mail forwarder (Obsolete - use MX)
	CNAME           5 the canonical name for an alias
	SOA             6 marks the start of a zone of authority
	MB              7 a mailbox domain name (EXPERIMENTAL)
	MG              8 a mail group member (EXPERIMENTAL)
	MR              9 a mail rename domain name (EXPERIMENTAL)
	NULL            10 a null RR (EXPERIMENTAL)
	WKS             11 a well known service description
	PTR             12 a domain name pointer
	HINFO           13 host information
	MINFO           14 mailbox or mail list information
	MX              15 mail exchange
	TXT             16 text strings
	*/
	if (request->type == 0x1 && request->class == 0x1) {
		return (nl >= 1 && nl <= (name_size-1));
	} else {
		return true;
	}
}

/* 大小写不敏感的字符串 hash/equal，用于 proxy_map（key 为域名串指针） */
static size_t str_case_hash(long key, void *ctx) {
	(void)ctx;
	const char *s = (const char *)key;
	size_t h = 0;
	while (*s) {
		h = h * 31 + tolower((unsigned char)*s);
		s++;
	}
	return h;
}

static bool str_case_equal(long key1, long key2, void *ctx) {
	(void)ctx;
	return strcasecmp((const char *)key1, (const char *)key2) == 0;
}

static dns_relay_t* dns_select_relay(dns_t* dns, char* name)
{
	size_t nl = strlen(name);
	if (nl == 0) return NULL;

	/* 1. 关键词 relay 线性扫描（静态，init 后不变，无需锁） */
	for (size_t ridx = 0; ridx < dns->relay_count; ++ridx) {
		dns_relay_t* relay = dns->relays[ridx];
		for (uint16_t kidx = 0; kidx < relay->keyword_count; ++kidx) {
			keyword_s* k = &relay->keyword_list[kidx];
			if (nl < k->keyword_len) {
				continue;
			}
			if (k->keyword_type == KEYWORD_CONTAIN) {
				if (strcasestr(name, k->keyword) != NULL) {
					return relay;
				}
			} else if (k->keyword_type == KEYWORD_SUFFIX) {
				if (strcasecmp(name + nl - k->keyword_len, k->keyword) == 0) {
					return relay;
				}
			} else if (k->keyword_type == KEYWORD_PREFIX) {
				if (strncasecmp(name, k->keyword, k->keyword_len) == 0) {
					return relay;
				}
			} else if (k->keyword_type == KEYWORD_STRICT) {
				if (nl == k->keyword_len && strncasecmp(name, k->keyword, k->keyword_len) == 0) {
					return relay;
				}
			}
		}
	}

	/* 2. proxy_relay.proxy_map：qname 从长到短逐字符缩短，查suffix（最长优先） */
	char lower[256];
	if (nl >= sizeof(lower)) nl = sizeof(lower) - 1;
	memcpy(lower, name, nl);
	lower[nl] = '\0';
	for (size_t i = 0; i < nl; ++i) {
		lower[i] = (char)tolower((unsigned char)lower[i]);
	}

	uint8_t active = atomic_load(&dns->proxy_relay.active);
	struct hashmap* pm = &dns->proxy_relay.proxy_map[active];
	for (size_t offset = 0; offset < nl; ++offset) {
		long value;
		if (hashmap_find(pm, (long)(lower + offset), &value)) {
			return (dns_relay_t*)dns->proxy_relay.relay;
		}
	}
	return NULL;
}

static struct sockaddr_in* dns_select_server(dns_t* dns, uint8_t retry) {
	uint8_t idx = atomic_load(&dns->default_relay.active);
	dns_resolv_t* relay = &dns->default_relay.slots[idx];
	if (relay->server_count == 0) {
		return NULL;
	}
	return &(relay->servers[retry % relay->server_count]);
}

static bool is_single_qname_query(dns_packet_t* packet) {
	return (packet->qr == 0 && packet->opcode == 0x0 && packet->qdcount == 0x1);
}

// DNS 记录/查询类型数值 → 短名，未知返回 NULL（用 dns_type_str 兜底）
static const char* dns_type_name(uint16_t type) {
	switch (type) {
		case 1:   return "A";
		case 2:   return "NS";
		case 5:   return "CNAME";
		case 6:   return "SOA";
		case 12:  return "PTR";
		case 15:  return "MX";
		case 16:  return "TXT";
		case 28:  return "AAAA";
		case 33:  return "SRV";
		case 35:  return "NAPTR";
		case 39:  return "DNAME";
		case 43:  return "DS";
		case 46:  return "RRSIG";
		case 47:  return "NSEC";
		case 48:  return "DNSKEY";
		case 50:  return "NSEC3";
		case 99:  return "SPF";
		case 257: return "CAA";
		default:  return NULL;
	}
}

// 把 type 拼进 buf（"A" / "T99"）
static void dns_type_str(uint16_t type, char *buf, size_t bufsize) {
	const char* name = dns_type_name(type);
	if (name) {
        snprintf(buf, bufsize, "%s", name);
    } else {
        snprintf(buf, bufsize, "T%u", (unsigned)type);
    }
}

/* 解析 DNS 名称（支持压缩指针），点分隔写入 out，返回从 ptr 起消耗的字节数。
 * base = 报文起始（压缩指针基准）。失败返回 0。 */
static size_t dns_parse_name(const uint8_t *base, const uint8_t *ptr, const uint8_t *end, char *out, size_t outsize) {
	size_t used = 0;
	int jumped = 0;
	const uint8_t *p = ptr;
	const uint8_t *first_end = ptr;
	int loops = 0;

	while (p < end && loops < 128) {
		loops++;
		uint8_t len = *p;
		if (len == 0) {
			if (!jumped) first_end = p + 1;
			break;
		}
		if ((len & 0xC0) == 0xC0) {
			if (p + 1 >= end) return 0;
			if (!jumped) first_end = p + 2;
			uint16_t off = ((len & 0x3F) << 8) | p[1];
			if (off >= (size_t)(end - base)) return 0;
			p = base + off;
			jumped = 1;
			continue;
		}
		if (p + 1 + len > end) return 0;
		if (used + len + 1 >= outsize) return 0;
		if (used) out[used++] = '.';
		memcpy(out + used, p + 1, len);
		used += len;
		p += 1 + len;
	}
	if (outsize) out[used < outsize ? used : outsize - 1] = '\0';
	return (size_t)(first_end - ptr);
}

/* 从 DNS 应答中提取所有 answer 记录，拼成 "CNAME x.y A 1.2.3.4 AAAA ::1" 写入 summary，
 * 并对每个 A 记录调用 Rust 回调 cb。返回 A 记录数量。 */
static int dns_extract_records_summary(dns_packet_t *packet, size_t packet_len,
                                       char *summary, size_t summary_size,
                                       dns_query_t *query) {
	dns_resolve_cb cb = proxy_get_dns_callback();
    summary[0] = '\0';
	uint8_t *base = (uint8_t *)packet;
	uint8_t *end = base + packet_len;
	uint8_t *ptr = packet->data;
	int a_count = 0;
	size_t sum_used = 0;

	/* 跳过 question section */
	for (uint16_t i = 0; i < packet->qdcount && ptr < end; i++) {
		char tmp[256];
		size_t n = dns_parse_name(base, ptr, end, tmp, sizeof(tmp));
		if (n == 0) return a_count;
		ptr += n;
		if (ptr + 4 > end) return a_count;
		ptr += 4; /* QTYPE + QCLASS */
	}

	/* 遍历 answer section */
	for (uint16_t i = 0; i < packet->ancount && ptr < end; i++) {
		char rrname[256];
		size_t n = dns_parse_name(base, ptr, end, rrname, sizeof(rrname));
		if (n == 0) break;
		ptr += n;

		if (ptr + 10 > end) break;
		uint16_t type, rdlength;
		memcpy(&type, ptr, 2); type = ntohs(type); ptr += 2;
		ptr += 2; /* CLASS */
		ptr += 4; /* TTL */
		memcpy(&rdlength, ptr, 2); rdlength = ntohs(rdlength); ptr += 2;
		if (ptr + rdlength > end) break;

		char tbuf[8];
		const char *tname = dns_type_name(type);
		if (!tname) { dns_type_str(type, tbuf, sizeof(tbuf)); tname = tbuf; }

		char value[80] = {0};
		if (type == 1 && rdlength == 4) {
			snprintf(value, sizeof(value), "%d.%d.%d.%d", ptr[0], ptr[1], ptr[2], ptr[3]);
			if (cb) {
                cb(value, query->name, query->relay ? query->relay->proxy : 0);
            }
			a_count++;
		} else if (type == 28 && rdlength == 16) {
			const uint8_t *b = ptr;
			snprintf(value, sizeof(value),
			         "%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x",
			         b[0],b[1],b[2],b[3],b[4],b[5],b[6],b[7],
			         b[8],b[9],b[10],b[11],b[12],b[13],b[14],b[15]);
		} else if (type == 5 || type == 2 || type == 12 || type == 39) {
			/* CNAME/NS/PTR/DNAME — 值为域名 */
			dns_parse_name(base, ptr, end, value, sizeof(value));
		} else if (type == 15 && rdlength >= 3) {
			/* MX: preference + domain */
		uint16_t pref = (ptr[0] << 8) | ptr[1];
		char mx[72];
		dns_parse_name(base, ptr + 2, end, mx, sizeof(mx));
		snprintf(value, sizeof(value), "%u %s", (unsigned)pref, mx);
	} else if (type == 16 && rdlength >= 1) {
		/* TXT：至少 1 字节长度前缀，rdlength==0 时 ptr 可能 == end，ptr[0] 越界 */
		size_t tl = ptr[0];
		if (tl + 1 <= rdlength && tl < sizeof(value)) {
				memcpy(value, ptr + 1, tl);
				value[tl] = '\0';
			}
		} else {
			snprintf(value, sizeof(value), "<%u bytes>", (unsigned)rdlength);
		}

        int w = snprintf(summary + sum_used, summary_size - sum_used, i ? " %s %s" : "%s %s", tname, value);
        if (w > 0 && (size_t)w < summary_size - sum_used) {
            sum_used += w;
        } else {
            break;
        }
		ptr += rdlength;
	}
	return a_count;
}

static void dns_handle_reply(dns_t* dns, dns_packet_t* packet, size_t packet_len, struct sockaddr_in* src_addr) {
	dns_query_t *r = NULL;
	if (hashmap_delete(&(dns->query_map), (long)packet->id, NULL, (long*)&r)) {
		char summary[512] = {0};
		dns_extract_records_summary(packet, packet_len, summary, sizeof(summary), r);
        INFO("reply 0x%x %s %n <- %n [%s]", (int)packet->id, r->name, &r->inaddr, src_addr, summary);
        dns_send_packet(event_get_fd(&dns->listen_event), packet, packet_len, &r->inaddr);
		free(r);
	} else {
		WARNING("reply from:%n id:0x%x, drop", src_addr, (int)packet->id);
	}
}

static void dns_handle_doh_reply(void* data, size_t len, void*ctx, void* arg) {
	dns_query_t *r = NULL;
	dns_t* dns = ctx;
	if (dns == NULL) {
		CRITICAL("invalid context when reply:\n%t", data, len);
		return;
	}

	long id = (long)arg;
	if (hashmap_delete(&(dns->query_map), id, NULL, (long*)&r)) {
		dns_packet_t reply;
		uint16_t packet_len = doh_convert(data, len, id, &reply);
		char summary[512] = {0};
		dns_extract_records_summary(&reply, packet_len, summary, sizeof(summary), r);
		INFO("dns reply 0x%x %s %n <- %s [%s]", (int)id, r->name, &r->inaddr, r->relay->server_url, summary);
		dns_send_packet(event_get_fd(&dns->listen_event), &reply, packet_len, &r->inaddr);
		free(r);
	} else {
		if (data != NULL && len > 0) {
			WARNING("dns reply 0x%x drop\n%t", id, data, len);
		} else {
			WARNING("dns reply 0x%x drop", id);
		}
	}
}

/* DoT 回包回调：data 为网络字节序的 DNS 报文（不含 2 字节长度前缀） */
static void dns_handle_dot_reply(void* data, size_t len, void* ctx, void* arg) {
	dns_t* dns = ctx;
	if (dns == NULL) return;

	/* data/len 为 0 表示连接失败，保留 query_map 条目等重试 */
	if (data == NULL || len < 12) {
		return;
	}

	long id = (long)arg;
	dns_query_t* r = NULL;
	if (hashmap_delete(&(dns->query_map), id, NULL, (long*)&r)) {
		dns_packet_t reply;
		if (len > sizeof(reply)) len = sizeof(reply);
		memcpy(&reply, data, len);
		dns_header_ntoh(&reply);

		char summary[512] = {0};
		dns_extract_records_summary(&reply, len, summary, sizeof(summary), r);
		INFO("dot reply 0x%x %s %n <- %n [%s]", (int)id, r->name, &r->inaddr, &r->relay->server, summary);
		dns_send_packet(event_get_fd(&dns->listen_event), &reply, len, &r->inaddr);
		free(r);
	} else {
		WARNING("dot reply 0x%x drop", (int)id);
	}
}

/* 把 DNS 报文按网络字节序经 DoT 持久连接发出（header 临时翻序后还原） */
static void dns_send_via_dot(dns_t* dns, dns_packet_t* packet, uint16_t len, struct sockaddr_in* server, uint16_t id) {
	dns_header_t* h = (dns_header_t*) packet;
	for (uint8_t idx = 0; idx < 6; ++idx) h->raw[idx] = htons(h->raw[idx]);
	dot_request(dns->base, server, packet, len, id, dns, (void*)(long)id, dns_handle_dot_reply);
	for (uint8_t idx = 0; idx < 6; ++idx) h->raw[idx] = ntohs(h->raw[idx]);
}

static void dns_handle_query(dns_t* dns, dns_query_t* request) {
	dns_packet_t* packet = &request->packet;
	if (!is_single_qname_query(packet)) {
		ERROR("query from:%n id:0x%x, invalid query\n%t", &request->inaddr, (int) packet->id, (void*)packet, (int)(request->length));
		free(request);
		return;
	}

	dns_read_qname(request);
	if (request->type != 0x0) {
		dns_query_t *old = NULL;
		hm_insert(&dns->query_map, (long) packet->id, (long) request, HASHMAP_SET, NULL, (long *) &old);
		if (old) {
			WARNING("query 0x%x, replace old", (int)packet->id);
			free(old);
		}
	} else {
		ERROR("query 0x%x, invalid\n%t", (int) packet->id, (void*)packet, (int)(request->length));
		free(request);
		return;
	}

    struct sockaddr_in* next_dns = NULL;
	char*  next_doh = NULL;
	struct sockaddr_in* next_dot = NULL;
	if (request->type == 0x1) {
		//host query
		request->relay = dns_select_relay(dns, request->name);
	} else {
		request->relay = NULL;
	}

	if (request->relay) {
		if (request->relay->doh) {
			next_doh = request->relay->server_url;
		} else if (request->relay->dot) {
			next_dot = &request->relay->server;
		} else {
			next_dns = &request->relay->server;
		}
	} else {
		next_dns = dns_select_server(dns, request->retry);
	}

	if (next_doh) {
		char url[550] = {0};
		snprintf(url, sizeof(url), "%s?name=%s", next_doh, request->name);
		doh_request(dns->base, url, dns, (void *) (long) packet->id, dns_handle_doh_reply);
	} else if (next_dot) {
		char qt[8]; dns_type_str(request->type, qt, sizeof(qt));
		INFO("query 0x%x %s %s %n => %n (dot)", (int) packet->id, request->name, qt, &request->inaddr, next_dot);
		dns_send_via_dot(dns, packet, request->length, next_dot, packet->id);
	} else if (next_dns) {
		char qt[8]; dns_type_str(request->type, qt, sizeof(qt));
		INFO("query 0x%x %s %s %n -> %n", (int) packet->id, request->name, qt, &request->inaddr, next_dns);
		dns_send_packet(event_get_fd(&dns->client_event), packet, request->length, next_dns);
	} else {
		WARNING("no upstream DNS available, drop query 0x%x %s", (int) packet->id, request->name);
	}
}

static void dns_handle_retry(dns_t* dns, dns_query_t* request) {
	dns_packet_t* packet = &request->packet;
	struct sockaddr_in* next_dns = NULL;
	char*  next_doh = NULL;
	struct sockaddr_in* next_dot = NULL;
	if (request->relay) {
		if (request->relay->doh) {
			next_doh = request->relay->server_url;
		} else if (request->relay->dot) {
			next_dot = &request->relay->server;
		} else {
			next_dns = &request->relay->server;
		}
	} else {
		next_dns = dns_select_server(dns, request->retry);
	}

	if (next_doh) {
		char url[550] = {0};
		snprintf(url, sizeof(url), "%s?name=%s", next_doh, request->name);
		doh_request(dns->base, url, dns, (void *) (long) packet->id, dns_handle_doh_reply);
	} else if (next_dot) {
		char qt[8]; dns_type_str(request->type, qt, sizeof(qt));
		INFO("query 0x%x %s %s %n => %n (dot retry)", (int) packet->id, request->name, qt, &request->inaddr, next_dot);
		dns_send_via_dot(dns, packet, request->length, next_dot, packet->id);
	} else if (next_dns) {
		char qt[8]; dns_type_str(request->type, qt, sizeof(qt));
		INFO("query 0x%x %s %s %n -> %n", (int) packet->id, request->name, qt, &request->inaddr, next_dns);
		dns_send_packet(event_get_fd(&dns->client_event), packet, request->length, next_dns);
	} else {
		WARNING("no upstream dns available, drop retry 0x%x %s", (int) packet->id, request->name);
	}
}

static void dns_recv_query(int fd, short what, void *_arg) {
    (void)what;
    dns_t* dns = _arg;
    dns_query_t *request = malloc(sizeof(dns_query_t));
    if (request == NULL) {
        ERRNO("malloc memory for incoming query failed");
        return;
    }

    assert(fd == event_get_fd(&dns->listen_event));
    ssize_t   recv_len;
    socklen_t in_addr_len = sizeof(request->inaddr);
    recv_len = recvfrom(fd, (void*)&request->packet, sizeof(request->packet), MSG_DONTWAIT, &request->inaddr, &in_addr_len);
    if (recv_len == -1) {
        ERRNO("recv query failed");
        free(request);
        return;
    }

    if (recv_len <= 12) {
        WARNING("query from %n, invalid length\n%t", &request->inaddr, request->packet, recv_len);
        free(request);
        return;
    }

    request->length = recv_len;
    request->create = unixtimelong();
    request->retry = 0;
    dns_header_ntoh(&request->packet);
    dns_handle_query(dns, request);
}

static void dns_recv_reply(int fd, short what, void *_arg) {
	(void)what;
	dns_t*             dns = _arg;
	struct sockaddr_in inaddr;
	dns_packet_t       packet;
	ssize_t            recv_len;

	assert(fd == event_get_fd(&dns->client_event));
	socklen_t inaddr_len = sizeof(inaddr);
	recv_len = recvfrom(fd, (void*)&packet, sizeof(packet), 0, &inaddr, &inaddr_len);
	if (recv_len == -1) {
		ERRNO("recv response failed");
		return;
	}

	if (recv_len <= 12) {
		WARNING("resp length %n invalid\n%t", &inaddr, &packet, recv_len);
		return;
	}

	dns_header_ntoh(&packet);
	dns_handle_reply(dns, &packet, recv_len, &inaddr);
}

static void dns_timer(int fd, short what, void *_arg) {
	(void)fd; (void)what;
	dns_t *dns = _arg;
	struct hashmap_entry* cur;
	size_t bkt;
	uint64_t now = unixtimelong();

	long      delete_key[128] = {0};
	size_t    delete_size = 0;

	long      retry_key[128] = {0};
	uint16_t  retry_size = 0;

	struct hashmap* m = &dns->query_map;

	hashmap__for_each_entry(m, cur, bkt) {
		dns_query_t* v = cur->pvalue;
		if ( ((now - v->create) >= 2000) || (v->retry >= 3)) {
			if (delete_size < COUNTOF(delete_key)) {
				++delete_size;
				delete_key[delete_size-1] = cur->key;
			}
		} else if (((now - v->create) >= 100)) {
			if (retry_size < COUNTOF(retry_key)) {
				++retry_size;
				retry_key[retry_size-1] = cur->key;
			}
		}
	}

	for (; delete_size > 0; delete_size--) {
		dns_query_t* v = NULL;
		long k = delete_key[delete_size-1];
		if (hashmap_delete(m, k, NULL, (long*)&v)) {
			assert(v);
			int32_t delay = (int32_t)(now - v->create);
			if (is_single_qname_query(&v->packet)) {
				if (v->relay != NULL && v->relay->doh) {
					WARNING("query 0x%x %s -> %s timeout %dms", (int) k, v->name, v->relay->server_url, delay);
				} else if (v->relay){
					WARNING("query 0x%x %s -> %n timeout %dms", (int) k, v->name, &v->relay->server, delay);
				} else {
					WARNING("query 0x%x %s timeout %dms", (int) k, v->name, delay);
				}
			} else {
				WARNING("query 0x%x %n timeout %dms", (int) k, delay);
			}

			free(v);
		}
	}

	for (; retry_size > 0; retry_size--) {
		dns_query_t* v = NULL;
		long k = retry_key[retry_size-1];
		if (hashmap_find(m, k, (long*)&v)) {
			v->retry += 1;
			dns_handle_retry(dns, v);
		}
	}
}

static int dns_add_server(dns_t* dns, const char* keywords, const char* addr, bool proxy) {
	if (dns->relay_count >= MAX_NAME_SERVER_NUMBER) {
		fprintf(stderr, "no enough entry for server, max %d",(int)MAX_NAME_SERVER_NUMBER);
		return -1;
	}

	struct sockaddr_in dns_addr;
	bool doh = (strncmp(addr, "https://", 8) == 0) || (strncmp(addr, "http://", 7) == 0);
	bool dot = (strncmp(addr, "tcp:", 4) == 0);
	const char* addr_to_parse = addr;
	if (dot) {
		addr_to_parse = addr + 4;
		/* 兼容 tcp://8.8.8.8:53（与 https:///http:// 风格一致）和 tcp:8.8.8.8:53 */
		if (strncmp(addr_to_parse, "//", 2) == 0) {
			addr_to_parse += 2;
		}
	}
	if (!doh) {
		if (parse_addr(addr_to_parse, &dns_addr) <= 0) {
			fprintf(stderr, "invalid addr in %s", addr);
			return -1;
		}

        if (dns_addr.sin_addr.s_addr == INADDR_ANY) {
            fprintf(stderr, "addr %s, ip must be provided", addr);
            return -1;
        }
	}

	//count keyword_storage count
	uint16_t count = 1;
	size_t   keyword_len = 0;
	char* p = (char*)keywords;
	while( *p != 0 ) {
		if (*p == '|') {++count;}
		++keyword_len; ++p;
	}

	if (keyword_len <= 0) {
		fprintf(stderr, "invalid match for server %s", addr);
		return -1;
	}

	// relay_map_t + keyword_s * count +  keywords_len
	size_t alloc_len = sizeof(dns_relay_t) + count*sizeof(keyword_s) + keyword_len;
	dns_relay_t* relay = malloc(alloc_len);
	if (relay == NULL) {
		ERROR("malloc failed");
		return -1;
	}
	memset(relay, 0, alloc_len);

	relay->keyword_storage = (char*)(relay->keyword_list + count);

	//copy and build keyword pointers
	{
		//copy string
		memcpy(relay->keyword_storage, keywords, keyword_len);
		char* keyword_end = relay->keyword_storage + keyword_len;

		//set | to 0
		p = relay->keyword_storage;
		while (p < keyword_end) {	if (*p == '|') { *p = 0;} ++p;}

		//build keyword pointer table
		char* ps = relay->keyword_storage; //part start
		do {
			char *pe = ps;             //part end
			while (pe < keyword_end && *pe != 0) { ++pe; }
			//if keyword is not empty
			if ((pe - ps) == 1) {
				relay->keyword_list[relay->keyword_count].keyword = ps;
                relay->keyword_list[relay->keyword_count].keyword_len = pe - ps;
                relay->keyword_list[relay->keyword_count].keyword_type = KEYWORD_CONTAIN;
                relay->keyword_count += 1;
			} else if ((pe - ps) > 1) {
                char s = *ps;
                char* p1 = pe - 1;
                char e = *p1;
                if (s == '^' && e == '$') {
                    *ps = 0; *p1 = 0;
                    relay->keyword_list[relay->keyword_count].keyword = ps + 1;
                    relay->keyword_list[relay->keyword_count].keyword_len = pe - ps - 2;
                    relay->keyword_list[relay->keyword_count].keyword_type = KEYWORD_STRICT;
                }  else if (s == '^') {
                    *ps = 0;
                    relay->keyword_list[relay->keyword_count].keyword = ps + 1;
                    relay->keyword_list[relay->keyword_count].keyword_len = pe - ps - 1;
                    relay->keyword_list[relay->keyword_count].keyword_type = KEYWORD_PREFIX;
                } else if (e == '$'){
                    *p1 = 0;
                    relay->keyword_list[relay->keyword_count].keyword = ps;
                    relay->keyword_list[relay->keyword_count].keyword_len = pe - ps - 1;
                    relay->keyword_list[relay->keyword_count].keyword_type = KEYWORD_SUFFIX;
                } else {
                    relay->keyword_list[relay->keyword_count].keyword = ps;
                    relay->keyword_list[relay->keyword_count].keyword_len = pe - ps;
                    relay->keyword_list[relay->keyword_count].keyword_type = KEYWORD_CONTAIN;
                }
                relay->keyword_count += 1;
            }
			ps = pe + 1;
		} while(ps < keyword_end);
	}
	relay->parent = dns;
	relay->doh = doh;
	relay->dot = dot;
	relay->proxy = proxy;
	if (doh) {
		strncpy(relay->server_url, addr, sizeof(relay->server_url) - 1);
	} else {
        memcpy(&relay->server, &dns_addr, sizeof(relay->server));
	}

	dns->relays[dns->relay_count] = relay;
	dns->relay_count += 1;
	return 0;
}

/* 配置默认的dns relay
 * 流程：解析填入standby槽 → 原子翻转active索引切到新数据。
 * 读方始终从active槽读，翻转瞬间完成切换，无部分更新可见。 */
void dns_set_default_relay(dns_t* dns, const char* const* ips, int count) {
    uint8_t cur = atomic_load(&dns->default_relay.active);
    uint8_t standby = cur ^ 1;  /* 0↔1 */

    dns_resolv_t resolv;
    memset(&resolv, 0, sizeof(resolv));
    for (int i = 0; i < count && resolv.server_count < (int)COUNTOF(resolv.servers); i++) {
        struct in_addr a;
        if (inet_pton(AF_INET, ips[i], &a) == 1) {
            resolv.servers[resolv.server_count].sin_addr = a;
            resolv.servers[resolv.server_count].sin_family = AF_INET;
            resolv.servers[resolv.server_count].sin_port = htons(53);
            resolv.server_count++;
        }
    }

    if (resolv.server_count == 0) {
        return;
    }

    /* 写空闲槽，完成后原子切换 active */
    memcpy(&dns->default_relay.slots[standby], &resolv, sizeof(resolv));
    atomic_store(&dns->default_relay.active, standby);
    TRACE("default_relay switched to slot %d: %d servers", standby, resolv.server_count);
}

/* 从 /etc/resolv.conf 读取 nameserver，写入 default_relay。
 * 跳过注释行（# / ;）和 127.x loopback。读取失败不影响启动（default_relay 保持空，
 * 后续 inotify 可再推）。 */
void dns_load_resolv_conf(dns_t* dns) {
	FILE* fp = fopen("/etc/resolv.conf", "r");
	if (fp == NULL) {
		ERRNO("open /etc/resolv.conf");
		return;
	}

	const char* ips[8];
	int count = 0;
	char line[256];

	while (fgets(line, sizeof(line), fp) != NULL && count < (int)COUNTOF(ips)) {
		/* 截断行内注释 */
		char* hash = strchr(line, '#');
		if (hash) *hash = '\0';

		/* 跳过前导空白 */
		char* p = line;
		while (*p == ' ' || *p == '\t') p++;

		if (strncmp(p, "nameserver", 10) != 0) continue;
		p += 10;
		if (*p != ' ' && *p != '\t') continue;

		/* 跳过空白取 IP */
		while (*p == ' ' || *p == '\t') p++;
		char* start = p;
		while (*p != '\0' && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') p++;
		*p = '\0';
		if (*start == '\0') continue;

		/* 跳过 127.x loopback */
		if (strncmp(start, "127.", 4) == 0) continue;

		/* 校验为合法 IPv4 */
		struct in_addr a;
		if (inet_pton(AF_INET, start, &a) != 1) continue;

		ips[count++] = start;
	}
	fclose(fp);

	if (count > 0) {
		/* 复制到本地存储再调用（dns_set_default_relay 只在调用期间引用字符串） */
		char storage[8][INET_ADDRSTRLEN];
		const char* ptrs[8];
		for (int i = 0; i < count; i++) {
			strncpy(storage[i], ips[i], INET_ADDRSTRLEN - 1);
			storage[i][INET_ADDRSTRLEN - 1] = '\0';
			ptrs[i] = storage[i];
		}
		dns_set_default_relay(dns, ptrs, count);
		INFO("loaded %d nameserver(s) from /etc/resolv.conf", count);
	} else {
		WARNING("no external nameserver found in /etc/resolv.conf");
	}
}

/* 热更新 @goproxy 域名到 proxy_relay.proxy_map（主从双缓冲）。
 * 流程：拷贝域名到新 key_storage → 清空 standby 槽 → 填充 standby → 原子翻转 active。
 * 全程持锁，与 dns_select_relay 的 proxy_map 查询互斥，避免 hm_clear 期间的 UAF。 */
void dns_reconfigure_servers(dns_t* dns, int domain_count, const char* const* domains) {
	/* 先在锁外计算总长度并分配 key_storage（NUL 分隔的域名串） */
	size_t total = 0;
	for (int i = 0; i < domain_count; ++i) {
		if (domains[i] != NULL) {
			total += strlen(domains[i]) + 1;
		}
	}
	char* storage = NULL;
	size_t added = 0;
	if (total > 0) {
		storage = malloc(total);
		if (storage != NULL) {
			char* p = storage;
			for (int i = 0; i < domain_count; ++i) {
				if (domains[i] == NULL) continue;
				size_t len = strlen(domains[i]) + 1;
				memcpy(p, domains[i], len);
				p += len;
				added++;
			}
		}
	}

	uint8_t cur = atomic_load(&dns->proxy_relay.active);
	uint8_t standby = cur ^ 1;

	/* 释放旧 standby 存储 + 清空 map（锁内，无读者并发） */
	free(dns->proxy_relay.key_storage[standby]);
	dns->proxy_relay.key_storage[standby] = NULL;
	hm_clear(&dns->proxy_relay.proxy_map[standby]);

	/* 写入新 standby：每个域名作 key，value 指向 proxy_relay.relay */
	dns->proxy_relay.key_storage[standby] = storage;
	if (storage != NULL && dns->proxy_relay.relay != NULL) {
		char* ks = storage;
		for (size_t i = 0; i < added; ++i) {
			hashmap__add(&dns->proxy_relay.proxy_map[standby], (long)ks, (long)dns->proxy_relay.relay);
			ks += strlen(ks) + 1;
		}
	}
	atomic_store(&dns->proxy_relay.active, standby);
	TRACE("dns proxy_map reconfigured: slot %d, %d domains", standby, (int)added);
}

static size_t dns_query_id_hash_fn(long key, void *ctx) {
	(void)ctx;
	return key;
}

static bool dns_query_id_equal_fn(long key1, long key2, void *ctx) {
	(void)ctx;
	return key1 == key2;
}

int dns_init(dns_t* dns, const char *listen_addr, uint16_t client_port, const char *proxy_addr, int server_count, void *servers_ptr, int domain_count, const char* const* domains) {
	int error;
	int fd0 = -1, fd1 = -1, fd2 = -1;
    int optval = 1;
	proxy_dns_server_t *srv_arr = (proxy_dns_server_t*)servers_ptr;

	if (!listen_addr) {
		fprintf(stderr, "empty listen which must be config");
		return -1;
	} else {
        if (parse_addr(listen_addr, &dns->listen_addr) <= 0) {
            fprintf(stderr, "invalid listen %s", listen_addr);
        }
    }

	if (client_port < 1 || client_port > 65534 || client_port == 53) {
		fprintf(stderr, "invalid dns client port %d", client_port);
		return -1;
	}

	dns->client_addr.sin_family = AF_INET;
	dns->client_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	dns->client_addr.sin_port = htons(client_port);

	/* 解析 dns.proxy 配置：分配 proxy_relay.relay（命中 proxy_map 时返回） */
	if (proxy_addr != NULL) {
		struct sockaddr_in proxy_sin;
		if (parse_addr(proxy_addr, &proxy_sin) > 0) {
			dns_relay_t* r = malloc(sizeof(dns_relay_t));
			if (r != NULL) {
				memset(r, 0, sizeof(dns_relay_t));
				r->parent = dns;
				r->doh = false;
				r->proxy = true;
				r->server = proxy_sin;
				r->keyword_count = 0;
				r->keyword_storage = NULL;
				dns->proxy_relay.relay = r;
			}
		} else {
			ERROR("invalid dns proxy addr %s", proxy_addr);
		}
	}

	/* 初始化 proxy_map 双缓冲 + 自旋锁 */
	for (int s = 0; s < DNS_RELAY_SLOTS; ++s) {
		hm_init(&dns->proxy_relay.proxy_map[s], str_case_hash, str_case_equal, NULL);
	}
	atomic_store(&dns->proxy_relay.active, 0);

	/* 填入初始 @goproxy 域名到 proxy_map（relay 已在上面分配） */
	if (domain_count > 0 && domains != NULL) {
		dns_reconfigure_servers(dns, domain_count, domains);
	}

	/* add servers */
	for (int i = 0; i < server_count; ++i) {
		if (srv_arr[i].match == NULL || srv_arr[i].server == NULL) {
			ERROR("invalid servers[%d], match and server empty", i);
			return -1;
		}
		if (dns_add_server(dns, srv_arr[i].match, srv_arr[i].server, srv_arr[i].proxy) < 0) {
			return -1;
		}
	}

	hm_init(&dns->query_map, dns_query_id_hash_fn, dns_query_id_equal_fn, &dns);

    fd0 = socket(AF_INET, SOCK_RAW, IPPROTO_UDP);
    if (fd0 == -1) {
        ERRNO("open raw udp socket");
        goto dns_init_failed;
    }

    //允许非特权端口上的使用原始套接字发送IP包
    optval = 1;
    if (setsockopt(fd0, IPPROTO_IP, IP_HDRINCL, &optval, sizeof(optval)) < 0) {
        ERRNO("setsockopt IP_HDRINCL");
        goto dns_init_failed;
    }
    dns->socket_raw_udp = fd0;

	// the udp socket for receive local query
	fd1 = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd1 == -1) {
		ERRNO("open socket");
		goto dns_init_failed;
	}

	error = bind(fd1, (struct sockaddr *) &dns->listen_addr, sizeof(dns->listen_addr));
	if (error) {
		ERRNO("bind %n", &dns->listen_addr);
		goto dns_init_failed;
	}

	error = fcntl_nonblock(fd1);
	if (error) {
		ERRNO("fcntl set nonblock");
		goto dns_init_failed;
	}

	event_set(&(dns->listen_event), fd1, EV_READ | EV_PERSIST, dns_recv_query, (void*)dns);
	error = event_add(&(dns->listen_event), NULL);
	if (error) {
		ERROR("event_add");
		goto dns_init_failed;
	}

    //the udp socket use to do dns query
	fd2 = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd2 == -1) {
		ERRNO("socket");
		goto dns_init_failed;
	}

	error = bind(fd2, (struct sockaddr *) &(dns->client_addr), sizeof(dns->client_addr));
	if (error) {
		ERRNO("bind %n", &dns->client_addr);
		goto dns_init_failed;
	}

	error = fcntl_nonblock(fd2);
	if (error) {
		ERRNO("fcntl set nonblock");
		goto dns_init_failed;
	}

	event_set(&(dns->client_event), fd2, EV_READ | EV_PERSIST, dns_recv_reply, dns);
	error = event_add(&(dns->client_event), NULL);
	if (error) {
		ERROR("event_add");
		goto dns_init_failed;
	}

	struct timeval time;
	time.tv_sec = 0;
	time.tv_usec = 10000;

	event_set(&dns->timer_event, 0, EV_PERSIST, dns_timer, dns);
	error = evtimer_add(&dns->timer_event, &time);
	if (error) {
		ERROR("timer add failed");
		goto dns_init_failed;
	}

	INFO("rdns start, listen: %n", &dns->listen_addr);

	/* 直接从 /etc/resolv.conf 加载默认上游，无需等待外部推送 */
	dns_load_resolv_conf(dns);

	return 0;

dns_init_failed:
    if (fd0 != -1) {
        if (close(fd0) != 0) {
            ERRNO("close %d", fd0);
        }
    }

	if (fd1 != -1) {
		if (close(fd1) != 0) {
			ERRNO("close %d", fd1);
		}
	}

	if (fd2 != -1) {
		if (close(fd2) != 0) {
			ERRNO("close %d", fd2);
		}
	}

	return -1;
}

void dns_term(dns_t* dns) {
	if (event_initialized(&dns->listen_event)) {
		if (event_del(&dns->listen_event) != 0) {
			ERROR("event_del client");
		}

		if (close(event_get_fd(&dns->listen_event)) != 0) {
			ERROR("close listen");
		}
	}

	if (event_initialized(&dns->client_event)) {
		if (event_del(&dns->client_event) != 0) {
			ERROR("event_del client");
		}

		if (close(event_get_fd(&dns->client_event)) != 0) {
			ERROR("close client");
		}
	}

	hm_clear(&dns->query_map);

	for (size_t i = 0; i < dns->relay_count; ++i) {
		free(dns->relays[i]);
	}

	for (int s = 0; s < DNS_RELAY_SLOTS; ++s) {
		hm_clear(&dns->proxy_relay.proxy_map[s]);
		free(dns->proxy_relay.key_storage[s]);
	}
	if (dns->proxy_relay.relay != NULL) {
		free(dns->proxy_relay.relay);
		dns->proxy_relay.relay = NULL;
	}

	/* 释放 DoT 持久连接池 */
	dot_term();
}