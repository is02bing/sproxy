#pragma once
#include <openssl/ssl.h>
void base_init(int log_level, int fast_mode);
void base_term();
extern SSL_CTX* g_ssl_ctx;
extern int32_t g_fast_mode;