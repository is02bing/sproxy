#include "trace.h"
#include "utils.h"
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>

//日志Printf Buffer长度
#define PRINT_BUF_LEN 5120

//日志配置
static int     dbg_log_level = 9;
static int     dbg_single_line = 0;

void trace_set_debug_level(int level)
{
	dbg_log_level = level;
}

void trace_set_line_mode(int mode)
{
	dbg_single_line = mode;
}

typedef struct  {
	size_t   buffer_len;
	size_t   buffer_cap;
	char     buffer[PRINT_BUF_LEN];
} line_buffer;

void init_buffer(line_buffer* lb) {
	lb->buffer_len = 0;
	//reserved 6 byte for ending
	lb->buffer_cap = sizeof(lb->buffer)-4;
}

void done_buffer(line_buffer* lb) {
	if (lb->buffer_len >= lb->buffer_cap) {
		lb->buffer[lb->buffer_len] = ' ';
		lb->buffer[lb->buffer_len+1] = '.';
		lb->buffer[lb->buffer_len+2] = '.';
		lb->buffer[lb->buffer_len+3] = '.';
		lb->buffer_len += 4;
	} else {
		if (lb->buffer[lb->buffer_len - 1] != '\n') {
			lb->buffer[lb->buffer_len] = '\n';
			lb->buffer[lb->buffer_len + 1] = 0;
			lb->buffer_len += 1;
		}
	}
}

void write_line_buffer(line_buffer* lb, const char* append, size_t len)
{
	size_t rest_len = lb->buffer_cap - lb->buffer_len;
	if (rest_len == 0) {
		return;
	}

	char* pos = lb->buffer + lb->buffer_len;
	if (rest_len >= len) {
		memcpy(pos, append, len);
		lb->buffer_len += len;
	} else {
		memcpy(pos, append, rest_len);
		lb->buffer_len += rest_len;
	}
}

void trace_dump(const void* data, int len, void* offset, int byte_per_line, line_buffer* lb);

