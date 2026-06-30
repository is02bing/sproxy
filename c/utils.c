#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <linux/netfilter_ipv4.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include "utils.h"
#include "trace.h"

int fcntl_nonblock(int fd)
{
	int error;
	int flags;

	flags = fcntl(fd, F_GETFL);
	if (flags == -1) {
		return -1;
	}

	if ((flags & O_NONBLOCK) == 0) {
		error = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
		if (error < 0) {
			return -1;
		} else {
			return 0;
		}
	} else {
		return 0;
	}
}


static int tz_offset_valid = 0;
static unsigned int tz_offset = 0;

void init_timezone()
{
	tz_offset_valid = 0;
	time_t    tp = 0;
	struct tm tm;
	localtime_r(&tp, &tm);
	tz_offset = (unsigned)tm.tm_gmtoff;
}

void getdatetime(datetime_t* dt)
{
	if (!tz_offset_valid) {
		init_timezone();
	}

	if (dt)
	{
		struct timeval tt;
		struct tm      mm;
		gettimeofday(&tt, NULL);
		tt.tv_sec += tz_offset;
		gmtime_r(&tt.tv_sec, &mm);
		dt->tm_sec = mm.tm_sec;
		dt->tm_min = mm.tm_min;
		dt->tm_hour = mm.tm_hour;
		dt->tm_mday = mm.tm_mday;
		dt->tm_mon = mm.tm_mon + 1;
		dt->tm_year = mm.tm_year + 1900;
		dt->tm_wday = mm.tm_wday;
		dt->tv_sec = (unsigned  int)tt.tv_sec;
		dt->tv_usec = (unsigned int)tt.tv_usec;
	}
}

__attribute__((unused)) uint32_t unixtime()
{
	struct timespec spec;
	clock_gettime(CLOCK_REALTIME, &spec);
	return (uint32_t)spec.tv_sec;
}

__attribute__((unused)) uint64_t unixtimelong()
{
	struct timespec spec;
	clock_gettime(CLOCK_REALTIME, &spec);
	return (uint64_t)spec.tv_sec*1000 + spec.tv_nsec/1000000;
}

int get_dest_addr(int fd, struct sockaddr_in *dest_addr)
{
	socklen_t socklen = sizeof(*dest_addr);
	int error;

	error = getsockopt(fd, SOL_IP, SO_ORIGINAL_DST, dest_addr, &socklen);
	if (error) {
		return -1;
	}
	return 0;
}

static bool    g_tcp_keepalive = true;
static uint16_t g_tcp_keepalive_time = 300;
static uint16_t g_tcp_keepalive_probes = 3;
static uint16_t g_tcp_keepalive_intvl = 300;

int set_tcp_keepalive(int fd)
{
	if (g_tcp_keepalive) {
		int value;
		int error;

		value = 1;
		error = setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &value, sizeof(value));
		if (error) {
			ERRNO("setsockopt so_keepalive %d", fd, value);
			return -1;
		}

		value = g_tcp_keepalive_time;
		error = setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &value, sizeof(value));
		if (error) {
			ERRNO("setsockopt tcp_keepidle %d", fd, value);
			return -1;
		}

		value = g_tcp_keepalive_probes;
		error = setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &value, sizeof(value));
		if (error) {
			ERRNO("setsockopt tcp_keepcnt %d", fd, value);
			return -1;
		}

		value = g_tcp_keepalive_intvl;
		error = setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &value, sizeof(value));
		if (error) {
			ERRNO("setsockopt tcp_keepintvl %d", fd, value);
			return -1;
		}
	}

	return 0;
}

int parse_addr(const char* input, struct sockaddr_in *addr)
{
    char ip[16] = {0};
    struct in_addr addr_v4;
    unsigned int port;
    if (input == NULL) {
        return 0;
    }

    const char *s = strstr(input, ":");
    if (s != NULL) {
        int len = s - input;
        if (len >= 15) {
            return 0;
        }
        if (len > 0) {
            strncpy(ip, input, len);
            if (inet_pton(AF_INET, ip, &addr_v4) != 1) {
                return 0;
            }
        } else {
            addr_v4.s_addr = INADDR_ANY;
        }

        port = atoi(s+1);
        if (port <= 0 || port > 65535) {
            return 0;
        }
    } else {
        port = atoi(input);
        if (port <= 0 || port > 65535) {
            return 0;
        }
    }

    if (addr != NULL) {
        addr->sin_family = AF_INET;
        addr->sin_port = htons(port);
        addr->sin_addr = addr_v4;
    }
    return 1;
}