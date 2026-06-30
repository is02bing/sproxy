#pragma once

#include <stdint.h>
#include "utils.h"

/* ── SOCKS5 protocol structures ── */

typedef struct {
	uint8_t version;
	uint8_t method;
} PACKED socks5_method_reply_t;

typedef struct {
	uint8_t version;
	uint8_t status;
} PACKED socks5_auth_reply_t;

typedef struct {
	uint32_t addr;
	uint16_t port;
} PACKED socks5_addr_ipv4_t;

typedef struct {
	uint8_t version;
	uint8_t command;
	uint8_t reserved;
	uint8_t addrtype;
} PACKED socks5_req_t;

typedef struct {
	uint8_t ver;
	uint8_t rep;
	uint8_t rsv;
	uint8_t atyp;
} PACKED socks5_reply_t;

/* ── SOCKS5 protocol constants ── */

static const uint8_t socks5_version           = 0x05;
static const uint8_t socks5_auth_method_none   = 0x00;
static const uint8_t socks5_auth_method_pass   = 0x02;
static const uint8_t socks5_auth_method_invalid = 0xff;

static const uint8_t socks5_password_version  = 0x01;
static const uint8_t socks5_password_passed   = 0x00;

static const uint8_t socks5_addrtype_ipv4     = 0x01;
static const uint8_t socks5_addrtype_domain   = 0x03;
static const uint8_t socks5_addrtype_ipv6     = 0x04;

/* RFC1928 Ch6 reply codes */
static const uint8_t socks5_reply_succeeded                    = 0x00;
static const uint8_t socks5_reply_server_failure                = 0x01;
static const uint8_t socks5_reply_connection_not_allowed       = 0x02;
static const uint8_t socks5_reply_network_unreachable          = 0x03;
static const uint8_t socks5_reply_host_unreachable             = 0x04;
static const uint8_t socks5_reply_connection_refused           = 0x05;
static const uint8_t socks5_reply_ttl_expired                  = 0x06;
static const uint8_t socks5_reply_command_not_supported        = 0x07;
static const uint8_t socks5_reply_address_type_not_supported   = 0x08;

static const uint8_t socks5_command_connect = 1;

/* ── SOCKS5 connection state ── */

typedef enum {
	socks5_state_init = 0,
	socks5_state_method,    /* reading method selection */
	socks5_state_auth,      /* reading username/password */
	socks5_state_request,   /* reading connect request */
	socks5_state_relay,     /* connected, relaying data */
	socks5_state_error
} socks5_state_t;
