/*==============================================================================
 Copyright (c) 2020 YaoYuan <ibireme@gmail.com>
 
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.
 *============================================================================*/

/** 
 @file json.h
 @date 2019-03-09
 @author YaoYuan
 */

#ifndef JSON_H
#define JSON_H



/*==============================================================================
 * Header Files
 *============================================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <limits.h>
#include <string.h>
#include <float.h>



/*==============================================================================
 * Compile-time Options
 *============================================================================*/

/*
 Define as 1 to disable JSON reader if JSON parsing is not required.
 
 This will disable these functions at compile-time:
    - json_read()
    - json_read_opts()
    - json_read_file()
    - json_read_number()
    - json_mut_read_number()
 
 This will reduce the binary size by about 60%.
 */
#ifndef JSON_DISABLE_READER
#endif

/*
 Define as 1 to disable JSON writer if JSON serialization is not required.
 
 This will disable these functions at compile-time:
    - json_write()
    - json_write_file()
    - json_write_opts()
    - json_val_write()
    - json_val_write_file()
    - json_val_write_opts()
    - json_mut_write()
    - json_mut_write_file()
    - json_mut_write_opts()
    - json_mut_val_write()
    - json_mut_val_write_file()
    - json_mut_val_write_opts()
 
 This will reduce the binary size by about 30%.
 */
#ifndef JSON_DISABLE_WRITER
#endif

/*
 Define as 1 to disable JSON Pointer, JSON Patch and JSON Merge Patch supports.
 
 This will disable these functions at compile-time:
    - json_ptr_xxx()
    - json_mut_ptr_xxx()
    - json_doc_ptr_xxx()
    - json_mut_doc_ptr_xxx()
    - json_patch()
    - json_mut_patch()
    - json_merge_patch()
    - json_mut_merge_patch()
 */
#ifndef JSON_DISABLE_UTILS
#endif

/*
 Define as 1 to disable the fast floating-point number conversion in json,
 and use libc's `strtod/snprintf` instead.
 
 This will reduce the binary size by about 30%, but significantly slow down the
 floating-point read/write speed.
 */
#ifndef JSON_DISABLE_FAST_FP_CONV
#endif

/*
 Define as 1 to disable non-standard JSON support at compile-time:
    - Reading and writing inf/nan literal, such as `NaN`, `-Infinity`.
    - Single line and multiple line comments.
    - Single trailing comma at the end of an object or array.
    - Invalid unicode in string value.
 
 This will also invalidate these run-time options:
    - JSON_READ_ALLOW_INF_AND_NAN
    - JSON_READ_ALLOW_COMMENTS
    - JSON_READ_ALLOW_TRAILING_COMMAS
    - JSON_READ_ALLOW_INVALID_UNICODE
    - JSON_WRITE_ALLOW_INF_AND_NAN
    - JSON_WRITE_ALLOW_INVALID_UNICODE
 
 This will reduce the binary size by about 10%, and speed up the reading and
 writing speed by about 2% to 6%.
 */
#ifndef JSON_DISABLE_NON_STANDARD
#endif

/*
 Define as 1 to disable UTF-8 validation at compile time.
 
 If all input strings are guaranteed to be valid UTF-8 encoding (for example,
 some language's String object has already validated the encoding), using this
 flag can avoid redundant UTF-8 validation in json.
 
 This flag can speed up the reading and writing speed of non-ASCII encoded
 strings by about 3% to 7%.
 
 Note: If this flag is used while passing in illegal UTF-8 strings, the
 following errors may occur:
 - Escaped characters may be ignored when parsing JSON strings.
 - Ending quotes may be ignored when parsing JSON strings, causing the string
   to be concatenated to the next value.
 - When accessing `json_mut_val` for serialization, the string ending may be
   accessed out of bounds, causing a segmentation fault.
 */
#ifndef JSON_DISABLE_UTF8_VALIDATION
#endif

/*
 Define as 1 to indicate that the target architecture does not support unaligned
 memory access. Please refer to the comments in the C file for details.
 */
#ifndef JSON_DISABLE_UNALIGNED_MEMORY_ACCESS
#endif

/* Define as 1 to export symbols when building this library as Windows DLL. */
#ifndef JSON_EXPORTS
#endif

/* Define as 1 to import symbols when using this library as Windows DLL. */
#ifndef JSON_IMPORTS
#endif

/* Define as 1 to include <stdint.h> for compiler which doesn't support C99. */
#ifndef JSON_HAS_STDINT_H
#endif

/* Define as 1 to include <stdbool.h> for compiler which doesn't support C99. */
#ifndef JSON_HAS_STDBOOL_H
#endif



/*==============================================================================
 * Compiler Macros
 *============================================================================*/

/** compiler version (MSVC) */
#ifdef _MSC_VER
#   define JSON_MSC_VER _MSC_VER
#else
#   define JSON_MSC_VER 0
#endif

/** compiler version (GCC) */
#ifdef __GNUC__
#   define JSON_GCC_VER __GNUC__
#   if defined(__GNUC_PATCHLEVEL__)
#       define json_gcc_available(major, minor, patch) \
            ((__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__) \
            >= (major * 10000 + minor * 100 + patch))
#   else
#       define json_gcc_available(major, minor, patch) \
            ((__GNUC__ * 10000 + __GNUC_MINOR__ * 100) \
            >= (major * 10000 + minor * 100 + patch))
#   endif
#else
#   define JSON_GCC_VER 0
#   define json_gcc_available(major, minor, patch) 0
#endif

/** real gcc check */
#if !defined(__clang__) && !defined(__INTEL_COMPILER) && !defined(__ICC) && \
    defined(__GNUC__)
#   define JSON_IS_REAL_GCC 1
#else
#   define JSON_IS_REAL_GCC 0
#endif

/** C version (STDC) */
#if defined(__STDC__) && (__STDC__ >= 1) && defined(__STDC_VERSION__)
#   define JSON_STDC_VER __STDC_VERSION__
#else
#   define JSON_STDC_VER 0
#endif

/** C++ version */
#if defined(__cplusplus)
#   define JSON_CPP_VER __cplusplus
#else
#   define JSON_CPP_VER 0
#endif

/** compiler builtin check (since gcc 10.0, clang 2.6, icc 2021) */
#ifndef json_has_builtin
#   ifdef __has_builtin
#       define json_has_builtin(x) __has_builtin(x)
#   else
#       define json_has_builtin(x) 0
#   endif
#endif

/** compiler attribute check (since gcc 5.0, clang 2.9, icc 17) */
#ifndef json_has_attribute
#   ifdef __has_attribute
#       define json_has_attribute(x) __has_attribute(x)
#   else
#       define json_has_attribute(x) 0
#   endif
#endif

/** compiler feature check (since clang 2.6, icc 17) */
#ifndef json_has_feature
#   ifdef __has_feature
#       define json_has_feature(x) __has_feature(x)
#   else
#       define json_has_feature(x) 0
#   endif
#endif

/** include check (since gcc 5.0, clang 2.7, icc 16, msvc 2017 15.3) */
#ifndef json_has_include
#   ifdef __has_include
#       define json_has_include(x) __has_include(x)
#   else
#       define json_has_include(x) 0
#   endif
#endif

/** inline for compiler */
#ifndef json_inline
#   if JSON_MSC_VER >= 1200
#       define json_inline __forceinline
#   elif defined(_MSC_VER)
#       define json_inline __inline
#   elif json_has_attribute(always_inline) || JSON_GCC_VER >= 4
#       define json_inline __inline__ __attribute__((always_inline))
#   elif defined(__clang__) || defined(__GNUC__)
#       define json_inline __inline__
#   elif defined(__cplusplus) || JSON_STDC_VER >= 199901L
#       define json_inline inline
#   else
#       define json_inline
#   endif
#endif

/** noinline for compiler */
#ifndef json_noinline
#   if JSON_MSC_VER >= 1400
#       define json_noinline __declspec(noinline)
#   elif json_has_attribute(noinline) || JSON_GCC_VER >= 4
#       define json_noinline __attribute__((noinline))
#   else
#       define json_noinline
#   endif
#endif

/** align for compiler */
#ifndef json_align
#   if JSON_MSC_VER >= 1300
#       define json_align(x) __declspec(align(x))
#   elif json_has_attribute(aligned) || defined(__GNUC__)
#       define json_align(x) __attribute__((aligned(x)))
#   elif JSON_CPP_VER >= 201103L
#       define json_align(x) alignas(x)
#   else
#       define json_align(x)
#   endif
#endif

/** likely for compiler */
#ifndef json_likely
#   if json_has_builtin(__builtin_expect) || \
    (JSON_GCC_VER >= 4 && JSON_GCC_VER != 5)
#       define json_likely(expr) __builtin_expect(!!(expr), 1)
#   else
#       define json_likely(expr) (expr)
#   endif
#endif

/** unlikely for compiler */
#ifndef json_unlikely
#   if json_has_builtin(__builtin_expect) || \
    (JSON_GCC_VER >= 4 && JSON_GCC_VER != 5)
#       define json_unlikely(expr) __builtin_expect(!!(expr), 0)
#   else
#       define json_unlikely(expr) (expr)
#   endif
#endif

/** compile-time constant check for compiler */
#ifndef json_constant_p
#   if json_has_builtin(__builtin_constant_p) || (JSON_GCC_VER >= 3)
#       define JSON_HAS_CONSTANT_P 1
#       define json_constant_p(value) __builtin_constant_p(value)
#   else
#       define JSON_HAS_CONSTANT_P 0
#       define json_constant_p(value) 0
#   endif
#endif

/** deprecate warning */
#ifndef json_deprecated
#   if JSON_MSC_VER >= 1400
#       define json_deprecated(msg) __declspec(deprecated(msg))
#   elif json_has_feature(attribute_deprecated_with_message) || \
        (JSON_GCC_VER > 4 || (JSON_GCC_VER == 4 && __GNUC_MINOR__ >= 5))
#       define json_deprecated(msg) __attribute__((deprecated(msg)))
#   elif JSON_GCC_VER >= 3
#       define json_deprecated(msg) __attribute__((deprecated))
#   else
#       define json_deprecated(msg)
#   endif
#endif

/** function export */
#ifndef json_api
#   if defined(_WIN32)
#       if defined(JSON_EXPORTS) && JSON_EXPORTS
#           define json_api __declspec(dllexport)
#       elif defined(JSON_IMPORTS) && JSON_IMPORTS
#           define json_api __declspec(dllimport)
#       else
#           define json_api
#       endif
#   elif json_has_attribute(visibility) || JSON_GCC_VER >= 4
#       define json_api __attribute__((visibility("default")))
#   else
#       define json_api
#   endif
#endif

/** inline function export */
#ifndef json_api_inline
#   define json_api_inline static json_inline
#endif

/** stdint (C89 compatible) */
#if (defined(JSON_HAS_STDINT_H) && JSON_HAS_STDINT_H) || \
    JSON_MSC_VER >= 1600 || JSON_STDC_VER >= 199901L || \
    defined(_STDINT_H) || defined(_STDINT_H_) || \
    defined(__CLANG_STDINT_H) || defined(_STDINT_H_INCLUDED) || \
    json_has_include(<stdint.h>)
#   include <stdint.h>
#elif defined(_MSC_VER)
#   if _MSC_VER < 1300
        typedef signed char         int8_t;
        typedef signed short        int16_t;
        typedef signed int          int32_t;
        typedef unsigned char       uint8_t;
        typedef unsigned short      uint16_t;
        typedef unsigned int        uint32_t;
        typedef signed __int64      int64_t;
        typedef unsigned __int64    uint64_t;
#   else
        typedef signed __int8       int8_t;
        typedef signed __int16      int16_t;
        typedef signed __int32      int32_t;
        typedef unsigned __int8     uint8_t;
        typedef unsigned __int16    uint16_t;
        typedef unsigned __int32    uint32_t;
        typedef signed __int64      int64_t;
        typedef unsigned __int64    uint64_t;
#   endif
#else
#   if UCHAR_MAX == 0xFFU
        typedef signed char     int8_t;
        typedef unsigned char   uint8_t;
#   else
#       error cannot find 8-bit integer type
#   endif
#   if USHRT_MAX == 0xFFFFU
        typedef unsigned short  uint16_t;
        typedef signed short    int16_t;
#   elif UINT_MAX == 0xFFFFU
        typedef unsigned int    uint16_t;
        typedef signed int      int16_t;
#   else
#       error cannot find 16-bit integer type
#   endif
#   if UINT_MAX == 0xFFFFFFFFUL
        typedef unsigned int    uint32_t;
        typedef signed int      int32_t;
#   elif ULONG_MAX == 0xFFFFFFFFUL
        typedef unsigned long   uint32_t;
        typedef signed long     int32_t;
#   elif USHRT_MAX == 0xFFFFFFFFUL
        typedef unsigned short  uint32_t;
        typedef signed short    int32_t;
#   else
#       error cannot find 32-bit integer type
#   endif
#   if defined(__INT64_TYPE__) && defined(__UINT64_TYPE__)
        typedef __INT64_TYPE__  int64_t;
        typedef __UINT64_TYPE__ uint64_t;
#   elif defined(__GNUC__) || defined(__clang__)
#       if !defined(_SYS_TYPES_H) && !defined(__int8_t_defined)
        __extension__ typedef long long             int64_t;
#       endif
        __extension__ typedef unsigned long long    uint64_t;
#   elif defined(_LONG_LONG) || defined(__MWERKS__) || defined(_CRAYC) || \
        defined(__SUNPRO_C) || defined(__SUNPRO_CC)
        typedef long long           int64_t;
        typedef unsigned long long  uint64_t;
#   elif (defined(__BORLANDC__) && __BORLANDC__ > 0x460) || \
        defined(__WATCOM_INT64__) || defined (__alpha) || defined (__DECC)
        typedef __int64             int64_t;
        typedef unsigned __int64    uint64_t;
#   else
#       error cannot find 64-bit integer type
#   endif
#endif

/** stdbool (C89 compatible) */
#if (defined(JSON_HAS_STDBOOL_H) && JSON_HAS_STDBOOL_H) || \
    (json_has_include(<stdbool.h>) && !defined(__STRICT_ANSI__)) || \
    JSON_MSC_VER >= 1800 || JSON_STDC_VER >= 199901L
#   include <stdbool.h>
#elif !defined(__bool_true_false_are_defined)
#   define __bool_true_false_are_defined 1
#   if defined(__cplusplus)
#       if defined(__GNUC__) && !defined(__STRICT_ANSI__)
#           define _Bool bool
#           if __cplusplus < 201103L
#               define bool bool
#               define false false
#               define true true
#           endif
#       endif
#   else
#       define bool unsigned char
#       define true 1
#       define false 0
#   endif
#endif

/** char bit check */
#if defined(CHAR_BIT)
#   if CHAR_BIT != 8
#       error non 8-bit char is not supported
#   endif
#endif

/**
 Microsoft Visual C++ 6.0 doesn't support converting number from u64 to f64:
 error C2520: conversion from unsigned __int64 to double not implemented.
 */
#ifndef JSON_U64_TO_F64_NO_IMPL
#   if (0 < JSON_MSC_VER) && (JSON_MSC_VER <= 1200)
#       define JSON_U64_TO_F64_NO_IMPL 1
#   else
#       define JSON_U64_TO_F64_NO_IMPL 0
#   endif
#endif



/*==============================================================================
 * Compile Hint Begin
 *============================================================================*/

