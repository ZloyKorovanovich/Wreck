#ifndef _BASE_INCLDUED
#define _BASE_INCLDUED

/* compile flags:
    #define DEBUG_LOG */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
    #include <windows.h>

    #define strcat(dst, src, size) strcat_s(dst, (rsize_t)size, src)
    #define strcpy(dst, src, size) strcpy_s(dst, (rsize_t)size, src)
#else
    #define strcpy(dst, src, size) (strcpy)(dst, src)
    #define strcat(dst, src, size) (strcat)(dst, src)
#endif

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
#ifndef _WIN32
    #define TRUE  (1)
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
/* mutex */
typedef enum {
    MUTEX_OBJECT      = 0,
    MUTEX_ABANDONED   = 1,
    MUTEX_TIMEOUT     = 2,
    MUTEX_FAILED      = 3
} MutexResult;

#ifdef _WIN32
    typedef HANDLE Mutex;

    static inline Mutex createMutexHandle(
        const char* name
    ) {
        return (Mutex)CreateMutexA(NULL, FALSE, name);
    }

    static inline MutexResult waitForMutex(
        Mutex mutex, 
        u32   ms
    ) {
        DWORD result =  WaitForSingleObject((HANDLE)mutex, ms);
        if(result == WAIT_OBJECT_0) {
            return MUTEX_OBJECT;
        }
        if(result == WAIT_ABANDONED) {
            return MUTEX_ABANDONED;
        }
        if(result == WAIT_TIMEOUT) {
            return MUTEX_TIMEOUT;
        }
        if(result == WAIT_FAILED) {
            return MUTEX_FAILED;
        }

        return MUTEX_FAILED;
    }

    static inline void releaseMutex(
        Mutex mutex
    ) {
        ReleaseMutex((HANDLE)mutex);
    }

    static inline void destroyMutexHandle(
        Mutex mutex
    ) {
        CloseHandle((HANDLE)mutex);
    }
#endif

/* simple log */
#ifdef DEBUG_LOG
    #define DEBUG_TRACE                 " *** " __FILE__ ":" TOSTRING(__LINE__)
    #define LOG_MESSAGE(log, ...)       printf(":: " log             "\r\n", __VA_ARGS__)
    #define LOG_ERROR(log, ...)         printf(":! " log DEBUG_TRACE "\r\n", __VA_ARGS__)
    #define LOG_WARNING(log, ...)       printf(":? " log DEBUG_TRACE "\r\n", __VA_ARGS__)
    #define LOG_MESSAGE_TRACE(log, ...) printf(":: " log DEBUG_TRACE "\r\n", __VA_ARGS__)
#else
    #define DEBUG_TRACE
    #define LOG_MESSAGE(log, ...)       ;
    #define LOG_ERROR(log, ...)         ;
    #define LOG_WARNING(log, ...)       ;
    #define LOG_MESSAGE_TRACE(log, ...) ;
#endif

typedef void* CtxHandle;

/* HASHES 
    CtxHandle is void*, which means you could accidentally substitute something different there.
    We are aware of this problem and insert uique 64bit hash value as fisrt filed of each ctx struct.
        - this value should be set when creating ctx;
        - erased when destroying it;
        - checked every time you get ctx through CtxHandle (void*);                                       
    You can also use has to idetify structs in memory damp                                             */
#define VULKAN_CTX_HASH (0x123981741)
#define LOADER_CTX_HASH (0x215135324)

#define VULKAN_CTX_SIZE (4096)
#define LOADER_CTX_SIZE (4096)

#endif
