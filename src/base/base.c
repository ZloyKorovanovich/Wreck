#include "base.h"

/* INCLUDES */
#if defined(_WIN32)
    #include <windows.h>
#endif

/* ALLOCATORS */

#if defined(_WIN32)

    /* ARENA */

    b32 createArena(Arena* arena, u64 limit) {
        /* validate allocation, align to page size */
        limit = ALIGN((limit == 0) ? ARENA_DEFAULT_VIRTUAL_SIZE : limit, ARENA_DEFAULT_EXPANSION);
        /* reserve virtual address space */
        void* virtual_allocation = VirtualAlloc(NULL, limit, MEM_LARGE_PAGES | MEM_RESERVE, 0);
        if(!virtual_allocation) {
            return FALSE;
        }
        /* fill struct */
        *arena = (Arena) {
            .begin = virtual_allocation,
            .end = virtual_allocation,
            .virtual_size = limit
        };
        return TRUE;
    }

    void* allocateArena(Arena* arena, u64 size, u64 aligment) {
        if(size == 0) {
            return NULL;
        }
        /* default aligment if aligment == 0 */
        aligment = (aligment == 0) ? 8 : aligment;
        /* calculate where user allocation will begin */
        u64 alloc_offset = ALIGN(((u64)arena->end - (u64)arena->begin), aligment);
        /* if exceed virtual size, things are very bad */
        if(alloc_offset + size > arena->virtual_size) {
            return NULL;
        }
        /* if physical size is too small, allocate more physical memory */
        if(alloc_offset + size > arena->physical_size) {
            u64 commit_size = ALIGN((alloc_offset + size - arena->physical_size), ARENA_DEFAULT_EXPANSION);
            if(!VirtualAlloc((byte*)arena->begin + arena->physical_size, commit_size, MEM_COMMIT, 0)) {
                return NULL;
            }
            arena->physical_size += commit_size;
        }
        /* adjust arena and return user pointer */
        arena->end = (byte*)arena->begin + alloc_offset + size;
        return (byte*)arena->begin + alloc_offset;
    }

    void resetArena(Arena* arena) {
        /* move end to begin and assume everything that was begin and end will be now overwritten */
        arena->end = arena->begin;
    }

    b32 freeArena(Arena* arena) {
        /* free on windows might fail */
        if(!VirtualFree(arena->begin, arena->virtual_size, MEM_RELEASE)) {
            return FALSE;
        }
        *arena = (Arena){0};
        return TRUE;
    }

    /* STACK */

    b32 createStack(Stack* stack, u64 limit) {
        /* validate allocation, align to page size */
        limit = ALIGN((limit == 0) ? STACK_DEFAULT_VIRTUAL_SIZE : limit, STACK_DEFAULT_PAGE_SIZE);
        /* reserve virtual address space */
        void* virtual_allocation = VirtualAlloc(NULL, limit, MEM_64K_PAGES | MEM_RESERVE, 0);
        if(!virtual_allocation) {
            return FALSE;
        }
        /* fill struct */
        *stack = (Stack) {
            .begin = virtual_allocation,
            .edge = virtual_allocation,
            .end = virtual_allocation,
            .virtual_size = limit
        };
        return TRUE;
    }

    void* allocateStack(Stack* stack, u64 size, u64 aligment) {
        if(size == 0) {
            return NULL;
        }
        /* default aligment if aligment == 0 */
        aligment = (aligment == 0) ? 8 : aligment;
        /* calculate where user allocation will begin */
        u64 alloc_offset = ALIGN(((u64)stack->end - (u64)stack->begin), aligment);
        /* if exceed virtual size, things are very bad */
        if(alloc_offset + size > stack->virtual_size) {
            return NULL;
        }
        /* if physical size is too small, allocate more physical memory */
        if(alloc_offset + size > stack->physical_size) {
            u64 commit_size = ALIGN((alloc_offset + size - stack->physical_size), ARENA_DEFAULT_EXPANSION);
            if(!VirtualAlloc((byte*)stack->begin + stack->physical_size, commit_size, MEM_COMMIT, 0)) {
                return NULL;
            }
            stack->physical_size += commit_size;
        }
        /* adjust arena and return user pointer */
        stack->end = (byte*)stack->begin + alloc_offset + size;
        return (byte*)stack->begin + alloc_offset;
    }

    void* pushStack(Stack* stack) {
        void** push_slot = allocateStack(stack, sizeof(void*), 8);
        if(!push_slot) {
            return NULL;
        }
        push_slot = stack->edge;
        stack->edge = stack->end;
        return stack->edge;
    }

    void* popStack(Stack* stack) {
        if(stack->begin == stack->edge) {
            return NULL;
        }
        stack->end = stack->edge;
        stack->edge = *((void**)stack->edge);
        return stack->edge;
    }

    void clearStack(Stack* stack) {
        stack->edge = stack->begin;
        stack->end = stack->begin;
    }

    b32 freeStack(Stack* stack) {
        /* free on windows might fail */
        if(!VirtualFree(stack->begin, stack->virtual_size, MEM_RELEASE)) {
            return FALSE;
        }
        *stack = (Stack){0};
        return TRUE;
    }