/* extern "C" begin */
#ifdef __cplusplus
extern "C" {
#endif

/* warning suppress begin */
#if defined(__clang__)
#   pragma clang diagnostic push
#   pragma clang diagnostic ignored "-Wunused-function"
#   pragma clang diagnostic ignored "-Wunused-parameter"
#elif defined(__GNUC__)
#   if (__GNUC__ > 4) || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)
#   pragma GCC diagnostic push
#   endif
#   pragma GCC diagnostic ignored "-Wunused-function"
#   pragma GCC diagnostic ignored "-Wunused-parameter"
#elif defined(_MSC_VER)
#   pragma warning(push)
#   pragma warning(disable:4800) /* 'int': forcing value to 'true' or 'false' */
#endif



/*==============================================================================
 * Version
 *============================================================================*/

/** The major version of json. */
#define JSON_VERSION_MAJOR  0

/** The minor version of json. */
#define JSON_VERSION_MINOR  10

/** The patch version of json. */
#define JSON_VERSION_PATCH  0

/** The version of json in hex: `(major << 16) | (minor << 8) | (patch)`. */
#define JSON_VERSION_HEX    0x000A00

/** The version string of json. */
#define JSON_VERSION_STRING "0.10.0"

/** The version of json in hex, same as `JSON_VERSION_HEX`. */
json_api uint32_t json_version(void);



/*==============================================================================
 * JSON Types
 *============================================================================*/

/** Type of a JSON value (3 bit). */
typedef uint8_t json_type;
/** No type, invalid. */
#define JSON_TYPE_NONE        ((uint8_t)0)        /* _____000 */
/** Raw string type, no subtype. */
#define JSON_TYPE_RAW         ((uint8_t)1)        /* _____001 */
/** Null type: `null` literal, no subtype. */
#define JSON_TYPE_NULL        ((uint8_t)2)        /* _____010 */
/** Boolean type, subtype: TRUE, FALSE. */
#define JSON_TYPE_BOOL        ((uint8_t)3)        /* _____011 */
/** Number type, subtype: UINT, SINT, REAL. */
#define JSON_TYPE_NUM         ((uint8_t)4)        /* _____100 */
/** String type, subtype: NONE, NOESC. */
#define JSON_TYPE_STR         ((uint8_t)5)        /* _____101 */
/** Array type, no subtype. */
#define JSON_TYPE_ARR         ((uint8_t)6)        /* _____110 */
/** Object type, no subtype. */
#define JSON_TYPE_OBJ         ((uint8_t)7)        /* _____111 */

/** Subtype of a JSON value (2 bit). */
typedef uint8_t json_subtype;
/** No subtype. */
#define JSON_SUBTYPE_NONE     ((uint8_t)(0 << 3)) /* ___00___ */
/** False subtype: `false` literal. */
#define JSON_SUBTYPE_FALSE    ((uint8_t)(0 << 3)) /* ___00___ */
/** True subtype: `true` literal. */
#define JSON_SUBTYPE_TRUE     ((uint8_t)(1 << 3)) /* ___01___ */
/** Unsigned integer subtype: `uint64_t`. */
#define JSON_SUBTYPE_UINT     ((uint8_t)(0 << 3)) /* ___00___ */
/** Signed integer subtype: `int64_t`. */
#define JSON_SUBTYPE_SINT     ((uint8_t)(1 << 3)) /* ___01___ */
/** Real number subtype: `double`. */
#define JSON_SUBTYPE_REAL     ((uint8_t)(2 << 3)) /* ___10___ */
/** String that do not need to be escaped for writing (internal use). */
#define JSON_SUBTYPE_NOESC    ((uint8_t)(1 << 3)) /* ___01___ */

/** The mask used to extract the type of a JSON value. */
#define JSON_TYPE_MASK        ((uint8_t)0x07)     /* _____111 */
/** The number of bits used by the type. */
#define JSON_TYPE_BIT         ((uint8_t)3)
/** The mask used to extract the subtype of a JSON value. */
#define JSON_SUBTYPE_MASK     ((uint8_t)0x18)     /* ___11___ */
/** The number of bits used by the subtype. */
#define JSON_SUBTYPE_BIT      ((uint8_t)2)
/** The mask used to extract the reserved bits of a JSON value. */
#define JSON_RESERVED_MASK    ((uint8_t)0xE0)     /* 111_____ */
/** The number of reserved bits. */
#define JSON_RESERVED_BIT     ((uint8_t)3)
/** The mask used to extract the tag of a JSON value. */
#define JSON_TAG_MASK         ((uint8_t)0xFF)     /* 11111111 */
/** The number of bits used by the tag. */
#define JSON_TAG_BIT          ((uint8_t)8)

/** Padding size for JSON reader. */
#define JSON_PADDING_SIZE     4



/*==============================================================================
 * Allocator
 *============================================================================*/

/**
 A memory allocator.
 
 Typically you don't need to use it, unless you want to customize your own
 memory allocator.
 */
typedef struct json_alc {
    /** Same as libc's malloc(size), should not be NULL. */
    void *(*malloc)(void *ctx, size_t size);
    /** Same as libc's realloc(ptr, size), should not be NULL. */
    void *(*realloc)(void *ctx, void *ptr, size_t old_size, size_t size);
    /** Same as libc's free(ptr), should not be NULL. */
    void (*free)(void *ctx, void *ptr);
    /** A context for malloc/realloc/free, can be NULL. */
    void *ctx;
} json_alc;

/**
 A pool allocator uses fixed length pre-allocated memory.
 
 This allocator may be used to avoid malloc/realloc calls. The pre-allocated 
 memory should be held by the caller. The maximum amount of memory required to
 read a JSON can be calculated using the `json_read_max_memory_usage()`
 function, but the amount of memory required to write a JSON cannot be directly 
 calculated.
 
 This is not a general-purpose allocator. It is designed to handle a single JSON
 data at a time. If it is used for overly complex memory tasks, such as parsing
 multiple JSON documents using the same allocator but releasing only a few of
 them, it may cause memory fragmentation, resulting in performance degradation
 and memory waste.
 
 @param alc The allocator to be initialized.
    If this parameter is NULL, the function will fail and return false.
    If `buf` or `size` is invalid, this will be set to an empty allocator.
 @param buf The buffer memory for this allocator.
    If this parameter is NULL, the function will fail and return false.
 @param size The size of `buf`, in bytes.
    If this parameter is less than 8 words (32/64 bytes on 32/64-bit OS), the
    function will fail and return false.
 @return true if the `alc` has been successfully initialized.
 
 @par Example
 @code
    // parse JSON with stack memory
    char buf[1024];
    json_alc alc;
    json_alc_pool_init(&alc, buf, 1024);
    
    const char *json = "{\"name\":\"Helvetica\",\"size\":16}"
    json_doc *doc = json_read_opts(json, strlen(json), 0, &alc, NULL);
    // the memory of `doc` is on the stack
 @endcode
 
 @warning This Allocator is not thread-safe.
 */
json_api bool json_alc_pool_init(json_alc *alc, void *buf, size_t size);

/**
 A dynamic allocator.
 
 This allocator has a similar usage to the pool allocator above. However, when
 there is not enough memory, this allocator will dynamically request more memory
 using libc's `malloc` function, and frees it all at once when it is destroyed.
 
 @return A new dynamic allocator, or NULL if memory allocation failed.
 @note The returned value should be freed with `json_alc_dyn_free()`.
 
 @warning This Allocator is not thread-safe.
 */
json_api json_alc *json_alc_dyn_new(void);

/**
 Free a dynamic allocator which is created by `json_alc_dyn_new()`.
 @param alc The dynamic allocator to be destroyed.
 */
json_api void json_alc_dyn_free(json_alc *alc);



/*==============================================================================
 * Text Locating
 *============================================================================*/

/**
 Locate the line and column number for a byte position in a string.
 This can be used to get better description for error position.
 
 @param str The input string.
 @param len The byte length of the input string.
 @param pos The byte position within the input string.
 @param line A pointer to receive the line number, starting from 1.
 @param col  A pointer to receive the column number, starting from 1.
 @param chr  A pointer to receive the character index, starting from 0.
 @return true on success, false if `str` is NULL or `pos` is out of bounds.
 @note Line/column/character are calculated based on Unicode characters for
    compatibility with text editors. For multi-byte UTF-8 characters,
    the returned value may not directly correspond to the byte position.
 */
json_api bool json_locate_pos(const char *str, size_t len, size_t pos,
                                  size_t *line, size_t *col, size_t *chr);



/*==============================================================================
 * JSON Structure
 *============================================================================*/

/**
 An immutable document for reading JSON.
 This document holds memory for all its JSON values and strings. When it is no
 longer used, the user should call `json_doc_free()` to free its memory.
 */
typedef struct json_doc json_doc;

/**
 An immutable value for reading JSON.
 A JSON Value has the same lifetime as its document. The memory is held by its
 document and and cannot be freed alone.
 */
typedef struct json_val json_val;

/**
 A mutable document for building JSON.
 This document holds memory for all its JSON values and strings. When it is no
 longer used, the user should call `json_mut_doc_free()` to free its memory.
 */
typedef struct json_mut_doc json_mut_doc;

/**
 A mutable value for building JSON.
 A JSON Value has the same lifetime as its document. The memory is held by its
 document and and cannot be freed alone.
 */
typedef struct json_mut_val json_mut_val;



/*==============================================================================
 * JSON Reader API
 *============================================================================*/

/** Run-time options for JSON reader. */
typedef uint32_t json_read_flag;

/** Default option (RFC 8259 compliant):
    - Read positive integer as uint64_t.
    - Read negative integer as int64_t.
    - Read floating-point number as double with round-to-nearest mode.
    - Read integer which cannot fit in uint64_t or int64_t as double.
    - Report error if double number is infinity.
    - Report error if string contains invalid UTF-8 character or BOM.
    - Report error on trailing commas, comments, inf and nan literals. */
static const json_read_flag JSON_READ_NOFLAG                = 0;

/** Read the input data in-situ.
    This option allows the reader to modify and use input data to store string
    values, which can increase reading speed slightly.
    The caller should hold the input data before free the document.
    The input data must be padded by at least `JSON_PADDING_SIZE` bytes.
    For example: `[1,2]` should be `[1,2]\0\0\0\0`, input length should be 5. */
static const json_read_flag JSON_READ_INSITU                = 1 << 0;

/** Stop when done instead of issuing an error if there's additional content
    after a JSON document. This option may be used to parse small pieces of JSON
    in larger data, such as `NDJSON`. */
static const json_read_flag JSON_READ_STOP_WHEN_DONE        = 1 << 1;

/** Allow single trailing comma at the end of an object or array,
    such as `[1,2,3,]`, `{"a":1,"b":2,}` (non-standard). */
static const json_read_flag JSON_READ_ALLOW_TRAILING_COMMAS = 1 << 2;

/** Allow C-style single line and multiple line comments (non-standard). */
static const json_read_flag JSON_READ_ALLOW_COMMENTS        = 1 << 3;

/** Allow inf/nan number and literal, case-insensitive,
    such as 1e999, NaN, inf, -Infinity (non-standard). */
static const json_read_flag JSON_READ_ALLOW_INF_AND_NAN     = 1 << 4;

/** Read all numbers as raw strings (value with `JSON_TYPE_RAW` type),
    inf/nan literal is also read as raw with `ALLOW_INF_AND_NAN` flag. */
static const json_read_flag JSON_READ_NUMBER_AS_RAW         = 1 << 5;

/** Allow reading invalid unicode when parsing string values (non-standard).
    Invalid characters will be allowed to appear in the string values, but
    invalid escape sequences will still be reported as errors.
    This flag does not affect the performance of correctly encoded strings.
    
    @warning Strings in JSON values may contain incorrect encoding when this
    option is used, you need to handle these strings carefully to avoid security
    risks. */
static const json_read_flag JSON_READ_ALLOW_INVALID_UNICODE = 1 << 6;

/** Read big numbers as raw strings. These big numbers include integers that
    cannot be represented by `int64_t` and `uint64_t`, and floating-point
    numbers that cannot be represented by finite `double`.
    The flag will be overridden by `JSON_READ_NUMBER_AS_RAW` flag. */
static const json_read_flag JSON_READ_BIGNUM_AS_RAW         = 1 << 7;



/** Result code for JSON reader. */
typedef uint32_t json_read_code;

/** Success, no error. */
static const json_read_code JSON_READ_SUCCESS                       = 0;

/** Invalid parameter, such as NULL input string or 0 input length. */
static const json_read_code JSON_READ_ERROR_INVALID_PARAMETER       = 1;

/** Memory allocation failure occurs. */
static const json_read_code JSON_READ_ERROR_MEMORY_ALLOCATION       = 2;

/** Input JSON string is empty. */
static const json_read_code JSON_READ_ERROR_EMPTY_CONTENT           = 3;

/** Unexpected content after document, such as `[123]abc`. */
static const json_read_code JSON_READ_ERROR_UNEXPECTED_CONTENT      = 4;

/** Unexpected ending, such as `[123`. */
static const json_read_code JSON_READ_ERROR_UNEXPECTED_END          = 5;

/** Unexpected character inside the document, such as `[abc]`. */
static const json_read_code JSON_READ_ERROR_UNEXPECTED_CHARACTER    = 6;

/** Invalid JSON structure, such as `[1,]`. */
static const json_read_code JSON_READ_ERROR_JSON_STRUCTURE          = 7;

/** Invalid comment, such as unclosed multi-line comment. */
static const json_read_code JSON_READ_ERROR_INVALID_COMMENT         = 8;

/** Invalid number, such as `123.e12`, `000`. */
static const json_read_code JSON_READ_ERROR_INVALID_NUMBER          = 9;

/** Invalid string, such as invalid escaped character inside a string. */
static const json_read_code JSON_READ_ERROR_INVALID_STRING          = 10;

/** Invalid JSON literal, such as `truu`. */
static const json_read_code JSON_READ_ERROR_LITERAL                 = 11;

/** Failed to open a file. */
static const json_read_code JSON_READ_ERROR_FILE_OPEN               = 12;

/** Failed to read a file. */
static const json_read_code JSON_READ_ERROR_FILE_READ               = 13;

/** Error information for JSON reader. */
typedef struct json_read_err {
    /** Error code, see `json_read_code` for all possible values. */
    json_read_code code;
    /** Error message, constant, no need to free (NULL if success). */
    const char *msg;
    /** Error byte position for input data (0 if success). */
    size_t pos;
} json_read_err;



#if !defined(JSON_DISABLE_READER) || !JSON_DISABLE_READER

/**
 Read JSON with options.
 
 This function is thread-safe when:
 1. The `dat` is not modified by other threads.
 2. The `alc` is thread-safe or NULL.
 
 @param dat The JSON data (UTF-8 without BOM), null-terminator is not required.
    If this parameter is NULL, the function will fail and return NULL.
    The `dat` will not be modified without the flag `JSON_READ_INSITU`, so you
    can pass a `const char *` string and case it to `char *` if you don't use
    the `JSON_READ_INSITU` flag.
 @param len The length of JSON data in bytes.
    If this parameter is 0, the function will fail and return NULL.
 @param flg The JSON read options.
    Multiple options can be combined with `|` operator. 0 means no options.
 @param alc The memory allocator used by JSON reader.
    Pass NULL to use the libc's default allocator.
 @param err A pointer to receive error information.
    Pass NULL if you don't need error information.
 @return A new JSON document, or NULL if an error occurs.
    When it's no longer needed, it should be freed with `json_doc_free()`.
 */
json_api json_doc *json_read_opts(char *dat,
                                        size_t len,
                                        json_read_flag flg,
                                        const json_alc *alc,
                                        json_read_err *err);

/**
 Read a JSON file.
 
 This function is thread-safe when:
 1. The file is not modified by other threads.
 2. The `alc` is thread-safe or NULL.
 
 @param path The JSON file's path.
    If this path is NULL or invalid, the function will fail and return NULL.
 @param flg The JSON read options.
    Multiple options can be combined with `|` operator. 0 means no options.
 @param alc The memory allocator used by JSON reader.
    Pass NULL to use the libc's default allocator.
 @param err A pointer to receive error information.
    Pass NULL if you don't need error information.
 @return A new JSON document, or NULL if an error occurs.
    When it's no longer needed, it should be freed with `json_doc_free()`.
 
 @warning On 32-bit operating system, files larger than 2GB may fail to read.
 */
json_api json_doc *json_read_file(const char *path,
                                        json_read_flag flg,
                                        const json_alc *alc,
                                        json_read_err *err);

/**
 Read JSON from a file pointer.
 
 @param fp The file pointer.
    The data will be read from the current position of the FILE to the end.
    If this fp is NULL or invalid, the function will fail and return NULL.
 @param flg The JSON read options.
    Multiple options can be combined with `|` operator. 0 means no options.
 @param alc The memory allocator used by JSON reader.
    Pass NULL to use the libc's default allocator.
 @param err A pointer to receive error information.
    Pass NULL if you don't need error information.
 @return A new JSON document, or NULL if an error occurs.
    When it's no longer needed, it should be freed with `json_doc_free()`.
 
 @warning On 32-bit operating system, files larger than 2GB may fail to read.
 */
json_api json_doc *json_read_fp(FILE *fp,
                                      json_read_flag flg,
                                      const json_alc *alc,
                                      json_read_err *err);

/**
 Read a JSON string.
 
 This function is thread-safe.
 
 @param dat The JSON data (UTF-8 without BOM), null-terminator is not required.
    If this parameter is NULL, the function will fail and return NULL.
 @param len The length of JSON data in bytes.
    If this parameter is 0, the function will fail and return NULL.
 @param flg The JSON read options.
    Multiple options can be combined with `|` operator. 0 means no options.
 @return A new JSON document, or NULL if an error occurs.
    When it's no longer needed, it should be freed with `json_doc_free()`.
 */
json_api_inline json_doc *json_read(const char *dat,
                                          size_t len,
                                          json_read_flag flg) {
    flg &= ~JSON_READ_INSITU; /* const string cannot be modified */
    return json_read_opts((char *)(void *)(size_t)(const void *)dat,
                            len, flg, NULL, NULL);
}

/**
 Returns the size of maximum memory usage to read a JSON data.
 
 You may use this value to avoid malloc() or calloc() call inside the reader
 to get better performance, or read multiple JSON with one piece of memory.
 
 @param len The length of JSON data in bytes.
 @param flg The JSON read options.
 @return The maximum memory size to read this JSON, or 0 if overflow.
 
 @par Example
 @code
    // read multiple JSON with same pre-allocated memory
    
    char *dat1, *dat2, *dat3; // JSON data
    size_t len1, len2, len3; // JSON length
    size_t max_len = MAX(len1, MAX(len2, len3));
    json_doc *doc;
    
    // use one allocator for multiple JSON
    size_t size = json_read_max_memory_usage(max_len, 0);
    void *buf = malloc(size);
    json_alc alc;
    json_alc_pool_init(&alc, buf, size);
    
    // no more alloc() or realloc() call during reading
    doc = json_read_opts(dat1, len1, 0, &alc, NULL);
    json_doc_free(doc);
    doc = json_read_opts(dat2, len2, 0, &alc, NULL);
    json_doc_free(doc);
    doc = json_read_opts(dat3, len3, 0, &alc, NULL);
    json_doc_free(doc);
    
    free(buf);
 @endcode
 @see json_alc_pool_init()
 */
json_api_inline size_t json_read_max_memory_usage(size_t len,
                                                      json_read_flag flg) {
    /*
     1. The max value count is (json_size / 2 + 1),
        for example: "[1,2,3,4]" size is 9, value count is 5.
     2. Some broken JSON may cost more memory during reading, but fail at end,
        for example: "[[[[[[[[".
     3. json use 16 bytes per value, see struct json_val.
     4. json use dynamic memory with a growth factor of 1.5.
     
     The max memory size is (json_size / 2 * 16 * 1.5 + padding).
     */
    size_t mul = (size_t)12 + !(flg & JSON_READ_INSITU);
    size_t pad = 256;
    size_t max = (size_t)(~(size_t)0);
    if (flg & JSON_READ_STOP_WHEN_DONE) len = len < 256 ? 256 : len;
    if (len >= (max - pad - mul) / mul) return 0;
    return len * mul + pad;
}

/**
 Read a JSON number.

 This function is thread-safe when data is not modified by other threads.

 @param dat The JSON data (UTF-8 without BOM), null-terminator is required.
    If this parameter is NULL, the function will fail and return NULL.
 @param val The output value where result is stored.
    If this parameter is NULL, the function will fail and return NULL.
    The value will hold either UINT or SINT or REAL number;
 @param flg The JSON read options.
    Multiple options can be combined with `|` operator. 0 means no options.
    Supports `JSON_READ_NUMBER_AS_RAW` and `JSON_READ_ALLOW_INF_AND_NAN`.
 @param alc The memory allocator used for long number.
    It is only used when the built-in floating point reader is disabled.
    Pass NULL to use the libc's default allocator.
 @param err A pointer to receive error information.
    Pass NULL if you don't need error information.
 @return If successful, a pointer to the character after the last character
    used in the conversion, NULL if an error occurs.
 */
json_api const char *json_read_number(const char *dat,
                                          json_val *val,
                                          json_read_flag flg,
                                          const json_alc *alc,
                                          json_read_err *err);

/**
 Read a JSON number.

 This function is thread-safe when data is not modified by other threads.

 @param dat The JSON data (UTF-8 without BOM), null-terminator is required.
    If this parameter is NULL, the function will fail and return NULL.
 @param val The output value where result is stored.
    If this parameter is NULL, the function will fail and return NULL.
    The value will hold either UINT or SINT or REAL number;
 @param flg The JSON read options.
    Multiple options can be combined with `|` operator. 0 means no options.
    Supports `JSON_READ_NUMBER_AS_RAW` and `JSON_READ_ALLOW_INF_AND_NAN`.
 @param alc The memory allocator used for long number.
    It is only used when the built-in floating point reader is disabled.
    Pass NULL to use the libc's default allocator.
 @param err A pointer to receive error information.
    Pass NULL if you don't need error information.
 @return If successful, a pointer to the character after the last character
    used in the conversion, NULL if an error occurs.
 */
json_api_inline const char *json_mut_read_number(const char *dat,
                                                     json_mut_val *val,
                                                     json_read_flag flg,
                                                     const json_alc *alc,
                                                     json_read_err *err) {
    return json_read_number(dat, (json_val *)val, flg, alc, err);
}

#endif /* JSON_DISABLE_READER) */



/*==============================================================================
 * JSON Writer API
 *============================================================================*/

/** Run-time options for JSON writer. */
typedef uint32_t json_write_flag;

/** Default option:
    - Write JSON minify.
    - Report error on inf or nan number.
    - Report error on invalid UTF-8 string.
    - Do not escape unicode or slash. */
static const json_write_flag JSON_WRITE_NOFLAG                  = 0;

/** Write JSON pretty with 4 space indent. */
static const json_write_flag JSON_WRITE_PRETTY                  = 1 << 0;

/** Escape unicode as `uXXXX`, make the output ASCII only. */
static const json_write_flag JSON_WRITE_ESCAPE_UNICODE          = 1 << 1;

/** Escape '/' as '\/'. */
static const json_write_flag JSON_WRITE_ESCAPE_SLASHES          = 1 << 2;

/** Write inf and nan number as 'Infinity' and 'NaN' literal (non-standard). */
static const json_write_flag JSON_WRITE_ALLOW_INF_AND_NAN       = 1 << 3;

/** Write inf and nan number as null literal.
    This flag will override `JSON_WRITE_ALLOW_INF_AND_NAN` flag. */
static const json_write_flag JSON_WRITE_INF_AND_NAN_AS_NULL     = 1 << 4;

/** Allow invalid unicode when encoding string values (non-standard).
    Invalid characters in string value will be copied byte by byte.
    If `JSON_WRITE_ESCAPE_UNICODE` flag is also set, invalid character will be
    escaped as `U+FFFD` (replacement character).
    This flag does not affect the performance of correctly encoded strings. */
static const json_write_flag JSON_WRITE_ALLOW_INVALID_UNICODE   = 1 << 5;

/** Write JSON pretty with 2 space indent.
    This flag will override `JSON_WRITE_PRETTY` flag. */
static const json_write_flag JSON_WRITE_PRETTY_TWO_SPACES       = 1 << 6;

/** Adds a newline character `\n` at the end of the JSON.
    This can be helpful for text editors or NDJSON. */
static const json_write_flag JSON_WRITE_NEWLINE_AT_END          = 1 << 7;



/** Result code for JSON writer */
typedef uint32_t json_write_code;

/** Success, no error. */
static const json_write_code JSON_WRITE_SUCCESS                     = 0;

/** Invalid parameter, such as NULL document. */
static const json_write_code JSON_WRITE_ERROR_INVALID_PARAMETER     = 1;

/** Memory allocation failure occurs. */
static const json_write_code JSON_WRITE_ERROR_MEMORY_ALLOCATION     = 2;

/** Invalid value type in JSON document. */
static const json_write_code JSON_WRITE_ERROR_INVALID_VALUE_TYPE    = 3;

/** NaN or Infinity number occurs. */
static const json_write_code JSON_WRITE_ERROR_NAN_OR_INF            = 4;

/** Failed to open a file. */
static const json_write_code JSON_WRITE_ERROR_FILE_OPEN             = 5;

/** Failed to write a file. */
static const json_write_code JSON_WRITE_ERROR_FILE_WRITE            = 6;

/** Invalid unicode in string. */
static const json_write_code JSON_WRITE_ERROR_INVALID_STRING        = 7;

/** Error information for JSON writer. */
typedef struct json_write_err {
    /** Error code, see `json_write_code` for all possible values. */
    json_write_code code;
    /** Error message, constant, no need to free (NULL if success). */
    const char *msg;
} json_write_err;



#if !defined(JSON_DISABLE_WRITER) || !JSON_DISABLE_WRITER

/*==============================================================================
 * JSON Document Writer API
 *============================================================================*/

/**
 Write a document to JSON string with options.
 
 This function is thread-safe when:
 The `alc` is thread-safe or NULL.
 
 @param doc The JSON document.
    If this doc is NULL or has no root, the function will fail and return false.
 @param flg The JSON write options.
    Multiple options can be combined with `|` operator. 0 means no options.
 @param alc The memory allocator used by JSON writer.
    Pass NULL to use the libc's default allocator.
 @param len A pointer to receive output length in bytes (not including the
    null-terminator). Pass NULL if you don't need length information.
 @param err A pointer to receive error information.
    Pass NULL if you don't need error information.
 @return A new JSON string, or NULL if an error occurs.
    This string is encoded as UTF-8 with a null-terminator.
    When it's no longer needed, it should be freed with free() or alc->free().
 */
json_api char *json_write_opts(const json_doc *doc,
                                   json_write_flag flg,
                                   const json_alc *alc,
                                   size_t *len,
                                   json_write_err *err);

/**
 Write a document to JSON file with options.
 
 This function is thread-safe when:
 1. The file is not accessed by other threads.
 2. The `alc` is thread-safe or NULL.

 @param path The JSON file's path.
    If this path is NULL or invalid, the function will fail and return false.
    If this file is not empty, the content will be discarded.
 @param doc The JSON document.
    If this doc is NULL or has no root, the function will fail and return false.
 @param flg The JSON write options.
    Multiple options can be combined with `|` operator. 0 means no options.
 @param alc The memory allocator used by JSON writer.
    Pass NULL to use the libc's default allocator.
 @param err A pointer to receive error information.
    Pass NULL if you don't need error information.
 @return true if successful, false if an error occurs.
 
 @warning On 32-bit operating system, files larger than 2GB may fail to write.
 */
json_api bool json_write_file(const char *path,
                                  const json_doc *doc,
                                  json_write_flag flg,
                                  const json_alc *alc,
                                  json_write_err *err);

/**
 Write a document to file pointer with options.
 
 @param fp The file pointer.
    The data will be written to the current position of the file.
    If this fp is NULL or invalid, the function will fail and return false.
 @param doc The JSON document.
    If this doc is NULL or has no root, the function will fail and return false.
 @param flg The JSON write options.
    Multiple options can be combined with `|` operator. 0 means no options.
 @param alc The memory allocator used by JSON writer.
    Pass NULL to use the libc's default allocator.
 @param err A pointer to receive error information.
    Pass NULL if you don't need error information.
 @return true if successful, false if an error occurs.
 
 @warning On 32-bit operating system, files larger than 2GB may fail to write.
 */
json_api bool json_write_fp(FILE *fp,
                                const json_doc *doc,
                                json_write_flag flg,
                                const json_alc *alc,
                                json_write_err *err);

/**
 Write a document to JSON string.
 
 This function is thread-safe.
 
 @param doc The JSON document.
    If this doc is NULL or has no root, the function will fail and return false.
 @param flg The JSON write options.
    Multiple options can be combined with `|` operator. 0 means no options.
 @param len A pointer to receive output length in bytes (not including the
    null-terminator). Pass NULL if you don't need length information.
 @return A new JSON string, or NULL if an error occurs.
    This string is encoded as UTF-8 with a null-terminator.
    When it's no longer needed, it should be freed with free().
 */
json_api_inline char *json_write(const json_doc *doc,
                                     json_write_flag flg,
                                     size_t *len) {
    return json_write_opts(doc, flg, NULL, len, NULL);
}



/**
 Write a document to JSON string with options.
 
 This function is thread-safe when:
 1. The `doc` is not modified by other threads.
 2. The `alc` is thread-safe or NULL.

 @param doc The mutable JSON document.
    If this doc is NULL or has no root, the function will fail and return false.
 @param flg The JSON write options.
    Multiple options can be combined with `|` operator. 0 means no options.
 @param alc The memory allocator used by JSON writer.
    Pass NULL to use the libc's default allocator.
 @param len A pointer to receive output length in bytes (not including the
    null-terminator). Pass NULL if you don't need length information.
 @param err A pointer to receive error information.
    Pass NULL if you don't need error information.
 @return A new JSON string, or NULL if an error occurs.
    This string is encoded as UTF-8 with a null-terminator.
    When it's no longer needed, it should be freed with free() or alc->free().
 */
json_api char *json_mut_write_opts(const json_mut_doc *doc,
                                       json_write_flag flg,
                                       const json_alc *alc,
                                       size_t *len,
                                       json_write_err *err);

/**
 Write a document to JSON file with options.
 
 This function is thread-safe when:
 1. The file is not accessed by other threads.
 2. The `doc` is not modified by other threads.
 3. The `alc` is thread-safe or NULL.
 
 @param path The JSON file's path.
    If this path is NULL or invalid, the function will fail and return false.
    If this file is not empty, the content will be discarded.
 @param doc The mutable JSON document.
    If this doc is NULL or has no root, the function will fail and return false.
 @param flg The JSON write options.
    Multiple options can be combined with `|` operator. 0 means no options.
 @param alc The memory allocator used by JSON writer.
    Pass NULL to use the libc's default allocator.
 @param err A pointer to receive error information.
    Pass NULL if you don't need error information.
 @return true if successful, false if an error occurs.
 
 @warning On 32-bit operating system, files larger than 2GB may fail to write.
 */
json_api bool json_mut_write_file(const char *path,
                                      const json_mut_doc *doc,
                                      json_write_flag flg,
                                      const json_alc *alc,
                                      json_write_err *err);

/**
 Write a document to file pointer with options.
 
 @param fp The file pointer.
    The data will be written to the current position of the file.
    If this fp is NULL or invalid, the function will fail and return false.
 @param doc The mutable JSON document.
    If this doc is NULL or has no root, the function will fail and return false.
 @param flg The JSON write options.
    Multiple options can be combined with `|` operator. 0 means no options.
 @param alc The memory allocator used by JSON writer.
    Pass NULL to use the libc's default allocator.
 @param err A pointer to receive error information.
    Pass NULL if you don't need error information.
 @return true if successful, false if an error occurs.
 
 @warning On 32-bit operating system, files larger than 2GB may fail to write.
 */
json_api bool json_mut_write_fp(FILE *fp,
                                    const json_mut_doc *doc,
                                    json_write_flag flg,
                                    const json_alc *alc,
                                    json_write_err *err);

/**
 Write a document to JSON string.
 
 This function is thread-safe when:
 The `doc` is not modified by other threads.
 
 @param doc The JSON document.
    If this doc is NULL or has no root, the function will fail and return false.
 @param flg The JSON write options.
    Multiple options can be combined with `|` operator. 0 means no options.
 @param len A pointer to receive output length in bytes (not including the
    null-terminator). Pass NULL if you don't need length information.
 @return A new JSON string, or NULL if an error occurs.
    This string is encoded as UTF-8 with a null-terminator.
    When it's no longer needed, it should be freed with free().
 */
json_api_inline char *json_mut_write(const json_mut_doc *doc,
                                         json_write_flag flg,
                                         size_t *len) {
    return json_mut_write_opts(doc, flg, NULL, len, NULL);
}



/*==============================================================================
 * JSON Value Writer API
 *============================================================================*/

/**
 Write a value to JSON string with options.
 
 This function is thread-safe when:
 The `alc` is thread-safe or NULL.
 
 @param val The JSON root value.
    If this parameter is NULL, the function will fail and return NULL.
 @param flg The JSON write options.
    Multiple options can be combined with `|` operator. 0 means no options.
 @param alc The memory allocator used by JSON writer.
    Pass NULL to use the libc's default allocator.
 @param len A pointer to receive output length in bytes (not including the
    null-terminator). Pass NULL if you don't need length information.
 @param err A pointer to receive error information.
    Pass NULL if you don't need error information.
 @return A new JSON string, or NULL if an error occurs.
    This string is encoded as UTF-8 with a null-terminator.
    When it's no longer needed, it should be freed with free() or alc->free().
 */
json_api char *json_val_write_opts(const json_val *val,
                                       json_write_flag flg,
                                       const json_alc *alc,
                                       size_t *len,
                                       json_write_err *err);

/**
 Write a value to JSON file with options.
 
 This function is thread-safe when:
 1. The file is not accessed by other threads.
 2. The `alc` is thread-safe or NULL.
 
 @param path The JSON file's path.
    If this path is NULL or invalid, the function will fail and return false.
    If this file is not empty, the content will be discarded.
 @param val The JSON root value.
    If this parameter is NULL, the function will fail and return NULL.
 @param flg The JSON write options.
    Multiple options can be combined with `|` operator. 0 means no options.
 @param alc The memory allocator used by JSON writer.
    Pass NULL to use the libc's default allocator.
 @param err A pointer to receive error information.
    Pass NULL if you don't need error information.
 @return true if successful, false if an error occurs.
 
 @warning On 32-bit operating system, files larger than 2GB may fail to write.
 */
json_api bool json_val_write_file(const char *path,
                                      const json_val *val,
                                      json_write_flag flg,
                                      const json_alc *alc,
                                      json_write_err *err);

/**
 Write a value to file pointer with options.
 
 @param fp The file pointer.
    The data will be written to the current position of the file.
    If this path is NULL or invalid, the function will fail and return false.
 @param val The JSON root value.
    If this parameter is NULL, the function will fail and return NULL.
 @param flg The JSON write options.
    Multiple options can be combined with `|` operator. 0 means no options.
 @param alc The memory allocator used by JSON writer.
    Pass NULL to use the libc's default allocator.
 @param err A pointer to receive error information.
    Pass NULL if you don't need error information.
 @return true if successful, false if an error occurs.
 
 @warning On 32-bit operating system, files larger than 2GB may fail to write.
 */
json_api bool json_val_write_fp(FILE *fp,
                                    const json_val *val,
                                    json_write_flag flg,
                                    const json_alc *alc,
                                    json_write_err *err);

/**
 Write a value to JSON string.
 
 This function is thread-safe.
 
 @param val The JSON root value.
    If this parameter is NULL, the function will fail and return NULL.
 @param flg The JSON write options.
    Multiple options can be combined with `|` operator. 0 means no options.
 @param len A pointer to receive output length in bytes (not including the
    null-terminator). Pass NULL if you don't need length information.
 @return A new JSON string, or NULL if an error occurs.
    This string is encoded as UTF-8 with a null-terminator.
    When it's no longer needed, it should be freed with free().
 */
json_api_inline char *json_val_write(const json_val *val,
                                         json_write_flag flg,
                                         size_t *len) {
    return json_val_write_opts(val, flg, NULL, len, NULL);
}

/**
 Write a value to JSON string with options.
 
 This function is thread-safe when:
 1. The `val` is not modified by other threads.
 2. The `alc` is thread-safe or NULL.
 
 @param val The mutable JSON root value.
    If this parameter is NULL, the function will fail and return NULL.
 @param flg The JSON write options.
    Multiple options can be combined with `|` operator. 0 means no options.
 @param alc The memory allocator used by JSON writer.
    Pass NULL to use the libc's default allocator.
 @param len A pointer to receive output length in bytes (not including the
    null-terminator). Pass NULL if you don't need length information.
 @param err A pointer to receive error information.
    Pass NULL if you don't need error information.
 @return  A new JSON string, or NULL if an error occurs.
    This string is encoded as UTF-8 with a null-terminator.
    When it's no longer needed, it should be freed with free() or alc->free().
 */
json_api char *json_mut_val_write_opts(const json_mut_val *val,
                                           json_write_flag flg,
                                           const json_alc *alc,
                                           size_t *len,
                                           json_write_err *err);

/**
 Write a value to JSON file with options.
 
 This function is thread-safe when:
 1. The file is not accessed by other threads.
 2. The `val` is not modified by other threads.
 3. The `alc` is thread-safe or NULL.
 
 @param path The JSON file's path.
    If this path is NULL or invalid, the function will fail and return false.
    If this file is not empty, the content will be discarded.
 @param val The mutable JSON root value.
    If this parameter is NULL, the function will fail and return NULL.
 @param flg The JSON write options.
    Multiple options can be combined with `|` operator. 0 means no options.
 @param alc The memory allocator used by JSON writer.
    Pass NULL to use the libc's default allocator.
 @param err A pointer to receive error information.
    Pass NULL if you don't need error information.
 @return true if successful, false if an error occurs.
 
 @warning On 32-bit operating system, files larger than 2GB may fail to write.
 */
json_api bool json_mut_val_write_file(const char *path,
                                          const json_mut_val *val,
                                          json_write_flag flg,
                                          const json_alc *alc,
                                          json_write_err *err);

/**
 Write a value to JSON file with options.
 
 @param fp The file pointer.
    The data will be written to the current position of the file.
    If this path is NULL or invalid, the function will fail and return false.
 @param val The mutable JSON root value.
    If this parameter is NULL, the function will fail and return NULL.
 @param flg The JSON write options.
    Multiple options can be combined with `|` operator. 0 means no options.
 @param alc The memory allocator used by JSON writer.
    Pass NULL to use the libc's default allocator.
 @param err A pointer to receive error information.
    Pass NULL if you don't need error information.
 @return true if successful, false if an error occurs.
 
 @warning On 32-bit operating system, files larger than 2GB may fail to write.
 */
json_api bool json_mut_val_write_fp(FILE *fp,
                                        const json_mut_val *val,
                                        json_write_flag flg,
                                        const json_alc *alc,
                                        json_write_err *err);

/**
 Write a value to JSON string.
 
 This function is thread-safe when:
 The `val` is not modified by other threads.
 
 @param val The JSON root value.
    If this parameter is NULL, the function will fail and return NULL.
 @param flg The JSON write options.
    Multiple options can be combined with `|` operator. 0 means no options.
 @param len A pointer to receive output length in bytes (not including the
    null-terminator). Pass NULL if you don't need length information.
 @return A new JSON string, or NULL if an error occurs.
    This string is encoded as UTF-8 with a null-terminator.
    When it's no longer needed, it should be freed with free().
 */
json_api_inline char *json_mut_val_write(const json_mut_val *val,
                                             json_write_flag flg,
                                             size_t *len) {
    return json_mut_val_write_opts(val, flg, NULL, len, NULL);
}

#endif /* JSON_DISABLE_WRITER */



/*==============================================================================
 * JSON Document API
 *============================================================================*/

/** Returns the root value of this JSON document.
    Returns NULL if `doc` is NULL. */
json_api_inline json_val *json_doc_get_root(json_doc *doc);

/** Returns read size of input JSON data.
    Returns 0 if `doc` is NULL.
    For example: the read size of `[1,2,3]` is 7 bytes.  */
json_api_inline size_t json_doc_get_read_size(json_doc *doc);

/** Returns total value count in this JSON document.
    Returns 0 if `doc` is NULL.
    For example: the value count of `[1,2,3]` is 4. */
json_api_inline size_t json_doc_get_val_count(json_doc *doc);

/** Release the JSON document and free the memory.
    After calling this function, the `doc` and all values from the `doc` are no
    longer available. This function will do nothing if the `doc` is NULL. */
json_api_inline void json_doc_free(json_doc *doc);



/*==============================================================================
 * JSON Value Type API
 *============================================================================*/

/** Returns whether the JSON value is raw.
    Returns false if `val` is NULL. */
json_api_inline bool json_is_raw(json_val *val);

/** Returns whether the JSON value is `null`.
    Returns false if `val` is NULL. */
json_api_inline bool json_is_null(json_val *val);

/** Returns whether the JSON value is `true`.
    Returns false if `val` is NULL. */
json_api_inline bool json_is_true(json_val *val);

/** Returns whether the JSON value is `false`.
    Returns false if `val` is NULL. */
json_api_inline bool json_is_false(json_val *val);

/** Returns whether the JSON value is bool (true/false).
    Returns false if `val` is NULL. */
json_api_inline bool json_is_bool(json_val *val);

/** Returns whether the JSON value is unsigned integer (uint64_t).
    Returns false if `val` is NULL. */
json_api_inline bool json_is_uint(json_val *val);

/** Returns whether the JSON value is signed integer (int64_t).
    Returns false if `val` is NULL. */
json_api_inline bool json_is_sint(json_val *val);

/** Returns whether the JSON value is integer (uint64_t/int64_t).
    Returns false if `val` is NULL. */
json_api_inline bool json_is_int(json_val *val);

/** Returns whether the JSON value is real number (double).
    Returns false if `val` is NULL. */
json_api_inline bool json_is_real(json_val *val);

/** Returns whether the JSON value is number (uint64_t/int64_t/double).
    Returns false if `val` is NULL. */
json_api_inline bool json_is_num(json_val *val);

/** Returns whether the JSON value is string.
    Returns false if `val` is NULL. */
json_api_inline bool json_is_str(json_val *val);

/** Returns whether the JSON value is array.
    Returns false if `val` is NULL. */
json_api_inline bool json_is_arr(json_val *val);

/** Returns whether the JSON value is object.
    Returns false if `val` is NULL. */
json_api_inline bool json_is_obj(json_val *val);

/** Returns whether the JSON value is container (array/object).
    Returns false if `val` is NULL. */
json_api_inline bool json_is_ctn(json_val *val);



/*==============================================================================
 * JSON Value Content API
 *============================================================================*/

/** Returns the JSON value's type.
    Returns JSON_TYPE_NONE if `val` is NULL. */
json_api_inline json_type json_get_type(json_val *val);

/** Returns the JSON value's subtype.
    Returns JSON_SUBTYPE_NONE if `val` is NULL. */
json_api_inline json_subtype json_get_subtype(json_val *val);

/** Returns the JSON value's tag.
    Returns 0 if `val` is NULL. */
json_api_inline uint8_t json_get_tag(json_val *val);

/** Returns the JSON value's type description.
    The return value should be one of these strings: "raw", "null", "string",
    "array", "object", "true", "false", "uint", "sint", "real", "unknown". */
json_api_inline const char *json_get_type_desc(json_val *val);

/** Returns the content if the value is raw.
    Returns NULL if `val` is NULL or type is not raw. */
json_api_inline const char *json_get_raw(json_val *val);

/** Returns the content if the value is bool.
    Returns NULL if `val` is NULL or type is not bool. */
json_api_inline bool json_get_bool(json_val *val);

/** Returns the content and cast to uint64_t.
    Returns 0 if `val` is NULL or type is not integer(sint/uint). */
json_api_inline uint64_t json_get_uint(json_val *val);

/** Returns the content and cast to int64_t.
    Returns 0 if `val` is NULL or type is not integer(sint/uint). */
json_api_inline int64_t json_get_sint(json_val *val);

/** Returns the content and cast to int.
    Returns 0 if `val` is NULL or type is not integer(sint/uint). */
json_api_inline int json_get_int(json_val *val);

/** Returns the content if the value is real number, or 0.0 on error.
    Returns 0.0 if `val` is NULL or type is not real(double). */
json_api_inline double json_get_real(json_val *val);

/** Returns the content and typecast to `double` if the value is number.
    Returns 0.0 if `val` is NULL or type is not number(uint/sint/real). */
json_api_inline double json_get_num(json_val *val);

/** Returns the content if the value is string.
    Returns NULL if `val` is NULL or type is not string. */
json_api_inline const char *json_get_str(json_val *val);

/** Returns the content length (string length, array size, object size.
    Returns 0 if `val` is NULL or type is not string/array/object. */
json_api_inline size_t json_get_len(json_val *val);

/** Returns whether the JSON value is equals to a string.
    Returns false if input is NULL or type is not string. */
json_api_inline bool json_equals_str(json_val *val, const char *str);

/** Returns whether the JSON value is equals to a string.
    The `str` should be a UTF-8 string, null-terminator is not required.
    Returns false if input is NULL or type is not string. */
json_api_inline bool json_equals_strn(json_val *val, const char *str,
                                          size_t len);

/** Returns whether two JSON values are equal (deep compare).
    Returns false if input is NULL.
    @note the result may be inaccurate if object has duplicate keys.
    @warning This function is recursive and may cause a stack overflow
        if the object level is too deep. */
json_api_inline bool json_equals(json_val *lhs, json_val *rhs);

/** Set the value to raw.
    Returns false if input is NULL or `val` is object or array.
    @warning This will modify the `immutable` value, use with caution. */
json_api_inline bool json_set_raw(json_val *val,
                                      const char *raw, size_t len);

/** Set the value to null.
    Returns false if input is NULL or `val` is object or array.
    @warning This will modify the `immutable` value, use with caution. */
json_api_inline bool json_set_null(json_val *val);

/** Set the value to bool.
    Returns false if input is NULL or `val` is object or array.
    @warning This will modify the `immutable` value, use with caution. */
json_api_inline bool json_set_bool(json_val *val, bool num);

/** Set the value to uint.
    Returns false if input is NULL or `val` is object or array.
    @warning This will modify the `immutable` value, use with caution. */
json_api_inline bool json_set_uint(json_val *val, uint64_t num);

/** Set the value to sint.
    Returns false if input is NULL or `val` is object or array.
    @warning This will modify the `immutable` value, use with caution. */
json_api_inline bool json_set_sint(json_val *val, int64_t num);

/** Set the value to int.
    Returns false if input is NULL or `val` is object or array.
    @warning This will modify the `immutable` value, use with caution. */
json_api_inline bool json_set_int(json_val *val, int num);

/** Set the value to real.
    Returns false if input is NULL or `val` is object or array.
    @warning This will modify the `immutable` value, use with caution. */
json_api_inline bool json_set_real(json_val *val, double num);

/** Set the value to string (null-terminated).
    Returns false if input is NULL or `val` is object or array.
    @warning This will modify the `immutable` value, use with caution. */
json_api_inline bool json_set_str(json_val *val, const char *str);

/** Set the value to string (with length).
    Returns false if input is NULL or `val` is object or array.
    @warning This will modify the `immutable` value, use with caution. */
json_api_inline bool json_set_strn(json_val *val,
                                       const char *str, size_t len);



/*==============================================================================
 * JSON Array API
 *============================================================================*/

/** Returns the number of elements in this array.
    Returns 0 if `arr` is NULL or type is not array. */
json_api_inline size_t json_arr_size(json_val *arr);

/** Returns the element at the specified position in this array.
    Returns NULL if array is NULL/empty or the index is out of bounds.
    @warning This function takes a linear search time if array is not flat.
        For example: `[1,{},3]` is flat, `[1,[2],3]` is not flat. */
json_api_inline json_val *json_arr_get(json_val *arr, size_t idx);

/** Returns the first element of this array.
    Returns NULL if `arr` is NULL/empty or type is not array. */
json_api_inline json_val *json_arr_get_first(json_val *arr);

/** Returns the last element of this array.
    Returns NULL if `arr` is NULL/empty or type is not array.
    @warning This function takes a linear search time if array is not flat.
        For example: `[1,{},3]` is flat, `[1,[2],3]` is not flat.*/
json_api_inline json_val *json_arr_get_last(json_val *arr);



/*==============================================================================
 * JSON Array Iterator API
 *============================================================================*/

/**
 A JSON array iterator.
 
 @par Example
 @code
    json_val *val;
    json_arr_iter iter = json_arr_iter_with(arr);
    while ((val = json_arr_iter_next(&iter))) {
        your_func(val);
    }
 @endcode
 */
typedef struct json_arr_iter {
    size_t idx; /**< next value's index */
    size_t max; /**< maximum index (arr.size) */
    json_val *cur; /**< next value */
} json_arr_iter;

/**
 Initialize an iterator for this array.
 
 @param arr The array to be iterated over.
    If this parameter is NULL or not an array, `iter` will be set to empty.
 @param iter The iterator to be initialized.
    If this parameter is NULL, the function will fail and return false.
 @return true if the `iter` has been successfully initialized.
 
 @note The iterator does not need to be destroyed.
 */
json_api_inline bool json_arr_iter_init(json_val *arr,
                                            json_arr_iter *iter);

/**
 Create an iterator with an array , same as `json_arr_iter_init()`.
 
 @param arr The array to be iterated over.
    If this parameter is NULL or not an array, an empty iterator will returned.
 @return A new iterator for the array.
 
 @note The iterator does not need to be destroyed.
 */
json_api_inline json_arr_iter json_arr_iter_with(json_val *arr);

/**
 Returns whether the iteration has more elements.
 If `iter` is NULL, this function will return false.
 */
json_api_inline bool json_arr_iter_has_next(json_arr_iter *iter);

/**
 Returns the next element in the iteration, or NULL on end.
 If `iter` is NULL, this function will return NULL.
 */
json_api_inline json_val *json_arr_iter_next(json_arr_iter *iter);

/**
 Macro for iterating over an array.
 It works like iterator, but with a more intuitive API.
 
 @par Example
 @code
    size_t idx, max;
    json_val *val;
    json_arr_foreach(arr, idx, max, val) {
        your_func(idx, val);
    }
 @endcode
 */
#define json_arr_foreach(arr, idx, max, val) \
    for ((idx) = 0, \
        (max) = json_arr_size(arr), \
        (val) = json_arr_get_first(arr); \
        (idx) < (max); \
        (idx)++, \
        (val) = unsafe_json_get_next(val))



/*==============================================================================
 * JSON Object API
 *============================================================================*/

/** Returns the number of key-value pairs in this object.
    Returns 0 if `obj` is NULL or type is not object. */
json_api_inline size_t json_obj_size(json_val *obj);

/** Returns the value to which the specified key is mapped.
    Returns NULL if this object contains no mapping for the key.
    Returns NULL if `obj/key` is NULL, or type is not object.
    
    The `key` should be a null-terminated UTF-8 string.
    
    @warning This function takes a linear search time. */
json_api_inline json_val *json_obj_get(json_val *obj, const char *key);

/** Returns the value to which the specified key is mapped.
    Returns NULL if this object contains no mapping for the key.
    Returns NULL if `obj/key` is NULL, or type is not object.
    
    The `key` should be a UTF-8 string, null-terminator is not required.
    The `key_len` should be the length of the key, in bytes.
    
    @warning This function takes a linear search time. */
json_api_inline json_val *json_obj_getn(json_val *obj, const char *key,
                                              size_t key_len);



/*==============================================================================
 * JSON Object Iterator API
 *============================================================================*/

/**
 A JSON object iterator.
 
 @par Example
 @code
    json_val *key, *val;
    json_obj_iter iter = json_obj_iter_with(obj);
    while ((key = json_obj_iter_next(&iter))) {
        val = json_obj_iter_get_val(key);
        your_func(key, val);
    }
 @endcode
 
 If the ordering of the keys is known at compile-time, you can use this method
 to speed up value lookups:
 @code
    // {"k1":1, "k2": 3, "k3": 3}
    json_val *key, *val;
    json_obj_iter iter = json_obj_iter_with(obj);
    json_val *v1 = json_obj_iter_get(&iter, "k1");
    json_val *v3 = json_obj_iter_get(&iter, "k3");
 @endcode
 @see json_obj_iter_get() and json_obj_iter_getn()
 */
typedef struct json_obj_iter {
    size_t idx; /**< next key's index */
    size_t max; /**< maximum key index (obj.size) */
    json_val *cur; /**< next key */
    json_val *obj; /**< the object being iterated */
} json_obj_iter;

/**
 Initialize an iterator for this object.
 
 @param obj The object to be iterated over.
    If this parameter is NULL or not an object, `iter` will be set to empty.
 @param iter The iterator to be initialized.
    If this parameter is NULL, the function will fail and return false.
 @return true if the `iter` has been successfully initialized.
 
 @note The iterator does not need to be destroyed.
 */
json_api_inline bool json_obj_iter_init(json_val *obj,
                                            json_obj_iter *iter);

/**
 Create an iterator with an object, same as `json_obj_iter_init()`.
 
 @param obj The object to be iterated over.
    If this parameter is NULL or not an object, an empty iterator will returned.
 @return A new iterator for the object.
 
 @note The iterator does not need to be destroyed.
 */
json_api_inline json_obj_iter json_obj_iter_with(json_val *obj);

/**
 Returns whether the iteration has more elements.
 If `iter` is NULL, this function will return false.
 */
json_api_inline bool json_obj_iter_has_next(json_obj_iter *iter);

/**
 Returns the next key in the iteration, or NULL on end.
 If `iter` is NULL, this function will return NULL.
 */
json_api_inline json_val *json_obj_iter_next(json_obj_iter *iter);

/**
 Returns the value for key inside the iteration.
 If `iter` is NULL, this function will return NULL.
 */
json_api_inline json_val *json_obj_iter_get_val(json_val *key);

/**
 Iterates to a specified key and returns the value.
 
 This function does the same thing as `json_obj_get()`, but is much faster
 if the ordering of the keys is known at compile-time and you are using the same
 order to look up the values. If the key exists in this object, then the
 iterator will stop at the next key, otherwise the iterator will not change and
 NULL is returned.
 
 @param iter The object iterator, should not be NULL.
 @param key The key, should be a UTF-8 string with null-terminator.
 @return The value to which the specified key is mapped.
    NULL if this object contains no mapping for the key or input is invalid.
 
 @warning This function takes a linear search time if the key is not nearby.
 */
json_api_inline json_val *json_obj_iter_get(json_obj_iter *iter,
                                                  const char *key);

/**
 Iterates to a specified key and returns the value.

 This function does the same thing as `json_obj_getn()`, but is much faster
 if the ordering of the keys is known at compile-time and you are using the same
 order to look up the values. If the key exists in this object, then the
 iterator will stop at the next key, otherwise the iterator will not change and
 NULL is returned.
 
 @param iter The object iterator, should not be NULL.
 @param key The key, should be a UTF-8 string, null-terminator is not required.
 @param key_len The the length of `key`, in bytes.
 @return The value to which the specified key is mapped.
    NULL if this object contains no mapping for the key or input is invalid.
 
 @warning This function takes a linear search time if the key is not nearby.
 */
json_api_inline json_val *json_obj_iter_getn(json_obj_iter *iter,
                                                   const char *key,
                                                   size_t key_len);

/**
 Macro for iterating over an object.
 It works like iterator, but with a more intuitive API.
 
 @par Example
 @code
    size_t idx, max;
    json_val *key, *val;
    json_obj_foreach(obj, idx, max, key, val) {
        your_func(key, val);
    }
 @endcode
 */
#define json_obj_foreach(obj, idx, max, key, val) \
    for ((idx) = 0, \
        (max) = json_obj_size(obj), \
        (key) = (obj) ? unsafe_json_get_first(obj) : NULL, \
        (val) = (key) + 1; \
        (idx) < (max); \
        (idx)++, \
        (key) = unsafe_json_get_next(val), \
        (val) = (key) + 1)



/*==============================================================================
 * Mutable JSON Document API
 *============================================================================*/

/** Returns the root value of this JSON document.
    Returns NULL if `doc` is NULL. */
json_api_inline json_mut_val *json_mut_doc_get_root(json_mut_doc *doc);

/** Sets the root value of this JSON document.
    Pass NULL to clear root value of the document. */
json_api_inline void json_mut_doc_set_root(json_mut_doc *doc,
                                               json_mut_val *root);

/**
 Set the string pool size for a mutable document.
 This function does not allocate memory immediately, but uses the size when
 the next memory allocation is needed.
 
 If the caller knows the approximate bytes of strings that the document needs to
 store (e.g. copy string with `json_mut_strcpy` function), setting a larger
 size can avoid multiple memory allocations and improve performance.
 
 @param doc The mutable document.
 @param len The desired string pool size in bytes (total string length).
 @return true if successful, false if size is 0 or overflow.
 */
json_api bool json_mut_doc_set_str_pool_size(json_mut_doc *doc,
                                                 size_t len);

/**
 Set the value pool size for a mutable document.
 This function does not allocate memory immediately, but uses the size when
 the next memory allocation is needed.
 
 If the caller knows the approximate number of values that the document needs to
 store (e.g. create new value with `json_mut_xxx` functions), setting a larger
 size can avoid multiple memory allocations and improve performance.
 
 @param doc The mutable document.
 @param count The desired value pool size (number of `json_mut_val`).
 @return true if successful, false if size is 0 or overflow.
 */
json_api bool json_mut_doc_set_val_pool_size(json_mut_doc *doc,
                                                 size_t count);

/** Release the JSON document and free the memory.
    After calling this function, the `doc` and all values from the `doc` are no
    longer available. This function will do nothing if the `doc` is NULL.  */
json_api void json_mut_doc_free(json_mut_doc *doc);

/** Creates and returns a new mutable JSON document, returns NULL on error.
    If allocator is NULL, the default allocator will be used. */
json_api json_mut_doc *json_mut_doc_new(const json_alc *alc);

/** Copies and returns a new mutable document from input, returns NULL on error.
    This makes a `deep-copy` on the immutable document.
    If allocator is NULL, the default allocator will be used.
    @note `imut_doc` -> `mut_doc`. */
json_api json_mut_doc *json_doc_mut_copy(json_doc *doc,
                                               const json_alc *alc);

/** Copies and returns a new mutable document from input, returns NULL on error.
    This makes a `deep-copy` on the mutable document.
    If allocator is NULL, the default allocator will be used.
    @note `mut_doc` -> `mut_doc`. */
json_api json_mut_doc *json_mut_doc_mut_copy(json_mut_doc *doc,
                                                   const json_alc *alc);

/** Copies and returns a new mutable value from input, returns NULL on error.
    This makes a `deep-copy` on the immutable value.
    The memory was managed by mutable document.
    @note `imut_val` -> `mut_val`. */
json_api json_mut_val *json_val_mut_copy(json_mut_doc *doc,
                                               json_val *val);

/** Copies and returns a new mutable value from input, returns NULL on error.
    This makes a `deep-copy` on the mutable value.
    The memory was managed by mutable document.
    @note `mut_val` -> `mut_val`.
    @warning This function is recursive and may cause a stack overflow
        if the object level is too deep. */
json_api json_mut_val *json_mut_val_mut_copy(json_mut_doc *doc,
                                                   json_mut_val *val);

/** Copies and returns a new immutable document from input,
    returns NULL on error. This makes a `deep-copy` on the mutable document.
    The returned document should be freed with `json_doc_free()`.
    @note `mut_doc` -> `imut_doc`.
    @warning This function is recursive and may cause a stack overflow
        if the object level is too deep. */
json_api json_doc *json_mut_doc_imut_copy(json_mut_doc *doc,
                                                const json_alc *alc);

/** Copies and returns a new immutable document from input,
    returns NULL on error. This makes a `deep-copy` on the mutable value.
    The returned document should be freed with `json_doc_free()`.
    @note `mut_val` -> `imut_doc`.
    @warning This function is recursive and may cause a stack overflow
        if the object level is too deep. */
json_api json_doc *json_mut_val_imut_copy(json_mut_val *val,
                                                const json_alc *alc);



/*==============================================================================
 * Mutable JSON Value Type API
 *============================================================================*/

/** Returns whether the JSON value is raw.
    Returns false if `val` is NULL. */
json_api_inline bool json_mut_is_raw(json_mut_val *val);

/** Returns whether the JSON value is `null`.
    Returns false if `val` is NULL. */
json_api_inline bool json_mut_is_null(json_mut_val *val);

/** Returns whether the JSON value is `true`.
    Returns false if `val` is NULL. */
json_api_inline bool json_mut_is_true(json_mut_val *val);

/** Returns whether the JSON value is `false`.
    Returns false if `val` is NULL. */
json_api_inline bool json_mut_is_false(json_mut_val *val);

/** Returns whether the JSON value is bool (true/false).
    Returns false if `val` is NULL. */
json_api_inline bool json_mut_is_bool(json_mut_val *val);

/** Returns whether the JSON value is unsigned integer (uint64_t).
    Returns false if `val` is NULL. */
json_api_inline bool json_mut_is_uint(json_mut_val *val);

/** Returns whether the JSON value is signed integer (int64_t).
    Returns false if `val` is NULL. */
json_api_inline bool json_mut_is_sint(json_mut_val *val);

/** Returns whether the JSON value is integer (uint64_t/int64_t).
    Returns false if `val` is NULL. */
json_api_inline bool json_mut_is_int(json_mut_val *val);

/** Returns whether the JSON value is real number (double).
    Returns false if `val` is NULL. */
json_api_inline bool json_mut_is_real(json_mut_val *val);

/** Returns whether the JSON value is number (uint/sint/real).
    Returns false if `val` is NULL. */
json_api_inline bool json_mut_is_num(json_mut_val *val);

/** Returns whether the JSON value is string.
    Returns false if `val` is NULL. */
json_api_inline bool json_mut_is_str(json_mut_val *val);

/** Returns whether the JSON value is array.
    Returns false if `val` is NULL. */
json_api_inline bool json_mut_is_arr(json_mut_val *val);

/** Returns whether the JSON value is object.
    Returns false if `val` is NULL. */
json_api_inline bool json_mut_is_obj(json_mut_val *val);

/** Returns whether the JSON value is container (array/object).
    Returns false if `val` is NULL. */
json_api_inline bool json_mut_is_ctn(json_mut_val *val);



/*==============================================================================
 * Mutable JSON Value Content API
 *============================================================================*/

/** Returns the JSON value's type.
    Returns `JSON_TYPE_NONE` if `val` is NULL. */
json_api_inline json_type json_mut_get_type(json_mut_val *val);

/** Returns the JSON value's subtype.
    Returns `JSON_SUBTYPE_NONE` if `val` is NULL. */
json_api_inline json_subtype json_mut_get_subtype(json_mut_val *val);

/** Returns the JSON value's tag.
    Returns 0 if `val` is NULL. */
json_api_inline uint8_t json_mut_get_tag(json_mut_val *val);

/** Returns the JSON value's type description.
    The return value should be one of these strings: "raw", "null", "string",
    "array", "object", "true", "false", "uint", "sint", "real", "unknown". */
json_api_inline const char *json_mut_get_type_desc(json_mut_val *val);

/** Returns the content if the value is raw.
    Returns NULL if `val` is NULL or type is not raw. */
json_api_inline const char *json_mut_get_raw(json_mut_val *val);

/** Returns the content if the value is bool.
    Returns NULL if `val` is NULL or type is not bool. */
json_api_inline bool json_mut_get_bool(json_mut_val *val);

/** Returns the content and cast to uint64_t.
    Returns 0 if `val` is NULL or type is not integer(sint/uint). */
json_api_inline uint64_t json_mut_get_uint(json_mut_val *val);

/** Returns the content and cast to int64_t.
    Returns 0 if `val` is NULL or type is not integer(sint/uint). */
json_api_inline int64_t json_mut_get_sint(json_mut_val *val);

/** Returns the content and cast to int.
    Returns 0 if `val` is NULL or type is not integer(sint/uint). */
json_api_inline int json_mut_get_int(json_mut_val *val);

/** Returns the content if the value is real number.
    Returns 0.0 if `val` is NULL or type is not real(double). */
json_api_inline double json_mut_get_real(json_mut_val *val);

/** Returns the content and typecast to `double` if the value is number.
    Returns 0.0 if `val` is NULL or type is not number(uint/sint/real). */
json_api_inline double json_mut_get_num(json_mut_val *val);

/** Returns the content if the value is string.
    Returns NULL if `val` is NULL or type is not string. */
json_api_inline const char *json_mut_get_str(json_mut_val *val);

/** Returns the content length (string length, array size, object size.
    Returns 0 if `val` is NULL or type is not string/array/object. */
json_api_inline size_t json_mut_get_len(json_mut_val *val);

/** Returns whether the JSON value is equals to a string.
    The `str` should be a null-terminated UTF-8 string.
    Returns false if input is NULL or type is not string. */
json_api_inline bool json_mut_equals_str(json_mut_val *val,
                                             const char *str);

/** Returns whether the JSON value is equals to a string.
    The `str` should be a UTF-8 string, null-terminator is not required.
    Returns false if input is NULL or type is not string. */
json_api_inline bool json_mut_equals_strn(json_mut_val *val,
                                              const char *str, size_t len);

/** Returns whether two JSON values are equal (deep compare).
    Returns false if input is NULL.
    @note the result may be inaccurate if object has duplicate keys.
    @warning This function is recursive and may cause a stack overflow
        if the object level is too deep. */
json_api_inline bool json_mut_equals(json_mut_val *lhs,
                                         json_mut_val *rhs);

/** Set the value to raw.
    Returns false if input is NULL.
    @warning This function should not be used on an existing object or array. */
json_api_inline bool json_mut_set_raw(json_mut_val *val,
                                          const char *raw, size_t len);

/** Set the value to null.
    Returns false if input is NULL.
    @warning This function should not be used on an existing object or array. */
json_api_inline bool json_mut_set_null(json_mut_val *val);

/** Set the value to bool.
    Returns false if input is NULL.
    @warning This function should not be used on an existing object or array. */
json_api_inline bool json_mut_set_bool(json_mut_val *val, bool num);

/** Set the value to uint.
    Returns false if input is NULL.
    @warning This function should not be used on an existing object or array. */
json_api_inline bool json_mut_set_uint(json_mut_val *val, uint64_t num);

/** Set the value to sint.
    Returns false if input is NULL.
    @warning This function should not be used on an existing object or array. */
json_api_inline bool json_mut_set_sint(json_mut_val *val, int64_t num);

/** Set the value to int.
    Returns false if input is NULL.
    @warning This function should not be used on an existing object or array. */
json_api_inline bool json_mut_set_int(json_mut_val *val, int num);

/** Set the value to real.
    Returns false if input is NULL.
    @warning This function should not be used on an existing object or array. */
json_api_inline bool json_mut_set_real(json_mut_val *val, double num);

/** Set the value to string (null-terminated).
    Returns false if input is NULL.
    @warning This function should not be used on an existing object or array. */
json_api_inline bool json_mut_set_str(json_mut_val *val, const char *str);

/** Set the value to string (with length).
    Returns false if input is NULL.
    @warning This function should not be used on an existing object or array. */
json_api_inline bool json_mut_set_strn(json_mut_val *val,
                                           const char *str, size_t len);

/** Set the value to array.
    Returns false if input is NULL.
    @warning This function should not be used on an existing object or array. */
json_api_inline bool json_mut_set_arr(json_mut_val *val);

/** Set the value to array.
    Returns false if input is NULL.
    @warning This function should not be used on an existing object or array. */
json_api_inline bool json_mut_set_obj(json_mut_val *val);



/*==============================================================================
 * Mutable JSON Value Creation API
 *============================================================================*/

/** Creates and returns a raw value, returns NULL on error.
    The `str` should be a null-terminated UTF-8 string.
    
    @warning The input string is not copied, you should keep this string
        unmodified for the lifetime of this JSON document. */
json_api_inline json_mut_val *json_mut_raw(json_mut_doc *doc,
                                                 const char *str);

/** Creates and returns a raw value, returns NULL on error.
    The `str` should be a UTF-8 string, null-terminator is not required.
    
    @warning The input string is not copied, you should keep this string
        unmodified for the lifetime of this JSON document. */
json_api_inline json_mut_val *json_mut_rawn(json_mut_doc *doc,
                                                  const char *str,
                                                  size_t len);

/** Creates and returns a raw value, returns NULL on error.
    The `str` should be a null-terminated UTF-8 string.
    The input string is copied and held by the document. */
json_api_inline json_mut_val *json_mut_rawcpy(json_mut_doc *doc,
                                                    const char *str);

/** Creates and returns a raw value, returns NULL on error.
    The `str` should be a UTF-8 string, null-terminator is not required.
    The input string is copied and held by the document. */
json_api_inline json_mut_val *json_mut_rawncpy(json_mut_doc *doc,
                                                     const char *str,
                                                     size_t len);

/** Creates and returns a null value, returns NULL on error. */
json_api_inline json_mut_val *json_mut_null(json_mut_doc *doc);

/** Creates and returns a true value, returns NULL on error. */
json_api_inline json_mut_val *json_mut_true(json_mut_doc *doc);

/** Creates and returns a false value, returns NULL on error. */
json_api_inline json_mut_val *json_mut_false(json_mut_doc *doc);

/** Creates and returns a bool value, returns NULL on error. */
json_api_inline json_mut_val *json_mut_bool(json_mut_doc *doc,
                                                  bool val);

/** Creates and returns an unsigned integer value, returns NULL on error. */
json_api_inline json_mut_val *json_mut_uint(json_mut_doc *doc,
                                                  uint64_t num);

/** Creates and returns a signed integer value, returns NULL on error. */
json_api_inline json_mut_val *json_mut_sint(json_mut_doc *doc,
                                                  int64_t num);

/** Creates and returns a signed integer value, returns NULL on error. */
json_api_inline json_mut_val *json_mut_int(json_mut_doc *doc,
                                                 int64_t num);

/** Creates and returns an real number value, returns NULL on error. */
json_api_inline json_mut_val *json_mut_real(json_mut_doc *doc,
                                                  double num);

/** Creates and returns a string value, returns NULL on error.
    The `str` should be a null-terminated UTF-8 string.
    @warning The input string is not copied, you should keep this string
        unmodified for the lifetime of this JSON document. */
json_api_inline json_mut_val *json_mut_str(json_mut_doc *doc,
                                                 const char *str);

/** Creates and returns a string value, returns NULL on error.
    The `str` should be a UTF-8 string, null-terminator is not required.
    @warning The input string is not copied, you should keep this string
        unmodified for the lifetime of this JSON document. */
json_api_inline json_mut_val *json_mut_strn(json_mut_doc *doc,
                                                  const char *str,
                                                  size_t len);

/** Creates and returns a string value, returns NULL on error.
    The `str` should be a null-terminated UTF-8 string.
    The input string is copied and held by the document. */
json_api_inline json_mut_val *json_mut_strcpy(json_mut_doc *doc,
                                                    const char *str);

/** Creates and returns a string value, returns NULL on error.
    The `str` should be a UTF-8 string, null-terminator is not required.
    The input string is copied and held by the document. */
json_api_inline json_mut_val *json_mut_strncpy(json_mut_doc *doc,
                                                     const char *str,
                                                     size_t len);



/*==============================================================================
 * Mutable JSON Array API
 *============================================================================*/

/** Returns the number of elements in this array.
    Returns 0 if `arr` is NULL or type is not array. */
json_api_inline size_t json_mut_arr_size(json_mut_val *arr);

/** Returns the element at the specified position in this array.
    Returns NULL if array is NULL/empty or the index is out of bounds.
    @warning This function takes a linear search time. */
json_api_inline json_mut_val *json_mut_arr_get(json_mut_val *arr,
                                                     size_t idx);

/** Returns the first element of this array.
    Returns NULL if `arr` is NULL/empty or type is not array. */
json_api_inline json_mut_val *json_mut_arr_get_first(json_mut_val *arr);

/** Returns the last element of this array.
    Returns NULL if `arr` is NULL/empty or type is not array. */
json_api_inline json_mut_val *json_mut_arr_get_last(json_mut_val *arr);



/*==============================================================================
 * Mutable JSON Array Iterator API
 *============================================================================*/

/**
 A mutable JSON array iterator.
 
 @warning You should not modify the array while iterating over it, but you can
    use `json_mut_arr_iter_remove()` to remove current value.
 
 @par Example
 @code
    json_mut_val *val;
    json_mut_arr_iter iter = json_mut_arr_iter_with(arr);
    while ((val = json_mut_arr_iter_next(&iter))) {
        your_func(val);
        if (your_val_is_unused(val)) {
            json_mut_arr_iter_remove(&iter);
        }
    }
 @endcode
 */
typedef struct json_mut_arr_iter {
    size_t idx; /**< next value's index */
    size_t max; /**< maximum index (arr.size) */
    json_mut_val *cur; /**< current value */
    json_mut_val *pre; /**< previous value */
    json_mut_val *arr; /**< the array being iterated */
} json_mut_arr_iter;

/**
 Initialize an iterator for this array.
 
 @param arr The array to be iterated over.
    If this parameter is NULL or not an array, `iter` will be set to empty.
 @param iter The iterator to be initialized.
    If this parameter is NULL, the function will fail and return false.
 @return true if the `iter` has been successfully initialized.
 
 @note The iterator does not need to be destroyed.
 */
json_api_inline bool json_mut_arr_iter_init(json_mut_val *arr,
    json_mut_arr_iter *iter);

/**
 Create an iterator with an array , same as `json_mut_arr_iter_init()`.
 
 @param arr The array to be iterated over.
    If this parameter is NULL or not an array, an empty iterator will returned.
 @return A new iterator for the array.
 
 @note The iterator does not need to be destroyed.
 */
json_api_inline json_mut_arr_iter json_mut_arr_iter_with(
    json_mut_val *arr);

/**
 Returns whether the iteration has more elements.
 If `iter` is NULL, this function will return false.
 */
json_api_inline bool json_mut_arr_iter_has_next(
    json_mut_arr_iter *iter);

/**
 Returns the next element in the iteration, or NULL on end.
 If `iter` is NULL, this function will return NULL.
 */
json_api_inline json_mut_val *json_mut_arr_iter_next(
    json_mut_arr_iter *iter);

/**
 Removes and returns current element in the iteration.
 If `iter` is NULL, this function will return NULL.
 */
json_api_inline json_mut_val *json_mut_arr_iter_remove(
    json_mut_arr_iter *iter);

/**
 Macro for iterating over an array.
 It works like iterator, but with a more intuitive API.
 
 @warning You should not modify the array while iterating over it.
 
 @par Example
 @code
    size_t idx, max;
    json_mut_val *val;
    json_mut_arr_foreach(arr, idx, max, val) {
        your_func(idx, val);
    }
 @endcode
 */
#define json_mut_arr_foreach(arr, idx, max, val) \
    for ((idx) = 0, \
        (max) = json_mut_arr_size(arr), \
        (val) = json_mut_arr_get_first(arr); \
        (idx) < (max); \
        (idx)++, \
        (val) = (val)->next)



/*==============================================================================
 * Mutable JSON Array Creation API
 *============================================================================*/

/**
 Creates and returns an empty mutable array.
 @param doc A mutable document, used for memory allocation only.
 @return The new array. NULL if input is NULL or memory allocation failed.
 */
json_api_inline json_mut_val *json_mut_arr(json_mut_doc *doc);

/**
 Creates and returns a new mutable array with the given boolean values.
 
 @param doc A mutable document, used for memory allocation only.
    If this parameter is NULL, the function will fail and return NULL.
 @param vals A C array of boolean values.
 @param count The value count. If this value is 0, an empty array will return.
 @return The new array. NULL if input is invalid or memory allocation failed.
 
 @par Example
 @code
    const bool vals[3] = { true, false, true };
    json_mut_val *arr = json_mut_arr_with_bool(doc, vals, 3);
 @endcode
 */
json_api_inline json_mut_val *json_mut_arr_with_bool(
    json_mut_doc *doc, const bool *vals, size_t count);

/**
 Creates and returns a new mutable array with the given sint numbers.
 
 @param doc A mutable document, used for memory allocation only.
    If this parameter is NULL, the function will fail and return NULL.
 @param vals A C array of sint numbers.
 @param count The number count. If this value is 0, an empty array will return.
 @return The new array. NULL if input is invalid or memory allocation failed.
 
 @par Example
 @code
    const int64_t vals[3] = { -1, 0, 1 };
    json_mut_val *arr = json_mut_arr_with_sint64(doc, vals, 3);
 @endcode
 */
json_api_inline json_mut_val *json_mut_arr_with_sint(
    json_mut_doc *doc, const int64_t *vals, size_t count);

/**
 Creates and returns a new mutable array with the given uint numbers.
 
 @param doc A mutable document, used for memory allocation only.
    If this parameter is NULL, the function will fail and return NULL.
 @param vals A C array of uint numbers.
 @param count The number count. If this value is 0, an empty array will return.
 @return The new array. NULL if input is invalid or memory allocation failed.
 
 @par Example
 @code
    const uint64_t vals[3] = { 0, 1, 0 };
    json_mut_val *arr = json_mut_arr_with_uint(doc, vals, 3);
 @endcode
 */
json_api_inline json_mut_val *json_mut_arr_with_uint(
    json_mut_doc *doc, const uint64_t *vals, size_t count);

/**
 Creates and returns a new mutable array with the given real numbers.
 
 @param doc A mutable document, used for memory allocation only.
    If this parameter is NULL, the function will fail and return NULL.
 @param vals A C array of real numbers.
 @param count The number count. If this value is 0, an empty array will return.
 @return The new array. NULL if input is invalid or memory allocation failed.
 
 @par Example
 @code
    const double vals[3] = { 0.1, 0.2, 0.3 };
    json_mut_val *arr = json_mut_arr_with_real(doc, vals, 3);
 @endcode
 */
json_api_inline json_mut_val *json_mut_arr_with_real(
    json_mut_doc *doc, const double *vals, size_t count);

/**
 Creates and returns a new mutable array with the given int8 numbers.
 
 @param doc A mutable document, used for memory allocation only.
    If this parameter is NULL, the function will fail and return NULL.
 @param vals A C array of int8 numbers.
 @param count The number count. If this value is 0, an empty array will return.
 @return The new array. NULL if input is invalid or memory allocation failed.
 
 @par Example
 @code
    const int8_t vals[3] = { -1, 0, 1 };
    json_mut_val *arr = json_mut_arr_with_sint8(doc, vals, 3);
 @endcode
 */
json_api_inline json_mut_val *json_mut_arr_with_sint8(
    json_mut_doc *doc, const int8_t *vals, size_t count);

/**
 Creates and returns a new mutable array with the given int16 numbers.
 
 @param doc A mutable document, used for memory allocation only.
    If this parameter is NULL, the function will fail and return NULL.
 @param vals A C array of int16 numbers.
 @param count The number count. If this value is 0, an empty array will return.
 @return The new array. NULL if input is invalid or memory allocation failed.
 
 @par Example
 @code
    const int16_t vals[3] = { -1, 0, 1 };
    json_mut_val *arr = json_mut_arr_with_sint16(doc, vals, 3);
 @endcode
 */
json_api_inline json_mut_val *json_mut_arr_with_sint16(
    json_mut_doc *doc, const int16_t *vals, size_t count);

/**
 Creates and returns a new mutable array with the given int32 numbers.
 
 @param doc A mutable document, used for memory allocation only.
    If this parameter is NULL, the function will fail and return NULL.
 @param vals A C array of int32 numbers.
 @param count The number count. If this value is 0, an empty array will return.
 @return The new array. NULL if input is invalid or memory allocation failed.
 
 @par Example
 @code
    const int32_t vals[3] = { -1, 0, 1 };
    json_mut_val *arr = json_mut_arr_with_sint32(doc, vals, 3);
 @endcode
 */
json_api_inline json_mut_val *json_mut_arr_with_sint32(
    json_mut_doc *doc, const int32_t *vals, size_t count);

/**
 Creates and returns a new mutable array with the given int64 numbers.
 
 @param doc A mutable document, used for memory allocation only.
    If this parameter is NULL, the function will fail and return NULL.
 @param vals A C array of int64 numbers.
 @param count The number count. If this value is 0, an empty array will return.
 @return The new array. NULL if input is invalid or memory allocation failed.
 
 @par Example
 @code
    const int64_t vals[3] = { -1, 0, 1 };
    json_mut_val *arr = json_mut_arr_with_sint64(doc, vals, 3);
 @endcode
 */
json_api_inline json_mut_val *json_mut_arr_with_sint64(
    json_mut_doc *doc, const int64_t *vals, size_t count);

/**
 Creates and returns a new mutable array with the given uint8 numbers.
 
 @param doc A mutable document, used for memory allocation only.
    If this parameter is NULL, the function will fail and return NULL.
 @param vals A C array of uint8 numbers.
 @param count The number count. If this value is 0, an empty array will return.
 @return The new array. NULL if input is invalid or memory allocation failed.
 
 @par Example
 @code
    const uint8_t vals[3] = { 0, 1, 0 };
    json_mut_val *arr = json_mut_arr_with_uint8(doc, vals, 3);
 @endcode
 */
json_api_inline json_mut_val *json_mut_arr_with_uint8(
    json_mut_doc *doc, const uint8_t *vals, size_t count);

/**
 Creates and returns a new mutable array with the given uint16 numbers.
 
 @param doc A mutable document, used for memory allocation only.
    If this parameter is NULL, the function will fail and return NULL.
 @param vals A C array of uint16 numbers.
 @param count The number count. If this value is 0, an empty array will return.
 @return The new array. NULL if input is invalid or memory allocation failed.
 
 @par Example
 @code
    const uint16_t vals[3] = { 0, 1, 0 };
    json_mut_val *arr = json_mut_arr_with_uint16(doc, vals, 3);
 @endcode
 */
json_api_inline json_mut_val *json_mut_arr_with_uint16(
    json_mut_doc *doc, const uint16_t *vals, size_t count);

/**
 Creates and returns a new mutable array with the given uint32 numbers.
 
 @param doc A mutable document, used for memory allocation only.
    If this parameter is NULL, the function will fail and return NULL.
 @param vals A C array of uint32 numbers.
 @param count The number count. If this value is 0, an empty array will return.
 @return The new array. NULL if input is invalid or memory allocation failed.
 
 @par Example
 @code
    const uint32_t vals[3] = { 0, 1, 0 };
    json_mut_val *arr = json_mut_arr_with_uint32(doc, vals, 3);
 @endcode
 */
json_api_inline json_mut_val *json_mut_arr_with_uint32(
    json_mut_doc *doc, const uint32_t *vals, size_t count);

/**
 Creates and returns a new mutable array with the given uint64 numbers.
 
 @param doc A mutable document, used for memory allocation only.
    If this parameter is NULL, the function will fail and return NULL.
 @param vals A C array of uint64 numbers.
 @param count The number count. If this value is 0, an empty array will return.
 @return The new array. NULL if input is invalid or memory allocation failed.
 
 @par Example
 @code
     const uint64_t vals[3] = { 0, 1, 0 };
     json_mut_val *arr = json_mut_arr_with_uint64(doc, vals, 3);
 @endcode
 */
json_api_inline json_mut_val *json_mut_arr_with_uint64(
    json_mut_doc *doc, const uint64_t *vals, size_t count);

/**
 Creates and returns a new mutable array with the given float numbers.
 
 @param doc A mutable document, used for memory allocation only.
    If this parameter is NULL, the function will fail and return NULL.
 @param vals A C array of float numbers.
 @param count The number count. If this value is 0, an empty array will return.
 @return The new array. NULL if input is invalid or memory allocation failed.
 
 @par Example
 @code
    const float vals[3] = { -1.0f, 0.0f, 1.0f };
    json_mut_val *arr = json_mut_arr_with_float(doc, vals, 3);
 @endcode
 */
json_api_inline json_mut_val *json_mut_arr_with_float(
    json_mut_doc *doc, const float *vals, size_t count);

/**
 Creates and returns a new mutable array with the given double numbers.
 
 @param doc A mutable document, used for memory allocation only.
    If this parameter is NULL, the function will fail and return NULL.
 @param vals A C array of double numbers.
 @param count The number count. If this value is 0, an empty array will return.
 @return The new array. NULL if input is invalid or memory allocation failed.
 
 @par Example
 @code
    const double vals[3] = { -1.0, 0.0, 1.0 };
    json_mut_val *arr = json_mut_arr_with_double(doc, vals, 3);
 @endcode
 */
json_api_inline json_mut_val *json_mut_arr_with_double(
    json_mut_doc *doc, const double *vals, size_t count);

/**
 Creates and returns a new mutable array with the given strings, these strings
 will not be copied.
 
 @param doc A mutable document, used for memory allocation only.
    If this parameter is NULL, the function will fail and return NULL.
 @param vals A C array of UTF-8 null-terminator strings.
    If this array contains NULL, the function will fail and return NULL.
 @param count The number of values in `vals`.
    If this value is 0, an empty array will return.
 @return The new array. NULL if input is invalid or memory allocation failed.
 
 @warning The input strings are not copied, you should keep these strings
    unmodified for the lifetime of this JSON document. If these strings will be
    modified, you should use `json_mut_arr_with_strcpy()` instead.
 
 @par Example
 @code
    const char *vals[3] = { "a", "b", "c" };
    json_mut_val *arr = json_mut_arr_with_str(doc, vals, 3);
 @endcode
 */
json_api_inline json_mut_val *json_mut_arr_with_str(
    json_mut_doc *doc, const char **vals, size_t count);

/**
 Creates and returns a new mutable array with the given strings and string
 lengths, these strings will not be copied.
 
 @param doc A mutable document, used for memory allocation only.
    If this parameter is NULL, the function will fail and return NULL.
 @param vals A C array of UTF-8 strings, null-terminator is not required.
    If this array contains NULL, the function will fail and return NULL.
 @param lens A C array of string lengths, in bytes.
 @param count The number of strings in `vals`.
    If this value is 0, an empty array will return.
 @return The new array. NULL if input is invalid or memory allocation failed.
 
 @warning The input strings are not copied, you should keep these strings
    unmodified for the lifetime of this JSON document. If these strings will be
    modified, you should use `json_mut_arr_with_strncpy()` instead.
 
 @par Example
 @code
    const char *vals[3] = { "a", "bb", "c" };
    const size_t lens[3] = { 1, 2, 1 };
    json_mut_val *arr = json_mut_arr_with_strn(doc, vals, lens, 3);
 @endcode
 */
json_api_inline json_mut_val *json_mut_arr_with_strn(
    json_mut_doc *doc, const char **vals, const size_t *lens, size_t count);

/**
 Creates and returns a new mutable array with the given strings, these strings
 will be copied.
 
 @param doc A mutable document, used for memory allocation only.
    If this parameter is NULL, the function will fail and return NULL.
 @param vals A C array of UTF-8 null-terminator strings.
    If this array contains NULL, the function will fail and return NULL.
 @param count The number of values in `vals`.
    If this value is 0, an empty array will return.
 @return The new array. NULL if input is invalid or memory allocation failed.
 
 @par Example
 @code
    const char *vals[3] = { "a", "b", "c" };
    json_mut_val *arr = json_mut_arr_with_strcpy(doc, vals, 3);
 @endcode
 */
json_api_inline json_mut_val *json_mut_arr_with_strcpy(
    json_mut_doc *doc, const char **vals, size_t count);

/**
 Creates and returns a new mutable array with the given strings and string
 lengths, these strings will be copied.
 
 @param doc A mutable document, used for memory allocation only.
    If this parameter is NULL, the function will fail and return NULL.
 @param vals A C array of UTF-8 strings, null-terminator is not required.
    If this array contains NULL, the function will fail and return NULL.
 @param lens A C array of string lengths, in bytes.
 @param count The number of strings in `vals`.
    If this value is 0, an empty array will return.
 @return The new array. NULL if input is invalid or memory allocation failed.
 
 @par Example
 @code
    const char *vals[3] = { "a", "bb", "c" };
    const size_t lens[3] = { 1, 2, 1 };
    json_mut_val *arr = json_mut_arr_with_strn(doc, vals, lens, 3);
 @endcode
 */
json_api_inline json_mut_val *json_mut_arr_with_strncpy(
    json_mut_doc *doc, const char **vals, const size_t *lens, size_t count);



/*==============================================================================
 * Mutable JSON Array Modification API
 *============================================================================*/

/**
 Inserts a value into an array at a given index.
 @param arr The array to which the value is to be inserted.
    Returns false if it is NULL or not an array.
 @param val The value to be inserted. Returns false if it is NULL.
 @param idx The index to which to insert the new value.
    Returns false if the index is out of range.
 @return Whether successful.
 @warning This function takes a linear search time.
 */
json_api_inline bool json_mut_arr_insert(json_mut_val *arr,
                                             json_mut_val *val, size_t idx);

/**
 Inserts a value at the end of the array.
 @param arr The array to which the value is to be inserted.
    Returns false if it is NULL or not an array.
 @param val The value to be inserted. Returns false if it is NULL.
 @return Whether successful.
 */
json_api_inline bool json_mut_arr_append(json_mut_val *arr,
                                             json_mut_val *val);

/**
 Inserts a value at the head of the array.
 @param arr The array to which the value is to be inserted.
    Returns false if it is NULL or not an array.
 @param val The value to be inserted. Returns false if it is NULL.
 @return    Whether successful.
 */
json_api_inline bool json_mut_arr_prepend(json_mut_val *arr,
                                              json_mut_val *val);

/**
 Replaces a value at index and returns old value.
 @param arr The array to which the value is to be replaced.
    Returns false if it is NULL or not an array.
 @param idx The index to which to replace the value.
    Returns false if the index is out of range.
 @param val The new value to replace. Returns false if it is NULL.
 @return Old value, or NULL on error.
 @warning This function takes a linear search time.
 */
json_api_inline json_mut_val *json_mut_arr_replace(json_mut_val *arr,
                                                         size_t idx,
                                                         json_mut_val *val);

/**
 Removes and returns a value at index.
 @param arr The array from which the value is to be removed.
    Returns false if it is NULL or not an array.
 @param idx The index from which to remove the value.
    Returns false if the index is out of range.
 @return Old value, or NULL on error.
 @warning This function takes a linear search time.
 */
json_api_inline json_mut_val *json_mut_arr_remove(json_mut_val *arr,
                                                        size_t idx);

/**
 Removes and returns the first value in this array.
 @param arr The array from which the value is to be removed.
    Returns false if it is NULL or not an array.
 @return The first value, or NULL on error.
 */
json_api_inline json_mut_val *json_mut_arr_remove_first(
    json_mut_val *arr);

/**
 Removes and returns the last value in this array.
 @param arr The array from which the value is to be removed.
    Returns false if it is NULL or not an array.
 @return The last value, or NULL on error.
 */
json_api_inline json_mut_val *json_mut_arr_remove_last(
    json_mut_val *arr);

/**
 Removes all values within a specified range in the array.
 @param arr The array from which the value is to be removed.
    Returns false if it is NULL or not an array.
 @param idx The start index of the range (0 is the first).
 @param len The number of items in the range (can be 0).
 @return Whether successful.
 @warning This function takes a linear search time.
 */
json_api_inline bool json_mut_arr_remove_range(json_mut_val *arr,
                                                   size_t idx, size_t len);

/**
 Removes all values in this array.
 @param arr The array from which all of the values are to be removed.
    Returns false if it is NULL or not an array.
 @return Whether successful.
 */
json_api_inline bool json_mut_arr_clear(json_mut_val *arr);

/**
 Rotates values in this array for the given number of times.
 For example: `[1,2,3,4,5]` rotate 2 is `[3,4,5,1,2]`.
 @param arr The array to be rotated.
 @param idx Index (or times) to rotate.
 @warning This function takes a linear search time.
 */
json_api_inline bool json_mut_arr_rotate(json_mut_val *arr,
                                             size_t idx);



/*==============================================================================
 * Mutable JSON Array Modification Convenience API
 *============================================================================*/

/**
 Adds a value at the end of the array.
 @param arr The array to which the value is to be inserted.
    Returns false if it is NULL or not an array.
 @param val The value to be inserted. Returns false if it is NULL.
 @return Whether successful.
 */
json_api_inline bool json_mut_arr_add_val(json_mut_val *arr,
                                              json_mut_val *val);

/**
 Adds a `null` value at the end of the array.
 @param doc The `doc` is only used for memory allocation.
 @param arr The array to which the value is to be inserted.
    Returns false if it is NULL or not an array.
 @return Whether successful.
 */
json_api_inline bool json_mut_arr_add_null(json_mut_doc *doc,
                                               json_mut_val *arr);

/**
 Adds a `true` value at the end of the array.
 @param doc The `doc` is only used for memory allocation.
 @param arr The array to which the value is to be inserted.
    Returns false if it is NULL or not an array.
 @return Whether successful.
 */
json_api_inline bool json_mut_arr_add_true(json_mut_doc *doc,
                                               json_mut_val *arr);

/**
 Adds a `false` value at the end of the array.
 @param doc The `doc` is only used for memory allocation.
 @param arr The array to which the value is to be inserted.
    Returns false if it is NULL or not an array.
 @return Whether successful.
 */
json_api_inline bool json_mut_arr_add_false(json_mut_doc *doc,
                                                json_mut_val *arr);

/**
 Adds a bool value at the end of the array.
 @param doc The `doc` is only used for memory allocation.
 @param arr The array to which the value is to be inserted.
    Returns false if it is NULL or not an array.
 @param val The bool value to be added.
 @return Whether successful.
 */
json_api_inline bool json_mut_arr_add_bool(json_mut_doc *doc,
                                               json_mut_val *arr,
                                               bool val);

/**
 Adds an unsigned integer value at the end of the array.
 @param doc The `doc` is only used for memory allocation.
 @param arr The array to which the value is to be inserted.
    Returns false if it is NULL or not an array.
 @param num The number to be added.
 @return Whether successful.
 */
json_api_inline bool json_mut_arr_add_uint(json_mut_doc *doc,
                                               json_mut_val *arr,
                                               uint64_t num);

/**
 Adds a signed integer value at the end of the array.
 @param doc The `doc` is only used for memory allocation.
 @param arr The array to which the value is to be inserted.
    Returns false if it is NULL or not an array.
 @param num The number to be added.
 @return Whether successful.
 */
json_api_inline bool json_mut_arr_add_sint(json_mut_doc *doc,
                                               json_mut_val *arr,
                                               int64_t num);

/**
 Adds a integer value at the end of the array.
 @param doc The `doc` is only used for memory allocation.
 @param arr The array to which the value is to be inserted.
    Returns false if it is NULL or not an array.
 @param num The number to be added.
 @return Whether successful.
 */
json_api_inline bool json_mut_arr_add_int(json_mut_doc *doc,
                                              json_mut_val *arr,
                                              int64_t num);

/**
 Adds a double value at the end of the array.
 @param doc The `doc` is only used for memory allocation.
 @param arr The array to which the value is to be inserted.
    Returns false if it is NULL or not an array.
 @param num The number to be added.
 @return Whether successful.
 */
json_api_inline bool json_mut_arr_add_real(json_mut_doc *doc,
                                               json_mut_val *arr,
                                               double num);

/**
 Adds a string value at the end of the array (no copy).
 @param doc The `doc` is only used for memory allocation.
 @param arr The array to which the value is to be inserted.
    Returns false if it is NULL or not an array.
 @param str A null-terminated UTF-8 string.
 @return Whether successful.
 @warning The input string is not copied, you should keep this string unmodified
    for the lifetime of this JSON document.
 */
json_api_inline bool json_mut_arr_add_str(json_mut_doc *doc,
                                              json_mut_val *arr,
                                              const char *str);

/**
 Adds a string value at the end of the array (no copy).
 @param doc The `doc` is only used for memory allocation.
 @param arr The array to which the value is to be inserted.
    Returns false if it is NULL or not an array.
 @param str A UTF-8 string, null-terminator is not required.
 @param len The length of the string, in bytes.
 @return Whether successful.
 @warning The input string is not copied, you should keep this string unmodified
    for the lifetime of this JSON document.
 */
json_api_inline bool json_mut_arr_add_strn(json_mut_doc *doc,
                                               json_mut_val *arr,
                                               const char *str,
                                               size_t len);

/**
 Adds a string value at the end of the array (copied).
 @param doc The `doc` is only used for memory allocation.
 @param arr The array to which the value is to be inserted.
    Returns false if it is NULL or not an array.
 @param str A null-terminated UTF-8 string.
 @return Whether successful.
 */
json_api_inline bool json_mut_arr_add_strcpy(json_mut_doc *doc,
                                                 json_mut_val *arr,
                                                 const char *str);

/**
 Adds a string value at the end of the array (copied).
 @param doc The `doc` is only used for memory allocation.
 @param arr The array to which the value is to be inserted.
    Returns false if it is NULL or not an array.
 @param str A UTF-8 string, null-terminator is not required.
 @param len The length of the string, in bytes.
 @return Whether successful.
 */
json_api_inline bool json_mut_arr_add_strncpy(json_mut_doc *doc,
                                                  json_mut_val *arr,
                                                  const char *str,
                                                  size_t len);

/**
 Creates and adds a new array at the end of the array.
 @param doc The `doc` is only used for memory allocation.
 @param arr The array to which the value is to be inserted.
    Returns false if it is NULL or not an array.
 @return The new array, or NULL on error.
 */
json_api_inline json_mut_val *json_mut_arr_add_arr(json_mut_doc *doc,
                                                         json_mut_val *arr);

/**
 Creates and adds a new object at the end of the array.
 @param doc The `doc` is only used for memory allocation.
 @param arr The array to which the value is to be inserted.
    Returns false if it is NULL or not an array.
 @return The new object, or NULL on error.
 */
json_api_inline json_mut_val *json_mut_arr_add_obj(json_mut_doc *doc,
                                                         json_mut_val *arr);



/*==============================================================================
 * Mutable JSON Object API
 *============================================================================*/

/** Returns the number of key-value pairs in this object.
    Returns 0 if `obj` is NULL or type is not object. */
json_api_inline size_t json_mut_obj_size(json_mut_val *obj);

/** Returns the value to which the specified key is mapped.
    Returns NULL if this object contains no mapping for the key.
    Returns NULL if `obj/key` is NULL, or type is not object.
    
    The `key` should be a null-terminated UTF-8 string.
    
    @warning This function takes a linear search time. */
json_api_inline json_mut_val *json_mut_obj_get(json_mut_val *obj,
                                                     const char *key);

/** Returns the value to which the specified key is mapped.
    Returns NULL if this object contains no mapping for the key.
    Returns NULL if `obj/key` is NULL, or type is not object.
    
    The `key` should be a UTF-8 string, null-terminator is not required.
    The `key_len` should be the length of the key, in bytes.
    
    @warning This function takes a linear search time. */
json_api_inline json_mut_val *json_mut_obj_getn(json_mut_val *obj,
                                                      const char *key,
                                                      size_t key_len);



/*==============================================================================
 * Mutable JSON Object Iterator API
 *============================================================================*/

/**
 A mutable JSON object iterator.
 
 @warning You should not modify the object while iterating over it, but you can
    use `json_mut_obj_iter_remove()` to remove current value.
 
 @par Example
 @code
    json_mut_val *key, *val;
    json_mut_obj_iter iter = json_mut_obj_iter_with(obj);
    while ((key = json_mut_obj_iter_next(&iter))) {
        val = json_mut_obj_iter_get_val(key);
        your_func(key, val);
        if (your_val_is_unused(key, val)) {
            json_mut_obj_iter_remove(&iter);
        }
    }
 @endcode
 
 If the ordering of the keys is known at compile-time, you can use this method
 to speed up value lookups:
 @code
    // {"k1":1, "k2": 3, "k3": 3}
    json_mut_val *key, *val;
    json_mut_obj_iter iter = json_mut_obj_iter_with(obj);
    json_mut_val *v1 = json_mut_obj_iter_get(&iter, "k1");
    json_mut_val *v3 = json_mut_obj_iter_get(&iter, "k3");
 @endcode
 @see `json_mut_obj_iter_get()` and `json_mut_obj_iter_getn()`
 */
typedef struct json_mut_obj_iter {
    size_t idx; /**< next key's index */
    size_t max; /**< maximum key index (obj.size) */
    json_mut_val *cur; /**< current key */
    json_mut_val *pre; /**< previous key */
    json_mut_val *obj; /**< the object being iterated */
} json_mut_obj_iter;

/**
 Initialize an iterator for this object.
 
 @param obj The object to be iterated over.
    If this parameter is NULL or not an array, `iter` will be set to empty.
 @param iter The iterator to be initialized.
    If this parameter is NULL, the function will fail and return false.
 @return true if the `iter` has been successfully initialized.
 
 @note The iterator does not need to be destroyed.
 */
json_api_inline bool json_mut_obj_iter_init(json_mut_val *obj,
    json_mut_obj_iter *iter);

/**
 Create an iterator with an object, same as `json_obj_iter_init()`.
 
 @param obj The object to be iterated over.
    If this parameter is NULL or not an object, an empty iterator will returned.
 @return A new iterator for the object.
 
 @note The iterator does not need to be destroyed.
 */
json_api_inline json_mut_obj_iter json_mut_obj_iter_with(
    json_mut_val *obj);

/**
 Returns whether the iteration has more elements.
 If `iter` is NULL, this function will return false.
 */
json_api_inline bool json_mut_obj_iter_has_next(
    json_mut_obj_iter *iter);

/**
 Returns the next key in the iteration, or NULL on end.
 If `iter` is NULL, this function will return NULL.
 */
json_api_inline json_mut_val *json_mut_obj_iter_next(
    json_mut_obj_iter *iter);

/**
 Returns the value for key inside the iteration.
 If `iter` is NULL, this function will return NULL.
 */
json_api_inline json_mut_val *json_mut_obj_iter_get_val(
    json_mut_val *key);

/**
 Removes current key-value pair in the iteration, returns the removed value.
 If `iter` is NULL, this function will return NULL.
 */
json_api_inline json_mut_val *json_mut_obj_iter_remove(
    json_mut_obj_iter *iter);

/**
 Iterates to a specified key and returns the value.
 
 This function does the same thing as `json_mut_obj_get()`, but is much faster
 if the ordering of the keys is known at compile-time and you are using the same
 order to look up the values. If the key exists in this object, then the
 iterator will stop at the next key, otherwise the iterator will not change and
 NULL is returned.
 
 @param iter The object iterator, should not be NULL.
 @param key The key, should be a UTF-8 string with null-terminator.
 @return The value to which the specified key is mapped.
    NULL if this object contains no mapping for the key or input is invalid.
 
 @warning This function takes a linear search time if the key is not nearby.
 */
json_api_inline json_mut_val *json_mut_obj_iter_get(
    json_mut_obj_iter *iter, const char *key);

/**
 Iterates to a specified key and returns the value.
 
 This function does the same thing as `json_mut_obj_getn()` but is much faster
 if the ordering of the keys is known at compile-time and you are using the same
 order to look up the values. If the key exists in this object, then the
 iterator will stop at the next key, otherwise the iterator will not change and
 NULL is returned.
 
 @param iter The object iterator, should not be NULL.
 @param key The key, should be a UTF-8 string, null-terminator is not required.
 @param key_len The the length of `key`, in bytes.
 @return The value to which the specified key is mapped.
    NULL if this object contains no mapping for the key or input is invalid.
 
 @warning This function takes a linear search time if the key is not nearby.
 */
json_api_inline json_mut_val *json_mut_obj_iter_getn(
    json_mut_obj_iter *iter, const char *key, size_t key_len);

/**
 Macro for iterating over an object.
 It works like iterator, but with a more intuitive API.
 
 @warning You should not modify the object while iterating over it.
 
 @par Example
 @code
    size_t idx, max;
    json_val *key, *val;
    json_obj_foreach(obj, idx, max, key, val) {
        your_func(key, val);
    }
 @endcode
 */
#define json_mut_obj_foreach(obj, idx, max, key, val) \
    for ((idx) = 0, \
        (max) = json_mut_obj_size(obj), \
        (key) = (max) ? ((json_mut_val *)(obj)->uni.ptr)->next->next : NULL, \
        (val) = (key) ? (key)->next : NULL; \
        (idx) < (max); \
        (idx)++, \
        (key) = (val)->next, \
        (val) = (key)->next)



/*==============================================================================
 * Mutable JSON Object Creation API
 *============================================================================*/

/** Creates and returns a mutable object, returns NULL on error. */
json_api_inline json_mut_val *json_mut_obj(json_mut_doc *doc);

/**
 Creates and returns a mutable object with keys and values, returns NULL on
 error. The keys and values are not copied. The strings should be a
 null-terminated UTF-8 string.
 
 @warning The input string is not copied, you should keep this string
    unmodified for the lifetime of this JSON document.
 
 @par Example
 @code
    const char *keys[2] = { "id", "name" };
    const char *vals[2] = { "01", "Harry" };
    json_mut_val *obj = json_mut_obj_with_str(doc, keys, vals, 2);
 @endcode
 */
json_api_inline json_mut_val *json_mut_obj_with_str(json_mut_doc *doc,
                                                          const char **keys,
                                                          const char **vals,
                                                          size_t count);

/**
 Creates and returns a mutable object with key-value pairs and pair count,
 returns NULL on error. The keys and values are not copied. The strings should
 be a null-terminated UTF-8 string.
 
 @warning The input string is not copied, you should keep this string
    unmodified for the lifetime of this JSON document.
 
 @par Example
 @code
    const char *kv_pairs[4] = { "id", "01", "name", "Harry" };
    json_mut_val *obj = json_mut_obj_with_kv(doc, kv_pairs, 2);
 @endcode
 */
json_api_inline json_mut_val *json_mut_obj_with_kv(json_mut_doc *doc,
                                                         const char **kv_pairs,
                                                         size_t pair_count);



/*==============================================================================
 * Mutable JSON Object Modification API
 *============================================================================*/

/**
 Adds a key-value pair at the end of the object.
 This function allows duplicated key in one object.
 @param obj The object to which the new key-value pair is to be added.
 @param key The key, should be a string which is created by `json_mut_str()`,
    `json_mut_strn()`, `json_mut_strcpy()` or `json_mut_strncpy()`.
 @param val The value to add to the object.
 @return Whether successful.
 */
json_api_inline bool json_mut_obj_add(json_mut_val *obj,
                                          json_mut_val *key,
                                          json_mut_val *val);
/**
 Sets a key-value pair at the end of the object.
 This function may remove all key-value pairs for the given key before add.
 @param obj The object to which the new key-value pair is to be added.
 @param key The key, should be a string which is created by `json_mut_str()`,
    `json_mut_strn()`, `json_mut_strcpy()` or `json_mut_strncpy()`.
 @param val The value to add to the object. If this value is null, the behavior
    is same as `json_mut_obj_remove()`.
 @return Whether successful.
 */
json_api_inline bool json_mut_obj_put(json_mut_val *obj,
                                          json_mut_val *key,
                                          json_mut_val *val);

/**
 Inserts a key-value pair to the object at the given position.
 This function allows duplicated key in one object.
 @param obj The object to which the new key-value pair is to be added.
 @param key The key, should be a string which is created by `json_mut_str()`,
    `json_mut_strn()`, `json_mut_strcpy()` or `json_mut_strncpy()`.
 @param val The value to add to the object.
 @param idx The index to which to insert the new pair.
 @return Whether successful.
 */
json_api_inline bool json_mut_obj_insert(json_mut_val *obj,
                                             json_mut_val *key,
                                             json_mut_val *val,
                                             size_t idx);

/**
 Removes all key-value pair from the object with given key.
 @param obj The object from which the key-value pair is to be removed.
 @param key The key, should be a string value.
 @return The first matched value, or NULL if no matched value.
 @warning This function takes a linear search time.
 */
json_api_inline json_mut_val *json_mut_obj_remove(json_mut_val *obj,
                                                        json_mut_val *key);

/**
 Removes all key-value pair from the object with given key.
 @param obj The object from which the key-value pair is to be removed.
 @param key The key, should be a UTF-8 string with null-terminator.
 @return The first matched value, or NULL if no matched value.
 @warning This function takes a linear search time.
 */
json_api_inline json_mut_val *json_mut_obj_remove_key(
    json_mut_val *obj, const char *key);

/**
 Removes all key-value pair from the object with given key.
 @param obj The object from which the key-value pair is to be removed.
 @param key The key, should be a UTF-8 string, null-terminator is not required.
 @param key_len The length of the key.
 @return The first matched value, or NULL if no matched value.
 @warning This function takes a linear search time.
 */
json_api_inline json_mut_val *json_mut_obj_remove_keyn(
    json_mut_val *obj, const char *key, size_t key_len);

/**
 Removes all key-value pairs in this object.
 @param obj The object from which all of the values are to be removed.
 @return Whether successful.
 */
json_api_inline bool json_mut_obj_clear(json_mut_val *obj);

/**
 Replaces value from the object with given key.
 If the key is not exist, or the value is NULL, it will fail.
 @param obj The object to which the value is to be replaced.
 @param key The key, should be a string value.
 @param val The value to replace into the object.
 @return Whether successful.
 @warning This function takes a linear search time.
 */
json_api_inline bool json_mut_obj_replace(json_mut_val *obj,
                                              json_mut_val *key,
                                              json_mut_val *val);

/**
 Rotates key-value pairs in the object for the given number of times.
 For example: `{"a":1,"b":2,"c":3,"d":4}` rotate 1 is
 `{"b":2,"c":3,"d":4,"a":1}`.
 @param obj The object to be rotated.
 @param idx Index (or times) to rotate.
 @return Whether successful.
 @warning This function takes a linear search time.
 */
json_api_inline bool json_mut_obj_rotate(json_mut_val *obj,
                                             size_t idx);



/*==============================================================================
 * Mutable JSON Object Modification Convenience API
 *============================================================================*/

/** Adds a `null` value at the end of the object.
    The `key` should be a null-terminated UTF-8 string.
    This function allows duplicated key in one object.
    
    @warning The key string is not copied, you should keep the string
        unmodified for the lifetime of this JSON document. */
json_api_inline bool json_mut_obj_add_null(json_mut_doc *doc,
                                               json_mut_val *obj,
                                               const char *key);

/** Adds a `true` value at the end of the object.
    The `key` should be a null-terminated UTF-8 string.
    This function allows duplicated key in one object.
    
    @warning The key string is not copied, you should keep the string
        unmodified for the lifetime of this JSON document. */
json_api_inline bool json_mut_obj_add_true(json_mut_doc *doc,
                                               json_mut_val *obj,
                                               const char *key);

/** Adds a `false` value at the end of the object.
    The `key` should be a null-terminated UTF-8 string.
    This function allows duplicated key in one object.
    
    @warning The key string is not copied, you should keep the string
        unmodified for the lifetime of this JSON document. */
json_api_inline bool json_mut_obj_add_false(json_mut_doc *doc,
                                                json_mut_val *obj,
                                                const char *key);

/** Adds a bool value at the end of the object.
    The `key` should be a null-terminated UTF-8 string.
    This function allows duplicated key in one object.
    
    @warning The key string is not copied, you should keep the string
        unmodified for the lifetime of this JSON document. */
json_api_inline bool json_mut_obj_add_bool(json_mut_doc *doc,
                                               json_mut_val *obj,
                                               const char *key, bool val);

/** Adds an unsigned integer value at the end of the object.
    The `key` should be a null-terminated UTF-8 string.
    This function allows duplicated key in one object.
    
    @warning The key string is not copied, you should keep the string
        unmodified for the lifetime of this JSON document. */
json_api_inline bool json_mut_obj_add_uint(json_mut_doc *doc,
                                               json_mut_val *obj,
                                               const char *key, uint64_t val);

/** Adds a signed integer value at the end of the object.
    The `key` should be a null-terminated UTF-8 string.
    This function allows duplicated key in one object.
    
    @warning The key string is not copied, you should keep the string
        unmodified for the lifetime of this JSON document. */
json_api_inline bool json_mut_obj_add_sint(json_mut_doc *doc,
                                               json_mut_val *obj,
                                               const char *key, int64_t val);

/** Adds an int value at the end of the object.
    The `key` should be a null-terminated UTF-8 string.
    This function allows duplicated key in one object.
    
    @warning The key string is not copied, you should keep the string
        unmodified for the lifetime of this JSON document. */
json_api_inline bool json_mut_obj_add_int(json_mut_doc *doc,
                                              json_mut_val *obj,
                                              const char *key, int64_t val);

/** Adds a double value at the end of the object.
    The `key` should be a null-terminated UTF-8 string.
    This function allows duplicated key in one object.
    
    @warning The key string is not copied, you should keep the string
        unmodified for the lifetime of this JSON document. */
json_api_inline bool json_mut_obj_add_real(json_mut_doc *doc,
                                               json_mut_val *obj,
                                               const char *key, double val);

/** Adds a string value at the end of the object.
    The `key` and `val` should be null-terminated UTF-8 strings.
    This function allows duplicated key in one object.
    
    @warning The key/value strings are not copied, you should keep these strings
        unmodified for the lifetime of this JSON document. */
json_api_inline bool json_mut_obj_add_str(json_mut_doc *doc,
                                              json_mut_val *obj,
                                              const char *key, const char *val);

/** Adds a string value at the end of the object.
    The `key` should be a null-terminated UTF-8 string.
    The `val` should be a UTF-8 string, null-terminator is not required.
    The `len` should be the length of the `val`, in bytes.
    This function allows duplicated key in one object.
    
    @warning The key/value strings are not copied, you should keep these strings
        unmodified for the lifetime of this JSON document. */
json_api_inline bool json_mut_obj_add_strn(json_mut_doc *doc,
                                               json_mut_val *obj,
                                               const char *key,
                                               const char *val, size_t len);

/** Adds a string value at the end of the object.
    The `key` and `val` should be null-terminated UTF-8 strings.
    The value string is copied.
    This function allows duplicated key in one object.
    
    @warning The key string is not copied, you should keep the string
        unmodified for the lifetime of this JSON document. */
json_api_inline bool json_mut_obj_add_strcpy(json_mut_doc *doc,
                                                 json_mut_val *obj,
                                                 const char *key,
                                                 const char *val);

/** Adds a string value at the end of the object.
    The `key` should be a null-terminated UTF-8 string.
    The `val` should be a UTF-8 string, null-terminator is not required.
    The `len` should be the length of the `val`, in bytes.
    This function allows duplicated key in one object.
    
    @warning The key strings are not copied, you should keep these strings
        unmodified for the lifetime of this JSON document. */
json_api_inline bool json_mut_obj_add_strncpy(json_mut_doc *doc,
                                                  json_mut_val *obj,
                                                  const char *key,
                                                  const char *val, size_t len);

/**
 Creates and adds a new array to the target object.
 The `key` should be a null-terminated UTF-8 string.
 This function allows duplicated key in one object.
 
 @warning The key string is not copied, you should keep these strings
          unmodified for the lifetime of this JSON document.
 @return The new array, or NULL on error.
 */
json_api_inline json_mut_val *json_mut_obj_add_arr(json_mut_doc *doc,
                                                         json_mut_val *obj,
                                                         const char *key);

/**
 Creates and adds a new object to the target object.
 The `key` should be a null-terminated UTF-8 string.
 This function allows duplicated key in one object.
 
 @warning The key string is not copied, you should keep these strings
          unmodified for the lifetime of this JSON document.
 @return The new object, or NULL on error.
 */
json_api_inline json_mut_val *json_mut_obj_add_obj(json_mut_doc *doc,
                                                         json_mut_val *obj,
                                                         const char *key);

/** Adds a JSON value at the end of the object.
    The `key` should be a null-terminated UTF-8 string.
    This function allows duplicated key in one object.
    
    @warning The key string is not copied, you should keep the string
        unmodified for the lifetime of this JSON document. */
json_api_inline bool json_mut_obj_add_val(json_mut_doc *doc,
                                              json_mut_val *obj,
                                              const char *key,
                                              json_mut_val *val);

/** Removes all key-value pairs for the given key.
    Returns the first value to which the specified key is mapped or NULL if this
    object contains no mapping for the key.
    The `key` should be a null-terminated UTF-8 string.
    
    @warning This function takes a linear search time. */
json_api_inline json_mut_val *json_mut_obj_remove_str(
    json_mut_val *obj, const char *key);

/** Removes all key-value pairs for the given key.
    Returns the first value to which the specified key is mapped or NULL if this
    object contains no mapping for the key.
    The `key` should be a UTF-8 string, null-terminator is not required.
    The `len` should be the length of the key, in bytes.
    
    @warning This function takes a linear search time. */
json_api_inline json_mut_val *json_mut_obj_remove_strn(
    json_mut_val *obj, const char *key, size_t len);

/** Replaces all matching keys with the new key.
    Returns true if at least one key was renamed.
    The `key` and `new_key` should be a null-terminated UTF-8 string.
    The `new_key` is copied and held by doc.
    
    @warning This function takes a linear search time.
    If `new_key` already exists, it will cause duplicate keys.
 */
json_api_inline bool json_mut_obj_rename_key(json_mut_doc *doc,
                                                 json_mut_val *obj,
                                                 const char *key,
                                                 const char *new_key);

/** Replaces all matching keys with the new key.
    Returns true if at least one key was renamed.
    The `key` and `new_key` should be a UTF-8 string,
    null-terminator is not required. The `new_key` is copied and held by doc.
    
    @warning This function takes a linear search time.
    If `new_key` already exists, it will cause duplicate keys.
 */
json_api_inline bool json_mut_obj_rename_keyn(json_mut_doc *doc,
                                                  json_mut_val *obj,
                                                  const char *key,
                                                  size_t len,
                                                  const char *new_key,
                                                  size_t new_len);



#if !defined(JSON_DISABLE_UTILS) || !JSON_DISABLE_UTILS

/*==============================================================================
 * JSON Pointer API (RFC 6901)
 * https://tools.ietf.org/html/rfc6901
 *============================================================================*/

/** JSON Pointer error code. */
typedef uint32_t json_ptr_code;

/** No JSON pointer error. */
static const json_ptr_code JSON_PTR_ERR_NONE = 0;

/** Invalid input parameter, such as NULL input. */
static const json_ptr_code JSON_PTR_ERR_PARAMETER = 1;

/** JSON pointer syntax error, such as invalid escape, token no prefix. */
static const json_ptr_code JSON_PTR_ERR_SYNTAX = 2;

/** JSON pointer resolve failed, such as index out of range, key not found. */
static const json_ptr_code JSON_PTR_ERR_RESOLVE = 3;

/** Document's root is NULL, but it is required for the function call. */
static const json_ptr_code JSON_PTR_ERR_NULL_ROOT = 4;

/** Cannot set root as the target is not a document. */
static const json_ptr_code JSON_PTR_ERR_SET_ROOT = 5;

/** The memory allocation failed and a new value could not be created. */
static const json_ptr_code JSON_PTR_ERR_MEMORY_ALLOCATION = 6;

/** Error information for JSON pointer. */
typedef struct json_ptr_err {
    /** Error code, see `json_ptr_code` for all possible values. */
    json_ptr_code code;
    /** Error message, constant, no need to free (NULL if no error). */
    const char *msg;
    /** Error byte position for input JSON pointer (0 if no error). */
    size_t pos;
} json_ptr_err;

/**
 A context for JSON pointer operation.
 
 This struct stores the context of JSON Pointer operation result. The struct
 can be used with three helper functions: `ctx_append()`, `ctx_replace()`, and
 `ctx_remove()`, which perform the corresponding operations on the container
 without re-parsing the JSON Pointer.
 
 For example:
 @code
    // doc before: {"a":[0,1,null]}
    // ptr: "/a/2"
    val = json_mut_doc_ptr_getx(doc, ptr, strlen(ptr), &ctx, &err);
    if (json_is_null(val)) {
        json_ptr_ctx_remove(&ctx);
    }
    // doc after: {"a":[0,1]}
 @endcode
 */
typedef struct json_ptr_ctx {
    /**
     The container (parent) of the target value. It can be either an array or
     an object. If the target location has no value, but all its parent
     containers exist, and the target location can be used to insert a new
     value, then `ctn` is the parent container of the target location.
     Otherwise, `ctn` is NULL.
     */
    json_mut_val *ctn;
    /**
     The previous sibling of the target value. It can be either a value in an
     array or a key in an object. As the container is a `circular linked list`
     of elements, `pre` is the previous node of the target value. If the
     operation is `add` or `set`, then `pre` is the previous node of the new
     value, not the original target value. If the target value does not exist,
     `pre` is NULL.
     */
    json_mut_val *pre;
    /**
     The removed value if the operation is `set`, `replace` or `remove`. It can
     be used to restore the original state of the document if needed.
     */
    json_mut_val *old;
} json_ptr_ctx;

/**
 Get value by a JSON Pointer.
 @param doc The JSON document to be queried.
 @param ptr The JSON pointer string (UTF-8 with null-terminator).
 @return The value referenced by the JSON pointer.
    NULL if `doc` or `ptr` is NULL, or the JSON pointer cannot be resolved.
 */
json_api_inline json_val *json_doc_ptr_get(json_doc *doc,
                                                 const char *ptr);

/**
 Get value by a JSON Pointer.
 @param doc The JSON document to be queried.
 @param ptr The JSON pointer string (UTF-8, null-terminator is not required).
 @param len The length of `ptr` in bytes.
 @return The value referenced by the JSON pointer.
    NULL if `doc` or `ptr` is NULL, or the JSON pointer cannot be resolved.
 */
json_api_inline json_val *json_doc_ptr_getn(json_doc *doc,
                                                  const char *ptr, size_t len);

/**
 Get value by a JSON Pointer.
 @param doc The JSON document to be queried.
 @param ptr The JSON pointer string (UTF-8, null-terminator is not required).
 @param len The length of `ptr` in bytes.
 @param err A pointer to store the error information, or NULL if not needed.
 @return The value referenced by the JSON pointer.
    NULL if `doc` or `ptr` is NULL, or the JSON pointer cannot be resolved.
 */
json_api_inline json_val *json_doc_ptr_getx(json_doc *doc,
                                                  const char *ptr, size_t len,
                                                  json_ptr_err *err);

/**
 Get value by a JSON Pointer.
 @param val The JSON value to be queried.
 @param ptr The JSON pointer string (UTF-8 with null-terminator).
 @return The value referenced by the JSON pointer.
    NULL if `val` or `ptr` is NULL, or the JSON pointer cannot be resolved.
 */
json_api_inline json_val *json_ptr_get(json_val *val,
                                             const char *ptr);

/**
 Get value by a JSON Pointer.
 @param val The JSON value to be queried.
 @param ptr The JSON pointer string (UTF-8, null-terminator is not required).
 @param len The length of `ptr` in bytes.
 @return The value referenced by the JSON pointer.
    NULL if `val` or `ptr` is NULL, or the JSON pointer cannot be resolved.
 */
json_api_inline json_val *json_ptr_getn(json_val *val,
                                              const char *ptr, size_t len);

/**
 Get value by a JSON Pointer.
 @param val The JSON value to be queried.
 @param ptr The JSON pointer string (UTF-8, null-terminator is not required).
 @param len The length of `ptr` in bytes.
 @param err A pointer to store the error information, or NULL if not needed.
 @return The value referenced by the JSON pointer.
    NULL if `val` or `ptr` is NULL, or the JSON pointer cannot be resolved.
 */
json_api_inline json_val *json_ptr_getx(json_val *val,
                                              const char *ptr, size_t len,
                                              json_ptr_err *err);

/**
 Get value by a JSON Pointer.
 @param doc The JSON document to be queried.
 @param ptr The JSON pointer string (UTF-8 with null-terminator).
 @return The value referenced by the JSON pointer.
    NULL if `doc` or `ptr` is NULL, or the JSON pointer cannot be resolved.
 */
json_api_inline json_mut_val *json_mut_doc_ptr_get(json_mut_doc *doc,
                                                         const char *ptr);

/**
 Get value by a JSON Pointer.
 @param doc The JSON document to be queried.
 @param ptr The JSON pointer string (UTF-8, null-terminator is not required).
 @param len The length of `ptr` in bytes.
 @return The value referenced by the JSON pointer.
    NULL if `doc` or `ptr` is NULL, or the JSON pointer cannot be resolved.
 */
json_api_inline json_mut_val *json_mut_doc_ptr_getn(json_mut_doc *doc,
                                                          const char *ptr,
                                                          size_t len);

/**
 Get value by a JSON Pointer.
 @param doc The JSON document to be queried.
 @param ptr The JSON pointer string (UTF-8, null-terminator is not required).
 @param len The length of `ptr` in bytes.
 @param ctx A pointer to store the result context, or NULL if not needed.
 @param err A pointer to store the error information, or NULL if not needed.
 @return The value referenced by the JSON pointer.
    NULL if `doc` or `ptr` is NULL, or the JSON pointer cannot be resolved.
 */
json_api_inline json_mut_val *json_mut_doc_ptr_getx(json_mut_doc *doc,
                                                          const char *ptr,
                                                          size_t len,
                                                          json_ptr_ctx *ctx,
                                                          json_ptr_err *err);

/**
 Get value by a JSON Pointer.
 @param val The JSON value to be queried.
 @param ptr The JSON pointer string (UTF-8 with null-terminator).
 @return The value referenced by the JSON pointer.
    NULL if `val` or `ptr` is NULL, or the JSON pointer cannot be resolved.
 */
json_api_inline json_mut_val *json_mut_ptr_get(json_mut_val *val,
                                                     const char *ptr);

/**
 Get value by a JSON Pointer.
 @param val The JSON value to be queried.
 @param ptr The JSON pointer string (UTF-8, null-terminator is not required).
 @param len The length of `ptr` in bytes.
 @return The value referenced by the JSON pointer.
    NULL if `val` or `ptr` is NULL, or the JSON pointer cannot be resolved.
 */
json_api_inline json_mut_val *json_mut_ptr_getn(json_mut_val *val,
                                                      const char *ptr,
                                                      size_t len);

/**
 Get value by a JSON Pointer.
 @param val The JSON value to be queried.
 @param ptr The JSON pointer string (UTF-8, null-terminator is not required).
 @param len The length of `ptr` in bytes.
 @param ctx A pointer to store the result context, or NULL if not needed.
 @param err A pointer to store the error information, or NULL if not needed.
 @return The value referenced by the JSON pointer.
    NULL if `val` or `ptr` is NULL, or the JSON pointer cannot be resolved.
 */
json_api_inline json_mut_val *json_mut_ptr_getx(json_mut_val *val,
                                                      const char *ptr,
                                                      size_t len,
                                                      json_ptr_ctx *ctx,
                                                      json_ptr_err *err);

/**
 Add (insert) value by a JSON pointer.
 @param doc The target JSON document.
 @param ptr The JSON pointer string (UTF-8 with null-terminator).
 @param new_val The value to be added.
 @return true if JSON pointer is valid and new value is added, false otherwise.
 @note The parent nodes will be created if they do not exist.
 */
json_api_inline bool json_mut_doc_ptr_add(json_mut_doc *doc,
                                              const char *ptr,
                                              json_mut_val *new_val);

/**
 Add (insert) value by a JSON pointer.
 @param doc The target JSON document.
 @param ptr The JSON pointer string (UTF-8, null-terminator is not required).
 @param len The length of `ptr` in bytes.
 @param new_val The value to be added.
 @return true if JSON pointer is valid and new value is added, false otherwise.
 @note The parent nodes will be created if they do not exist.
 */
json_api_inline bool json_mut_doc_ptr_addn(json_mut_doc *doc,
                                               const char *ptr, size_t len,
                                               json_mut_val *new_val);

/**
 Add (insert) value by a JSON pointer.
 @param doc The target JSON document.
 @param ptr The JSON pointer string (UTF-8, null-terminator is not required).
 @param len The length of `ptr` in bytes.
 @param new_val The value to be added.
 @param create_parent Whether to create parent nodes if not exist.
 @param ctx A pointer to store the result context, or NULL if not needed.
 @param err A pointer to store the error information, or NULL if not needed.
 @return true if JSON pointer is valid and new value is added, false otherwise.
 */
json_api_inline bool json_mut_doc_ptr_addx(json_mut_doc *doc,
                                               const char *ptr, size_t len,
                                               json_mut_val *new_val,
                                               bool create_parent,
                                               json_ptr_ctx *ctx,
                                               json_ptr_err *err);

/**
 Add (insert) value by a JSON pointer.
 @param val The target JSON value.
 @param ptr The JSON pointer string (UTF-8 with null-terminator).
 @param doc Only used to create new values when needed.
 @param new_val The value to be added.
 @return true if JSON pointer is valid and new value is added, false otherwise.
 @note The parent nodes will be created if they do not exist.
 */
json_api_inline bool json_mut_ptr_add(json_mut_val *val,
                                          const char *ptr,
                                          json_mut_val *new_val,
                                          json_mut_doc *doc);

/**
 Add (insert) value by a JSON pointer.
 @param val The target JSON value.
 @param ptr The JSON pointer string (UTF-8, null-terminator is not required).
 @param len The length of `ptr` in bytes.
 @param doc Only used to create new values when needed.
 @param new_val The value to be added.
 @return true if JSON pointer is valid and new value is added, false otherwise.
 @note The parent nodes will be created if they do not exist.
 */
json_api_inline bool json_mut_ptr_addn(json_mut_val *val,
                                           const char *ptr, size_t len,
                                           json_mut_val *new_val,
                                           json_mut_doc *doc);

/**
 Add (insert) value by a JSON pointer.
 @param val The target JSON value.
 @param ptr The JSON pointer string (UTF-8, null-terminator is not required).
 @param len The length of `ptr` in bytes.
 @param doc Only used to create new values when needed.
 @param new_val The value to be added.
 @param create_parent Whether to create parent nodes if not exist.
 @param ctx A pointer to store the result context, or NULL if not needed.
 @param err A pointer to store the error information, or NULL if not needed.
 @return true if JSON pointer is valid and new value is added, false otherwise.
 */
json_api_inline bool json_mut_ptr_addx(json_mut_val *val,
                                           const char *ptr, size_t len,
                                           json_mut_val *new_val,
                                           json_mut_doc *doc,
                                           bool create_parent,
                                           json_ptr_ctx *ctx,
                                           json_ptr_err *err);

/**
 Set value by a JSON pointer.
 @param doc The target JSON document.
 @param ptr The JSON pointer string (UTF-8 with null-terminator).
 @param new_val The value to be set, pass NULL to remove.
 @return true if JSON pointer is valid and new value is set, false otherwise.
 @note The parent nodes will be created if they do not exist.
    If the target value already exists, it will be replaced by the new value.
 */
json_api_inline bool json_mut_doc_ptr_set(json_mut_doc *doc,
                                              const char *ptr,
                                              json_mut_val *new_val);

/**
 Set value by a JSON pointer.
 @param doc The target JSON document.
 @param ptr The JSON pointer string (UTF-8, null-terminator is not required).
 @param len The length of `ptr` in bytes.
 @param new_val The value to be set, pass NULL to remove.
 @return true if JSON pointer is valid and new value is set, false otherwise.
 @note The parent nodes will be created if they do not exist.
    If the target value already exists, it will be replaced by the new value.
 */
json_api_inline bool json_mut_doc_ptr_setn(json_mut_doc *doc,
                                               const char *ptr, size_t len,
                                               json_mut_val *new_val);

/**
 Set value by a JSON pointer.
 @param doc The target JSON document.
 @param ptr The JSON pointer string (UTF-8, null-terminator is not required).
 @param len The length of `ptr` in bytes.
 @param new_val The value to be set, pass NULL to remove.
 @param create_parent Whether to create parent nodes if not exist.
 @param ctx A pointer to store the result context, or NULL if not needed.
 @param err A pointer to store the error information, or NULL if not needed.
 @return true if JSON pointer is valid and new value is set, false otherwise.
 @note If the target value already exists, it will be replaced by the new value.
 */
json_api_inline bool json_mut_doc_ptr_setx(json_mut_doc *doc,
                                               const char *ptr, size_t len,
                                               json_mut_val *new_val,
                                               bool create_parent,
                                               json_ptr_ctx *ctx,
                                               json_ptr_err *err);

/**
 Set value by a JSON pointer.
 @param val The target JSON value.
 @param ptr The JSON pointer string (UTF-8 with null-terminator).
 @param new_val The value to be set, pass NULL to remove.
 @param doc Only used to create new values when needed.
 @return true if JSON pointer is valid and new value is set, false otherwise.
 @note The parent nodes will be created if they do not exist.
    If the target value already exists, it will be replaced by the new value.
 */
json_api_inline bool json_mut_ptr_set(json_mut_val *val,
                                          const char *ptr,
                                          json_mut_val *new_val,
                                          json_mut_doc *doc);

/**
 Set value by a JSON pointer.
 @param val The target JSON value.
 @param ptr The JSON pointer string (UTF-8, null-terminator is not required).
 @param len The length of `ptr` in bytes.
 @param new_val The value to be set, pass NULL to remove.
 @param doc Only used to create new values when needed.
 @return true if JSON pointer is valid and new value is set, false otherwise.
 @note The parent nodes will be created if they do not exist.
    If the target value already exists, it will be replaced by the new value.
 */
json_api_inline bool json_mut_ptr_setn(json_mut_val *val,
                                           const char *ptr, size_t len,
                                           json_mut_val *new_val,
                                           json_mut_doc *doc);

/**
 Set value by a JSON pointer.
 @param val The target JSON value.
 @param ptr The JSON pointer string (UTF-8, null-terminator is not required).
 @param len The length of `ptr` in bytes.
 @param new_val The value to be set, pass NULL to remove.
 @param doc Only used to create new values when needed.
 @param create_parent Whether to create parent nodes if not exist.
 @param ctx A pointer to store the result context, or NULL if not needed.
 @param err A pointer to store the error information, or NULL if not needed.
 @return true if JSON pointer is valid and new value is set, false otherwise.
 @note If the target value already exists, it will be replaced by the new value.
 */
json_api_inline bool json_mut_ptr_setx(json_mut_val *val,
                                           const char *ptr, size_t len,
                                           json_mut_val *new_val,
                                           json_mut_doc *doc,
                                           bool create_parent,
                                           json_ptr_ctx *ctx,
                                           json_ptr_err *err);

/**
 Replace value by a JSON pointer.
 @param doc The target JSON document.
 @param ptr The JSON pointer string (UTF-8 with null-terminator).
 @param new_val The new value to replace the old one.
 @return The old value that was replaced, or NULL if not found.
 */
json_api_inline json_mut_val *json_mut_doc_ptr_replace(
    json_mut_doc *doc, const char *ptr, json_mut_val *new_val);

/**
 Replace value by a JSON pointer.
 @param doc The target JSON document.
 @param ptr The JSON pointer string (UTF-8, null-terminator is not required).
 @param len The length of `ptr` in bytes.
 @param new_val The new value to replace the old one.
 @return The old value that was replaced, or NULL if not found.
 */
json_api_inline json_mut_val *json_mut_doc_ptr_replacen(
    json_mut_doc *doc, const char *ptr, size_t len, json_mut_val *new_val);

/**
 Replace value by a JSON pointer.
 @param doc The target JSON document.
 @param ptr The JSON pointer string (UTF-8, null-terminator is not required).
 @param len The length of `ptr` in bytes.
 @param new_val The new value to replace the old one.
 @param ctx A pointer to store the result context, or NULL if not needed.
 @param err A pointer to store the error information, or NULL if not needed.
 @return The old value that was replaced, or NULL if not found.
 */
json_api_inline json_mut_val *json_mut_doc_ptr_replacex(
    json_mut_doc *doc, const char *ptr, size_t len, json_mut_val *new_val,
    json_ptr_ctx *ctx, json_ptr_err *err);

/**
 Replace value by a JSON pointer.
 @param val The target JSON value.
 @param ptr The JSON pointer string (UTF-8 with null-terminator).
 @param new_val The new value to replace the old one.
 @return The old value that was replaced, or NULL if not found.
 */
json_api_inline json_mut_val *json_mut_ptr_replace(
    json_mut_val *val, const char *ptr, json_mut_val *new_val);

/**
 Replace value by a JSON pointer.
 @param val The target JSON value.
 @param ptr The JSON pointer string (UTF-8, null-terminator is not required).
 @param len The length of `ptr` in bytes.
 @param new_val The new value to replace the old one.
 @return The old value that was replaced, or NULL if not found.
 */
json_api_inline json_mut_val *json_mut_ptr_replacen(
    json_mut_val *val, const char *ptr, size_t len, json_mut_val *new_val);

/**
 Replace value by a JSON pointer.
 @param val The target JSON value.
 @param ptr The JSON pointer string (UTF-8, null-terminator is not required).
 @param len The length of `ptr` in bytes.
 @param new_val The new value to replace the old one.
 @param ctx A pointer to store the result context, or NULL if not needed.
 @param err A pointer to store the error information, or NULL if not needed.
 @return The old value that was replaced, or NULL if not found.
 */
json_api_inline json_mut_val *json_mut_ptr_replacex(
    json_mut_val *val, const char *ptr, size_t len, json_mut_val *new_val,
    json_ptr_ctx *ctx, json_ptr_err *err);

/**
 Remove value by a JSON pointer.
 @param doc The target JSON document.
 @param ptr The JSON pointer string (UTF-8 with null-terminator).
 @return The removed value, or NULL on error.
 */
json_api_inline json_mut_val *json_mut_doc_ptr_remove(
    json_mut_doc *doc, const char *ptr);

/**
 Remove value by a JSON pointer.
 @param doc The target JSON document.
 @param ptr The JSON pointer string (UTF-8, null-terminator is not required).
 @param len The length of `ptr` in bytes.
 @return The removed value, or NULL on error.
 */
json_api_inline json_mut_val *json_mut_doc_ptr_removen(
    json_mut_doc *doc, const char *ptr, size_t len);

/**
 Remove value by a JSON pointer.
 @param doc The target JSON document.
 @param ptr The JSON pointer string (UTF-8, null-terminator is not required).
 @param len The length of `ptr` in bytes.
 @param ctx A pointer to store the result context, or NULL if not needed.
 @param err A pointer to store the error information, or NULL if not needed.
 @return The removed value, or NULL on error.
 */
json_api_inline json_mut_val *json_mut_doc_ptr_removex(
    json_mut_doc *doc, const char *ptr, size_t len,
    json_ptr_ctx *ctx, json_ptr_err *err);

/**
 Remove value by a JSON pointer.
 @param val The target JSON value.
 @param ptr The JSON pointer string (UTF-8 with null-terminator).
 @return The removed value, or NULL on error.
 */
json_api_inline json_mut_val *json_mut_ptr_remove(json_mut_val *val,
                                                        const char *ptr);

/**
 Remove value by a JSON pointer.
 @param val The target JSON value.
 @param ptr The JSON pointer string (UTF-8, null-terminator is not required).
 @param len The length of `ptr` in bytes.
 @return The removed value, or NULL on error.
 */
json_api_inline json_mut_val *json_mut_ptr_removen(json_mut_val *val,
                                                         const char *ptr,
                                                         size_t len);

/**
 Remove value by a JSON pointer.
 @param val The target JSON value.
 @param ptr The JSON pointer string (UTF-8, null-terminator is not required).
 @param len The length of `ptr` in bytes.
 @param ctx A pointer to store the result context, or NULL if not needed.
 @param err A pointer to store the error information, or NULL if not needed.
 @return The removed value, or NULL on error.
 */
json_api_inline json_mut_val *json_mut_ptr_removex(json_mut_val *val,
                                                         const char *ptr,
                                                         size_t len,
                                                         json_ptr_ctx *ctx,
                                                         json_ptr_err *err);

/**
 Append value by JSON pointer context.
 @param ctx The context from the `json_mut_ptr_xxx()` calls.
 @param key New key if `ctx->ctn` is object, or NULL if `ctx->ctn` is array.
 @param val New value to be added.
 @return true on success or false on fail.
 */
json_api_inline bool json_ptr_ctx_append(json_ptr_ctx *ctx,
                                             json_mut_val *key,
                                             json_mut_val *val);

/**
 Replace value by JSON pointer context.
 @param ctx The context from the `json_mut_ptr_xxx()` calls.
 @param val New value to be replaced.
 @return true on success or false on fail.
 @note If success, the old value will be returned via `ctx->old`.
 */
json_api_inline bool json_ptr_ctx_replace(json_ptr_ctx *ctx,
                                              json_mut_val *val);

/**
 Remove value by JSON pointer context.
 @param ctx The context from the `json_mut_ptr_xxx()` calls.
 @return true on success or false on fail.
 @note If success, the old value will be returned via `ctx->old`.
 */
json_api_inline bool json_ptr_ctx_remove(json_ptr_ctx *ctx);



/*==============================================================================
 * JSON Patch API (RFC 6902)
 * https://tools.ietf.org/html/rfc6902
 *============================================================================*/

/** Result code for JSON patch. */
typedef uint32_t json_patch_code;

/** Success, no error. */
static const json_patch_code JSON_PATCH_SUCCESS = 0;

/** Invalid parameter, such as NULL input or non-array patch. */
static const json_patch_code JSON_PATCH_ERROR_INVALID_PARAMETER = 1;

/** Memory allocation failure occurs. */
static const json_patch_code JSON_PATCH_ERROR_MEMORY_ALLOCATION = 2;

/** JSON patch operation is not object type. */
static const json_patch_code JSON_PATCH_ERROR_INVALID_OPERATION = 3;

/** JSON patch operation is missing a required key. */
static const json_patch_code JSON_PATCH_ERROR_MISSING_KEY = 4;

/** JSON patch operation member is invalid. */
static const json_patch_code JSON_PATCH_ERROR_INVALID_MEMBER = 5;

/** JSON patch operation `test` not equal. */
static const json_patch_code JSON_PATCH_ERROR_EQUAL = 6;

/** JSON patch operation failed on JSON pointer. */
static const json_patch_code JSON_PATCH_ERROR_POINTER = 7;

/** Error information for JSON patch. */
typedef struct json_patch_err {
    /** Error code, see `json_patch_code` for all possible values. */
    json_patch_code code;
    /** Index of the error operation (0 if no error). */
    size_t idx;
    /** Error message, constant, no need to free (NULL if no error). */
    const char *msg;
    /** JSON pointer error if `code == JSON_PATCH_ERROR_POINTER`. */
    json_ptr_err ptr;
} json_patch_err;

/**
 Creates and returns a patched JSON value (RFC 6902).
 The memory of the returned value is allocated by the `doc`.
 The `err` is used to receive error information, pass NULL if not needed.
 Returns NULL if the patch could not be applied.
 */
json_api json_mut_val *json_patch(json_mut_doc *doc,
                                        json_val *orig,
                                        json_val *patch,
                                        json_patch_err *err);

/**
 Creates and returns a patched JSON value (RFC 6902).
 The memory of the returned value is allocated by the `doc`.
 The `err` is used to receive error information, pass NULL if not needed.
 Returns NULL if the patch could not be applied.
 */
json_api json_mut_val *json_mut_patch(json_mut_doc *doc,
                                            json_mut_val *orig,
                                            json_mut_val *patch,
                                            json_patch_err *err);



/*==============================================================================
 * JSON Merge-Patch API (RFC 7386)
 * https://tools.ietf.org/html/rfc7386
 *============================================================================*/

/**
 Creates and returns a merge-patched JSON value (RFC 7386).
 The memory of the returned value is allocated by the `doc`.
 Returns NULL if the patch could not be applied.

 @warning This function is recursive and may cause a stack overflow if the
    object level is too deep.
 */
json_api json_mut_val *json_merge_patch(json_mut_doc *doc,
                                              json_val *orig,
                                              json_val *patch);

/**
 Creates and returns a merge-patched JSON value (RFC 7386).
 The memory of the returned value is allocated by the `doc`.
 Returns NULL if the patch could not be applied.

 @warning This function is recursive and may cause a stack overflow if the
    object level is too deep.
 */
json_api json_mut_val *json_mut_merge_patch(json_mut_doc *doc,
                                                  json_mut_val *orig,
                                                  json_mut_val *patch);

#endif /* JSON_DISABLE_UTILS */



/*==============================================================================
 * JSON Structure (Implementation)
 *============================================================================*/

/** Payload of a JSON value (8 bytes). */
typedef union json_val_uni {
    uint64_t    u64;
    int64_t     i64;
    double      f64;
    const char *str;
    void       *ptr;
    size_t      ofs;
} json_val_uni;

/**
 Immutable JSON value, 16 bytes.
 */
struct json_val {
    uint64_t tag; /**< type, subtype and length */
    json_val_uni uni; /**< payload */
};

struct json_doc {
    /** Root value of the document (nonnull). */
    json_val *root;
    /** Allocator used by document (nonnull). */
    json_alc alc;
    /** The total number of bytes read when parsing JSON (nonzero). */
    size_t dat_read;
    /** The total number of value read when parsing JSON (nonzero). */
    size_t val_read;
    /** The string pool used by JSON values (nullable). */
    char *str_pool;
};



/*==============================================================================
 * Unsafe JSON Value API (Implementation)
 *============================================================================*/

/*
 Whether the string does not need to be escaped for serialization.
 This function is used to optimize the writing speed of small constant strings.
 This function works only if the compiler can evaluate it at compile time.
 
 Clang supports it since v8.0,
    earlier versions do not support constant_p(strlen) and return false.
 GCC supports it since at least v4.4,
    earlier versions may compile it as run-time instructions.
 ICC supports it since at least v16,
    earlier versions are uncertain.
 
 @param str The C string.
 @param len The returnd value from strlen(str).
 */
json_api_inline bool unsafe_json_is_str_noesc(const char *str, size_t len) {
#if JSON_HAS_CONSTANT_P && \
    (!JSON_IS_REAL_GCC || json_gcc_available(4, 4, 0))
    if (json_constant_p(len) && len <= 32) {
        /*
         Same as the following loop:
         
         for (size_t i = 0; i < len; i++) {
             char c = str[i];
             if (c < ' ' || c > '~' || c == '"' || c == '\\') return false;
         }
         
         GCC evaluates it at compile time only if the string length is within 17
         and -O3 (which turns on the -fpeel-loops flag) is used.
         So the loop is unrolled for GCC.
         */
#       define json_repeat32_incr(x) \
            x(0)  x(1)  x(2)  x(3)  x(4)  x(5)  x(6)  x(7)  \
            x(8)  x(9)  x(10) x(11) x(12) x(13) x(14) x(15) \
            x(16) x(17) x(18) x(19) x(20) x(21) x(22) x(23) \
            x(24) x(25) x(26) x(27) x(28) x(29) x(30) x(31)
#       define json_check_char_noesc(i) \
            if (i < len) { \
                char c = str[i]; \
                if (c < ' ' || c > '~' || c == '"' || c == '\\') return false; }
        json_repeat32_incr(json_check_char_noesc)
#       undef json_repeat32_incr
#       undef json_check_char_noesc
        return true;
    }
#else
    (void)str;
    (void)len;
#endif
    return false;
}

json_api_inline json_type unsafe_json_get_type(void *val) {
    uint8_t tag = (uint8_t)((json_val *)val)->tag;
    return (json_type)(tag & JSON_TYPE_MASK);
}

json_api_inline json_subtype unsafe_json_get_subtype(void *val) {
    uint8_t tag = (uint8_t)((json_val *)val)->tag;
    return (json_subtype)(tag & JSON_SUBTYPE_MASK);
}

json_api_inline uint8_t unsafe_json_get_tag(void *val) {
    uint8_t tag = (uint8_t)((json_val *)val)->tag;
    return (uint8_t)(tag & JSON_TAG_MASK);
}

json_api_inline bool unsafe_json_is_raw(void *val) {
    return unsafe_json_get_type(val) == JSON_TYPE_RAW;
}

json_api_inline bool unsafe_json_is_null(void *val) {
    return unsafe_json_get_type(val) == JSON_TYPE_NULL;
}

json_api_inline bool unsafe_json_is_bool(void *val) {
    return unsafe_json_get_type(val) == JSON_TYPE_BOOL;
}

json_api_inline bool unsafe_json_is_num(void *val) {
    return unsafe_json_get_type(val) == JSON_TYPE_NUM;
}

json_api_inline bool unsafe_json_is_str(void *val) {
    return unsafe_json_get_type(val) == JSON_TYPE_STR;
}

json_api_inline bool unsafe_json_is_arr(void *val) {
    return unsafe_json_get_type(val) == JSON_TYPE_ARR;
}

json_api_inline bool unsafe_json_is_obj(void *val) {
    return unsafe_json_get_type(val) == JSON_TYPE_OBJ;
}

json_api_inline bool unsafe_json_is_ctn(void *val) {
    uint8_t mask = JSON_TYPE_ARR & JSON_TYPE_OBJ;
    return (unsafe_json_get_tag(val) & mask) == mask;
}

json_api_inline bool unsafe_json_is_uint(void *val) {
    const uint8_t patt = JSON_TYPE_NUM | JSON_SUBTYPE_UINT;
    return unsafe_json_get_tag(val) == patt;
}

json_api_inline bool unsafe_json_is_sint(void *val) {
    const uint8_t patt = JSON_TYPE_NUM | JSON_SUBTYPE_SINT;
    return unsafe_json_get_tag(val) == patt;
}

json_api_inline bool unsafe_json_is_int(void *val) {
    const uint8_t mask = JSON_TAG_MASK & (~JSON_SUBTYPE_SINT);
    const uint8_t patt = JSON_TYPE_NUM | JSON_SUBTYPE_UINT;
    return (unsafe_json_get_tag(val) & mask) == patt;
}

json_api_inline bool unsafe_json_is_real(void *val) {
    const uint8_t patt = JSON_TYPE_NUM | JSON_SUBTYPE_REAL;
    return unsafe_json_get_tag(val) == patt;
}

json_api_inline bool unsafe_json_is_true(void *val) {
    const uint8_t patt = JSON_TYPE_BOOL | JSON_SUBTYPE_TRUE;
    return unsafe_json_get_tag(val) == patt;
}

json_api_inline bool unsafe_json_is_false(void *val) {
    const uint8_t patt = JSON_TYPE_BOOL | JSON_SUBTYPE_FALSE;
    return unsafe_json_get_tag(val) == patt;
}

json_api_inline bool unsafe_json_arr_is_flat(json_val *val) {
    size_t ofs = val->uni.ofs;
    size_t len = (size_t)(val->tag >> JSON_TAG_BIT);
    return len * sizeof(json_val) + sizeof(json_val) == ofs;
}

json_api_inline const char *unsafe_json_get_raw(void *val) {
    return ((json_val *)val)->uni.str;
}

json_api_inline bool unsafe_json_get_bool(void *val) {
    uint8_t tag = unsafe_json_get_tag(val);
    return (bool)((tag & JSON_SUBTYPE_MASK) >> JSON_TYPE_BIT);
}

json_api_inline uint64_t unsafe_json_get_uint(void *val) {
    return ((json_val *)val)->uni.u64;
}

json_api_inline int64_t unsafe_json_get_sint(void *val) {
    return ((json_val *)val)->uni.i64;
}

json_api_inline int unsafe_json_get_int(void *val) {
    return (int)((json_val *)val)->uni.i64;
}

json_api_inline double unsafe_json_get_real(void *val) {
    return ((json_val *)val)->uni.f64;
}

json_api_inline double unsafe_json_get_num(void *val) {
    uint8_t tag = unsafe_json_get_tag(val);
    if (tag == (JSON_TYPE_NUM | JSON_SUBTYPE_REAL)) {
        return ((json_val *)val)->uni.f64;
    } else if (tag == (JSON_TYPE_NUM | JSON_SUBTYPE_SINT)) {
        return (double)((json_val *)val)->uni.i64;
    } else if (tag == (JSON_TYPE_NUM | JSON_SUBTYPE_UINT)) {
#if JSON_U64_TO_F64_NO_IMPL
        uint64_t msb = ((uint64_t)1) << 63;
        uint64_t num = ((json_val *)val)->uni.u64;
        if ((num & msb) == 0) {
            return (double)(int64_t)num;
        } else {
            return ((double)(int64_t)((num >> 1) | (num & 1))) * (double)2.0;
        }
#else
        return (double)((json_val *)val)->uni.u64;
#endif
    }
    return 0.0;
}

json_api_inline const char *unsafe_json_get_str(void *val) {
    return ((json_val *)val)->uni.str;
}

json_api_inline size_t unsafe_json_get_len(void *val) {
    return (size_t)(((json_val *)val)->tag >> JSON_TAG_BIT);
}

json_api_inline json_val *unsafe_json_get_first(json_val *ctn) {
    return ctn + 1;
}

json_api_inline json_val *unsafe_json_get_next(json_val *val) {
    bool is_ctn = unsafe_json_is_ctn(val);
    size_t ctn_ofs = val->uni.ofs;
    size_t ofs = (is_ctn ? ctn_ofs : sizeof(json_val));
    return (json_val *)(void *)((uint8_t *)val + ofs);
}

json_api_inline bool unsafe_json_equals_strn(void *val, const char *str,
                                                 size_t len) {
    return unsafe_json_get_len(val) == len &&
           memcmp(((json_val *)val)->uni.str, str, len) == 0;
}

json_api_inline bool unsafe_json_equals_str(void *val, const char *str) {
    return unsafe_json_equals_strn(val, str, strlen(str));
}

json_api_inline void unsafe_json_set_type(void *val, json_type type,
                                              json_subtype subtype) {
    uint8_t tag = (type | subtype);
    uint64_t new_tag = ((json_val *)val)->tag;
    new_tag = (new_tag & (~(uint64_t)JSON_TAG_MASK)) | (uint64_t)tag;
    ((json_val *)val)->tag = new_tag;
}

json_api_inline void unsafe_json_set_len(void *val, size_t len) {
    uint64_t tag = ((json_val *)val)->tag & JSON_TAG_MASK;
    tag |= (uint64_t)len << JSON_TAG_BIT;
    ((json_val *)val)->tag = tag;
}

json_api_inline void unsafe_json_inc_len(void *val) {
    uint64_t tag = ((json_val *)val)->tag;
    tag += (uint64_t)(1 << JSON_TAG_BIT);
    ((json_val *)val)->tag = tag;
}

json_api_inline void unsafe_json_set_raw(void *val, const char *raw,
                                             size_t len) {
    unsafe_json_set_type(val, JSON_TYPE_RAW, JSON_SUBTYPE_NONE);
    unsafe_json_set_len(val, len);
    ((json_val *)val)->uni.str = raw;
}

json_api_inline void unsafe_json_set_null(void *val) {
    unsafe_json_set_type(val, JSON_TYPE_NULL, JSON_SUBTYPE_NONE);
    unsafe_json_set_len(val, 0);
}

json_api_inline void unsafe_json_set_bool(void *val, bool num) {
    json_subtype subtype = num ? JSON_SUBTYPE_TRUE : JSON_SUBTYPE_FALSE;
    unsafe_json_set_type(val, JSON_TYPE_BOOL, subtype);
    unsafe_json_set_len(val, 0);
}

json_api_inline void unsafe_json_set_uint(void *val, uint64_t num) {
    unsafe_json_set_type(val, JSON_TYPE_NUM, JSON_SUBTYPE_UINT);
    unsafe_json_set_len(val, 0);
    ((json_val *)val)->uni.u64 = num;
}

json_api_inline void unsafe_json_set_sint(void *val, int64_t num) {
    unsafe_json_set_type(val, JSON_TYPE_NUM, JSON_SUBTYPE_SINT);
    unsafe_json_set_len(val, 0);
    ((json_val *)val)->uni.i64 = num;
}

json_api_inline void unsafe_json_set_real(void *val, double num) {
    unsafe_json_set_type(val, JSON_TYPE_NUM, JSON_SUBTYPE_REAL);
    unsafe_json_set_len(val, 0);
    ((json_val *)val)->uni.f64 = num;
}

json_api_inline void unsafe_json_set_str(void *val, const char *str) {
    size_t len = strlen(str);
    bool noesc = unsafe_json_is_str_noesc(str, len);
    json_subtype sub = noesc ? JSON_SUBTYPE_NOESC : JSON_SUBTYPE_NONE;
    unsafe_json_set_type(val, JSON_TYPE_STR, sub);
    unsafe_json_set_len(val, len);
    ((json_val *)val)->uni.str = str;
}

json_api_inline void unsafe_json_set_strn(void *val, const char *str,
                                              size_t len) {
    unsafe_json_set_type(val, JSON_TYPE_STR, JSON_SUBTYPE_NONE);
    unsafe_json_set_len(val, len);
    ((json_val *)val)->uni.str = str;
}

json_api_inline void unsafe_json_set_arr(void *val, size_t size) {
    unsafe_json_set_type(val, JSON_TYPE_ARR, JSON_SUBTYPE_NONE);
    unsafe_json_set_len(val, size);
}

json_api_inline void unsafe_json_set_obj(void *val, size_t size) {
    unsafe_json_set_type(val, JSON_TYPE_OBJ, JSON_SUBTYPE_NONE);
    unsafe_json_set_len(val, size);
}



/*==============================================================================
 * JSON Document API (Implementation)
 *============================================================================*/

json_api_inline json_val *json_doc_get_root(json_doc *doc) {
    return doc ? doc->root : NULL;
}

json_api_inline size_t json_doc_get_read_size(json_doc *doc) {
    return doc ? doc->dat_read : 0;
}

json_api_inline size_t json_doc_get_val_count(json_doc *doc) {
    return doc ? doc->val_read : 0;
}

json_api_inline void json_doc_free(json_doc *doc) {
    if (doc) {
        json_alc alc = doc->alc;
        memset(&doc->alc, 0, sizeof(alc));
        if (doc->str_pool) alc.free(alc.ctx, doc->str_pool);
        alc.free(alc.ctx, doc);
    }
}



/*==============================================================================
 * JSON Value Type API (Implementation)
 *============================================================================*/

json_api_inline bool json_is_raw(json_val *val) {
    return val ? unsafe_json_is_raw(val) : false;
}

json_api_inline bool json_is_null(json_val *val) {
    return val ? unsafe_json_is_null(val) : false;
}

json_api_inline bool json_is_true(json_val *val) {
    return val ? unsafe_json_is_true(val) : false;
}

json_api_inline bool json_is_false(json_val *val) {
    return val ? unsafe_json_is_false(val) : false;
}

json_api_inline bool json_is_bool(json_val *val) {
    return val ? unsafe_json_is_bool(val) : false;
}

json_api_inline bool json_is_uint(json_val *val) {
    return val ? unsafe_json_is_uint(val) : false;
}

json_api_inline bool json_is_sint(json_val *val) {
    return val ? unsafe_json_is_sint(val) : false;
}

json_api_inline bool json_is_int(json_val *val) {
    return val ? unsafe_json_is_int(val) : false;
}

json_api_inline bool json_is_real(json_val *val) {
    return val ? unsafe_json_is_real(val) : false;
}

json_api_inline bool json_is_num(json_val *val) {
    return val ? unsafe_json_is_num(val) : false;
}

json_api_inline bool json_is_str(json_val *val) {
    return val ? unsafe_json_is_str(val) : false;
}

json_api_inline bool json_is_arr(json_val *val) {
    return val ? unsafe_json_is_arr(val) : false;
}

json_api_inline bool json_is_obj(json_val *val) {
    return val ? unsafe_json_is_obj(val) : false;
}

json_api_inline bool json_is_ctn(json_val *val) {
    return val ? unsafe_json_is_ctn(val) : false;
}



/*==============================================================================
 * JSON Value Content API (Implementation)
 *============================================================================*/

json_api_inline json_type json_get_type(json_val *val) {
    return val ? unsafe_json_get_type(val) : JSON_TYPE_NONE;
}

json_api_inline json_subtype json_get_subtype(json_val *val) {
    return val ? unsafe_json_get_subtype(val) : JSON_SUBTYPE_NONE;
}

json_api_inline uint8_t json_get_tag(json_val *val) {
    return val ? unsafe_json_get_tag(val) : 0;
}

json_api_inline const char *json_get_type_desc(json_val *val) {
    switch (json_get_tag(val)) {
        case JSON_TYPE_RAW  | JSON_SUBTYPE_NONE:  return "raw";
        case JSON_TYPE_NULL | JSON_SUBTYPE_NONE:  return "null";
        case JSON_TYPE_STR  | JSON_SUBTYPE_NONE:  return "string";
        case JSON_TYPE_STR  | JSON_SUBTYPE_NOESC: return "string";
        case JSON_TYPE_ARR  | JSON_SUBTYPE_NONE:  return "array";
        case JSON_TYPE_OBJ  | JSON_SUBTYPE_NONE:  return "object";
        case JSON_TYPE_BOOL | JSON_SUBTYPE_TRUE:  return "true";
        case JSON_TYPE_BOOL | JSON_SUBTYPE_FALSE: return "false";
        case JSON_TYPE_NUM  | JSON_SUBTYPE_UINT:  return "uint";
        case JSON_TYPE_NUM  | JSON_SUBTYPE_SINT:  return "sint";
        case JSON_TYPE_NUM  | JSON_SUBTYPE_REAL:  return "real";
        default:                                      return "unknown";
    }
}

json_api_inline const char *json_get_raw(json_val *val) {
    return json_is_raw(val) ? unsafe_json_get_raw(val) : NULL;
}

json_api_inline bool json_get_bool(json_val *val) {
    return json_is_bool(val) ? unsafe_json_get_bool(val) : false;
}

json_api_inline uint64_t json_get_uint(json_val *val) {
    return json_is_int(val) ? unsafe_json_get_uint(val) : 0;
}

json_api_inline int64_t json_get_sint(json_val *val) {
    return json_is_int(val) ? unsafe_json_get_sint(val) : 0;
}

json_api_inline int json_get_int(json_val *val) {
    return json_is_int(val) ? unsafe_json_get_int(val) : 0;
}

json_api_inline double json_get_real(json_val *val) {
    return json_is_real(val) ? unsafe_json_get_real(val) : 0.0;
}

json_api_inline double json_get_num(json_val *val) {
    return val ? unsafe_json_get_num(val) : 0.0;
}

json_api_inline const char *json_get_str(json_val *val) {
    return json_is_str(val) ? unsafe_json_get_str(val) : NULL;
}

json_api_inline size_t json_get_len(json_val *val) {
    return val ? unsafe_json_get_len(val) : 0;
}

json_api_inline bool json_equals_str(json_val *val, const char *str) {
    if (json_likely(val && str)) {
        return unsafe_json_is_str(val) &&
               unsafe_json_equals_str(val, str);
    }
    return false;
}

json_api_inline bool json_equals_strn(json_val *val, const char *str,
                                          size_t len) {
    if (json_likely(val && str)) {
        return unsafe_json_is_str(val) &&
               unsafe_json_equals_strn(val, str, len);
    }
    return false;
}

json_api bool unsafe_json_equals(json_val *lhs, json_val *rhs);

json_api_inline bool json_equals(json_val *lhs, json_val *rhs) {
    if (json_unlikely(!lhs || !rhs)) return false;
    return unsafe_json_equals(lhs, rhs);
}

json_api_inline bool json_set_raw(json_val *val,
                                      const char *raw, size_t len) {
    if (json_unlikely(!val || unsafe_json_is_ctn(val))) return false;
    unsafe_json_set_raw(val, raw, len);
    return true;
}

json_api_inline bool json_set_null(json_val *val) {
    if (json_unlikely(!val || unsafe_json_is_ctn(val))) return false;
    unsafe_json_set_null(val);
    return true;
}

json_api_inline bool json_set_bool(json_val *val, bool num) {
    if (json_unlikely(!val || unsafe_json_is_ctn(val))) return false;
    unsafe_json_set_bool(val, num);
    return true;
}

json_api_inline bool json_set_uint(json_val *val, uint64_t num) {
    if (json_unlikely(!val || unsafe_json_is_ctn(val))) return false;
    unsafe_json_set_uint(val, num);
    return true;
}

json_api_inline bool json_set_sint(json_val *val, int64_t num) {
    if (json_unlikely(!val || unsafe_json_is_ctn(val))) return false;
    unsafe_json_set_sint(val, num);
    return true;
}

json_api_inline bool json_set_int(json_val *val, int num) {
    if (json_unlikely(!val || unsafe_json_is_ctn(val))) return false;
    unsafe_json_set_sint(val, (int64_t)num);
    return true;
}

json_api_inline bool json_set_real(json_val *val, double num) {
    if (json_unlikely(!val || unsafe_json_is_ctn(val))) return false;
    unsafe_json_set_real(val, num);
    return true;
}

json_api_inline bool json_set_str(json_val *val, const char *str) {
    if (json_unlikely(!val || unsafe_json_is_ctn(val))) return false;
    if (json_unlikely(!str)) return false;
    unsafe_json_set_str(val, str);
    return true;
}

json_api_inline bool json_set_strn(json_val *val,
                                       const char *str, size_t len) {
    if (json_unlikely(!val || unsafe_json_is_ctn(val))) return false;
    if (json_unlikely(!str)) return false;
    unsafe_json_set_strn(val, str, len);
    return true;
}



/*==============================================================================
 * JSON Array API (Implementation)
 *============================================================================*/

json_api_inline size_t json_arr_size(json_val *arr) {
    return json_is_arr(arr) ? unsafe_json_get_len(arr) : 0;
}

json_api_inline json_val *json_arr_get(json_val *arr, size_t idx) {
    if (json_likely(json_is_arr(arr))) {
        if (json_likely(unsafe_json_get_len(arr) > idx)) {
            json_val *val = unsafe_json_get_first(arr);
            if (unsafe_json_arr_is_flat(arr)) {
                return val + idx;
            } else {
                while (idx-- > 0) val = unsafe_json_get_next(val);
                return val;
            }
        }
    }
    return NULL;
}

json_api_inline json_val *json_arr_get_first(json_val *arr) {
    if (json_likely(json_is_arr(arr))) {
        if (json_likely(unsafe_json_get_len(arr) > 0)) {
            return unsafe_json_get_first(arr);
        }
    }
    return NULL;
}

json_api_inline json_val *json_arr_get_last(json_val *arr) {
    if (json_likely(json_is_arr(arr))) {
        size_t len = unsafe_json_get_len(arr);
        if (json_likely(len > 0)) {
            json_val *val = unsafe_json_get_first(arr);
            if (unsafe_json_arr_is_flat(arr)) {
                return val + (len - 1);
            } else {
                while (len-- > 1) val = unsafe_json_get_next(val);
                return val;
            }
        }
    }
    return NULL;
}



/*==============================================================================
 * JSON Array Iterator API (Implementation)
 *============================================================================*/

json_api_inline bool json_arr_iter_init(json_val *arr,
                                            json_arr_iter *iter) {
    if (json_likely(json_is_arr(arr) && iter)) {
        iter->idx = 0;
        iter->max = unsafe_json_get_len(arr);
        iter->cur = unsafe_json_get_first(arr);
        return true;
    }
    if (iter) memset(iter, 0, sizeof(json_arr_iter));
    return false;
}

json_api_inline json_arr_iter json_arr_iter_with(json_val *arr) {
    json_arr_iter iter;
    json_arr_iter_init(arr, &iter);
    return iter;
}

json_api_inline bool json_arr_iter_has_next(json_arr_iter *iter) {
    return iter ? iter->idx < iter->max : false;
}

json_api_inline json_val *json_arr_iter_next(json_arr_iter *iter) {
    json_val *val;
    if (iter && iter->idx < iter->max) {
        val = iter->cur;
        iter->cur = unsafe_json_get_next(val);
        iter->idx++;
        return val;
    }
    return NULL;
}



/*==============================================================================
 * JSON Object API (Implementation)
 *============================================================================*/

json_api_inline size_t json_obj_size(json_val *obj) {
    return json_is_obj(obj) ? unsafe_json_get_len(obj) : 0;
}

json_api_inline json_val *json_obj_get(json_val *obj,
                                             const char *key) {
    return json_obj_getn(obj, key, key ? strlen(key) : 0);
}

json_api_inline json_val *json_obj_getn(json_val *obj,
                                              const char *_key,
                                              size_t key_len) {
    if (json_likely(json_is_obj(obj) && _key)) {
        size_t len = unsafe_json_get_len(obj);
        json_val *key = unsafe_json_get_first(obj);
        while (len-- > 0) {
            if (unsafe_json_equals_strn(key, _key, key_len)) return key + 1;
            key = unsafe_json_get_next(key + 1);
        }
    }
    return NULL;
}



/*==============================================================================
 * JSON Object Iterator API (Implementation)
 *============================================================================*/

json_api_inline bool json_obj_iter_init(json_val *obj,
                                            json_obj_iter *iter) {
    if (json_likely(json_is_obj(obj) && iter)) {
        iter->idx = 0;
        iter->max = unsafe_json_get_len(obj);
        iter->cur = unsafe_json_get_first(obj);
        iter->obj = obj;
        return true;
    }
    if (iter) memset(iter, 0, sizeof(json_obj_iter));
    return false;
}

json_api_inline json_obj_iter json_obj_iter_with(json_val *obj) {
    json_obj_iter iter;
    json_obj_iter_init(obj, &iter);
    return iter;
}

json_api_inline bool json_obj_iter_has_next(json_obj_iter *iter) {
    return iter ? iter->idx < iter->max : false;
}

json_api_inline json_val *json_obj_iter_next(json_obj_iter *iter) {
    if (iter && iter->idx < iter->max) {
        json_val *key = iter->cur;
        iter->idx++;
        iter->cur = unsafe_json_get_next(key + 1);
        return key;
    }
    return NULL;
}

json_api_inline json_val *json_obj_iter_get_val(json_val *key) {
    return key ? key + 1 : NULL;
}

json_api_inline json_val *json_obj_iter_get(json_obj_iter *iter,
                                                  const char *key) {
    return json_obj_iter_getn(iter, key, key ? strlen(key) : 0);
}

json_api_inline json_val *json_obj_iter_getn(json_obj_iter *iter,
                                                   const char *key,
                                                   size_t key_len) {
    if (iter && key) {
        size_t idx = iter->idx;
        size_t max = iter->max;
        json_val *cur = iter->cur;
        if (json_unlikely(idx == max)) {
            idx = 0;
            cur = unsafe_json_get_first(iter->obj);
        }
        while (idx++ < max) {
            json_val *next = unsafe_json_get_next(cur + 1);
            if (unsafe_json_equals_strn(cur, key, key_len)) {
                iter->idx = idx;
                iter->cur = next;
                return cur + 1;
            }
            cur = next;
            if (idx == iter->max && iter->idx < iter->max) {
                idx = 0;
                max = iter->idx;
                cur = unsafe_json_get_first(iter->obj);
            }
        }
    }
    return NULL;
}



/*==============================================================================
 * Mutable JSON Structure (Implementation)
 *============================================================================*/

/**
 Mutable JSON value, 24 bytes.
 The 'tag' and 'uni' field is same as immutable value.
 The 'next' field links all elements inside the container to be a cycle.
 */
struct json_mut_val {
    uint64_t tag; /**< type, subtype and length */
    json_val_uni uni; /**< payload */
    json_mut_val *next; /**< the next value in circular linked list */
};

/**
 A memory chunk in string memory pool.
 */
typedef struct json_str_chunk {
    struct json_str_chunk *next; /* next chunk linked list */
    size_t chunk_size; /* chunk size in bytes */
    /* char str[]; flexible array member */
} json_str_chunk;

/**
 A memory pool to hold all strings in a mutable document.
 */
typedef struct json_str_pool {
    char *cur; /* cursor inside current chunk */
    char *end; /* the end of current chunk */
    size_t chunk_size; /* chunk size in bytes while creating new chunk */
    size_t chunk_size_max; /* maximum chunk size in bytes */
    json_str_chunk *chunks; /* a linked list of chunks, nullable */
} json_str_pool;

/**
 A memory chunk in value memory pool.
 `sizeof(json_val_chunk)` should not larger than `sizeof(json_mut_val)`.
 */
typedef struct json_val_chunk {
    struct json_val_chunk *next; /* next chunk linked list */
    size_t chunk_size; /* chunk size in bytes */
    /* char pad[sizeof(json_mut_val) - sizeof(json_val_chunk)]; padding */
    /* json_mut_val vals[]; flexible array member */
} json_val_chunk;

/**
 A memory pool to hold all values in a mutable document.
 */
typedef struct json_val_pool {
    json_mut_val *cur; /* cursor inside current chunk */
    json_mut_val *end; /* the end of current chunk */
    size_t chunk_size; /* chunk size in bytes while creating new chunk */
    size_t chunk_size_max; /* maximum chunk size in bytes */
    json_val_chunk *chunks; /* a linked list of chunks, nullable */
} json_val_pool;

struct json_mut_doc {
    json_mut_val *root; /**< root value of the JSON document, nullable */
    json_alc alc; /**< a valid allocator, nonnull */
    json_str_pool str_pool; /**< string memory pool */
    json_val_pool val_pool; /**< value memory pool */
};

/* Ensures the capacity to at least equal to the specified byte length. */
json_api bool unsafe_json_str_pool_grow(json_str_pool *pool,
                                            const json_alc *alc,
                                            size_t len);

/* Ensures the capacity to at least equal to the specified value count. */
json_api bool unsafe_json_val_pool_grow(json_val_pool *pool,
                                            const json_alc *alc,
                                            size_t count);

/* Allocate memory for string. */
json_api_inline char *unsafe_json_mut_str_alc(json_mut_doc *doc,
                                                  size_t len) {
    char *mem;
    const json_alc *alc = &doc->alc;
    json_str_pool *pool = &doc->str_pool;
    if (json_unlikely((size_t)(pool->end - pool->cur) <= len)) {
        if (json_unlikely(!unsafe_json_str_pool_grow(pool, alc, len + 1))) {
            return NULL;
        }
    }
    mem = pool->cur;
    pool->cur = mem + len + 1;
    return mem;
}

json_api_inline char *unsafe_json_mut_strncpy(json_mut_doc *doc,
                                                  const char *str, size_t len) {
    char *mem = unsafe_json_mut_str_alc(doc, len);
    if (json_unlikely(!mem)) return NULL;
    memcpy((void *)mem, (const void *)str, len);
    mem[len] = '\0';
    return mem;
}

json_api_inline json_mut_val *unsafe_json_mut_val(json_mut_doc *doc,
                                                        size_t count) {
    json_mut_val *val;
    json_alc *alc = &doc->alc;
    json_val_pool *pool = &doc->val_pool;
    if (json_unlikely((size_t)(pool->end - pool->cur) < count)) {
        if (json_unlikely(!unsafe_json_val_pool_grow(pool, alc, count))) {
            return NULL;
        }
    }
    val = pool->cur;
    pool->cur += count;
    return val;
}



/*==============================================================================
 * Mutable JSON Document API (Implementation)
 *============================================================================*/

json_api_inline json_mut_val *json_mut_doc_get_root(json_mut_doc *doc) {
    return doc ? doc->root : NULL;
}

json_api_inline void json_mut_doc_set_root(json_mut_doc *doc,
                                               json_mut_val *root) {
    if (doc) doc->root = root;
}



/*==============================================================================
 * Mutable JSON Value Type API (Implementation)
 *============================================================================*/

json_api_inline bool json_mut_is_raw(json_mut_val *val) {
    return val ? unsafe_json_is_raw(val) : false;
}

json_api_inline bool json_mut_is_null(json_mut_val *val) {
    return val ? unsafe_json_is_null(val) : false;
}

json_api_inline bool json_mut_is_true(json_mut_val *val) {
    return val ? unsafe_json_is_true(val) : false;
}

json_api_inline bool json_mut_is_false(json_mut_val *val) {
    return val ? unsafe_json_is_false(val) : false;
}

json_api_inline bool json_mut_is_bool(json_mut_val *val) {
    return val ? unsafe_json_is_bool(val) : false;
}

json_api_inline bool json_mut_is_uint(json_mut_val *val) {
    return val ? unsafe_json_is_uint(val) : false;
}

json_api_inline bool json_mut_is_sint(json_mut_val *val) {
    return val ? unsafe_json_is_sint(val) : false;
}

json_api_inline bool json_mut_is_int(json_mut_val *val) {
    return val ? unsafe_json_is_int(val) : false;
}

json_api_inline bool json_mut_is_real(json_mut_val *val) {
    return val ? unsafe_json_is_real(val) : false;
}

json_api_inline bool json_mut_is_num(json_mut_val *val) {
    return val ? unsafe_json_is_num(val) : false;
}

json_api_inline bool json_mut_is_str(json_mut_val *val) {
    return val ? unsafe_json_is_str(val) : false;
}

json_api_inline bool json_mut_is_arr(json_mut_val *val) {
    return val ? unsafe_json_is_arr(val) : false;
}

json_api_inline bool json_mut_is_obj(json_mut_val *val) {
    return val ? unsafe_json_is_obj(val) : false;
}

json_api_inline bool json_mut_is_ctn(json_mut_val *val) {
    return val ? unsafe_json_is_ctn(val) : false;
}



/*==============================================================================
 * Mutable JSON Value Content API (Implementation)
 *============================================================================*/

json_api_inline json_type json_mut_get_type(json_mut_val *val) {
    return json_get_type((json_val *)val);
}

json_api_inline json_subtype json_mut_get_subtype(json_mut_val *val) {
    return json_get_subtype((json_val *)val);
}

json_api_inline uint8_t json_mut_get_tag(json_mut_val *val) {
    return json_get_tag((json_val *)val);
}

json_api_inline const char *json_mut_get_type_desc(json_mut_val *val) {
    return json_get_type_desc((json_val *)val);
}

json_api_inline const char *json_mut_get_raw(json_mut_val *val) {
    return json_get_raw((json_val *)val);
}

json_api_inline bool json_mut_get_bool(json_mut_val *val) {
    return json_get_bool((json_val *)val);
}

json_api_inline uint64_t json_mut_get_uint(json_mut_val *val) {
    return json_get_uint((json_val *)val);
}

json_api_inline int64_t json_mut_get_sint(json_mut_val *val) {
    return json_get_sint((json_val *)val);
}

json_api_inline int json_mut_get_int(json_mut_val *val) {
    return json_get_int((json_val *)val);
}

json_api_inline double json_mut_get_real(json_mut_val *val) {
    return json_get_real((json_val *)val);
}

json_api_inline double json_mut_get_num(json_mut_val *val) {
    return json_get_num((json_val *)val);
}

json_api_inline const char *json_mut_get_str(json_mut_val *val) {
    return json_get_str((json_val *)val);
}

json_api_inline size_t json_mut_get_len(json_mut_val *val) {
    return json_get_len((json_val *)val);
}

json_api_inline bool json_mut_equals_str(json_mut_val *val,
                                             const char *str) {
    return json_equals_str((json_val *)val, str);
}

json_api_inline bool json_mut_equals_strn(json_mut_val *val,
                                              const char *str, size_t len) {
    return json_equals_strn((json_val *)val, str, len);
}

json_api bool unsafe_json_mut_equals(json_mut_val *lhs,
                                         json_mut_val *rhs);

json_api_inline bool json_mut_equals(json_mut_val *lhs,
                                         json_mut_val *rhs) {
    if (json_unlikely(!lhs || !rhs)) return false;
    return unsafe_json_mut_equals(lhs, rhs);
}

json_api_inline bool json_mut_set_raw(json_mut_val *val,
                                          const char *raw, size_t len) {
    if (json_unlikely(!val || !raw)) return false;
    unsafe_json_set_raw(val, raw, len);
    return true;
}

json_api_inline bool json_mut_set_null(json_mut_val *val) {
    if (json_unlikely(!val)) return false;
    unsafe_json_set_null(val);
    return true;
}

json_api_inline bool json_mut_set_bool(json_mut_val *val, bool num) {
    if (json_unlikely(!val)) return false;
    unsafe_json_set_bool(val, num);
    return true;
}

json_api_inline bool json_mut_set_uint(json_mut_val *val, uint64_t num) {
    if (json_unlikely(!val)) return false;
    unsafe_json_set_uint(val, num);
    return true;
}

json_api_inline bool json_mut_set_sint(json_mut_val *val, int64_t num) {
    if (json_unlikely(!val)) return false;
    unsafe_json_set_sint(val, num);
    return true;
}

json_api_inline bool json_mut_set_int(json_mut_val *val, int num) {
    if (json_unlikely(!val)) return false;
    unsafe_json_set_sint(val, (int64_t)num);
    return true;
}

json_api_inline bool json_mut_set_real(json_mut_val *val, double num) {
    if (json_unlikely(!val)) return false;
    unsafe_json_set_real(val, num);
    return true;
}

json_api_inline bool json_mut_set_str(json_mut_val *val,
                                          const char *str) {
    if (json_unlikely(!val || !str)) return false;
    unsafe_json_set_str(val, str);
    return true;
}

json_api_inline bool json_mut_set_strn(json_mut_val *val,
                                           const char *str, size_t len) {
    if (json_unlikely(!val || !str)) return false;
    unsafe_json_set_strn(val, str, len);
    return true;
}

json_api_inline bool json_mut_set_arr(json_mut_val *val) {
    if (json_unlikely(!val)) return false;
    unsafe_json_set_arr(val, 0);
    return true;
}

json_api_inline bool json_mut_set_obj(json_mut_val *val) {
    if (json_unlikely(!val)) return false;
    unsafe_json_set_obj(val, 0);
    return true;
}



/*==============================================================================
 * Mutable JSON Value Creation API (Implementation)
 *============================================================================*/

json_api_inline json_mut_val *json_mut_raw(json_mut_doc *doc,
                                                 const char *str) {
    if (json_likely(str)) return json_mut_rawn(doc, str, strlen(str));
    return NULL;
}

json_api_inline json_mut_val *json_mut_rawn(json_mut_doc *doc,
                                                  const char *str,
                                                  size_t len) {
    if (json_likely(doc && str)) {
        json_mut_val *val = unsafe_json_mut_val(doc, 1);
        if (json_likely(val)) {
            val->tag = ((uint64_t)len << JSON_TAG_BIT) | JSON_TYPE_RAW;
            val->uni.str = str;
            return val;
        }
    }
    return NULL;
}

json_api_inline json_mut_val *json_mut_rawcpy(json_mut_doc *doc,
                                                    const char *str) {
    if (json_likely(str)) return json_mut_rawncpy(doc, str, strlen(str));
    return NULL;
}

json_api_inline json_mut_val *json_mut_rawncpy(json_mut_doc *doc,
                                                     const char *str,
                                                     size_t len) {
    if (json_likely(doc && str)) {
        json_mut_val *val = unsafe_json_mut_val(doc, 1);
        char *new_str = unsafe_json_mut_strncpy(doc, str, len);
        if (json_likely(val && new_str)) {
            val->tag = ((uint64_t)len << JSON_TAG_BIT) | JSON_TYPE_RAW;
            val->uni.str = new_str;
            return val;
        }
    }
    return NULL;
}

json_api_inline json_mut_val *json_mut_null(json_mut_doc *doc) {
    if (json_likely(doc)) {
        json_mut_val *val = unsafe_json_mut_val(doc, 1);
        if (json_likely(val)) {
            val->tag = JSON_TYPE_NULL | JSON_SUBTYPE_NONE;
            return val;
        }
    }
    return NULL;
}

json_api_inline json_mut_val *json_mut_true(json_mut_doc *doc) {
    if (json_likely(doc)) {
        json_mut_val *val = unsafe_json_mut_val(doc, 1);
        if (json_likely(val)) {
            val->tag = JSON_TYPE_BOOL | JSON_SUBTYPE_TRUE;
            return val;
        }
    }
    return NULL;
}

json_api_inline json_mut_val *json_mut_false(json_mut_doc *doc) {
    if (json_likely(doc)) {
        json_mut_val *val = unsafe_json_mut_val(doc, 1);
        if (json_likely(val)) {
            val->tag = JSON_TYPE_BOOL | JSON_SUBTYPE_FALSE;
            return val;
        }
    }
    return NULL;
}

json_api_inline json_mut_val *json_mut_bool(json_mut_doc *doc,
                                                  bool _val) {
    if (json_likely(doc)) {
        json_mut_val *val = unsafe_json_mut_val(doc, 1);
        if (json_likely(val)) {
            _val = !!_val;
            val->tag = JSON_TYPE_BOOL | (uint8_t)((uint8_t)_val << 3);
            return val;
        }
    }
    return NULL;
}

json_api_inline json_mut_val *json_mut_uint(json_mut_doc *doc,
                                                  uint64_t num) {
    if (json_likely(doc)) {
        json_mut_val *val = unsafe_json_mut_val(doc, 1);
        if (json_likely(val)) {
            val->tag = JSON_TYPE_NUM | JSON_SUBTYPE_UINT;
            val->uni.u64 = num;
            return val;
        }
    }
    return NULL;
}

json_api_inline json_mut_val *json_mut_sint(json_mut_doc *doc,
                                                  int64_t num) {
    if (json_likely(doc)) {
        json_mut_val *val = unsafe_json_mut_val(doc, 1);
        if (json_likely(val)) {
            val->tag = JSON_TYPE_NUM | JSON_SUBTYPE_SINT;
            val->uni.i64 = num;
            return val;
        }
    }
    return NULL;
}

json_api_inline json_mut_val *json_mut_int(json_mut_doc *doc,
                                                 int64_t num) {
    return json_mut_sint(doc, num);
}

json_api_inline json_mut_val *json_mut_real(json_mut_doc *doc,
                                                  double num) {
    if (json_likely(doc)) {
        json_mut_val *val = unsafe_json_mut_val(doc, 1);
        if (json_likely(val)) {
            val->tag = JSON_TYPE_NUM | JSON_SUBTYPE_REAL;
            val->uni.f64 = num;
            return val;
        }
    }
    return NULL;
}

json_api_inline json_mut_val *json_mut_str(json_mut_doc *doc,
                                                 const char *str) {
    if (json_likely(doc && str)) {
        size_t len = strlen(str);
        bool noesc = unsafe_json_is_str_noesc(str, len);
        json_subtype sub = noesc ? JSON_SUBTYPE_NOESC : JSON_SUBTYPE_NONE;
        json_mut_val *val = unsafe_json_mut_val(doc, 1);
        if (json_likely(val)) {
            val->tag = ((uint64_t)len << JSON_TAG_BIT) |
                        (uint64_t)(JSON_TYPE_STR | sub);
            val->uni.str = str;
            return val;
        }
    }
    return NULL;
}

json_api_inline json_mut_val *json_mut_strn(json_mut_doc *doc,
                                                  const char *str,
                                                  size_t len) {
    if (json_likely(doc && str)) {
        json_mut_val *val = unsafe_json_mut_val(doc, 1);
        if (json_likely(val)) {
            val->tag = ((uint64_t)len << JSON_TAG_BIT) | JSON_TYPE_STR;
            val->uni.str = str;
            return val;
        }
    }
    return NULL;
}

json_api_inline json_mut_val *json_mut_strcpy(json_mut_doc *doc,
                                                    const char *str) {
    if (json_likely(doc && str)) {
        size_t len = strlen(str);
        bool noesc = unsafe_json_is_str_noesc(str, len);
        json_subtype sub = noesc ? JSON_SUBTYPE_NOESC : JSON_SUBTYPE_NONE;
        json_mut_val *val = unsafe_json_mut_val(doc, 1);
        char *new_str = unsafe_json_mut_strncpy(doc, str, len);
        if (json_likely(val && new_str)) {
            val->tag = ((uint64_t)len << JSON_TAG_BIT) |
                        (uint64_t)(JSON_TYPE_STR | sub);
            val->uni.str = new_str;
            return val;
        }
    }
    return NULL;
}

json_api_inline json_mut_val *json_mut_strncpy(json_mut_doc *doc,
                                                     const char *str,
                                                     size_t len) {
    if (json_likely(doc && str)) {
        json_mut_val *val = unsafe_json_mut_val(doc, 1);
        char *new_str = unsafe_json_mut_strncpy(doc, str, len);
        if (json_likely(val && new_str)) {
            val->tag = ((uint64_t)len << JSON_TAG_BIT) | JSON_TYPE_STR;
            val->uni.str = new_str;
            return val;
        }
    }
    return NULL;
}



/*==============================================================================
 * Mutable JSON Array API (Implementation)
 *============================================================================*/

json_api_inline size_t json_mut_arr_size(json_mut_val *arr) {
    return json_mut_is_arr(arr) ? unsafe_json_get_len(arr) : 0;
}

json_api_inline json_mut_val *json_mut_arr_get(json_mut_val *arr,
                                                     size_t idx) {
    if (json_likely(idx < json_mut_arr_size(arr))) {
        json_mut_val *val = (json_mut_val *)arr->uni.ptr;
        while (idx-- > 0) val = val->next;
        return val->next;
    }
    return NULL;
}

json_api_inline json_mut_val *json_mut_arr_get_first(
    json_mut_val *arr) {
    if (json_likely(json_mut_arr_size(arr) > 0)) {
        return ((json_mut_val *)arr->uni.ptr)->next;
    }
    return NULL;
}

json_api_inline json_mut_val *json_mut_arr_get_last(
    json_mut_val *arr) {
    if (json_likely(json_mut_arr_size(arr) > 0)) {
        return ((json_mut_val *)arr->uni.ptr);
    }
    return NULL;
}



/*==============================================================================
 * Mutable JSON Array Iterator API (Implementation)
 *============================================================================*/

json_api_inline bool json_mut_arr_iter_init(json_mut_val *arr,
                                                json_mut_arr_iter *iter) {
    if (json_likely(json_mut_is_arr(arr) && iter)) {
        iter->idx = 0;
        iter->max = unsafe_json_get_len(arr);
        iter->cur = iter->max ? (json_mut_val *)arr->uni.ptr : NULL;
        iter->pre = NULL;
        iter->arr = arr;
        return true;
    }
    if (iter) memset(iter, 0, sizeof(json_mut_arr_iter));
    return false;
}

json_api_inline json_mut_arr_iter json_mut_arr_iter_with(
    json_mut_val *arr) {
    json_mut_arr_iter iter;
    json_mut_arr_iter_init(arr, &iter);
    return iter;
}

json_api_inline bool json_mut_arr_iter_has_next(json_mut_arr_iter *iter) {
    return iter ? iter->idx < iter->max : false;
}

json_api_inline json_mut_val *json_mut_arr_iter_next(
    json_mut_arr_iter *iter) {
    if (iter && iter->idx < iter->max) {
        json_mut_val *val = iter->cur;
        iter->pre = val;
        iter->cur = val->next;
        iter->idx++;
        return iter->cur;
    }
    return NULL;
}

json_api_inline json_mut_val *json_mut_arr_iter_remove(
    json_mut_arr_iter *iter) {
    if (json_likely(iter && 0 < iter->idx && iter->idx <= iter->max)) {
        json_mut_val *prev = iter->pre;
        json_mut_val *cur = iter->cur;
        json_mut_val *next = cur->next;
        if (json_unlikely(iter->idx == iter->max)) iter->arr->uni.ptr = prev;
        iter->idx--;
        iter->max--;
        unsafe_json_set_len(iter->arr, iter->max);
        prev->next = next;
        iter->cur = next;
        return cur;
    }
    return NULL;
}



/*==============================================================================
 * Mutable JSON Array Creation API (Implementation)
 *============================================================================*/

json_api_inline json_mut_val *json_mut_arr(json_mut_doc *doc) {
    if (json_likely(doc)) {
        json_mut_val *val = unsafe_json_mut_val(doc, 1);
        if (json_likely(val)) {
            val->tag = JSON_TYPE_ARR | JSON_SUBTYPE_NONE;
            return val;
        }
    }
    return NULL;
}

#define json_mut_arr_with_func(func) \
    if (json_likely(doc && ((0 < count && count < \
        (~(size_t)0) / sizeof(json_mut_val) && vals) || count == 0))) { \
        json_mut_val *arr = unsafe_json_mut_val(doc, 1 + count); \
        if (json_likely(arr)) { \
            arr->tag = ((uint64_t)count << JSON_TAG_BIT) | JSON_TYPE_ARR; \
            if (count > 0) { \
                size_t i; \
                for (i = 0; i < count; i++) { \
                    json_mut_val *val = arr + i + 1; \
                    func \
                    val->next = val + 1; \
                } \
                arr[count].next = arr + 1; \
                arr->uni.ptr = arr + count; \
            } \
            return arr; \
        } \
    } \
    return NULL

json_api_inline json_mut_val *json_mut_arr_with_bool(
    json_mut_doc *doc, const bool *vals, size_t count) {
    json_mut_arr_with_func({
        bool _val = !!vals[i];
        val->tag = JSON_TYPE_BOOL | (uint8_t)((uint8_t)_val << 3);
    });
}

json_api_inline json_mut_val *json_mut_arr_with_sint(
    json_mut_doc *doc, const int64_t *vals, size_t count) {
    return json_mut_arr_with_sint64(doc, vals, count);
}

json_api_inline json_mut_val *json_mut_arr_with_uint(
    json_mut_doc *doc, const uint64_t *vals, size_t count) {
    return json_mut_arr_with_uint64(doc, vals, count);
}

json_api_inline json_mut_val *json_mut_arr_with_real(
    json_mut_doc *doc, const double *vals, size_t count) {
    return json_mut_arr_with_double(doc, vals, count);
}

json_api_inline json_mut_val *json_mut_arr_with_sint8(
    json_mut_doc *doc, const int8_t *vals, size_t count) {
    json_mut_arr_with_func({
        val->tag = JSON_TYPE_NUM | JSON_SUBTYPE_SINT;
        val->uni.i64 = (int64_t)vals[i];
    });
}

json_api_inline json_mut_val *json_mut_arr_with_sint16(
    json_mut_doc *doc, const int16_t *vals, size_t count) {
    json_mut_arr_with_func({
        val->tag = JSON_TYPE_NUM | JSON_SUBTYPE_SINT;
        val->uni.i64 = vals[i];
    });
}

json_api_inline json_mut_val *json_mut_arr_with_sint32(
    json_mut_doc *doc, const int32_t *vals, size_t count) {
    json_mut_arr_with_func({
        val->tag = JSON_TYPE_NUM | JSON_SUBTYPE_SINT;
        val->uni.i64 = vals[i];
    });
}

json_api_inline json_mut_val *json_mut_arr_with_sint64(
    json_mut_doc *doc, const int64_t *vals, size_t count) {
    json_mut_arr_with_func({
        val->tag = JSON_TYPE_NUM | JSON_SUBTYPE_SINT;
        val->uni.i64 = vals[i];
    });
}

json_api_inline json_mut_val *json_mut_arr_with_uint8(
    json_mut_doc *doc, const uint8_t *vals, size_t count) {
    json_mut_arr_with_func({
        val->tag = JSON_TYPE_NUM | JSON_SUBTYPE_UINT;
        val->uni.u64 = vals[i];
    });
}

json_api_inline json_mut_val *json_mut_arr_with_uint16(
    json_mut_doc *doc, const uint16_t *vals, size_t count) {
    json_mut_arr_with_func({
        val->tag = JSON_TYPE_NUM | JSON_SUBTYPE_UINT;
        val->uni.u64 = vals[i];
    });
}

json_api_inline json_mut_val *json_mut_arr_with_uint32(
    json_mut_doc *doc, const uint32_t *vals, size_t count) {
    json_mut_arr_with_func({
        val->tag = JSON_TYPE_NUM | JSON_SUBTYPE_UINT;
        val->uni.u64 = vals[i];
    });
}

json_api_inline json_mut_val *json_mut_arr_with_uint64(
    json_mut_doc *doc, const uint64_t *vals, size_t count) {
    json_mut_arr_with_func({
        val->tag = JSON_TYPE_NUM | JSON_SUBTYPE_UINT;
        val->uni.u64 = vals[i];
    });
}

json_api_inline json_mut_val *json_mut_arr_with_float(
    json_mut_doc *doc, const float *vals, size_t count) {
    json_mut_arr_with_func({
        val->tag = JSON_TYPE_NUM | JSON_SUBTYPE_REAL;
        val->uni.f64 = (double)vals[i];
    });
}

json_api_inline json_mut_val *json_mut_arr_with_double(
    json_mut_doc *doc, const double *vals, size_t count) {
    json_mut_arr_with_func({
        val->tag = JSON_TYPE_NUM | JSON_SUBTYPE_REAL;
        val->uni.f64 = vals[i];
    });
}

json_api_inline json_mut_val *json_mut_arr_with_str(
    json_mut_doc *doc, const char **vals, size_t count) {
    json_mut_arr_with_func({
        uint64_t len = (uint64_t)strlen(vals[i]);
        val->tag = (len << JSON_TAG_BIT) | JSON_TYPE_STR;
        val->uni.str = vals[i];
        if (json_unlikely(!val->uni.str)) return NULL;
    });
}

json_api_inline json_mut_val *json_mut_arr_with_strn(
    json_mut_doc *doc, const char **vals, const size_t *lens, size_t count) {
    if (json_unlikely(count > 0 && !lens)) return NULL;
    json_mut_arr_with_func({
        val->tag = ((uint64_t)lens[i] << JSON_TAG_BIT) | JSON_TYPE_STR;
        val->uni.str = vals[i];
        if (json_unlikely(!val->uni.str)) return NULL;
    });
}

json_api_inline json_mut_val *json_mut_arr_with_strcpy(
    json_mut_doc *doc, const char **vals, size_t count) {
    size_t len;
    const char *str;
    json_mut_arr_with_func({
        str = vals[i];
        if (!str) return NULL;
        len = strlen(str);
        val->tag = ((uint64_t)len << JSON_TAG_BIT) | JSON_TYPE_STR;
        val->uni.str = unsafe_json_mut_strncpy(doc, str, len);
        if (json_unlikely(!val->uni.str)) return NULL;
    });
}

json_api_inline json_mut_val *json_mut_arr_with_strncpy(
    json_mut_doc *doc, const char **vals, const size_t *lens, size_t count) {
    size_t len;
    const char *str;
    if (json_unlikely(count > 0 && !lens)) return NULL;
    json_mut_arr_with_func({
        str = vals[i];
        len = lens[i];
        val->tag = ((uint64_t)len << JSON_TAG_BIT) | JSON_TYPE_STR;
        val->uni.str = unsafe_json_mut_strncpy(doc, str, len);
        if (json_unlikely(!val->uni.str)) return NULL;
    });
}

#undef json_mut_arr_with_func



/*==============================================================================
 * Mutable JSON Array Modification API (Implementation)
 *============================================================================*/

json_api_inline bool json_mut_arr_insert(json_mut_val *arr,
                                             json_mut_val *val, size_t idx) {
    if (json_likely(json_mut_is_arr(arr) && val)) {
        size_t len = unsafe_json_get_len(arr);
        if (json_likely(idx <= len)) {
            unsafe_json_set_len(arr, len + 1);
            if (len == 0) {
                val->next = val;
                arr->uni.ptr = val;
            } else {
                json_mut_val *prev = ((json_mut_val *)arr->uni.ptr);
                json_mut_val *next = prev->next;
                if (idx == len) {
                    prev->next = val;
                    val->next = next;
                    arr->uni.ptr = val;
                } else {
                    while (idx-- > 0) {
                        prev = next;
                        next = next->next;
                    }
                    prev->next = val;
                    val->next = next;
                }
            }
            return true;
        }
    }
    return false;
}

json_api_inline bool json_mut_arr_append(json_mut_val *arr,
                                             json_mut_val *val) {
    if (json_likely(json_mut_is_arr(arr) && val)) {
        size_t len = unsafe_json_get_len(arr);
        unsafe_json_set_len(arr, len + 1);
        if (len == 0) {
            val->next = val;
        } else {
            json_mut_val *prev = ((json_mut_val *)arr->uni.ptr);
            json_mut_val *next = prev->next;
            prev->next = val;
            val->next = next;
        }
        arr->uni.ptr = val;
        return true;
    }
    return false;
}

json_api_inline bool json_mut_arr_prepend(json_mut_val *arr,
                                              json_mut_val *val) {
    if (json_likely(json_mut_is_arr(arr) && val)) {
        size_t len = unsafe_json_get_len(arr);
        unsafe_json_set_len(arr, len + 1);
        if (len == 0) {
            val->next = val;
            arr->uni.ptr = val;
        } else {
            json_mut_val *prev = ((json_mut_val *)arr->uni.ptr);
            json_mut_val *next = prev->next;
            prev->next = val;
            val->next = next;
        }
        return true;
    }
    return false;
}

json_api_inline json_mut_val *json_mut_arr_replace(json_mut_val *arr,
                                                         size_t idx,
                                                         json_mut_val *val) {
    if (json_likely(json_mut_is_arr(arr) && val)) {
        size_t len = unsafe_json_get_len(arr);
        if (json_likely(idx < len)) {
            if (json_likely(len > 1)) {
                json_mut_val *prev = ((json_mut_val *)arr->uni.ptr);
                json_mut_val *next = prev->next;
                while (idx-- > 0) {
                    prev = next;
                    next = next->next;
                }
                prev->next = val;
                val->next = next->next;
                if ((void *)next == arr->uni.ptr) arr->uni.ptr = val;
                return next;
            } else {
                json_mut_val *prev = ((json_mut_val *)arr->uni.ptr);
                val->next = val;
                arr->uni.ptr = val;
                return prev;
            }
        }
    }
    return NULL;
}

json_api_inline json_mut_val *json_mut_arr_remove(json_mut_val *arr,
                                                        size_t idx) {
    if (json_likely(json_mut_is_arr(arr))) {
        size_t len = unsafe_json_get_len(arr);
        if (json_likely(idx < len)) {
            unsafe_json_set_len(arr, len - 1);
            if (json_likely(len > 1)) {
                json_mut_val *prev = ((json_mut_val *)arr->uni.ptr);
                json_mut_val *next = prev->next;
                while (idx-- > 0) {
                    prev = next;
                    next = next->next;
                }
                prev->next = next->next;
                if ((void *)next == arr->uni.ptr) arr->uni.ptr = prev;
                return next;
            } else {
                return ((json_mut_val *)arr->uni.ptr);
            }
        }
    }
    return NULL;
}

json_api_inline json_mut_val *json_mut_arr_remove_first(
    json_mut_val *arr) {
    if (json_likely(json_mut_is_arr(arr))) {
        size_t len = unsafe_json_get_len(arr);
        if (len > 1) {
            json_mut_val *prev = ((json_mut_val *)arr->uni.ptr);
            json_mut_val *next = prev->next;
            prev->next = next->next;
            unsafe_json_set_len(arr, len - 1);
            return next;
        } else if (len == 1) {
            json_mut_val *prev = ((json_mut_val *)arr->uni.ptr);
            unsafe_json_set_len(arr, 0);
            return prev;
        }
    }
    return NULL;
}

json_api_inline json_mut_val *json_mut_arr_remove_last(
    json_mut_val *arr) {
    if (json_likely(json_mut_is_arr(arr))) {
        size_t len = unsafe_json_get_len(arr);
        if (json_likely(len > 1)) {
            json_mut_val *prev = ((json_mut_val *)arr->uni.ptr);
            json_mut_val *next = prev->next;
            unsafe_json_set_len(arr, len - 1);
            while (--len > 0) prev = prev->next;
            prev->next = next;
            next = (json_mut_val *)arr->uni.ptr;
            arr->uni.ptr = prev;
            return next;
        } else if (len == 1) {
            json_mut_val *prev = ((json_mut_val *)arr->uni.ptr);
            unsafe_json_set_len(arr, 0);
            return prev;
        }
    }
    return NULL;
}

json_api_inline bool json_mut_arr_remove_range(json_mut_val *arr,
                                                   size_t _idx, size_t _len) {
    if (json_likely(json_mut_is_arr(arr))) {
        json_mut_val *prev, *next;
        bool tail_removed;
        size_t len = unsafe_json_get_len(arr);
        if (json_unlikely(_idx + _len > len)) return false;
        if (json_unlikely(_len == 0)) return true;
        unsafe_json_set_len(arr, len - _len);
        if (json_unlikely(len == _len)) return true;
        tail_removed = (_idx + _len == len);
        prev = ((json_mut_val *)arr->uni.ptr);
        while (_idx-- > 0) prev = prev->next;
        next = prev->next;
        while (_len-- > 0) next = next->next;
        prev->next = next;
        if (json_unlikely(tail_removed)) arr->uni.ptr = prev;
        return true;
    }
    return false;
}

json_api_inline bool json_mut_arr_clear(json_mut_val *arr) {
    if (json_likely(json_mut_is_arr(arr))) {
        unsafe_json_set_len(arr, 0);
        return true;
    }
    return false;
}

json_api_inline bool json_mut_arr_rotate(json_mut_val *arr,
                                             size_t idx) {
    if (json_likely(json_mut_is_arr(arr) &&
                      unsafe_json_get_len(arr) > idx)) {
        json_mut_val *val = (json_mut_val *)arr->uni.ptr;
        while (idx-- > 0) val = val->next;
        arr->uni.ptr = (void *)val;
        return true;
    }
    return false;
}



/*==============================================================================
 * Mutable JSON Array Modification Convenience API (Implementation)
 *============================================================================*/

json_api_inline bool json_mut_arr_add_val(json_mut_val *arr,
                                              json_mut_val *val) {
    return json_mut_arr_append(arr, val);
}

json_api_inline bool json_mut_arr_add_null(json_mut_doc *doc,
                                               json_mut_val *arr) {
    if (json_likely(doc && json_mut_is_arr(arr))) {
        json_mut_val *val = json_mut_null(doc);
        return json_mut_arr_append(arr, val);
    }
    return false;
}

json_api_inline bool json_mut_arr_add_true(json_mut_doc *doc,
                                               json_mut_val *arr) {
    if (json_likely(doc && json_mut_is_arr(arr))) {
        json_mut_val *val = json_mut_true(doc);
        return json_mut_arr_append(arr, val);
    }
    return false;
}

json_api_inline bool json_mut_arr_add_false(json_mut_doc *doc,
                                                json_mut_val *arr) {
    if (json_likely(doc && json_mut_is_arr(arr))) {
        json_mut_val *val = json_mut_false(doc);
        return json_mut_arr_append(arr, val);
    }
    return false;
}

json_api_inline bool json_mut_arr_add_bool(json_mut_doc *doc,
                                               json_mut_val *arr,
                                               bool _val) {
    if (json_likely(doc && json_mut_is_arr(arr))) {
        json_mut_val *val = json_mut_bool(doc, _val);
        return json_mut_arr_append(arr, val);
    }
    return false;
}

json_api_inline bool json_mut_arr_add_uint(json_mut_doc *doc,
                                               json_mut_val *arr,
                                               uint64_t num) {
    if (json_likely(doc && json_mut_is_arr(arr))) {
        json_mut_val *val = json_mut_uint(doc, num);
        return json_mut_arr_append(arr, val);
    }
    return false;
}

json_api_inline bool json_mut_arr_add_sint(json_mut_doc *doc,
                                               json_mut_val *arr,
                                               int64_t num) {
    if (json_likely(doc && json_mut_is_arr(arr))) {
        json_mut_val *val = json_mut_sint(doc, num);
        return json_mut_arr_append(arr, val);
    }
    return false;
}

json_api_inline bool json_mut_arr_add_int(json_mut_doc *doc,
                                              json_mut_val *arr,
                                              int64_t num) {
    if (json_likely(doc && json_mut_is_arr(arr))) {
        json_mut_val *val = json_mut_sint(doc, num);
        return json_mut_arr_append(arr, val);
    }
    return false;
}

json_api_inline bool json_mut_arr_add_real(json_mut_doc *doc,
                                               json_mut_val *arr,
                                               double num) {
    if (json_likely(doc && json_mut_is_arr(arr))) {
        json_mut_val *val = json_mut_real(doc, num);
        return json_mut_arr_append(arr, val);
    }
    return false;
}

json_api_inline bool json_mut_arr_add_str(json_mut_doc *doc,
                                              json_mut_val *arr,
                                              const char *str) {
    if (json_likely(doc && json_mut_is_arr(arr))) {
        json_mut_val *val = json_mut_str(doc, str);
        return json_mut_arr_append(arr, val);
    }
    return false;
}

json_api_inline bool json_mut_arr_add_strn(json_mut_doc *doc,
                                               json_mut_val *arr,
                                               const char *str, size_t len) {
    if (json_likely(doc && json_mut_is_arr(arr))) {
        json_mut_val *val = json_mut_strn(doc, str, len);
        return json_mut_arr_append(arr, val);
    }
    return false;
}

json_api_inline bool json_mut_arr_add_strcpy(json_mut_doc *doc,
                                                 json_mut_val *arr,
                                                 const char *str) {
    if (json_likely(doc && json_mut_is_arr(arr))) {
        json_mut_val *val = json_mut_strcpy(doc, str);
        return json_mut_arr_append(arr, val);
    }
    return false;
}

json_api_inline bool json_mut_arr_add_strncpy(json_mut_doc *doc,
                                                  json_mut_val *arr,
                                                  const char *str, size_t len) {
    if (json_likely(doc && json_mut_is_arr(arr))) {
        json_mut_val *val = json_mut_strncpy(doc, str, len);
        return json_mut_arr_append(arr, val);
    }
    return false;
}

json_api_inline json_mut_val *json_mut_arr_add_arr(json_mut_doc *doc,
                                                         json_mut_val *arr) {
    if (json_likely(doc && json_mut_is_arr(arr))) {
        json_mut_val *val = json_mut_arr(doc);
        return json_mut_arr_append(arr, val) ? val : NULL;
    }
    return NULL;
}

json_api_inline json_mut_val *json_mut_arr_add_obj(json_mut_doc *doc,
                                                         json_mut_val *arr) {
    if (json_likely(doc && json_mut_is_arr(arr))) {
        json_mut_val *val = json_mut_obj(doc);
        return json_mut_arr_append(arr, val) ? val : NULL;
    }
    return NULL;
}



/*==============================================================================
 * Mutable JSON Object API (Implementation)
 *============================================================================*/

json_api_inline size_t json_mut_obj_size(json_mut_val *obj) {
    return json_mut_is_obj(obj) ? unsafe_json_get_len(obj) : 0;
}

json_api_inline json_mut_val *json_mut_obj_get(json_mut_val *obj,
                                                     const char *key) {
    return json_mut_obj_getn(obj, key, key ? strlen(key) : 0);
}

json_api_inline json_mut_val *json_mut_obj_getn(json_mut_val *obj,
                                                      const char *_key,
                                                      size_t key_len) {
    size_t len = json_mut_obj_size(obj);
    if (json_likely(len && _key)) {
        json_mut_val *key = ((json_mut_val *)obj->uni.ptr)->next->next;
        while (len-- > 0) {
            if (unsafe_json_equals_strn(key, _key, key_len)) return key->next;
            key = key->next->next;
        }
    }
    return NULL;
}



/*==============================================================================
 * Mutable JSON Object Iterator API (Implementation)
 *============================================================================*/

json_api_inline bool json_mut_obj_iter_init(json_mut_val *obj,
                                                json_mut_obj_iter *iter) {
    if (json_likely(json_mut_is_obj(obj) && iter)) {
        iter->idx = 0;
        iter->max = unsafe_json_get_len(obj);
        iter->cur = iter->max ? (json_mut_val *)obj->uni.ptr : NULL;
        iter->pre = NULL;
        iter->obj = obj;
        return true;
    }
    if (iter) memset(iter, 0, sizeof(json_mut_obj_iter));
    return false;
}

json_api_inline json_mut_obj_iter json_mut_obj_iter_with(
    json_mut_val *obj) {
    json_mut_obj_iter iter;
    json_mut_obj_iter_init(obj, &iter);
    return iter;
}

json_api_inline bool json_mut_obj_iter_has_next(json_mut_obj_iter *iter) {
    return iter ? iter->idx < iter->max : false;
}

json_api_inline json_mut_val *json_mut_obj_iter_next(
    json_mut_obj_iter *iter) {
    if (iter && iter->idx < iter->max) {
        json_mut_val *key = iter->cur;
        iter->pre = key;
        iter->cur = key->next->next;
        iter->idx++;
        return iter->cur;
    }
    return NULL;
}

json_api_inline json_mut_val *json_mut_obj_iter_get_val(
    json_mut_val *key) {
    return key ? key->next : NULL;
}

json_api_inline json_mut_val *json_mut_obj_iter_remove(
    json_mut_obj_iter *iter) {
    if (json_likely(iter && 0 < iter->idx && iter->idx <= iter->max)) {
        json_mut_val *prev = iter->pre;
        json_mut_val *cur = iter->cur;
        json_mut_val *next = cur->next->next;
        if (json_unlikely(iter->idx == iter->max)) iter->obj->uni.ptr = prev;
        iter->idx--;
        iter->max--;
        unsafe_json_set_len(iter->obj, iter->max);
        prev->next->next = next;
        iter->cur = prev;
        return cur->next;
    }
    return NULL;
}

json_api_inline json_mut_val *json_mut_obj_iter_get(
    json_mut_obj_iter *iter, const char *key) {
    return json_mut_obj_iter_getn(iter, key, key ? strlen(key) : 0);
}

json_api_inline json_mut_val *json_mut_obj_iter_getn(
    json_mut_obj_iter *iter, const char *key, size_t key_len) {
    if (iter && key) {
        size_t idx = 0;
        size_t max = iter->max;
        json_mut_val *pre, *cur = iter->cur;
        while (idx++ < max) {
            pre = cur;
            cur = cur->next->next;
            if (unsafe_json_equals_strn(cur, key, key_len)) {
                iter->idx += idx;
                if (iter->idx > max) iter->idx -= max + 1;
                iter->pre = pre;
                iter->cur = cur;
                return cur->next;
            }
        }
    }
    return NULL;
}



/*==============================================================================
 * Mutable JSON Object Creation API (Implementation)
 *============================================================================*/

json_api_inline json_mut_val *json_mut_obj(json_mut_doc *doc) {
    if (json_likely(doc)) {
        json_mut_val *val = unsafe_json_mut_val(doc, 1);
        if (json_likely(val)) {
            val->tag = JSON_TYPE_OBJ | JSON_SUBTYPE_NONE;
            return val;
        }
    }
    return NULL;
}

json_api_inline json_mut_val *json_mut_obj_with_str(json_mut_doc *doc,
                                                          const char **keys,
                                                          const char **vals,
                                                          size_t count) {
    if (json_likely(doc && ((count > 0 && keys && vals) || (count == 0)))) {
        json_mut_val *obj = unsafe_json_mut_val(doc, 1 + count * 2);
        if (json_likely(obj)) {
            obj->tag = ((uint64_t)count << JSON_TAG_BIT) | JSON_TYPE_OBJ;
            if (count > 0) {
                size_t i;
                for (i = 0; i < count; i++) {
                    json_mut_val *key = obj + (i * 2 + 1);
                    json_mut_val *val = obj + (i * 2 + 2);
                    uint64_t key_len = (uint64_t)strlen(keys[i]);
                    uint64_t val_len = (uint64_t)strlen(vals[i]);
                    key->tag = (key_len << JSON_TAG_BIT) | JSON_TYPE_STR;
                    val->tag = (val_len << JSON_TAG_BIT) | JSON_TYPE_STR;
                    key->uni.str = keys[i];
                    val->uni.str = vals[i];
                    key->next = val;
                    val->next = val + 1;
                }
                obj[count * 2].next = obj + 1;
                obj->uni.ptr = obj + (count * 2 - 1);
            }
            return obj;
        }
    }
    return NULL;
}

json_api_inline json_mut_val *json_mut_obj_with_kv(json_mut_doc *doc,
                                                         const char **pairs,
                                                         size_t count) {
    if (json_likely(doc && ((count > 0 && pairs) || (count == 0)))) {
        json_mut_val *obj = unsafe_json_mut_val(doc, 1 + count * 2);
        if (json_likely(obj)) {
            obj->tag = ((uint64_t)count << JSON_TAG_BIT) | JSON_TYPE_OBJ;
            if (count > 0) {
                size_t i;
                for (i = 0; i < count; i++) {
                    json_mut_val *key = obj + (i * 2 + 1);
                    json_mut_val *val = obj + (i * 2 + 2);
                    const char *key_str = pairs[i * 2 + 0];
                    const char *val_str = pairs[i * 2 + 1];
                    uint64_t key_len = (uint64_t)strlen(key_str);
                    uint64_t val_len = (uint64_t)strlen(val_str);
                    key->tag = (key_len << JSON_TAG_BIT) | JSON_TYPE_STR;
                    val->tag = (val_len << JSON_TAG_BIT) | JSON_TYPE_STR;
                    key->uni.str = key_str;
                    val->uni.str = val_str;
                    key->next = val;
                    val->next = val + 1;
                }
                obj[count * 2].next = obj + 1;
                obj->uni.ptr = obj + (count * 2 - 1);
            }
            return obj;
        }
    }
    return NULL;
}



/*==============================================================================
 * Mutable JSON Object Modification API (Implementation)
 *============================================================================*/

json_api_inline void unsafe_json_mut_obj_add(json_mut_val *obj,
                                                 json_mut_val *key,
                                                 json_mut_val *val,
                                                 size_t len) {
    if (json_likely(len)) {
        json_mut_val *prev_val = ((json_mut_val *)obj->uni.ptr)->next;
        json_mut_val *next_key = prev_val->next;
        prev_val->next = key;
        val->next = next_key;
    } else {
        val->next = key;
    }
    key->next = val;
    obj->uni.ptr = (void *)key;
    unsafe_json_set_len(obj, len + 1);
}

json_api_inline json_mut_val *unsafe_json_mut_obj_remove(
    json_mut_val *obj, const char *key, size_t key_len) {
    size_t obj_len = unsafe_json_get_len(obj);
    if (obj_len) {
        json_mut_val *pre_key = (json_mut_val *)obj->uni.ptr;
        json_mut_val *cur_key = pre_key->next->next;
        json_mut_val *removed_item = NULL;
        size_t i;
        for (i = 0; i < obj_len; i++) {
            if (unsafe_json_equals_strn(cur_key, key, key_len)) {
                if (!removed_item) removed_item = cur_key->next;
                cur_key = cur_key->next->next;
                pre_key->next->next = cur_key;
                if (i + 1 == obj_len) obj->uni.ptr = pre_key;
                i--;
                obj_len--;
            } else {
                pre_key = cur_key;
                cur_key = cur_key->next->next;
            }
        }
        unsafe_json_set_len(obj, obj_len);
        return removed_item;
    } else {
        return NULL;
    }
}

json_api_inline bool unsafe_json_mut_obj_replace(json_mut_val *obj,
                                                     json_mut_val *key,
                                                     json_mut_val *val) {
    size_t key_len = unsafe_json_get_len(key);
    size_t obj_len = unsafe_json_get_len(obj);
    if (obj_len) {
        json_mut_val *pre_key = (json_mut_val *)obj->uni.ptr;
        json_mut_val *cur_key = pre_key->next->next;
        size_t i;
        for (i = 0; i < obj_len; i++) {
            if (unsafe_json_equals_strn(cur_key, key->uni.str, key_len)) {
                cur_key->next->tag = val->tag;
                cur_key->next->uni.u64 = val->uni.u64;
                return true;
            } else {
                cur_key = cur_key->next->next;
            }
        }
    }
    return false;
}

json_api_inline void unsafe_json_mut_obj_rotate(json_mut_val *obj,
                                                    size_t idx) {
    json_mut_val *key = (json_mut_val *)obj->uni.ptr;
    while (idx-- > 0) key = key->next->next;
    obj->uni.ptr = (void *)key;
}

json_api_inline bool json_mut_obj_add(json_mut_val *obj,
                                          json_mut_val *key,
                                          json_mut_val *val) {
    if (json_likely(json_mut_is_obj(obj) &&
                      json_mut_is_str(key) && val)) {
        unsafe_json_mut_obj_add(obj, key, val, unsafe_json_get_len(obj));
        return true;
    }
    return false;
}

json_api_inline bool json_mut_obj_put(json_mut_val *obj,
                                          json_mut_val *key,
                                          json_mut_val *val) {
    bool replaced = false;
    size_t key_len;
    json_mut_obj_iter iter;
    json_mut_val *cur_key;
    if (json_unlikely(!json_mut_is_obj(obj) ||
                        !json_mut_is_str(key))) return false;
    key_len = unsafe_json_get_len(key);
    json_mut_obj_iter_init(obj, &iter);
    while ((cur_key = json_mut_obj_iter_next(&iter)) != 0) {
        if (unsafe_json_equals_strn(cur_key, key->uni.str, key_len)) {
            if (!replaced && val) {
                replaced = true;
                val->next = cur_key->next->next;
                cur_key->next = val;
            } else {
                json_mut_obj_iter_remove(&iter);
            }
        }
    }
    if (!replaced && val) unsafe_json_mut_obj_add(obj, key, val, iter.max);
    return true;
}

json_api_inline bool json_mut_obj_insert(json_mut_val *obj,
                                             json_mut_val *key,
                                             json_mut_val *val,
                                             size_t idx) {
    if (json_likely(json_mut_is_obj(obj) &&
                      json_mut_is_str(key) && val)) {
        size_t len = unsafe_json_get_len(obj);
        if (json_likely(len >= idx)) {
            if (len > idx) {
                void *ptr = obj->uni.ptr;
                unsafe_json_mut_obj_rotate(obj, idx);
                unsafe_json_mut_obj_add(obj, key, val, len);
                obj->uni.ptr = ptr;
            } else {
                unsafe_json_mut_obj_add(obj, key, val, len);
            }
            return true;
        }
    }
    return false;
}

json_api_inline json_mut_val *json_mut_obj_remove(json_mut_val *obj,
    json_mut_val *key) {
    if (json_likely(json_mut_is_obj(obj) && json_mut_is_str(key))) {
        return unsafe_json_mut_obj_remove(obj, key->uni.str,
                                            unsafe_json_get_len(key));
    }
    return NULL;
}

json_api_inline json_mut_val *json_mut_obj_remove_key(
    json_mut_val *obj, const char *key) {
    if (json_likely(json_mut_is_obj(obj) && key)) {
        size_t key_len = strlen(key);
        return unsafe_json_mut_obj_remove(obj, key, key_len);
    }
    return NULL;
}

json_api_inline json_mut_val *json_mut_obj_remove_keyn(
    json_mut_val *obj, const char *key, size_t key_len) {
    if (json_likely(json_mut_is_obj(obj) && key)) {
        return unsafe_json_mut_obj_remove(obj, key, key_len);
    }
    return NULL;
}

json_api_inline bool json_mut_obj_clear(json_mut_val *obj) {
    if (json_likely(json_mut_is_obj(obj))) {
        unsafe_json_set_len(obj, 0);
        return true;
    }
    return false;
}

json_api_inline bool json_mut_obj_replace(json_mut_val *obj,
                                              json_mut_val *key,
                                              json_mut_val *val) {
    if (json_likely(json_mut_is_obj(obj) &&
                      json_mut_is_str(key) && val)) {
        return unsafe_json_mut_obj_replace(obj, key, val);
    }
    return false;
}

json_api_inline bool json_mut_obj_rotate(json_mut_val *obj,
                                             size_t idx) {
    if (json_likely(json_mut_is_obj(obj) &&
                      unsafe_json_get_len(obj) > idx)) {
        unsafe_json_mut_obj_rotate(obj, idx);
        return true;
    }
    return false;
}



/*==============================================================================
 * Mutable JSON Object Modification Convenience API (Implementation)
 *============================================================================*/

#define json_mut_obj_add_func(func) \
    if (json_likely(doc && json_mut_is_obj(obj) && _key)) { \
        json_mut_val *key = unsafe_json_mut_val(doc, 2); \
        if (json_likely(key)) { \
            size_t len = unsafe_json_get_len(obj); \
            json_mut_val *val = key + 1; \
            size_t key_len = strlen(_key); \
            bool noesc = unsafe_json_is_str_noesc(_key, key_len); \
            key->tag = JSON_TYPE_STR; \
            key->tag |= noesc ? JSON_SUBTYPE_NOESC : JSON_SUBTYPE_NONE; \
            key->tag |= (uint64_t)strlen(_key) << JSON_TAG_BIT; \
            key->uni.str = _key; \
            func \
            unsafe_json_mut_obj_add(obj, key, val, len); \
            return true; \
        } \
    } \
    return false

json_api_inline bool json_mut_obj_add_null(json_mut_doc *doc,
                                               json_mut_val *obj,
                                               const char *_key) {
    json_mut_obj_add_func({
        val->tag = JSON_TYPE_NULL | JSON_SUBTYPE_NONE;
    });
}

json_api_inline bool json_mut_obj_add_true(json_mut_doc *doc,
                                               json_mut_val *obj,
                                               const char *_key) {
    json_mut_obj_add_func({
        val->tag = JSON_TYPE_BOOL | JSON_SUBTYPE_TRUE;
    });
}

json_api_inline bool json_mut_obj_add_false(json_mut_doc *doc,
                                                json_mut_val *obj,
                                                const char *_key) {
    json_mut_obj_add_func({
        val->tag = JSON_TYPE_BOOL | JSON_SUBTYPE_FALSE;
    });
}

json_api_inline bool json_mut_obj_add_bool(json_mut_doc *doc,
                                               json_mut_val *obj,
                                               const char *_key,
                                               bool _val) {
    json_mut_obj_add_func({
        _val = !!_val;
        val->tag = JSON_TYPE_BOOL | (uint8_t)((uint8_t)(_val) << 3);
    });
}

json_api_inline bool json_mut_obj_add_uint(json_mut_doc *doc,
                                               json_mut_val *obj,
                                               const char *_key,
                                               uint64_t _val) {
    json_mut_obj_add_func({
        val->tag = JSON_TYPE_NUM | JSON_SUBTYPE_UINT;
        val->uni.u64 = _val;
    });
}

json_api_inline bool json_mut_obj_add_sint(json_mut_doc *doc,
                                               json_mut_val *obj,
                                               const char *_key,
                                               int64_t _val) {
    json_mut_obj_add_func({
        val->tag = JSON_TYPE_NUM | JSON_SUBTYPE_SINT;
        val->uni.i64 = _val;
    });
}

json_api_inline bool json_mut_obj_add_int(json_mut_doc *doc,
                                              json_mut_val *obj,
                                              const char *_key,
                                              int64_t _val) {
    json_mut_obj_add_func({
        val->tag = JSON_TYPE_NUM | JSON_SUBTYPE_SINT;
        val->uni.i64 = _val;
    });
}

json_api_inline bool json_mut_obj_add_real(json_mut_doc *doc,
                                               json_mut_val *obj,
                                               const char *_key,
                                               double _val) {
    json_mut_obj_add_func({
        val->tag = JSON_TYPE_NUM | JSON_SUBTYPE_REAL;
        val->uni.f64 = _val;
    });
}

json_api_inline bool json_mut_obj_add_str(json_mut_doc *doc,
                                              json_mut_val *obj,
                                              const char *_key,
                                              const char *_val) {
    if (json_unlikely(!_val)) return false;
    json_mut_obj_add_func({
        size_t val_len = strlen(_val);
        bool val_noesc = unsafe_json_is_str_noesc(_val, val_len);
        val->tag = ((uint64_t)strlen(_val) << JSON_TAG_BIT) | JSON_TYPE_STR;
        val->tag |= val_noesc ? JSON_SUBTYPE_NOESC : JSON_SUBTYPE_NONE;
        val->uni.str = _val;
    });
}

json_api_inline bool json_mut_obj_add_strn(json_mut_doc *doc,
                                               json_mut_val *obj,
                                               const char *_key,
                                               const char *_val,
                                               size_t _len) {
    if (json_unlikely(!_val)) return false;
    json_mut_obj_add_func({
        val->tag = ((uint64_t)_len << JSON_TAG_BIT) | JSON_TYPE_STR;
        val->uni.str = _val;
    });
}

json_api_inline bool json_mut_obj_add_strcpy(json_mut_doc *doc,
                                                 json_mut_val *obj,
                                                 const char *_key,
                                                 const char *_val) {
    if (json_unlikely(!_val)) return false;
    json_mut_obj_add_func({
        size_t _len = strlen(_val);
        val->uni.str = unsafe_json_mut_strncpy(doc, _val, _len);
        if (json_unlikely(!val->uni.str)) return false;
        val->tag = ((uint64_t)_len << JSON_TAG_BIT) | JSON_TYPE_STR;
    });
}

json_api_inline bool json_mut_obj_add_strncpy(json_mut_doc *doc,
                                                  json_mut_val *obj,
                                                  const char *_key,
                                                  const char *_val,
                                                  size_t _len) {
    if (json_unlikely(!_val)) return false;
    json_mut_obj_add_func({
        val->uni.str = unsafe_json_mut_strncpy(doc, _val, _len);
        if (json_unlikely(!val->uni.str)) return false;
        val->tag = ((uint64_t)_len << JSON_TAG_BIT) | JSON_TYPE_STR;
    });
}

json_api_inline json_mut_val *json_mut_obj_add_arr(json_mut_doc *doc,
                                                         json_mut_val *obj,
                                                         const char *_key) {
    json_mut_val *key = json_mut_str(doc, _key);
    json_mut_val *val = json_mut_arr(doc);
    return json_mut_obj_add(obj, key, val) ? val : NULL;
}

json_api_inline json_mut_val *json_mut_obj_add_obj(json_mut_doc *doc,
                                                         json_mut_val *obj,
                                                         const char *_key) {
    json_mut_val *key = json_mut_str(doc, _key);
    json_mut_val *val = json_mut_obj(doc);
    return json_mut_obj_add(obj, key, val) ? val : NULL;
}

json_api_inline bool json_mut_obj_add_val(json_mut_doc *doc,
                                              json_mut_val *obj,
                                              const char *_key,
                                              json_mut_val *_val) {
    if (json_unlikely(!_val)) return false;
    json_mut_obj_add_func({
        val = _val;
    });
}

json_api_inline json_mut_val *json_mut_obj_remove_str(json_mut_val *obj,
                                                            const char *key) {
    return json_mut_obj_remove_strn(obj, key, key ? strlen(key) : 0);
}

json_api_inline json_mut_val *json_mut_obj_remove_strn(
    json_mut_val *obj, const char *_key, size_t _len) {
    if (json_likely(json_mut_is_obj(obj) && _key)) {
        json_mut_val *key;
        json_mut_obj_iter iter;
        json_mut_val *val_removed = NULL;
        json_mut_obj_iter_init(obj, &iter);
        while ((key = json_mut_obj_iter_next(&iter)) != NULL) {
            if (unsafe_json_equals_strn(key, _key, _len)) {
                if (!val_removed) val_removed = key->next;
                json_mut_obj_iter_remove(&iter);
            }
        }
        return val_removed;
    }
    return NULL;
}

json_api_inline bool json_mut_obj_rename_key(json_mut_doc *doc,
                                                 json_mut_val *obj,
                                                 const char *key,
                                                 const char *new_key) {
    if (!key || !new_key) return false;
    return json_mut_obj_rename_keyn(doc, obj, key, strlen(key),
                                      new_key, strlen(new_key));
}

json_api_inline bool json_mut_obj_rename_keyn(json_mut_doc *doc,
                                                  json_mut_val *obj,
                                                  const char *key,
                                                  size_t len,
                                                  const char *new_key,
                                                  size_t new_len) {
    char *cpy_key = NULL;
    json_mut_val *old_key;
    json_mut_obj_iter iter;
    if (!doc || !obj || !key || !new_key) return false;
    json_mut_obj_iter_init(obj, &iter);
    while ((old_key = json_mut_obj_iter_next(&iter))) {
        if (unsafe_json_equals_strn((void *)old_key, key, len)) {
            if (!cpy_key) {
                cpy_key = unsafe_json_mut_strncpy(doc, new_key, new_len);
                if (!cpy_key) return false;
            }
            json_mut_set_strn(old_key, cpy_key, new_len);
        }
    }
    return cpy_key != NULL;
}



#if !defined(JSON_DISABLE_UTILS) || !JSON_DISABLE_UTILS

/*==============================================================================
 * JSON Pointer API (Implementation)
 *============================================================================*/

#define json_ptr_set_err(_code, _msg) do { \
    if (err) { \
        err->code = JSON_PTR_ERR_##_code; \
        err->msg = _msg; \
        err->pos = 0; \
    } \
} while(false)

