#include "base.h"

/* INCLUDES */
#if defined(_WIN32)
    #include <windows.h>
#endif

/* ALLOCATORS */

#if defined(_WIN32)

    /* ARENA */

    b32 createArena(Arena *arena, u64 limit, u64 expansion) {
        /* validate allocation, align to page size */
        limit = ALIGN(((limit == 0) ? ARENA_DEFAULT_VIRTUAL_SIZE : limit), ARENA_DEFAULT_EXPANSION);
        /* validate expansion, align it to 4 KB at least */
        expansion = ALIGN((expansion == 0) ? ARENA_DEFAULT_EXPANSION : expansion, 4 * 1024);
        /* reserve virtual address space */
        void *virtual_allocation = VirtualAlloc(NULL, limit, MEM_RESERVE, PAGE_READWRITE);
        if(!virtual_allocation) {
            return FALSE;
        }
        /* fill struct */
        *arena = (Arena) {
            .begin = virtual_allocation,
            .end = virtual_allocation,
            .virtual_size = limit,
            .expansion = expansion
        };
        return TRUE;
    }

    void *allocateArena(Arena *arena, u64 size, u64 aligment) {
        if(size == 0) {
            return NULL;
        }
        /* default aligment if aligment == 0 */
        aligment = (aligment == 0) ? DEFAULT_ALIGMENT : aligment;
        /* calculate where user allocation will begin */
        u64 alloc_offset = ALIGN(((u64)arena->end - (u64)arena->begin), aligment);
        /* if exceed virtual size, things are very bad */
        if(alloc_offset + size > arena->virtual_size) {
            return NULL;
        }
        /* if physical size is too small, allocate more physical memory */
        if(alloc_offset + size > arena->physical_size) {
            u64 commit_size = ALIGN((alloc_offset + size - arena->physical_size), arena->expansion);
            if(!VirtualAlloc((byte*)arena->begin + arena->physical_size, commit_size, MEM_COMMIT, PAGE_READWRITE)) {
                return NULL;
            }
            arena->physical_size += commit_size;
        }
        /* adjust arena and return user pointer */
        arena->end = (byte*)arena->begin + alloc_offset + size;
        return (byte*)arena->begin + alloc_offset;
    }

    void clearArena(Arena *arena) {
        /* move end to begin and assume everything that was begin and end will be now overwritten */
        arena->end = arena->begin;
    }

    b32 resetArena(Arena *arena) {
        /* free on windows might fail need to specify size if releasing physical */
        if(!VirtualFree(arena->begin, arena->physical_size, MEM_DECOMMIT)) {
            return FALSE;
        }
        arena->physical_size = 0;
        arena->end = arena->begin;
        return TRUE;
    }

    b32 freeArena(Arena *arena) {
        /* free on windows might fail no need to specify size if releasing virtual */
        if(!VirtualFree(arena->begin, 0, MEM_RELEASE)) {
            return FALSE;
        }
        *arena = (Arena){0};
        return TRUE;
    }

    /* STACK */

    b32 createStack(Stack *stack, u64 limit, u64 expansion) {
        /* validate allocation, align to page size */
        limit = ALIGN((limit == 0) ? STACK_DEFAULT_VIRTUAL_SIZE : limit, STACK_DEFAULT_PAGE_SIZE);
        /* validate expansion, align it to 4 KB at least */
        expansion = ALIGN((expansion == 0) ? ARENA_DEFAULT_EXPANSION : expansion, 4 * 1024);
        /* reserve virtual address space */
        void *virtual_allocation = VirtualAlloc(NULL, limit, MEM_RESERVE, PAGE_READWRITE);
        if(!virtual_allocation) {
            return FALSE;
        }
        /* fill struct */
        *stack = (Stack) {
            .begin = virtual_allocation,
            .edge = virtual_allocation,
            .end = virtual_allocation,
            .virtual_size = limit,
            .expansion = expansion
        };
        return TRUE;
    }

    void *allocateStack(Stack *stack, u64 size, u64 aligment) {
        if(size == 0) {
            return NULL;
        }
        /* default aligment if aligment == 0 */
        aligment = (aligment == 0) ? DEFAULT_ALIGMENT : aligment;
        /* calculate where user allocation will begin */
        u64 alloc_offset = ALIGN(((u64)stack->end - (u64)stack->begin), aligment);
        /* if exceed virtual size, things are very bad */
        if(alloc_offset + size > stack->virtual_size) {
            return NULL;
        }
        /* if physical size is too small, allocate more physical memory */
        if(alloc_offset + size > stack->physical_size) {
            u64 commit_size = ALIGN((alloc_offset + size - stack->physical_size), stack->expansion);
            if(!VirtualAlloc((byte*)stack->begin + stack->physical_size, commit_size, MEM_COMMIT, PAGE_READWRITE)) {
                return NULL;
            }
            stack->physical_size += commit_size;
        }
        /* adjust arena and return user pointer */
        stack->end = (byte*)stack->begin + alloc_offset + size;
        return (byte*)stack->begin + alloc_offset;
    }

    void *pushStack(Stack *stack) {
        void** push_slot = allocateStack(stack, sizeof(void*), sizeof(void*));
        if(!push_slot) {
            return NULL;
        }
        *push_slot = stack->edge;
        stack->edge = stack->end;
        return stack->edge;
    }

    void *popStack(Stack *stack) {
        if(stack->begin == stack->edge) {
            return NULL;
        }
        stack->end = stack->edge;
        stack->edge = *((void**)stack->edge);
        return stack->edge;
    }

    void clearStack(Stack *stack) {
        stack->edge = stack->begin;
        stack->end = stack->begin;
    }

    b32 resetStack(Stack *stack) {
        /* free on windows might fail need to specify size if releasing physical */
        if(!VirtualFree(stack->begin, stack->physical_size, MEM_DECOMMIT)) {
            return FALSE;
        }
        stack->physical_size = 0;
        stack->edge = stack->begin;
        stack->end = stack->begin;
        return TRUE;
    }

    b32 freeStack(Stack *stack) {
        /* free on windows might fail no need to specify size if releasing virtual */
        if(!VirtualFree(stack->begin, 0, MEM_RELEASE)) {
            return FALSE;
        }
        *stack = (Stack){0};
        return TRUE;
    }

