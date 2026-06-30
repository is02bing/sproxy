#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <assert.h>
#include "base.h"
#include "doh.h"
#include "utils.h"
#include "trace.h"
#include "json.h"

static uint16_t json_bool(json_val *root, const char *key) {
	json_val *obj = json_obj_get(root, key);
	if (obj && json_is_bool(obj)) {
		return json_get_bool((json_val *) obj) ? 0x1 : 0x0;
	} else {
		return 0;
	}
}

static uint16_t json_uint(json_val* root, const char* key, uint16_t default_value) {
	json_val *obj = json_obj_get(root, key);
	if (obj && json_is_int(obj)) {
		return (uint16_t)json_get_uint((json_val *) obj);
	} else {
		return default_value;
	}
}

static const char* json_str(json_val* root, const char* key) {
	json_val *obj = json_obj_get(root, key);
	if (obj && json_is_str(obj)) {
		return json_get_str((json_val *) obj);
	} else {
		return NULL;
	}
}

//for dns name compression
typedef struct  {
	const char* name;
	uint16_t    offset;
} name_offset_t;

static uint16_t lookup_name_offset(name_offset_t* names, uint16_t count, const char* name) {
	for (uint16_t i = 0; i < count; ++i) {
		if (names[i].name == NULL) {
			return 0;
		}

		if (strcmp(name, names[i].name) == 0) {
			return names[i].offset;
		}
	}

	return 0;
}

static void insert_name_offset(name_offset_t* names, uint16_t count, const char* name, uint16_t offset) {
	if (name != NULL && offset > 0) {
		for (size_t i = 0; i < count; ++i) {
			if (names[i].name == NULL) {
				names[i].name = name;
				names[i].offset = offset;
				return;
			}
		}
	}
}

// write dns qname, return next write pointer
static uint8_t* write_qname(uint8_t* pwrite, const char* name, uint8_t* base, name_offset_t* names, uint16_t count) {
	uint16_t offset = lookup_name_offset(names, count, name);
	if (offset > 0) {
		//name already exist, just write offset.
		((uint16_t*)pwrite)[0] = (htons(offset) | 0xc0);
		return pwrite + 2;
	} else {
		//name not exist in names table
		uint8_t *plen = pwrite;     //length write pointer
		uint8_t *pstr = pwrite + 1; //string copy pointer
		const char *pn = name;
		const char *dot = pn;

		while (*pn != 0) {
			while (*dot != '.' && *dot != 0) ++dot;
			//part dot end

			//write length
			*plen = dot - pn;
			//copy string part
			memcpy(pstr, pn, *plen);

			//move to next write point
			plen = pstr + *plen;
			pstr = plen + 1;

			//next part
			pn = dot + 1;
			dot = pn;
		}

		*plen = 0;     //zero length end
		plen += 1;

		insert_name_offset(names, count, name, (uint16_t) (pwrite - base));
		return plen;
	}
}

static uint8_t* write_question(uint8_t* pwrite, const char* name, uint16_t type, uint8_t* base, name_offset_t* names, uint16_t count) {
	uint16_t* pwrite16 = (uint16_t*)write_qname(pwrite, name, base, names, count);
	pwrite16[0] = htons(type);  //type, for IPv4, should 0x1
	pwrite16[1] = htons(0x1);   //class, when name query, should be 0x1
	return (uint8_t*)pwrite16 + 4;
}

static uint8_t* write_address(uint8_t* pwrite, const char* addr) {
	evutil_inet_pton(AF_INET, addr, pwrite);
	return (uint8_t*)pwrite + 4;
}

static uint8_t* write_answer(uint8_t* pwrite, const char* name, uint16_t type, uint32_t ttl, const char* data, uint8_t* base, name_offset_t* offset, uint16_t count) {
	dns_answer_t* answer = (dns_answer_t*)write_qname(pwrite, name, base, offset, count);
	answer->type  = htons(type);  //type
	answer->class = htons(0x1);   //class, when name query, should be 0x1
	answer->ttl   = htonl(ttl);   //ttl
	uint8_t* pnext;
	if (type == 0x1) {  //host address
		pnext = write_address(answer->rdata, data);
	} else { //cname  type == 0x5
		pnext = write_qname(answer->rdata, data, base, offset, count);
	}
	answer->rdlength = htons((uint16_t)(pnext - answer->rdata));
	return pnext;
}