/* require: val != NULL, *ptr == '/', len > 0 */
json_api json_val *unsafe_json_ptr_getx(json_val *val,
                                              const char *ptr, size_t len,
                                              json_ptr_err *err);

/* require: val != NULL, *ptr == '/', len > 0 */
json_api json_mut_val *unsafe_json_mut_ptr_getx(json_mut_val *val,
                                                      const char *ptr,
                                                      size_t len,
                                                      json_ptr_ctx *ctx,
                                                      json_ptr_err *err);

/* require: val/new_val/doc != NULL, *ptr == '/', len > 0 */
json_api bool unsafe_json_mut_ptr_putx(json_mut_val *val,
                                           const char *ptr, size_t len,
                                           json_mut_val *new_val,
                                           json_mut_doc *doc,
                                           bool create_parent, bool insert_new,
                                           json_ptr_ctx *ctx,
                                           json_ptr_err *err);

/* require: val/err != NULL, *ptr == '/', len > 0 */
json_api json_mut_val *unsafe_json_mut_ptr_replacex(
    json_mut_val *val, const char *ptr, size_t len, json_mut_val *new_val,
    json_ptr_ctx *ctx, json_ptr_err *err);

/* require: val/err != NULL, *ptr == '/', len > 0 */
json_api json_mut_val *unsafe_json_mut_ptr_removex(json_mut_val *val,
                                                         const char *ptr,
                                                         size_t len,
                                                         json_ptr_ctx *ctx,
                                                         json_ptr_err *err);

