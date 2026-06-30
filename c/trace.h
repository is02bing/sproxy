#pragma once
#include <stdint.h>
#include <stdio.h>
#include <locale.h>
#include <wchar.h>
#include "filemacro.h"

//TRACE等级设置
#define TRACE_LEVEL_VERBOSE  0x9
#define TRACE_LEVEL_DEBUG    0x8
#define TRACE_LEVEL_INFO     0x4
#define TRACE_LEVEL_WARNING  0x3
#define TRACE_LEVEL_ERROR    0x2
#define TRACE_LEVEL_CRITICAL 0x1

//设置日志过滤等级.
void trace_set_debug_level(int level);

//日志长短模式
void trace_set_line_mode(int mode);

//通用日志写函数, 请用宏, 这里函数不要直接用
 void trace_write(int eno, int level, const char* file, int line, const char* func, const char* format, ...);

#define _trace_write(level, format, args...) trace_write(0, level, __FILENAME__, __LINE__, __FUNCTION__, format, ##args)
#define _trace_write_with_errno(level, format, args...) trace_write(errno, level, __FILENAME__, __LINE__, __FUNCTION__, format, ##args)

#define VERBOSE(format, args...)  _trace_write(TRACE_LEVEL_VERBOSE, format, ##args)

#define TRACE(format, args...)  _trace_write(TRACE_LEVEL_DEBUG, format, ##args)

#define INFO(format, args...)   _trace_write(TRACE_LEVEL_INFO, format, ##args)

#define WARNING(format, args...)  _trace_write(TRACE_LEVEL_WARNING,format, ##args)

#define ERROR(format, args...)    _trace_write(TRACE_LEVEL_ERROR, format, ##args)

#define ERRNO(format, args...)    _trace_write_with_errno(TRACE_LEVEL_ERROR, format, ##args)

#define CRITICAL(format, args...)  _trace_write(TRACE_LEVEL_CRITICAL,format, ##args)