uint16_t doh_convert(void* json, size_t len, uint16_t id, dns_packet_t* packet) {
	uint16_t packet_len = 0;
	json_doc* doc = NULL;
	json_val *root = NULL;

	packet->id = id;       //query id
	packet->qr = 1;        //response
	packet->opcode = 0x0;  //standard query

	packet_len = (uint16_t)(packet->data - (uint8_t*)packet);
	if (json == NULL || len <= 10) {
		packet->rcode = 0x2; //server error
		goto https_to_dns_packet_end;
	}

	//TRACE("%t", json, len);
	// read json and get root
	doc = json_read(json, len, 0);
	if (doc == NULL) {
		packet->rcode = 0x2; //server error
		goto https_to_dns_packet_end;
	}

	root = json_doc_get_root(doc);
	packet->rcode = 0x0; //json_get_int(root, "Status", 0);
	packet->tc = json_bool(root, "TC");
	packet->rd = json_bool(root, "RD");
	packet->ra = 0x1; //json_bool(root, "RA");
	packet->aa = json_bool(root, "AD");

	json_val *question = json_obj_get(root, "Question");
	if (question == NULL) {
		ERROR("Question not exist: \n%s", json);
		packet->rcode = 0x2; //server error
		goto https_to_dns_packet_end;
	}

	json_val* question_item = NULL;
	if (json_is_arr(question) && json_arr_size(question) == 1) {
		question_item = json_arr_get(question, 0);
	} else if (json_is_obj(question)) {
		question_item = question;
	} else {
		ERROR("Question invalid, should object or array: \n%s", json);
		packet->rcode = 0x1; //format error
		goto https_to_dns_packet_end;
	}

	json_val *answer = json_obj_get(root, "Answer");
	if (answer != NULL && !json_is_arr(answer)) {
		ERROR("Answer invalid, should be array: \n%s", json);
		packet->rcode = 0x1; //format error
		goto https_to_dns_packet_end;
	}

	packet->qdcount = 1;
	if (answer != NULL) {
		packet->ancount = (uint16_t)json_arr_size(answer);
	} else {
		packet->ancount = 0;
	}

	uint16_t names_count = packet->qdcount + packet->ancount;
	name_offset_t* names = alloca(sizeof(name_offset_t) * names_count);
	if (names == NULL) {
		ERRNO("alloca name error");
		packet->rcode = 0x2;
		goto https_to_dns_packet_end;
	}
	memset(names, 0, sizeof(name_offset_t) * names_count);

	//convert question part
	const char* qname = json_str(question_item, "name");
	uint16_t qtype = json_uint(question_item, "type", 1);
	uint8_t* pwrite = write_question(packet->data, qname, qtype, (uint8_t*)packet, names, names_count);

	//convert answer part
	if (answer) {
		json_val *item = NULL;
		for (uint16_t idx=0; idx < packet->ancount; idx++) {
			item = json_arr_get(answer, idx);
			if (item && json_is_obj(item)) {
				const char* name = json_str(item, "name");
				uint16_t type = json_uint(item, "type", 1);
				uint32_t ttl = json_uint(item, "TTL", 0);
				const char* data = json_str(item, "data");
				if (name != NULL && (type == 0x1 || type == 0x5) && data != NULL) {
					pwrite = write_answer(pwrite, name, type, ttl, data, (uint8_t*)packet, names, names_count);
				}
			}
		}
	}

	packet_len = (uint16_t)(pwrite - (uint8_t*)packet);
https_to_dns_packet_end:
	if (doc != NULL) {
		json_doc_free(doc);
	}
	return packet_len;
}

// check http status line and return status code
static int parse_status(char *line) {
	if (line[0] == 'H' && line[1] == 'T' && line[2] == 'T' && line[3] == 'P' &&
	 line[4] == '/' && line[5] == '1' && line[6] == '.' && line[7] == '1') {
		return (int)strtoul(line + 9, NULL, 10);
	} else {
		//fixme: invalid http request
		return 400;
	}
}

static int get_header_int(char *header, char* name, long* value) {
	size_t len = strlen(name);
	char* begin = strcasestr(header, name);
	if (begin == NULL) {
		return 0;
	}
	begin += len;

	if (*begin == ':') {
		++begin;
	} else {
		return 0;
	}

	if (*begin == ' ') {
		++begin;
	}

	char* end = strchr(begin, '\r');
	if (end == NULL) {
		return 0;
	}

	*value = strtol(begin, NULL, 10);
	return 1;
}