#endif

/* MEMORY */

void setMemory(void *dst, const void *scr, u64 size, u64 count) {
    
}

void copyMemory(void *dst, const void *src, u64 size) {
    
}

/* STRINGS */

void stringZero(String *str) {
    str->size = 0;
}

b32 stringAddString(String *dst, const String *src) {
    u64 new_size = dst->size + src->size;
    /* if string length + '\0' is bigger than capacity, return false */
    if(new_size + 1 > dst->capacity) {
        return FALSE;
    }
    /* set iterator to the end of dst (on '\0' symbol) */
    char *copy_begin = &dst->string[dst->size];
    /* copy with '\0' in the end */
    for(u64 i = 0; i < src->size; i++) {
        copy_begin[i] = src->string[i];
    }
    dst->size = new_size;
    dst->string[new_size] = '\0';
    return TRUE;
}

b32 stringAddCstring(String *dst, const char *src) {
    while (*src) {
        if(dst->size + 1 > dst->capacity) {
            dst->string[dst->size] = '\0';
            return FALSE;
        }
        
        dst->string[dst->size++] = *src++; 
    }
    dst->string[dst->size] = '\0';
    return TRUE;
}

b32 stringAddChar(String *dst, char c) {
    if(dst->size + 1 == dst->capacity) {
        return FALSE;
    }
    dst->string[dst->size++] = c;
    dst->string[dst->size] = '\0';
    return TRUE;
}

b32 stringAddU64(String *dst, u64 num) {
    if(num == 0) {
        if(dst->size + 1 > dst->capacity) {
            return FALSE;
        }
        dst->string[dst->size++] = '0';
        dst->string[dst->size] = '\0';
        
    } else {
        u64 reverse = 0;
        u32 digits = 0;
        while(num){
            reverse = reverse * 10 + num % 10;
            num = num / 10;
            digits++;
        }
        for(;digits; digits--) {
             dst->string[dst->size++] = reverse % 10 + 48;
             reverse /= 10;
        }
        dst->string[dst->size] = '\0';
    }
    return TRUE;
}

b32 stringAddI64(String *dst, i64 num) {
    return FALSE;
}

b32 stringAddf64(String *dst, u64 num) {
    return FALSE;
}


i64 stirngToI64(const String *src) {
    return 0;
}

u64 stirngToU64(const String *src) {
    return 0;
}

f64 stringToF64(const String *src) {
    return 0.0;
}


b32 stringUpFolder(String *path) {
    for(u64 i = path->size - 1; i >= 0; i--) {
        if(path->string[i] == '/' || path->string[i] == '\\') {
            path->string[i] = '\0';
            path->size = i;
            return TRUE;
        }
    }
    return FALSE;
}

