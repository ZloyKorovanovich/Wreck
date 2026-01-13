#ifndef _BASE_INCLUDED
#define _BASE_INCLUDED

/* TYPES */

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

typedef unsigned char byte;
typedef unsigned short word;
typedef unsigned dword;
typedef unsigned long dwordx;
typedef unsigned long long qword;

typedef unsigned b32;
#define TRUE (1)
#define FALSE (0)

/* LIMITS */

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

/* OPERATIONS */

#define MIN(a, b) (a < b ? a : b)
#define MAX(a, b) (a > b ? a : b)
#define CLAMP(a, b, t) ((t < a) ? a : (t > b) ? b : t)

#define ALIGN(ptr, alig) ((ptr + (alig - 1)) & ~(alig - 1))
#define ARRAY_SIZE(array) (sizeof(array) / sizeof(array[0]))

/* ALLOCATORS */

#ifndef ARENA_CUSTOM_STATS
    #define ARENA_DEFAULT_VIRTUAL_SIZE (1024 * 1024 * 1024)
    #define ARENA_DEFAULT_EXPANSION (1024 * 1024 * 4)
#endif
#ifndef STACK_CUSTOM_STATS
    #define STACK_DEFAULT_VIRTUAL_SIZE (1024 * 1024 * 256)
    #define STACK_DEFAULT_PAGE_SIZE (1024 * 64)
#endif


typedef struct {
    void* begin;
    void* end;
    u64 physical_size;
    u64 virtual_size;
} Arena;

/* if limit is default, use limit = 0 */
b32 createArena(Arena* arena, u64 limit);
/* if aligment is any use 0 */
void* allocateArena(Arena* arena, u64 size, u64 aligment);
/* clears arena, so that you can overwrite memory */
void resetArena(Arena* arena);
/* frees arena memory */
b32 freeArena(Arena* arena);


typedef struct {
    void* begin;
    void* edge;
    void* end;
    u64 physical_size;
    u64 virtual_size;
} Stack;

/* if limit is default, use limit = 0 */
b32 createStack(Stack* stack, u64 limit);
/* if aligment default any use 0 */
void* allocateStack(Stack* stack, u64 size, u64 aligment);
/* saves current edge and sets edge to end pointer */
void* pushStack(Stack* stack);
/* the oposite of push */
void* popStack(Stack* stack);
/* clears everything since begin, so that you can overwrite memory */
void clearStack(Stack* stack);
/* frees arena memory */
b32 freeStack(Stack* stack);

/* MEMORY */

typedef void* (*Allocate_pfn)(u64 size, u64 aligment);

typedef struct {
    void* buffer;
    u64 size;
} Buffer;

void setMemory(void* dst, const void* value, u64 size, u64 count);
void copyMemory(void* dst, const void* src, u64 size);

/* STRINGS */

/* even though it has size, its always zero terminated */
typedef struct {
    char* string;
    u64 size;
    u64 capacity;
} String;

#define CONST_STRING(cstr) ((String){(char *)cstr, sizeof(cstr)})

b32 stringAddString(String* dst, const String* src);
b32 stringAddCstring(String* dst, const char* src);

b32 stringAddChar(String* dst, char c);
b32 stringAddU64(String* dst, u64 num);
b32 stringAddI64(String* dst, i64 num);
b32 stringAddf64(String* dst, u64 num);

/* used for moving up in directories */
b32 stringUpFolder(String* path);

/* FILES */

b32 printConsole(const String* string);
b32 scanConsole(String* string);

void file2Buffer(const String* path, Buffer* buffer, Allocate_pfn realloc_callback);
void buffer2File(const String* path, const Buffer* buffer);

#endif