static int get_header_string(char *header, char* name, char** value) {
	size_t len = strlen(name);
	char* begin = strcasestr(header, name);
	if (begin == NULL) {
		return 0;
	}
	begin += len;

	if (*begin == ':') {
		++begin;
	} else {
		return 0;
	}

	if (*begin == ' ') {
		++begin;
	}

	char* end = strchr(begin, '\n');
	if (end == NULL) {
		return 0;
	}

	*value = begin;
	return 1;
}

// for dns over http
static int parse_header(char *data, size_t len, int* content_length, char** body) {
	char* header_end = strstr(data, "\r\n\r\n");
	if (header_end == NULL) {
		//header not ready
		return 0;
	}

	//make header null ternimate.
	header_end[2] = 0; header_end[3] = 0;
	char* body_start = header_end + 4;

	char* transfer_encoding = NULL;
	get_header_string(data, "Transfer-Encoding", &transfer_encoding);
	if (transfer_encoding != NULL && strncasecmp(transfer_encoding, "chunked", 7) == 0) {
		//chunked
		*content_length = 0;
		*body = body_start;
		return 1;
	} else {
		long cl;
		int rc = get_header_int(data, "Content-Length", &cl);
		if (rc == 0) {
			return 0;
		}

		if (cl <= 0 ) {
			return -1;
		}

		*content_length = (int)cl;
		*body = body_start;
		if ((data + len - body_start) >= cl) {
			//body total ready
			return 1;
		} else {
			return 0;
		}
	}
}

static void http_release_connection(http_t *http) {
	if (http->bev) {
		bufferevent_free(http->bev);
	}

	free(http);
}

/*
Chunked消息体格式如下：
hex的分块长度+<CR>回车+<LF>换行
chunked data
结束块的分块长度为0

如要发送的内容(消息体)为：1234567890123456那么消息体的格式为：
10<CR><LF>
123456789<CR><LF>
0<CR><LF>
*/
//FIXME: 8.8.8.8 didn't pass 0<CR><LF> as end
/*
 * decode chunk body
 * data: input, data pointer
 * len: input, data length
 * end:  output, point to data already decoded of data;
 * data_len: output, data length of data already decoded of data;
 * return  0 when data is not ready
 *        -1 when error
 *        1  decode succeed
 * */
static int parse_chunk_body(char* data, size_t len, char** end, size_t* data_len) {
	if (len <= 5) {
		//too small
		return 0;
	}

	char* data_end = data + len;

	//decode chunk data
	size_t copy_data_len = 0;
	char* copy_data_start = data;
	char* chunk_block_start = data;
	char* chunk_block_end = NULL;
	char* chunk_hex_start;
	char* chunk_hex_end;
	char* chunk_data_start;
	char* chunk_data_end;
	unsigned long chunk_len;
	while (chunk_block_start < data_end) {
		chunk_hex_start = chunk_block_start;
		chunk_hex_end = strstr(chunk_hex_start, "\n");
		if (chunk_hex_end != 0) {
			*chunk_hex_end = 0;
			chunk_data_start = chunk_hex_end + 1;
		} else {
			//error packet
			return -1;
		}
		chunk_len = strtoul(chunk_hex_start, 0, 16);
		chunk_data_end = chunk_data_start + chunk_len;
		chunk_block_end = chunk_data_end;

		//skip <CR>
		if (chunk_block_end < data_end && *chunk_block_end == '\r') {
			*chunk_block_end = 0;
			++chunk_block_end;
		}

		//skip <LF>
		if (chunk_block_end < data_end && *chunk_block_end == '\n') {
			*chunk_block_end = 0;
			++chunk_block_end;
		}

		//not ready
		if (chunk_block_end <= data_end) {
			if (chunk_len > 0) {
				//block ready
				memcpy(copy_data_start, chunk_data_start, chunk_len);
				copy_data_start += chunk_len;
				*copy_data_start = 0;
				copy_data_len += chunk_len;
			}
			chunk_block_start = chunk_block_end + 1;
		} else {
			break;
		}
	}

	if (chunk_block_end != NULL) {
		*end = chunk_block_end;
		*data_len = copy_data_len;
		return 1;
	}

	return 0;
}

static void do_callback(http_t* http, void* data, size_t len) {
	if (http->state != HTTP_CALLBACK_DONE) {
		http->callback(data, len, http->ctx, http->arg);
		http->state = HTTP_CALLBACK_DONE;
	}
}