#endif

/* MEMORY */

void setMemory(void* dst, const void* value, u64 size, u64 count) {
    
}

void copyMemory(void* dst, const void* src, u64 size) {
    
}

/* STRINGS */

b32 stringAddString(String* dst, const String* src) {
    u64 new_size = dst->size + src->size;
    /* if string length + '\0' is bigger than capacity, return false */
    if(new_size + 1 > src->capacity) {
        return FALSE;
    }

    char* copy_begin = &dst->string[dst->size];
    const u64 copy_size = src->size + 1;
    for(u64 i = 0; i < copy_size; i++) {
        copy_begin[i] = src->string[i];
    }
    dst->size = new_size;
    return TRUE;
}

b32 stringAddCstring(String* dst, const char* src) {
    u64 new_size = dst->size;
    while (*src) {
        if(new_size + 1 > dst->size) {
            dst->string[dst->size] = '\0';
            return FALSE;
        }
        
        dst->string[new_size++] = *src++; 
    }
    dst->string[new_size] = '\0';
    return TRUE;
}

b32 stringAddChar(String* dst, char c) {
    if(dst->size + 1 == dst->capacity) {
        return FALSE;
    }
    dst->string[dst->size++] = c;
    dst->string[dst->size] = '\0';
    return TRUE;
}

b32 stringAddU64(String* dst, u64 num) {
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
        for(;digits; digits--) dst->string[dst->size++] = reverse % 10 + 48;
        dst->string[dst->size] = '\0';
    }
    return TRUE;
}

b32 stringAddI64(String* dst, i64 num) {
    return FALSE;
}

b32 stringAddf64(String* dst, u64 num) {
    return FALSE;
}


b32 stringUpFolder(String* path) {
    for(u64 i = path->size - 1; i >= 0; i--) {
        if(path->string[i] == '/' || path->string[i] == '\\') {
            path->string[i] = '\0';
            path->size = i;
            return TRUE;
        }
    }
    return FALSE;
}

/* FILES */

static struct {
    HANDLE in;
    HANDLE out;
    HANDLE error;
} s_std_handles = {0};

b32 printConsole(const String* string) {
    if(s_std_handles.out == NULL) {
        s_std_handles.out = GetStdHandle(STD_OUTPUT_HANDLE);
    }
    return WriteFile(s_std_handles.out, string->string, string->size, NULL, NULL);
}

b32 scanConsole(String* string) {
    if(s_std_handles.in == NULL) {
        s_std_handles.in = GetStdHandle(STD_INPUT_HANDLE);
    }
    return ReadFile(s_std_handles.in, string, string->capacity, string->size, NULL);
}


void file2Buffer(const String* path, Buffer* buffer, Allocate_pfn realloc_callback) {

}

void buffer2File(const String* path, const Buffer* buffer) {

}