//通用日志写函数, ANSI
void trace_write(int errno_current, int level, const char* file, int line, const char* func, const char* format, ...)
{
	if (level > dbg_log_level) return;

    datetime_t datetime;
	getdatetime(&datetime);

	line_buffer log_data;
	init_buffer(&log_data);

   	char     buffer[1024];
	size_t   length = 0;
    char     flag;

	switch(level)
	{
        case TRACE_LEVEL_VERBOSE:
            flag = 'V';
            break;
        case TRACE_LEVEL_DEBUG:
            flag = 'D';
            break;
        case TRACE_LEVEL_INFO:
            flag = 'I';
            break;

        case TRACE_LEVEL_WARNING:
            flag = 'W';
            break;

        case TRACE_LEVEL_ERROR:
            flag = 'E';
            break;

        case TRACE_LEVEL_CRITICAL:
            flag = 'C';
            break;

        default:
            flag = 'D';
	}


    if (dbg_single_line <= 0) {
	    length = snprintf(buffer, sizeof(buffer), "[%c][%02u-%02u %02u:%02u:%02u.%06u] ",
                 flag, datetime.tm_mon, datetime.tm_mday,
                 datetime.tm_hour, datetime.tm_min, datetime.tm_sec, datetime.tv_usec);
    } else {
	    length = snprintf(buffer, sizeof(buffer), "\n[%c] %02u-%02u %02u:%02u:%02u.%06u %s:%d %s():\n",
                       flag, datetime.tm_mon, datetime.tm_mday,
                       datetime.tm_hour, datetime.tm_min, datetime.tm_sec, datetime.tv_usec,
                       file, line, func);
    }
	write_line_buffer(&log_data, buffer, length);

	const char* fs = format;
	const char* fp;
	va_list va;
	va_start(va, format);
	do {
		fp = fs;
		while (*fp != 0 && *fp != '%') ++fp;

		//copy [fs - fp) to buffer
		if (fp > fs) {
			write_line_buffer(&log_data, fs, fp-fs);
		}

		if (*fp == '%') {
			if (fp[1] == '%')
			{
				write_line_buffer(&log_data, "%", 1);
				fs = fp + 2;
			}
			else if (fp[1] == 'a')
			{
				length = sprintf(buffer, "%a", va_arg(va, double));
				write_line_buffer(&log_data, buffer, length);
				fs = fp + 2;
			}
			else if (fp[1] == 'A')
			{
				length = sprintf(buffer, "%A", va_arg(va, double));
				write_line_buffer(&log_data, buffer, length);
				fs = fp + 2;
			}
			else if (fp[1] == 'c')
			{
				buffer[0] = (char)va_arg(va, int);
				write_line_buffer(&log_data, buffer, 1);
				fs = fp + 2;
			}
			else if (fp[1] == 'd')
			{
				length = sprintf(buffer, "%d", va_arg(va, int));
				write_line_buffer(&log_data, buffer, length);
				fs = fp + 2;
			}
			else if (fp[1] == 'e')
			{
				length = sprintf(buffer, "%e", va_arg(va, double));
				write_line_buffer(&log_data, buffer, length);
				fs = fp + 2;
			}
			else if (fp[1] == 'E')
			{
				length = sprintf(buffer, "%E", va_arg(va, double));
				write_line_buffer(&log_data, buffer, length);
				fs = fp + 2;
			}
			else if (fp[1] == 'f')
			{
				length = sprintf(buffer, "%f", va_arg(va, double));
				write_line_buffer(&log_data, buffer, length);
				fs = fp + 2;
			}
			else if (fp[1] == 'g')
			{
				length = sprintf(buffer, "%g", va_arg(va, double));
				write_line_buffer(&log_data, buffer, length);
				fs = fp + 2;
			}
			else if (fp[1] == 'G')
			{
				length = sprintf(buffer, "%G", va_arg(va, double));
				write_line_buffer(&log_data, buffer, length);
				fs = fp + 2;
			}
			else if (fp[1] == 'i')
			{
				length = sprintf(buffer, "%i", va_arg(va, int));
				write_line_buffer(&log_data, buffer, length);
				fs = fp + 2;
			}
			else if (fp[1] == 'l')
			{
				length = sprintf(buffer, "%ld", va_arg(va, long));
				write_line_buffer(&log_data, buffer, length);
				fs = fp + 2;
			}
			else if (fp[1] == 'o')
			{
				length = sprintf(buffer, "%o", (int)va_arg(va, int));
				write_line_buffer(&log_data, buffer, length);
				fs = fp + 2;
			}
			else if (fp[1] == 'p')
			{
				length = sprintf(buffer, "%p", (void*)va_arg(va, void*));
				write_line_buffer(&log_data, buffer, length);
				fs = fp + 2;
			}
			else if (fp[1] == 's')
			{
				const char* cpy = (const char*)(va_arg(va, char*));
				size_t cpy_len = strlen(cpy);
				write_line_buffer(&log_data, cpy, cpy_len);
				fs = fp + 2;
			}
			else if (fp[1] == 'u') {
				length = sprintf(buffer, "%u", (unsigned int)va_arg(va, unsigned int));
				write_line_buffer(&log_data, buffer, length);
				fs = fp + 2;
			}
			else if (fp[1] == 'x')
			{
				length = sprintf(buffer, "%x", (int)va_arg(va, int));
				write_line_buffer(&log_data, buffer, length);
				fs = fp + 2;
			}
			else if (fp[1] == 'X')
			{
				length = sprintf(buffer, "%X", (int)va_arg(va, int));
				write_line_buffer(&log_data, buffer, length);
				fs = fp + 2;
			}
			else if (fp[1] == 'n')
			{
				struct sockaddr_in* sa = va_arg(va, struct sockaddr_in*);
				uint16_t port = 0;
				const char* result = NULL;

				if (sa->sin_family == AF_INET) {
					result = inet_ntop(AF_INET, &sa->sin_addr, buffer, sizeof(buffer));
					port = ntohs(((struct sockaddr_in*)sa)->sin_port);
				} else if (sa->sin_family == AF_INET6) {
					result = inet_ntop(AF_INET6, &((const struct sockaddr_in6*)sa)->sin6_addr, buffer, sizeof(buffer));
					port = ntohs(((struct sockaddr_in6*)sa)->sin6_port);
				} else {
				}

				if (result != NULL) {
					size_t length1 = strnlen(buffer, sizeof(buffer));
					length = sprintf(buffer+length1, ":%d", port);
					write_line_buffer(&log_data, buffer, length1 + length);
				} else {
					length = sprintf(buffer, "-:%d", port);
					write_line_buffer(&log_data, buffer, length);
				}
				fs = fp + 2;
			}
			else if (fp[1] == 't')
			{
				//dump next data, with size_t
				void* data = (void*)va_arg(va, void*);
				size_t len = (unsigned int)va_arg(va, size_t);
				trace_dump(data, len, 0, 16, &log_data);
				fs = fp + 2;
			}
			else
			{
				buffer[length] = '%';
				length += 1;
				fs = fp + 2;
			}
		}
	} while(*fp != 0);
	va_end(va);

	if (errno_current != 0) {
		char* error_current = strerror(errno_current);
		length = snprintf(buffer, sizeof(buffer), " errno: %d, error:%s ", errno_current, error_current);
		write_line_buffer(&log_data, buffer, length);
	}

	done_buffer(&log_data);
	if (level <= TRACE_LEVEL_ERROR) {
		fwrite(log_data.buffer, log_data.buffer_len, 1, stdout);
		fflush(stdout);
	} else {
		fwrite(log_data.buffer, log_data.buffer_len, 1, stdout);
		fflush(stdout);
	}
}