json_api_inline json_val *json_doc_ptr_get(json_doc *doc,
                                                 const char *ptr) {
    if (json_unlikely(!ptr)) return NULL;
    return json_doc_ptr_getn(doc, ptr, strlen(ptr));
}

json_api_inline json_val *json_doc_ptr_getn(json_doc *doc,
                                                  const char *ptr, size_t len) {
    return json_doc_ptr_getx(doc, ptr, len, NULL);
}

json_api_inline json_val *json_doc_ptr_getx(json_doc *doc,
                                                  const char *ptr, size_t len,
                                                  json_ptr_err *err) {
    json_ptr_set_err(NONE, NULL);
    if (json_unlikely(!doc || !ptr)) {
        json_ptr_set_err(PARAMETER, "input parameter is NULL");
        return NULL;
    }
    if (json_unlikely(!doc->root)) {
        json_ptr_set_err(NULL_ROOT, "document's root is NULL");
        return NULL;
    }
    if (json_unlikely(len == 0)) {
        return doc->root;
    }
    if (json_unlikely(*ptr != '/')) {
        json_ptr_set_err(SYNTAX, "no prefix '/'");
        return NULL;
    }
    return unsafe_json_ptr_getx(doc->root, ptr, len, err);
}

json_api_inline json_val *json_ptr_get(json_val *val,
                                             const char *ptr) {
    if (json_unlikely(!ptr)) return NULL;
    return json_ptr_getn(val, ptr, strlen(ptr));
}