b32 stringPattern(const String *pattern, const void** elements, String *out_string) {
    out_string->size = 0;
    u32 element_iterator = 0;
    for(u32 i = 0; i < pattern->size; i++) {
        if(pattern->string[i] == '%' && pattern->string[i + 1] != '%') {
            if(pattern->string[i + 1] == 's') {
                if(!stringAddString(out_string, (const String*)elements[element_iterator++])) {
                    return FALSE;
                }
                i += 1;
            }
            if(pattern->string[i + 1] == 'c') {
                if(!stringAddCstring(out_string, (const char*)elements[element_iterator++])) {
                    return FALSE;
                }
                i += 1;
            }
            if(pattern->string[i + 1] == 'u') {
                if(!stringAddU64(out_string, *((const u64*)elements[element_iterator++]))) {
                    return FALSE;
                }
                i += 1;
            }
            continue;
        }
        if(pattern->string[i] == '%' && pattern->string[i + 1] == '%') i++;
        /* check if exceed limit of memory */
        if(out_string->size >= out_string->capacity) {
            return FALSE;
        }
        out_string->string[out_string->size++] = pattern->string[i];
    }
    return TRUE;
}


b32 stringCmp(const String *a, const String *b) {
    if(a->size != b->size) {
        return FALSE;
    }
    for(u32 i = 0; i < a->size; i++) {
        if(a->string[i] != b->string[i]) {
            return FALSE;
        }
    }
    return TRUE;
}

b32 cstringCmp(const char *a, const char *b) {
    while (*a && *b) {
        if(*a++ != *b++) {
            return FALSE;
        }
    }
    return (*a == *b);
}

void cstringCpy(char *dst, const char *src) {
    while ((*dst++ = *src++));
}

/* FILES */

static struct {
    HANDLE in;
    HANDLE out;
    HANDLE error;
} s_std_handles = {0};

b32 printConsole(const String *string) {
    if(s_std_handles.out == NULL) {
        s_std_handles.out = GetStdHandle(STD_OUTPUT_HANDLE);
    }
    return WriteFile(s_std_handles.out, string->string, string->size, NULL, NULL);
}

b32 errorConsole(const String *string) {
    if(s_std_handles.error == NULL) {
        s_std_handles.error = GetStdHandle(STD_ERROR_HANDLE);
    }
    return WriteFile(s_std_handles.error, string->string, string->size, NULL, NULL);
}

b32 scanConsole(String *string) {
    if(s_std_handles.in == NULL) {
        s_std_handles.in = GetStdHandle(STD_INPUT_HANDLE);
    }
    return ReadFile(s_std_handles.in, string, string->capacity, (u32x*)&string->size, NULL);
}

/* retruns 0 if fail, size of file in other cases */
u64 fileToBuffer(const String *path, Buffer *buffer) {
    HANDLE file = CreateFile(path->string, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if(file == INVALID_HANDLE_VALUE) {
        return 0;
    }
    
    u64 file_size = 0;
    /* RETARDED WAY OF COMPUTING SIZE */ {
        u32x size_low;
        u32x size_high;
        size_low = GetFileSize(file, &size_high);
        file_size = ((u64)size_low) | ((u64)size_high << 32);
    }
    if(file_size == 0) {
        CloseHandle(file);
        return 0;
    }
    if(file_size > buffer->size) {
        CloseHandle(file);
        return file_size;
    }
    
    /* read file contents */
    u64 read_bytes = 0;
    while (read_bytes != file_size) {
        u32x just_read = 0;
        if(!ReadFile(file, (u8*)buffer->buffer + read_bytes, (u32)MIN(file_size - read_bytes, (u64)U32_MAX), &just_read, NULL)) {
            CloseHandle(file);
            return 0;
        }
        read_bytes += just_read;
    }

    CloseHandle(file);

    return file_size;
}

b32 bufferToFile(const String *path, const Buffer *buffer) {
    HANDLE file = CreateFile(path->string, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if(file == INVALID_HANDLE_VALUE) {
        return FALSE;
    }

    u64 bytes_written = 0;
    while(bytes_written != buffer->size) {
        u32x just_written = 0;
        if(!WriteFile(file, (u8*)buffer->buffer + bytes_written, (u32)(MIN(buffer->size - bytes_written, (u64)U32_MAX)), &just_written, NULL)) {
            CloseHandle(file);
            return FALSE;
        }
        bytes_written += just_written;
    }

    CloseHandle(file);
    return TRUE;
}