static int isprintable(char c)
{
    return (c >= 0x20 && c <= 0x7e);
}

static char strstrstrdata[17]="0123456789ABCDEF";
static char int2char(int index)
{
    return strstrstrdata[(index & 0x0F)];
}

void trace_dump(const void* data, int len, void* offset, int byte_per_line, line_buffer* lb)
{
	char* buffer;
	char* wptr;
	char* dptr = (char*)data;
	int   count = 0;
	int   index;
	char  str[258];
	int   slen;

	byte_per_line = (byte_per_line <= 256) ? byte_per_line : 256;
	buffer = (char*)alloca(byte_per_line * 4 + 32);
	//assert(wptr);

	while (count < len)
	{
		wptr = buffer;

		//print offset address
		if (sizeof(void*) == 4)
		{
			sprintf(wptr, "%08x: ", ((unsigned int)((unsigned long long)offset) + count));
			wptr += 10;
		}
		else
		{
			unsigned long long real_offset = (unsigned long long)offset + count;
			sprintf(wptr, "%08x%08x: ", (unsigned int)(real_offset >> 32), (unsigned int)(real_offset & 0xFFFFFFFF));
			wptr += 18;
		}

		//print hex data
		index = 0;
		while ((index < byte_per_line) && (count < len))
		{
			wptr[0] = int2char((dptr[count] & 0xF0) >> 4);
			wptr[1] = int2char(dptr[count] & 0x0F);
			wptr[2] = ' ';
			wptr += 3;
			str[index] = isprintable(dptr[count]) ? dptr[count] : '.';
			++count; ++index;
		}
		//terminate for string part
		str[index] = '\n';
		slen = index + 1;


		//the last line, fill hex part
		while (index < byte_per_line) {
			wptr[0] = ' '; wptr[1] = ' ';  wptr[2] = ' ';
			wptr += 3;
			++index;
		}

		write_line_buffer(lb, buffer, wptr-buffer);
		write_line_buffer(lb, str, slen);
	}
}