json_api_inline json_val *json_ptr_getn(json_val *val,
                                              const char *ptr, size_t len) {
    return json_ptr_getx(val, ptr, len, NULL);
}

json_api_inline json_val *json_ptr_getx(json_val *val,
                                              const char *ptr, size_t len,
                                              json_ptr_err *err) {
    json_ptr_set_err(NONE, NULL);
    if (json_unlikely(!val || !ptr)) {
        json_ptr_set_err(PARAMETER, "input parameter is NULL");
        return NULL;
    }
    if (json_unlikely(len == 0)) {
        return val;
    }
    if (json_unlikely(*ptr != '/')) {
        json_ptr_set_err(SYNTAX, "no prefix '/'");
        return NULL;
    }
    return unsafe_json_ptr_getx(val, ptr, len, err);
}

json_api_inline json_mut_val *json_mut_doc_ptr_get(json_mut_doc *doc,
                                                         const char *ptr) {
    if (!ptr) return NULL;
    return json_mut_doc_ptr_getn(doc, ptr, strlen(ptr));
}

json_api_inline json_mut_val *json_mut_doc_ptr_getn(json_mut_doc *doc,
                                                          const char *ptr,
                                                          size_t len) {
    return json_mut_doc_ptr_getx(doc, ptr, len, NULL, NULL);
}

