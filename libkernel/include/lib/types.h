#ifndef _TYPES_H
#define _TYPES_H

#include <stdint.h>
#include <stdlib.h>
#include <limits.h>

typedef __signed__ char __s8;
typedef unsigned char __u8;
typedef __signed__ short __s16;
typedef unsigned short __u16;
typedef __signed__ int __s32;
typedef unsigned int __u32;
typedef __signed__ long long __s64;
typedef unsigned long long __u64;

typedef __s8 s8;
typedef __u8 u8;
typedef __s16 s16;
typedef __u16 u16;
typedef __s32 s32;
typedef __u32 u32;
typedef __s64 s64;
typedef __u64 u64;
//
// typedef s8 int8_t;
// typedef s16 int16_t;
// typedef s32 int32_t;
// typedef s64 int64_t;
// typedef u8 uint8_t;
// typedef u16 uint16_t;
// typedef u32 uint32_t;
// typedef u64 uint64_t;
//
// typedef u64 uintptr_t;
// typedef s64 intptr_t;
//
// typedef u64 uintmax_t;
// typedef s64 intmax_t;
//
// typedef unsigned long size_t;
// typedef long ssize_t;
//
// typedef long off_t;
//
// typedef int8_t int_fast8_t;
// typedef int64_t int_fast64_t;
//
// typedef int8_t int_least8_t;
// typedef int16_t int_least16_t;
// typedef int32_t int_least32_t;
// typedef int64_t int_least64_t;
//
// typedef uint8_t uint_fast8_t;
// typedef uint64_t uint_fast64_t;
//
// typedef uint8_t uint_least8_t;
// typedef uint16_t uint_least16_t;
// typedef uint32_t uint_least32_t;
// typedef uint64_t uint_least64_t;
//
// #define INT8_MIN (-1 - 0x7f)
// #define INT16_MIN (-1 - 0x7fff)
// #define INT32_MIN (-1 - 0x7fffffff)
// #define INT64_MIN (-1 - 0x7fffffffffffffff)
//
// #define INT8_MAX (0x7f)
// #define INT16_MAX (0x7fff)
// #define INT32_MAX (0x7fffffff)
// #define INT64_MAX (0x7fffffffffffffff)
//
// #define UINT8_MAX (0xff)
// #define UINT16_MAX (0xffff)
// #define UINT32_MAX (0xffffffffu)
// #define UINT64_MAX (0xffffffffffffffffu)
//
// #define INT_FAST8_MIN INT8_MIN
// #define INT_FAST64_MIN INT64_MIN
//
// #define INT_LEAST8_MIN INT8_MIN
// #define INT_LEAST16_MIN INT16_MIN
// #define INT_LEAST32_MIN INT32_MIN
// #define INT_LEAST64_MIN INT64_MIN
//
// #define INT_FAST8_MAX INT8_MAX
// #define INT_FAST64_MAX INT64_MAX
//
// #define INT_LEAST8_MAX INT8_MAX
// #define INT_LEAST16_MAX INT16_MAX
// #define INT_LEAST32_MAX INT32_MAX
// #define INT_LEAST64_MAX INT64_MAX
//
// #define UINT_FAST8_MAX UINT8_MAX
// #define UINT_FAST64_MAX UINT64_MAX
//
// #define UINT_LEAST8_MAX UINT8_MAX
// #define UINT_LEAST16_MAX UINT16_MAX
// #define UINT_LEAST32_MAX UINT32_MAX
// #define UINT_LEAST64_MAX UINT64_MAX
//
// #define INTMAX_MIN INT64_MIN
// #define INTMAX_MAX INT64_MAX
// #define UINTMAX_MAX UINT64_MAX
//
// #define SCHAR_MAX __SCHAR_MAX__
// #define SHRT_MAX  __SHRT_MAX__
// #define INT_MAX   __INT_MAX__
// #define LONG_MAX  __LONG_MAX__
//
// #define SCHAR_MIN (-__SCHAR_MAX__-1)
// #define SHRT_MIN  (-__SHRT_MAX__ -1)
// #define INT_MIN   (-__INT_MAX__  -1)
// #define LONG_MIN  (-__LONG_MAX__ -1L)
//
// #define UCHAR_MAX (__SCHAR_MAX__*2  +1)
// #define USHRT_MAX (__SHRT_MAX__ *2  +1)
// #define UINT_MAX  (__INT_MAX__  *2U +1U)
// #define ULONG_MAX (__LONG_MAX__ *2UL+1UL)
//
// #define SIZE_MAX	(~(size_t)0)
// #define SSIZE_MAX	((ssize_t)(SIZE_MAX >> 1))
//
// #define U8_MAX		((u8)~0U)
// #define S8_MAX		((s8)(U8_MAX >> 1))
// #define S8_MIN		((s8)(-S8_MAX - 1))
// #define U16_MAX		((u16)~0U)
// #define S16_MAX		((s16)(U16_MAX >> 1))
// #define S16_MIN		((s16)(-S16_MAX - 1))
// #define U32_MAX		((u32)~0U)
// #define U32_MIN		((u32)0)
// #define S32_MAX		((s32)(U32_MAX >> 1))
// #define S32_MIN		((s32)(-S32_MAX - 1))
// #define U64_MAX		((u64)~0ULL)
// #define S64_MAX		((s64)(U64_MAX >> 1))
// #define S64_MIN		((s64)(-S64_MAX - 1))
//
// #define WINT_MIN 0U
// #define WINT_MAX UINT32_MAX
//
// #define LLONG_MAX  __LONG_LONG_MAX__
// #define LLONG_MIN  (-__LONG_LONG_MAX__-1LL)
// #define ULLONG_MAX (__LONG_LONG_MAX__*2ULL+1ULL)
//
// #define LONG_LONG_MAX  __LONG_LONG_MAX__
// #define LONG_LONG_MIN  (-__LONG_LONG_MAX__-1LL)
// #define ULONG_LONG_MAX (__LONG_LONG_MAX__*2ULL+1ULL)
//
// #if L'\0' - 1 > 0
// #define WCHAR_MAX (0xffffffffu + L'\0')
// #define WCHAR_MIN (0 + L'\0')
// #else
// #define WCHAR_MAX (0x7fffffff + L'\0')
// #define WCHAR_MIN (-1 - 0x7fffffff + L'\0')
// #endif
//
// #define SIG_ATOMIC_MIN INT32_MIN
// #define SIG_ATOMIC_MAX INT32_MAX
//
// #if UINTPTR_MAX == UINT64_MAX
// #define INT64_C(c) c##L
// #define UINT64_C(c) c##UL
// #define INTMAX_C(c) c##L
// #define UINTMAX_C(c) c##UL
// #else
// #define INT64_C(c) c##LL
// #define UINT64_C(c) c##ULL
// #define INTMAX_C(c) c##LL
// #define UINTMAX_C(c) c##ULL
// #endif
//
#endif
