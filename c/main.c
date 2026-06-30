#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <event.h>
#include <libconfig.h>
#include "base.h"
#include "tcp.h"
#include "dns.h"
#include "trace.h"

static void signal_handler(int sig, short what, void *arg) {
	if (event_loopbreak() != 0) {
		ERROR("event_loop break");
	}
}

static void parse_config(config_t *cfg, int argc, char **argv) {
	char config_file[256] = {0};
	if (argc >= 2 && argv[1] != NULL) {
		strncpy(config_file, argv[1], 255);
	} else {
		if (access("proxy.cfg", F_OK) == 0) {
			strcpy(config_file, "proxy.cfg");
		} else {
			strcpy(config_file, "/etc/proxy/proxy.cfg");
		}
	}

	fprintf(stderr, "use config file: %s\n", config_file);
	if (!config_read_file(cfg, config_file)) {
		fprintf(stderr, "%s:%d - %s\n", config_error_file(cfg), config_error_line(cfg), config_error_text(cfg));
		config_destroy(cfg);
		exit(EXIT_FAILURE);
	}
}

int main(int argc, char **argv) {
	static struct event sigterm;
	static struct event sigint;
	config_t cfg;
	proxy_tcp_t *tcp = NULL;
	dns_t *dns = NULL;
	struct event_base *evb;
	int error;

	evutil_secure_rng_init();
	if (event_get_struct_event_size() != sizeof(struct event)) {
		fprintf(stderr, "libevent event_get_struct_event_size() != sizeof(struct event)! check and recompile");
		return EXIT_FAILURE;
	}

	//parse config
	config_init(&cfg);
	parse_config(&cfg, argc, argv);
	base_init(&cfg);

	memset(&sigint, 0, sizeof(sigint));
	memset(&sigterm, 0, sizeof(sigterm));

	evb = event_init();

	signal_set(&sigterm, SIGTERM, signal_handler, NULL);
	if (signal_add(&sigterm, NULL) != 0) {
		ERRNO("signal_add");
		return EXIT_FAILURE;
	}

	signal_set(&sigint, SIGINT, signal_handler, NULL);
	if (signal_add(&sigint, NULL) != 0) {
		ERRNO("signal_add");
		return EXIT_FAILURE;
	}

	config_setting_t *setting = config_lookup(&cfg, "tcp");
	if (setting != NULL) {
		tcp = alloca(sizeof(proxy_tcp_t));
		if (tcp == NULL) {
			ERROR("tcp alloc failed");
			goto shutdown;
		}

		memset(tcp, 0, sizeof(proxy_tcp_t));
		tcp->base = evb;
		error = proxy_tcp_init(tcp, setting);
		if (error) {
			ERROR("tcp init failed");
			goto shutdown;
		}
	}

	setting = config_lookup(&cfg, "dns");
	if (setting != NULL) {
		dns = alloca(sizeof(dns_t));
		if (dns == NULL) {
			ERROR("dns alloc failed");
			goto shutdown;
		}

		memset(dns, 0, sizeof(dns_t));
		dns->base = evb;
		error = dns_init(dns, setting);
		if (error) {
			ERROR("dns init failed");
			goto shutdown;
		}
	}
	config_destroy(&cfg);

	event_dispatch();

shutdown:
	if (dns != NULL) {
		dns_term(dns);
	}

	if (tcp != NULL) {
		proxy_tcp_term(tcp);
	}

	base_term();

	if (evb != NULL) {
		event_base_free(evb);
	}

	if (signal_initialized(&sigint)) {
		signal_del(&sigint);
	}

	if (signal_initialized(&sigterm)) {
		signal_del(&sigterm);
	}
}