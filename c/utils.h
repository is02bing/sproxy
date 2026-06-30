#pragma once
#include <stddef.h>
#include <time.h>
#include <event.h>
#include <stdint.h>
#include <sys/time.h>

//i like bool
typedef char bool;
#define true 1;
#define false 0;

struct sockaddr_in;

#define COUNTOF(A)     (sizeof(A)/sizeof(A[0]))
#define MIN(X, Y)      ((X) >= (Y) ? (Y) : (X))
#define MAX(X, Y)      ((X) > (Y) ? (X) : (Y))

#if defined __GNUC__
#define PACKED __attribute__((packed))
#else
#error Unknown compiler, modify utils.h for it
#endif

typedef struct {
	int tm_sec;    /* Seconds (0-59) */
	int tm_min;    /* Minutes (0-59) */
	int tm_hour;   /* Hours (0-23) */
	int tm_mday;   /* Day of the month (1-31) */
	int tm_mon;    /* Month (0-11) */
	int tm_year;   /* 1900~2039*/
	int tm_wday;   /* Day of the week (0-6, Sunday = 0) */
	unsigned int tv_sec;    /* seconds */
	unsigned int tv_usec;   /* microseconds */
} datetime_t;


__attribute__((unused)) int       set_tcp_keepalive(int fd);
__attribute__((unused)) int       get_dest_addr(int fd, struct sockaddr_in *dest_addr);
__attribute__((unused)) int       fcntl_nonblock(int fd);
__attribute__((unused)) void      getdatetime(datetime_t* dt);
__attribute__((unused)) uint32_t  unixtime();
__attribute__((unused)) uint64_t  unixtimelong();
__attribute__((unused)) int       parse_addr(const char* addr_str, struct sockaddr_in *addr);