json_api_inline json_mut_val *json_mut_doc_ptr_getx(json_mut_doc *doc,
                                                          const char *ptr,
                                                          size_t len,
                                                          json_ptr_ctx *ctx,
                                                          json_ptr_err *err) {
    json_ptr_set_err(NONE, NULL);
    if (ctx) memset(ctx, 0, sizeof(*ctx));
    
    if (json_unlikely(!doc || !ptr)) {
        json_ptr_set_err(PARAMETER, "input parameter is NULL");
        return NULL;
    }
    if (json_unlikely(!doc->root)) {
        json_ptr_set_err(NULL_ROOT, "document's root is NULL");
        return NULL;
    }
    if (json_unlikely(len == 0)) {
        return doc->root;
    }
    if (json_unlikely(*ptr != '/')) {
        json_ptr_set_err(SYNTAX, "no prefix '/'");
        return NULL;
    }
    return unsafe_json_mut_ptr_getx(doc->root, ptr, len, ctx, err);
}

json_api_inline json_mut_val *json_mut_ptr_get(json_mut_val *val,
                                                     const char *ptr) {
    if (!ptr) return NULL;
    return json_mut_ptr_getn(val, ptr, strlen(ptr));
}

json_api_inline json_mut_val *json_mut_ptr_getn(json_mut_val *val,
                                                      const char *ptr,
                                                      size_t len) {
    return json_mut_ptr_getx(val, ptr, len, NULL, NULL);
}