static void http_read_cb(struct bufferevent *buffev, void* _arg) {
	(void)buffev;
	http_t *http = _arg;
	struct bufferevent* bevent = http->bev;
	char data[2048];
	ssize_t rlen = evbuffer_copyout(bevent->input, data, sizeof(data)-1);
	if (rlen <= 13) {
		return;
	}

	//make it null terminate;
	data[rlen] = 0;

	if (http->state == HTTP_RECV_BODY) {
		char* end = NULL;
		size_t body_len = 0;
		int rc = parse_chunk_body(data, rlen, &end, &body_len);
		if (rc >= 1) {
			do_callback(http, data, body_len);

			//clear data and release connection
			evbuffer_drain(bevent->input, end - data);
		} else if (rc < 0) {
			//callback with error
			do_callback(http, NULL, 0);

			//clear data and release connection
			evbuffer_drain(bevent->input, rlen);

			//release connection
			http_release_connection(http);
		}
	} else {
		//status, check if http response
		int status = parse_status(data);
		if (status != 200) {
			ERROR("http=0x%x %s:%d%s status: %d", http, http->host, (int)http->port, http->uri, status);

			//callback with error
			do_callback(http, NULL, 0);

			//clear data and release connection
			evbuffer_drain(bevent->input, rlen);

			//release connection
			http_release_connection(http);
			return;
		}

		int content_length = 0;
		char* body = NULL;
		TRACE("packet:\n%t", data, rlen);
		int rcode = parse_header(data, rlen, &content_length, &body);
		if (rcode >= 1) {
			//body is ready
			if (content_length > 0) {
				//callback with body
				do_callback(http, body, content_length);
				//remove the header and body from buffer
				evbuffer_drain(bevent->input, body + content_length - data);
			} else {
				//chunked encoding
				//just remove the http header from buffer
				evbuffer_drain(bevent->input, body - data);

				//start decode chunked body
				http->state = HTTP_RECV_BODY;
				{
					char* end = NULL;
					size_t body_len = 0;
					int rc = parse_chunk_body(body, data + rlen - body, &end, &body_len);
					if (rc >= 1) {
						//callback with decode body.
						//FIXME: 8.8.8.8 repsonse not standard chunked encoding
						do_callback(http, body, body_len);
						//clear data and release connection
						evbuffer_drain(bevent->input, end - body);
					} else if (rc < 0) {
						//callback with error
						do_callback(http, NULL, 0);

						//clear data and release connection
						evbuffer_drain(bevent->input, rlen);

						//release connection
						http_release_connection(http);
					} else {
						//wait for more data
					}
				}
			}
		} else if (rcode < 0) {
			ERROR("invalid http response:\n%t", data, rlen);

			//callback with error
			do_callback(http, NULL, 0);

			//clear data
			evbuffer_drain(bevent->input, rlen);

			//release connection
			http_release_connection(http);
		} else {
			WARNING("http=0x%x %s:%d/%s waiting for response. bad network", http, http->host, (int)http->port, http->uri);
		}
	}
}

static void http_write_cb(struct bufferevent* buffev, void* _arg) {
	http_t *http = _arg;
	assert(buffev == http->bev);
	char req[1024] = {0};
	size_t len = snprintf(req, sizeof(req) - 1,
	                      "GET %s HTTP/1.1\r\n"
	                      "Host: %s\r\n"
	                      "Connection: close\r\n"
	                      "User-Agent: rdns\r\n"
	                      "\r\n",
	                      http->uri, http->host);
	int wlen = bufferevent_write(buffev, req, len);
	if (wlen < 0) {
		ERRNO("http=0x%x %s:%d%s send requests error", http, http->host, (int)http->port, http->uri);
		//callback with error
		do_callback(http, NULL, 0);

		//release connection
		http_release_connection(http);
	}
}

