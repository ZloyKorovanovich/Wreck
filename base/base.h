#ifndef _BASE_INCLUDED
#define _BASE_INCLUDED

#ifdef _WIN32
#include <windows.h>
#endif

/*======================================================================
    TYPES
  ======================================================================*/

typedef char i8;
typedef short i16;
typedef int i32;
typedef long i32x;
typedef long long i64;

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned u32;
typedef unsigned long u32x;
typedef unsigned long long u64;

typedef float f32;
typedef double f64;

typedef union {
  f32 real;
  u32 bits;
} f32Bits;

typedef union {
  f64 real;
  u64 bits;
} f64Bits;

typedef unsigned char byte;
typedef unsigned short word;
typedef unsigned dword;
typedef unsigned long dwordx;
typedef unsigned long long qword;

typedef unsigned b32;
#ifndef _WIN32
  #define TRUE (1)
  #define FALSE (0)
#endif

#ifndef NULL
  #define NULL ((void *)0)
#endif

#define ALLOCATION_GRANULARITY (64 * 1024)
#define MEMORY_PAGE_SIZE (4 * 1024)

/*======================================================================
    LIMITS
  ======================================================================*/

#define I8_MIN  ((i8)0x80)
#define I16_MIN ((i16)0x8000)
#define I32_MIN ((i32)0x80000000)
#define I64_MIN ((i64)0x8000000000000000)

#define I8_MAX  (127)
#define I16_MAX (32767)
#define I32_MAX (2147483647)
#define I64_MAX (9223372036854775807)

#define U8_MIN  (0x0)
#define U16_MIN (0x0)
#define U32_MIN (0x0)
#define U64_MIN (0x0)

#define U8_MAX  (0xff)
#define U16_MAX (0xffff)
#define U32_MAX (0xffffffff)
#define U64_MAX (0xffffffffffffffff)

#define F32_PINF (0x7f800000)
#define F32_NINF (0xff800000)
#define F32_QNAN (0x7fc00000)
#define F32_SNAN (0x7f800001)

#define F64_PZERO (0x0000000000000000)
#define F64_NZERO (0x8000000000000000)
#define F64_PINF (0x7ff0000000000000)
#define F64_NINF (0xfff0000000000000)
#define F64_QNAN (0x7ff8000000000000)
#define F64_SNAN (0x7ff8000000000001)

#define INVALID_ID U32_MAX

/*======================================================================
    OPERATIONS
  ======================================================================*/

#define MIN(a, b) (a < b ? a : b)
#define MAX(a, b) (a > b ? a : b)
#define CLAMP(a, b, t) ((t < a) ? a : (t > b) ? b : t)

#define ALIGN(ptr, alig) ((ptr + (alig - 1)) & ~(alig - 1))
#define ARRAY_SIZE(array) (sizeof(array) / sizeof(array[0]))

/*======================================================================
    ALLOCATORS
  ======================================================================*/

typedef struct {
    void *begin;
    void *edge;
    void *end;
} Arena;

void *allocateArena(Arena *arena, u64 size, u64 alignment, u64 *possible_size);
void freeArena(Arena *arena);

/*======================================================================
    MEMORY
  ======================================================================*/

typedef struct {
    void *begin;
    void *end;
} Segment;

typedef void *(*Allocate_pfn)(u64 size, u64 alignment);

typedef struct {
    void *buffer;
    u64 size;
} Buffer;

typedef struct {
  u64 offset;
  u64 size;
} BufferRegion;

void setMemory(void *dst, const void *value, u64 size, u64 count);
void copyMemory(void *dst, const void *src, u64 size);

/*======================================================================
    STRINGS
  ======================================================================*/

/* even though it has size, its always zero terminated */
typedef struct {
    char *string;
    u64 size;
    u64 capacity;
} String;

#define CONST_STRING(cstr) ((String){(char *)cstr, sizeof(cstr) - 1})
#define IS_EMPTY_STR(str) (str.size == 0)
#define STACK_STR(size) ((String){.string = (char[size]){0}, .capacity = size})

void stringZero(String *str);

b32 stringAddString(String *dst, const String *src);
b32 stringAddCstring(String *dst, const char *src);

b32 stringAddChar(String *dst, char c);
b32 stringAddU64(String *dst, u64 num);
b32 stringAddI64(String *dst, i64 num);
b32 stringAddF64(String *dst, f64 num);
b32 stringAddAddress(String *dst, u64 address);

i64 stringToI64(const String *src);
u64 stringToU64(const String *src);
f64 stringToF64(const String *src);

/* used for moving up in directories */
b32 stringUpFolder(String *path);
/* inserts elements into string %c cstring %s string %u u64 %*/
b32 stringPattern(const String *pattern, const void **elements, String *out_string);

b32 stringCmp(const String *a, const String *b);
b32 cstringCmp(const char *a, const char *b);
void cstringCpy(char *dst, const char *src);

/*======================================================================
    FILES
  ======================================================================*/

b32 printConsole(const String *string);
b32 errorConsole(const String *string);
b32 scanConsole(String *string);

u64 fileToBuffer(const String *path, Buffer *buffer);
b32 bufferToFile(const String *path, const Buffer *buffer);

/*======================================================================
    ASSEMBLY
  ======================================================================*/

b32 atomicCopyMemoryWrite_dword(void *dst, void* src, u64 count);
u32 atomicCmpExchange_dword(void *address, u32 value, u32 cmp_value);
u32 atomicExchange_dword(void* addres, u32 value);

/*======================================================================
    MSG
  ======================================================================*/

typedef enum {
    MSG_CODE_SUCCESS = 0,
    MSG_CODE_ERROR = -1,
    MSG_CODE_WARNING = 1
} MSG_TYPE;

typedef void (*MsgCallback_pfn)(i32 type, const String *msg);

#define MSG_LOG(callback, msg) if(callback) {callback(MSG_CODE_SUCCESS, msg);}
#define MSG_ERROR(callback, msg) if(callback) {callback(MSG_CODE_ERROR, msg);}
#define MSG_WARNING(callback, msg) if(callback) {callback(MSG_CODE_WARNING, msg);}

/* These stringfy look ugly, but thats the only way around :( */
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
#define LOCATION_TRACE " *** " __FILE__ ":" TOSTRING(__LINE__)

#define TRACED_STR(str) CONST_STRING(str LOCATION_TRACE)

#endif
