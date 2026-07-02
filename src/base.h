#ifndef _BASE_INCLUDED
#define _BASE_INCLUDED

#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* signed */
typedef char               i8;
typedef short              i16;
typedef int                i32;
typedef long               i32x;
typedef long long          i64;
/* unsigned */
typedef unsigned char      u8;
typedef unsigned short     u16;
typedef unsigned           u32;
typedef unsigned long      u32x;
typedef unsigned long long u64;
/* real */
typedef float              f32;
typedef double             f64;
/* boolean compatible with standart c int */
typedef int                b32;
/* chars */
typedef unsigned char      char8;
typedef unsigned short     char16;
typedef unsigned int       char32;


#ifndef TRUE
    #define TRUE  (1)
#endif
#ifndef FALSE
    #define FALSE (0)
#endif

#define U8_MAX  (0xff)
#define U16_MAX (0xffff)
#define U32_MAX (0xffffffff)
#define U64_MAX (0xffffffffffffffff)

/* basic math */
#define MIN(a, b)      (a < b ? a : b)
#define MAX(a, b)      (a > b ? a : b)
#define CLAMP(a, b, t) (t < a ? a : (t > b ? b : t))

/* bad-ass c stuff */
#define STRINGIFY(x)  #x
#define TOSTRING(x)   STRINGIFY(x)
#define ARRAY_SIZE(a) sizeof(a)/sizeof(a[0])

#define ALIGN(value, alignment)   (((value) + (alignment) - 1) & ~((alignment) - 1))

#define DEBUG_TRACE                 " *** " __FILE__ ":" TOSTRING(__LINE__)
#define LOG_MESSAGE(log, ...)       printf(":: " log             "\r\n", __VA_ARGS__)
#define LOG_ERROR(log, ...)         printf(":! " log DEBUG_TRACE "\r\n", __VA_ARGS__)
#define LOG_WARNING(log, ...)       printf(":? " log DEBUG_TRACE "\r\n", __VA_ARGS__)
#define LOG_MESSAGE_TRACE(log, ...) printf(":: " log DEBUG_TRACE "\r\n", __VA_ARGS__)
#define TRACE                       printf(":> " DEBUG_TRACE "\r\n");

typedef void* CtxHandle;

#endif
