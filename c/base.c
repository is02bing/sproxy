#include "base.h"
#include "trace.h"

SSL_CTX *g_ssl_ctx = NULL;
int32_t g_fast_mode = 0;

static void init_openssl() {
#if (OPENSSL_VERSION_NUMBER < 0x10100000L) || (defined(LIBRESSL_VERSION_NUMBER) && LIBRESSL_VERSION_NUMBER < 0x20700000L)
	// Initialize OpenSSL
	SSL_library_init();
	ERR_load_crypto_strings();
	SSL_load_error_strings();
	OpenSSL_add_all_algorithms();
#endif
	g_ssl_ctx = SSL_CTX_new(SSLv23_client_method());
}

void base_term() {
	if (g_ssl_ctx) {
		SSL_CTX_free(g_ssl_ctx);
	}
	g_ssl_ctx = NULL;
}


void base_init(int log_level, int fast_mode) {
	trace_set_debug_level(log_level);
	if (log_level > TRACE_LEVEL_INFO) {
		trace_set_line_mode(1);
	} else {
		trace_set_line_mode(0);
	}
    const char *level_str;
    switch (log_level) {
        case 9:  level_str = "trace";  break;
        case 8:  level_str = "debug";  break;
        case 4:  level_str = "info";   break;
        case 3:  level_str = "warn";   break;
        case 2:  level_str = "error";  break;
        case 1:  level_str = "fatal";  break;
        default: level_str = "unknown"; break;
    }
    INFO("log level: %s", level_str);
    INFO("log mode: %s", log_level > TRACE_LEVEL_INFO ? "multiline" : "singleline");

	g_fast_mode = fast_mode;
	init_openssl();
}