static void http_on_event(struct bufferevent *buffev, short what, void* _arg) {
	http_t *http = _arg;
	assert(buffev == http->bev);
	TRACE("http=0x%x event:0x%x", http, what);
	if (what & BEV_EVENT_CONNECTED) {
		TRACE("http=0x%x %s:%d/%s connected", http, http->host, (int)http->port, http->uri);
		http_write_cb(buffev, _arg);
	} else if (what & BEV_EVENT_ERROR) {
		if (http->schema == schema_https && (what & BEV_EVENT_READING)){
			//FIXME: when https, there is reading error at last
		} else {
			ERROR("http=0x%x %s:%d%s error:%s", http, http->host, (int)http->port, http->uri,  evutil_socket_error_to_string(EVUTIL_SOCKET_ERROR()));
			//callback with error
			do_callback(http, NULL, 0);
		}
		//release connection
		http_release_connection(http);
	} else if (what & BEV_EVENT_TIMEOUT) {
		ERROR("http=0x%x %s:%d%s timeout", http, http->host, (int)http->port, http->uri);
		//callback with error
		do_callback(http, NULL, 0);
		//release connection
		http_release_connection(http);
	} else if (what & BEV_EVENT_EOF) {
		TRACE("http=0x%x %s:%d%s close", http, http->host, (int)http->port, http->uri);
	}
}

static void http_start_request(struct event_base* base, http_t* http) {
	int error;
	if (http->schema == schema_http) {
		http->bev = bufferevent_socket_new(base, -1, BEV_OPT_CLOSE_ON_FREE);
		if (http->bev == NULL) {
			ERROR("bufferevent_socket_new");
			return;
		}
	} else {
		assert(g_ssl_ctx);
		http->ssl = SSL_new(g_ssl_ctx);
		http->bev = bufferevent_openssl_socket_new(base, -1, http->ssl, BUFFEREVENT_SSL_CONNECTING, BEV_OPT_CLOSE_ON_FREE|BEV_OPT_DEFER_CALLBACKS);
		if (http->bev == NULL) {
			ERRNO("bufferevent_openssl_socket_new");
			return;
		}
	}

	bufferevent_setcb(http->bev, http_read_cb, NULL, http_on_event, http);
	error = bufferevent_enable(http->bev, EV_WRITE|EV_READ|EV_PERSIST);
	if (error < 0) {
		ERROR("bufferevent_enable");
		return;
	}

	struct sockaddr_in addr;
	evutil_inet_pton(AF_INET, http->host, &addr.sin_addr.s_addr);
	addr.sin_family = AF_INET;
	addr.sin_port = htons(http->port);
	bufferevent_settimeout(http->bev, 3, 3);
	error = bufferevent_socket_connect(http->bev, (struct sockaddr*)&addr, sizeof(addr));
	if (error && errno != EINPROGRESS) {
		ERROR("connect");
	}
}

void doh_request(struct event_base* base, const char* url, void* ctx, void* arg, http_callback_t cb)
{
	http_t*             http = NULL;
	struct evhttp_uri   *http_uri = NULL;
	const char          *scheme, *host, *path, *query;
	int                 port;

	http_uri = evhttp_uri_parse(url);
	if (http_uri == NULL) {
		ERROR("invalid url: %s", url);
		return;
	}

	scheme = evhttp_uri_get_scheme(http_uri);
	if (scheme == NULL) {
		ERROR("url %s schema must be http", url);
		evhttp_uri_free(http_uri);
		return;
	}

	if (strcasecmp(scheme, "https") != 0 && strcasecmp(scheme, "http") != 0) {
		ERROR("url %s schema must be http/https", url);
		evhttp_uri_free(http_uri);
		return;
	}

	host = evhttp_uri_get_host(http_uri);
	if (host == NULL) {
		ERROR("url %s must have a host", url);
		evhttp_uri_free(http_uri);
		return;
	}

	http = malloc(sizeof(http_t));
	if (!http) {
		ERRNO("malloc http return null");
		evhttp_uri_free(http_uri);
		return;
	}

	memset(http, 0, sizeof(*http));
	http->schema = (strcasecmp(scheme, "https") == 0) ? schema_https : schema_http;
	http->state = HTTP_STATE_INIT;
	http->ctx = ctx;
	http->arg = arg;
	http->callback = cb;
	strncpy(http->host, host, sizeof(http->host) - 1);
	port = evhttp_uri_get_port(http_uri);
	if (port == -1) {
		http->port = (http->schema == schema_https) ? 443 : 80;
	} else {
		http->port = port;
	}

	path = evhttp_uri_get_path(http_uri);
	if (strlen(path) == 0) {
		path = "/";
	}
	query = evhttp_uri_get_query(http_uri);
	if (query == NULL) {
		snprintf(http->uri, sizeof(http->uri) - 1, "%s", path);
	} else {
		snprintf(http->uri, sizeof(http->uri) - 1, "%s?%s", path, query);
	}

	http_start_request(base, http);
	evhttp_uri_free(http_uri);
}