json_api_inline json_mut_val *json_mut_ptr_getx(json_mut_val *val,
                                                      const char *ptr,
                                                      size_t len,
                                                      json_ptr_ctx *ctx,
                                                      json_ptr_err *err) {
    json_ptr_set_err(NONE, NULL);
    if (ctx) memset(ctx, 0, sizeof(*ctx));
    
    if (json_unlikely(!val || !ptr)) {
        json_ptr_set_err(PARAMETER, "input parameter is NULL");
        return NULL;
    }
    if (json_unlikely(len == 0)) {
        return val;
    }
    if (json_unlikely(*ptr != '/')) {
        json_ptr_set_err(SYNTAX, "no prefix '/'");
        return NULL;
    }
    return unsafe_json_mut_ptr_getx(val, ptr, len, ctx, err);
}

json_api_inline bool json_mut_doc_ptr_add(json_mut_doc *doc,
                                              const char *ptr,
                                              json_mut_val *new_val) {
    if (json_unlikely(!ptr)) return false;
    return json_mut_doc_ptr_addn(doc, ptr, strlen(ptr), new_val);
}

json_api_inline bool json_mut_doc_ptr_addn(json_mut_doc *doc,
                                               const char *ptr,
                                               size_t len,
                                               json_mut_val *new_val) {
    return json_mut_doc_ptr_addx(doc, ptr, len, new_val, true, NULL, NULL);
}

