#pragma once
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <netinet/in.h>
#include <stddef.h>
#include <stdint.h>

/*
 * dot = DNS over TCP（RFC 1035 §4.2.2 TCP 用法）
 *
 *   - 每个 DNS 报文前加 2 字节大端长度前缀
 *   - 到同一上游的 TCP 连接持久复用，多个查询可并发，
 *     按 DNS ID 路由回包
 *   - 连接断开/超时后自动重连，未完成请求以失败回调
 */
#include "dns.h"

typedef void (*dot_callback_t)(void* data, size_t len, void* ctx, void* arg);

/*
 * 发起一次 DoT 查询。复用或新建到 server 的持久 TCP 连接。
 *   data/len : 待发送的 DNS 报文（网络字节序，不含长度前缀）
 *   id       : DNS 报文 ID，用于回包路由
 *   ctx/arg  : 透传给 cb
 *   cb       : 回包回调；data/len 为 0 表示失败（连接断开/超时），
 *              调用方应保留 query_map 条目以等待重试
 */
void dot_request(struct event_base* base, struct sockaddr_in* server,
                 const void* data, size_t len, uint16_t id,
                 void* ctx, void* arg, dot_callback_t cb);

/* 释放所有持久 TCP 连接及未完成的 pending 请求（以失败回调通知） */
void dot_term(void);