json_api_inline bool json_mut_doc_ptr_addx(json_mut_doc *doc,
                                               const char *ptr, size_t len,
                                               json_mut_val *new_val,
                                               bool create_parent,
                                               json_ptr_ctx *ctx,
                                               json_ptr_err *err) {
    json_ptr_set_err(NONE, NULL);
    if (ctx) memset(ctx, 0, sizeof(*ctx));
    
    if (json_unlikely(!doc || !ptr || !new_val)) {
        json_ptr_set_err(PARAMETER, "input parameter is NULL");
        return false;
    }
    if (json_unlikely(len == 0)) {
        if (doc->root) {
            json_ptr_set_err(SET_ROOT, "cannot set document's root");
            return false;
        } else {
            doc->root = new_val;
            return true;
        }
    }
    if (json_unlikely(*ptr != '/')) {
        json_ptr_set_err(SYNTAX, "no prefix '/'");
        return false;
    }
    if (json_unlikely(!doc->root && !create_parent)) {
        json_ptr_set_err(NULL_ROOT, "document's root is NULL");
        return false;
    }
    if (json_unlikely(!doc->root)) {
        json_mut_val *root = json_mut_obj(doc);
        if (json_unlikely(!root)) {
            json_ptr_set_err(MEMORY_ALLOCATION, "failed to create value");
            return false;
        }
        if (unsafe_json_mut_ptr_putx(root, ptr, len, new_val, doc,
                                       create_parent, true, ctx, err)) {
            doc->root = root;
            return true;
        }
        return false;
    }
    return unsafe_json_mut_ptr_putx(doc->root, ptr, len, new_val, doc,
                                      create_parent, true, ctx, err);
}

json_api_inline bool json_mut_ptr_add(json_mut_val *val,
                                          const char *ptr,
                                          json_mut_val *new_val,
                                          json_mut_doc *doc) {
    if (json_unlikely(!ptr)) return false;
    return json_mut_ptr_addn(val, ptr, strlen(ptr), new_val, doc);
}

json_api_inline bool json_mut_ptr_addn(json_mut_val *val,
                                           const char *ptr, size_t len,
                                           json_mut_val *new_val,
                                           json_mut_doc *doc) {
    return json_mut_ptr_addx(val, ptr, len, new_val, doc, true, NULL, NULL);
}

json_api_inline bool json_mut_ptr_addx(json_mut_val *val,
                                           const char *ptr, size_t len,
                                           json_mut_val *new_val,
                                           json_mut_doc *doc,
                                           bool create_parent,
                                           json_ptr_ctx *ctx,
                                           json_ptr_err *err) {
    json_ptr_set_err(NONE, NULL);
    if (ctx) memset(ctx, 0, sizeof(*ctx));
    
    if (json_unlikely(!val || !ptr || !new_val || !doc)) {
        json_ptr_set_err(PARAMETER, "input parameter is NULL");
        return false;
    }
    if (json_unlikely(len == 0)) {
        json_ptr_set_err(SET_ROOT, "cannot set root");
        return false;
    }
    if (json_unlikely(*ptr != '/')) {
        json_ptr_set_err(SYNTAX, "no prefix '/'");
        return false;
    }
    return unsafe_json_mut_ptr_putx(val, ptr, len, new_val,
                                       doc, create_parent, true, ctx, err);
}

json_api_inline bool json_mut_doc_ptr_set(json_mut_doc *doc,
                                              const char *ptr,
                                              json_mut_val *new_val) {
    if (json_unlikely(!ptr)) return false;
    return json_mut_doc_ptr_setn(doc, ptr, strlen(ptr), new_val);
}

json_api_inline bool json_mut_doc_ptr_setn(json_mut_doc *doc,
                                               const char *ptr, size_t len,
                                               json_mut_val *new_val) {
    return json_mut_doc_ptr_setx(doc, ptr, len, new_val, true, NULL, NULL);
}

json_api_inline bool json_mut_doc_ptr_setx(json_mut_doc *doc,
                                               const char *ptr, size_t len,
                                               json_mut_val *new_val,
                                               bool create_parent,
                                               json_ptr_ctx *ctx,
                                               json_ptr_err *err) {
    json_ptr_set_err(NONE, NULL);
    if (ctx) memset(ctx, 0, sizeof(*ctx));
    
    if (json_unlikely(!doc || !ptr)) {
        json_ptr_set_err(PARAMETER, "input parameter is NULL");
        return false;
    }
    if (json_unlikely(len == 0)) {
        if (ctx) ctx->old = doc->root;
        doc->root = new_val;
        return true;
    }
    if (json_unlikely(*ptr != '/')) {
        json_ptr_set_err(SYNTAX, "no prefix '/'");
        return false;
    }
    if (!new_val) {
        if (!doc->root) {
            json_ptr_set_err(RESOLVE, "JSON pointer cannot be resolved");
            return false;
        }
        return !!unsafe_json_mut_ptr_removex(doc->root, ptr, len, ctx, err);
    }
    if (json_unlikely(!doc->root && !create_parent)) {
        json_ptr_set_err(NULL_ROOT, "document's root is NULL");
        return false;
    }
    if (json_unlikely(!doc->root)) {
        json_mut_val *root = json_mut_obj(doc);
        if (json_unlikely(!root)) {
            json_ptr_set_err(MEMORY_ALLOCATION, "failed to create value");
            return false;
        }
        if (unsafe_json_mut_ptr_putx(root, ptr, len, new_val, doc,
                                       create_parent, false, ctx, err)) {
            doc->root = root;
            return true;
        }
        return false;
    }
    return unsafe_json_mut_ptr_putx(doc->root, ptr, len, new_val, doc,
                                      create_parent, false, ctx, err);
}

json_api_inline bool json_mut_ptr_set(json_mut_val *val,
                                          const char *ptr,
                                          json_mut_val *new_val,
                                          json_mut_doc *doc) {
    if (json_unlikely(!ptr)) return false;
    return json_mut_ptr_setn(val, ptr, strlen(ptr), new_val, doc);
}

json_api_inline bool json_mut_ptr_setn(json_mut_val *val,
                                           const char *ptr, size_t len,
                                           json_mut_val *new_val,
                                           json_mut_doc *doc) {
    return json_mut_ptr_setx(val, ptr, len, new_val, doc, true, NULL, NULL);
}

json_api_inline bool json_mut_ptr_setx(json_mut_val *val,
                                           const char *ptr, size_t len,
                                           json_mut_val *new_val,
                                           json_mut_doc *doc,
                                           bool create_parent,
                                           json_ptr_ctx *ctx,
                                           json_ptr_err *err) {
    json_ptr_set_err(NONE, NULL);
    if (ctx) memset(ctx, 0, sizeof(*ctx));
    
    if (json_unlikely(!val || !ptr || !doc)) {
        json_ptr_set_err(PARAMETER, "input parameter is NULL");
        return false;
    }
    if (json_unlikely(len == 0)) {
        json_ptr_set_err(SET_ROOT, "cannot set root");
        return false;
    }
    if (json_unlikely(*ptr != '/')) {
        json_ptr_set_err(SYNTAX, "no prefix '/'");
        return false;
    }
    if (!new_val) {
        return !!unsafe_json_mut_ptr_removex(val, ptr, len, ctx, err);
    }
    return unsafe_json_mut_ptr_putx(val, ptr, len, new_val, doc,
                                      create_parent, false, ctx, err);
}

json_api_inline json_mut_val *json_mut_doc_ptr_replace(
    json_mut_doc *doc, const char *ptr, json_mut_val *new_val) {
    if (!ptr) return NULL;
    return json_mut_doc_ptr_replacen(doc, ptr, strlen(ptr), new_val);
}

json_api_inline json_mut_val *json_mut_doc_ptr_replacen(
    json_mut_doc *doc, const char *ptr, size_t len, json_mut_val *new_val) {
    return json_mut_doc_ptr_replacex(doc, ptr, len, new_val, NULL, NULL);
}

json_api_inline json_mut_val *json_mut_doc_ptr_replacex(
    json_mut_doc *doc, const char *ptr, size_t len, json_mut_val *new_val,
    json_ptr_ctx *ctx, json_ptr_err *err) {
    
    json_ptr_set_err(NONE, NULL);
    if (ctx) memset(ctx, 0, sizeof(*ctx));
    
    if (json_unlikely(!doc || !ptr || !new_val)) {
        json_ptr_set_err(PARAMETER, "input parameter is NULL");
        return NULL;
    }
    if (json_unlikely(len == 0)) {
        json_mut_val *root = doc->root;
        if (json_unlikely(!root)) {
            json_ptr_set_err(RESOLVE, "JSON pointer cannot be resolved");
            return NULL;
        }
        if (ctx) ctx->old = root;
        doc->root = new_val;
        return root;
    }
    if (json_unlikely(!doc->root)) {
        json_ptr_set_err(NULL_ROOT, "document's root is NULL");
        return NULL;
    }
    if (json_unlikely(*ptr != '/')) {
        json_ptr_set_err(SYNTAX, "no prefix '/'");
        return NULL;
    }
    return unsafe_json_mut_ptr_replacex(doc->root, ptr, len, new_val,
                                          ctx, err);
}

json_api_inline json_mut_val *json_mut_ptr_replace(
    json_mut_val *val, const char *ptr, json_mut_val *new_val) {
    if (!ptr) return NULL;
    return json_mut_ptr_replacen(val, ptr, strlen(ptr), new_val);
}

json_api_inline json_mut_val *json_mut_ptr_replacen(
    json_mut_val *val, const char *ptr, size_t len, json_mut_val *new_val) {
    return json_mut_ptr_replacex(val, ptr, len, new_val, NULL, NULL);
}

json_api_inline json_mut_val *json_mut_ptr_replacex(
    json_mut_val *val, const char *ptr, size_t len, json_mut_val *new_val,
    json_ptr_ctx *ctx, json_ptr_err *err) {
    
    json_ptr_set_err(NONE, NULL);
    if (ctx) memset(ctx, 0, sizeof(*ctx));
    
    if (json_unlikely(!val || !ptr || !new_val)) {
        json_ptr_set_err(PARAMETER, "input parameter is NULL");
        return NULL;
    }
    if (json_unlikely(len == 0)) {
        json_ptr_set_err(SET_ROOT, "cannot set root");
        return NULL;
    }
    if (json_unlikely(*ptr != '/')) {
        json_ptr_set_err(SYNTAX, "no prefix '/'");
        return NULL;
    }
    return unsafe_json_mut_ptr_replacex(val, ptr, len, new_val, ctx, err);
}

json_api_inline json_mut_val *json_mut_doc_ptr_remove(
    json_mut_doc *doc, const char *ptr) {
    if (!ptr) return NULL;
    return json_mut_doc_ptr_removen(doc, ptr, strlen(ptr));
}

json_api_inline json_mut_val *json_mut_doc_ptr_removen(
    json_mut_doc *doc, const char *ptr, size_t len) {
    return json_mut_doc_ptr_removex(doc, ptr, len, NULL, NULL);
}

json_api_inline json_mut_val *json_mut_doc_ptr_removex(
    json_mut_doc *doc, const char *ptr, size_t len,
    json_ptr_ctx *ctx, json_ptr_err *err) {
    
    json_ptr_set_err(NONE, NULL);
    if (ctx) memset(ctx, 0, sizeof(*ctx));
    
    if (json_unlikely(!doc || !ptr)) {
        json_ptr_set_err(PARAMETER, "input parameter is NULL");
        return NULL;
    }
    if (json_unlikely(!doc->root)) {
        json_ptr_set_err(NULL_ROOT, "document's root is NULL");
        return NULL;
    }
    if (json_unlikely(len == 0)) {
        json_mut_val *root = doc->root;
        if (ctx) ctx->old = root;
        doc->root = NULL;
        return root;
    }
    if (json_unlikely(*ptr != '/')) {
        json_ptr_set_err(SYNTAX, "no prefix '/'");
        return NULL;
    }
    return unsafe_json_mut_ptr_removex(doc->root, ptr, len, ctx, err);
}

json_api_inline json_mut_val *json_mut_ptr_remove(json_mut_val *val,
                                                        const char *ptr) {
    if (!ptr) return NULL;
    return json_mut_ptr_removen(val, ptr, strlen(ptr));
}

json_api_inline json_mut_val *json_mut_ptr_removen(json_mut_val *val,
                                                         const char *ptr,
                                                         size_t len) {
    return json_mut_ptr_removex(val, ptr, len, NULL, NULL);
}

json_api_inline json_mut_val *json_mut_ptr_removex(json_mut_val *val,
                                                         const char *ptr,
                                                         size_t len,
                                                         json_ptr_ctx *ctx,
                                                         json_ptr_err *err) {
    json_ptr_set_err(NONE, NULL);
    if (ctx) memset(ctx, 0, sizeof(*ctx));
    
    if (json_unlikely(!val || !ptr)) {
        json_ptr_set_err(PARAMETER, "input parameter is NULL");
        return NULL;
    }
    if (json_unlikely(len == 0)) {
        json_ptr_set_err(SET_ROOT, "cannot set root");
        return NULL;
    }
    if (json_unlikely(*ptr != '/')) {
        json_ptr_set_err(SYNTAX, "no prefix '/'");
        return NULL;
    }
    return unsafe_json_mut_ptr_removex(val, ptr, len, ctx, err);
}

json_api_inline bool json_ptr_ctx_append(json_ptr_ctx *ctx,
                                             json_mut_val *key,
                                             json_mut_val *val) {
    json_mut_val *ctn, *pre_key, *pre_val, *cur_key, *cur_val;
    if (!ctx || !ctx->ctn || !val) return false;
    ctn = ctx->ctn;
    
    if (json_mut_is_obj(ctn)) {
        if (!key) return false;
        key->next = val;
        pre_key = ctx->pre;
        if (unsafe_json_get_len(ctn) == 0) {
            val->next = key;
            ctn->uni.ptr = key;
            ctx->pre = key;
        } else if (!pre_key) {
            pre_key = (json_mut_val *)ctn->uni.ptr;
            pre_val = pre_key->next;
            val->next = pre_val->next;
            pre_val->next = key;
            ctn->uni.ptr = key;
            ctx->pre = pre_key;
        } else {
            cur_key = pre_key->next->next;
            cur_val = cur_key->next;
            val->next = cur_val->next;
            cur_val->next = key;
            if (ctn->uni.ptr == cur_key) ctn->uni.ptr = key;
            ctx->pre = cur_key;
        }
    } else {
        pre_val = ctx->pre;
        if (unsafe_json_get_len(ctn) == 0) {
            val->next = val;
            ctn->uni.ptr = val;
            ctx->pre = val;
        } else if (!pre_val) {
            pre_val = (json_mut_val *)ctn->uni.ptr;
            val->next = pre_val->next;
            pre_val->next = val;
            ctn->uni.ptr = val;
            ctx->pre = pre_val;
        } else {
            cur_val = pre_val->next;
            val->next = cur_val->next;
            cur_val->next = val;
            if (ctn->uni.ptr == cur_val) ctn->uni.ptr = val;
            ctx->pre = cur_val;
        }
    }
    unsafe_json_inc_len(ctn);
    return true;
}

json_api_inline bool json_ptr_ctx_replace(json_ptr_ctx *ctx,
                                              json_mut_val *val) {
    json_mut_val *ctn, *pre_key, *cur_key, *pre_val, *cur_val;
    if (!ctx || !ctx->ctn || !ctx->pre || !val) return false;
    ctn = ctx->ctn;
    if (json_mut_is_obj(ctn)) {
        pre_key = ctx->pre;
        pre_val = pre_key->next;
        cur_key = pre_val->next;
        cur_val = cur_key->next;
        /* replace current value */
        cur_key->next = val;
        val->next = cur_val->next;
        ctx->old = cur_val;
    } else {
        pre_val = ctx->pre;
        cur_val = pre_val->next;
        /* replace current value */
        if (pre_val != cur_val) {
            val->next = cur_val->next;
            pre_val->next = val;
            if (ctn->uni.ptr == cur_val) ctn->uni.ptr = val;
        } else {
            val->next = val;
            ctn->uni.ptr = val;
            ctx->pre = val;
        }
        ctx->old = cur_val;
    }
    return true;
}

json_api_inline bool json_ptr_ctx_remove(json_ptr_ctx *ctx) {
    json_mut_val *ctn, *pre_key, *pre_val, *cur_key, *cur_val;
    size_t len;
    if (!ctx || !ctx->ctn || !ctx->pre) return false;
    ctn = ctx->ctn;
    if (json_mut_is_obj(ctn)) {
        pre_key = ctx->pre;
        pre_val = pre_key->next;
        cur_key = pre_val->next;
        cur_val = cur_key->next;
        /* remove current key-value */
        pre_val->next = cur_val->next;
        if (ctn->uni.ptr == cur_key) ctn->uni.ptr = pre_key;
        ctx->pre = NULL;
        ctx->old = cur_val;
    } else {
        pre_val = ctx->pre;
        cur_val = pre_val->next;
        /* remove current key-value */
        pre_val->next = cur_val->next;
        if (ctn->uni.ptr == cur_val) ctn->uni.ptr = pre_val;
        ctx->pre = NULL;
        ctx->old = cur_val;
    }
    len = unsafe_json_get_len(ctn) - 1;
    if (len == 0) ctn->uni.ptr = NULL;
    unsafe_json_set_len(ctn, len);
    return true;
}

#undef json_ptr_set_err



/*==============================================================================
 * JSON Value at Pointer API (Implementation)
 *============================================================================*/

/**
 Set provided `value` if the JSON Pointer (RFC 6901) exists and is type bool.
 Returns true if value at `ptr` exists and is the correct type, otherwise false.
 */
json_api_inline bool json_ptr_get_bool(
    json_val *root, const char *ptr, bool *value) {
    json_val *val = json_ptr_get(root, ptr);
    if (value && json_is_bool(val)) {
        *value = unsafe_json_get_bool(val);
        return true;
    } else {
        return false;
    }
}

/**
 Set provided `value` if the JSON Pointer (RFC 6901) exists and is an integer
 that fits in `uint64_t`. Returns true if successful, otherwise false.
 */
json_api_inline bool json_ptr_get_uint(
    json_val *root, const char *ptr, uint64_t *value) {
    json_val *val = json_ptr_get(root, ptr);
    if (value && val) {
        uint64_t ret = val->uni.u64;
        if (unsafe_json_is_uint(val) ||
            (unsafe_json_is_sint(val) && !(ret >> 63))) {
            *value = ret;
            return true;
        }
    }
    return false;
}

/**
 Set provided `value` if the JSON Pointer (RFC 6901) exists and is an integer
 that fits in `int64_t`. Returns true if successful, otherwise false.
 */
json_api_inline bool json_ptr_get_sint(
    json_val *root, const char *ptr, int64_t *value) {
    json_val *val = json_ptr_get(root, ptr);
    if (value && val) {
        int64_t ret = val->uni.i64;
        if (unsafe_json_is_sint(val) ||
            (unsafe_json_is_uint(val) && ret >= 0)) {
            *value = ret;
            return true;
        }
    }
    return false;
}

/**
 Set provided `value` if the JSON Pointer (RFC 6901) exists and is type real.
 Returns true if value at `ptr` exists and is the correct type, otherwise false.
 */
json_api_inline bool json_ptr_get_real(
    json_val *root, const char *ptr, double *value) {
    json_val *val = json_ptr_get(root, ptr);
    if (value && json_is_real(val)) {
        *value = unsafe_json_get_real(val);
        return true;
    } else {
        return false;
    }
}

/**
 Set provided `value` if the JSON Pointer (RFC 6901) exists and is type sint,
 uint or real.
 Returns true if value at `ptr` exists and is the correct type, otherwise false.
 */
json_api_inline bool json_ptr_get_num(
    json_val *root, const char *ptr, double *value) {
    json_val *val = json_ptr_get(root, ptr);
    if (value && json_is_num(val)) {
        *value = unsafe_json_get_num(val);
        return true;
    } else {
        return false;
    }
}

/**
 Set provided `value` if the JSON Pointer (RFC 6901) exists and is type string.
 Returns true if value at `ptr` exists and is the correct type, otherwise false.
 */
json_api_inline bool json_ptr_get_str(
    json_val *root, const char *ptr, const char **value) {
    json_val *val = json_ptr_get(root, ptr);
    if (value && json_is_str(val)) {
        *value = unsafe_json_get_str(val);
        return true;
    } else {
        return false;
    }
}



/*==============================================================================
 * Deprecated
 *============================================================================*/

/** @deprecated renamed to `json_doc_ptr_get` */
json_deprecated("renamed to json_doc_ptr_get")
json_api_inline json_val *json_doc_get_pointer(json_doc *doc,
                                                     const char *ptr) {
    return json_doc_ptr_get(doc, ptr);
}

/** @deprecated renamed to `json_doc_ptr_getn` */
json_deprecated("renamed to json_doc_ptr_getn")
json_api_inline json_val *json_doc_get_pointern(json_doc *doc,
                                                      const char *ptr,
                                                      size_t len) {
    return json_doc_ptr_getn(doc, ptr, len);
}

/** @deprecated renamed to `json_mut_doc_ptr_get` */
json_deprecated("renamed to json_mut_doc_ptr_get")
json_api_inline json_mut_val *json_mut_doc_get_pointer(
    json_mut_doc *doc, const char *ptr) {
    return json_mut_doc_ptr_get(doc, ptr);
}

/** @deprecated renamed to `json_mut_doc_ptr_getn` */
json_deprecated("renamed to json_mut_doc_ptr_getn")
json_api_inline json_mut_val *json_mut_doc_get_pointern(
    json_mut_doc *doc, const char *ptr, size_t len) {
    return json_mut_doc_ptr_getn(doc, ptr, len);
}

/** @deprecated renamed to `json_ptr_get` */
json_deprecated("renamed to json_ptr_get")
json_api_inline json_val *json_get_pointer(json_val *val,
                                                 const char *ptr) {
    return json_ptr_get(val, ptr);
}

/** @deprecated renamed to `json_ptr_getn` */
json_deprecated("renamed to json_ptr_getn")
json_api_inline json_val *json_get_pointern(json_val *val,
                                                  const char *ptr,
                                                  size_t len) {
    return json_ptr_getn(val, ptr, len);
}

/** @deprecated renamed to `json_mut_ptr_get` */
json_deprecated("renamed to json_mut_ptr_get")
json_api_inline json_mut_val *json_mut_get_pointer(json_mut_val *val,
                                                         const char *ptr) {
    return json_mut_ptr_get(val, ptr);
}

/** @deprecated renamed to `json_mut_ptr_getn` */
json_deprecated("renamed to json_mut_ptr_getn")
json_api_inline json_mut_val *json_mut_get_pointern(json_mut_val *val,
                                                          const char *ptr,
                                                          size_t len) {
    return json_mut_ptr_getn(val, ptr, len);
}

/** @deprecated renamed to `json_mut_ptr_getn` */
json_deprecated("renamed to unsafe_json_ptr_getn")
json_api_inline json_val *unsafe_json_get_pointer(json_val *val,
                                                        const char *ptr,
                                                        size_t len) {
    json_ptr_err err;
    return unsafe_json_ptr_getx(val, ptr, len, &err);
}

/** @deprecated renamed to `unsafe_json_mut_ptr_getx` */
json_deprecated("renamed to unsafe_json_mut_ptr_getx")
json_api_inline json_mut_val *unsafe_json_mut_get_pointer(
    json_mut_val *val, const char *ptr, size_t len) {
    json_ptr_err err;
    return unsafe_json_mut_ptr_getx(val, ptr, len, NULL, &err);
}

#endif /* JSON_DISABLE_UTILS */



/*==============================================================================
 * Compiler Hint End
 *============================================================================*/

#if defined(__clang__)
#   pragma clang diagnostic pop
#elif defined(__GNUC__)
#   if (__GNUC__ > 4) || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)
#   pragma GCC diagnostic pop
#   endif
#elif defined(_MSC_VER)
#   pragma warning(pop)
#endif /* warning suppress end */

#ifdef __cplusplus
}
#endif /* extern "C" end */

#endif /* JSON_H */
