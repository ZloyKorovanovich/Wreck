#include "base.h"

/* ALLOCATORS */


void * 
allocateArena(
    Arena *arena, 
    u64 size, 
    u64 alignment,
    u64 *possible_size
) {
    if(alignment == 0 || size == 0) {
        if(possible_size) {
            possible_size = 0;
        }
        return NULL;
    }

    void *alloc_address = (void *)ALIGN((u64)arena->edge, alignment); 
    void *new_edge = (u8 *)alloc_address + size;

    if((u64)alloc_address > (u64)arena->end) {
        if(possible_size) {
            *possible_size = 0;
        }
        return NULL;
    }
    /* too big allocation */
    if((u64)new_edge > (u64)arena->end) {
        if(possible_size) {
            *possible_size = (u64)arena->end - (u64)alloc_address;
        }
        return NULL;
    }
    /* correct allocation, apply transform */
    arena->edge = new_edge;
    return alloc_address;
}

void
freeArena(
    Arena *arena
) {
    arena->edge = arena->begin;
}

/* MEMORY */

void setMemory(void *dst, const void *src, u64 size, u64 count) {
    
}

void copyMemory(void *dst, const void *src, u64 size) {
    for(u64 i = 0; i < size; i++) {
        ((u8 *)dst)[i] = ((u8 *)src)[i];
    }
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


b32 stringAddAddress(String *dst, u64 address) {
    const u64 new_size = dst->size + 16;
    if(new_size >= dst->capacity) {
        return FALSE;
    }
    u32 j = 15 * 4;
    for(u32 i = dst->size; i < new_size; i++, j-=4) {
        u64 digit = (address >> j) & 0xf;
        dst->string[i] = (digit > 9) ? (digit - 10) + 65 : digit + 48;
    }
    dst->string[new_size] = '\0';
    dst->size = new_size;
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
    if(num == 0) {
        if(dst->size + 1 > dst->capacity) {
            return FALSE;
        }
        dst->string[dst->size++] = '0';
        dst->string[dst->size] = '\0';
        
    } else {
        b32 is_negative = (num < 0) ? TRUE : FALSE;

        u64 abs_num = (num < 0) ? (u64)(-num) : (u64)num; 
        u64 reverse_abs = 0;
        u32 digits = 0;
        while(abs_num){
            reverse_abs = reverse_abs * 10 + abs_num % 10;
            abs_num = abs_num / 10;
            digits++;
        }
        if(is_negative) {
            dst->string[dst->size++] = '-';
        }
        for(;digits; digits--) {
             dst->string[dst->size++] = reverse_abs % 10 + 48;
             reverse_abs /= 10;
        }
        dst->string[dst->size] = '\0';
    }
    return TRUE;
}

b32 stringAddF64(String *dst, f64 num) {
    f64Bits bits = (f64Bits){.real = num};
    if(dst->size >= dst->capacity) {
        return FALSE;
    }
    if(bits.bits == F64_PZERO) {
        return stringAddCstring(dst, "+0.0");
    }
    if(bits.bits == F64_NZERO) {
        return stringAddCstring(dst, "-0.0");
    }
    if(bits.bits == F64_PINF) {
        return stringAddCstring(dst, "+inf");
    }
    if(bits.bits == F64_NINF) {
        return stringAddCstring(dst, "-inf");
    }
    if(bits.bits == F64_QNAN) {
        return stringAddCstring(dst, "qNaN");
    }
    if(bits.bits == F64_SNAN) {
        return stringAddCstring(dst, "sNaN");
    }

    b32 is_negative = (num < 0) ? TRUE : FALSE;
    u64 int_part = is_negative ? (u64)(-num) : (u64)num;
    u64 real_part = (u64)(((num < 0) ? -(num - (f64)(int_part)) : (num - (f64)(int_part))) * 1000000.0);

    if(is_negative) {
        if(!stringAddChar(dst, '-')) {
            return FALSE;
        }
    }
    if(!stringAddU64(dst, int_part)) {
        return FALSE;
    }
    if(!stringAddChar(dst, '.')) {
        return FALSE;
    }
    if(dst->size + 6 >= dst->capacity) {
        return FALSE;
    }
    u32 j = dst->size + 5;
    dst->size += 6;
    for(u32 i = 0; i < 6; i++) {
        dst->string[j] = real_part % 10 + 48;
        real_part /= 10;
        j--;
    }
    return TRUE;
}


i64 stringToI64(const String *src) {
    if(src->size == 0) {
        return 0;
    }

    i64 num = 0;
    b32 is_negative = (src->string[0] == '-') ? TRUE : FALSE;

    i64 number_end = 0;
    for(u32 i = is_negative ? 1 : 0; i < src->size; i++) {
        if(src->string[i] < 48 || src->string[i] > 58) {
            break;
        }
        number_end++;
    }

    if(number_end == 0) {
        return 0;
    }

    number_end += is_negative ? 1 : 0;
    i64 tens = 1;
    /* get digits */
    for(u32 i = number_end - 1; i >= (is_negative ? 1 : 0); i--) {
        num += (src->string[i] - 48) * tens;
        tens *= 10;
        if(i == 0) {
            break;
        }
    }
    return is_negative ? -num : num;
}

u64 stringToU64(const String *src) {
    if(src->size == 0) {
        return 0;
    }

    u64 num = 0;
    u64 number_end = 0;
    for(u32 i = 0; i < src->size; i++) {
        if(src->string[i] < 48 || src->string[i] > 58) {
            break;
        }
        number_end++;
    }

    if(number_end == 0) {
        return 0;
    }

    u64 tens = 1;
    /* get digits */
    for(u32 i = number_end - 1; i >= 0; i--) {
        num += (src->string[i] - 48) * tens;
        tens *= 10;
        if(i == 0) {
            break;
        }
    }
    return num;
}

f64 stringToF64(const String *src) {
    if(src->size == 0) {
        return 0.0;
    }

    b32 is_negative = (src->string[0] == '-') ? TRUE : FALSE;
    /* find special locations */
    u64 num_dot = U64_MAX;
    u64 num_begin = is_negative ? 1 : 0;
    u64 num_end = src->size;
    for(u32 i = num_begin; i < src->size; i++) {
        if(src->string[i] == '.') {
            if(num_dot == U64_MAX) {
                num_dot = i;
                continue;
            }
            num_end = i;
            break;
        }
        if(src->string[i] < 48 || src->string[i] > 58) {
            num_end = i;
            break;
        }
    }

    /* get integer part */
    f64 num = 0.0;
    u64 digits = 0;
    u64 tens = 1;
    for(u32 i = MIN(num_dot, num_end) - 1; i >= num_begin; i--) {
        digits += (src->string[i] - 48) * tens;
        tens *= 10;
        if(i == num_begin) {
            break;
        }
    }
    num += (f64)digits;
    /* if float part doesnt exist */
    if(num_dot == U64_MAX) {
        return is_negative ? -num : num;
    }

    /* get float part */
    digits = 0;
    tens = 1;
    for(u32 i = num_end - 1; i > num_dot; i--) {
        digits += (src->string[i] - 48) * tens;
        tens *= 10;
    }
    num += ((f64)digits / (f64)(tens));
    return is_negative ? -num : num;
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
            
            /* STRINGS */
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

            if(pattern->string[i + 1] == 'p') {
                if(!stringAddAddress(out_string, *((const u64*)elements[element_iterator++]))) {
                    return FALSE;
                }
                i += 1;
            }

            /* UNSIGNED */
            if(pattern->string[i + 1] == 'u' && pattern->string[i + 2] == '6' && pattern->string[i + 3] == '4') {
                if(!stringAddU64(out_string, *((const u64*)elements[element_iterator++]))) {
                    return FALSE;
                }
                i += 3;
            }
            if(pattern->string[i + 1] == 'u' && pattern->string[i + 2] == '3' && pattern->string[i + 3] == '2') {
                if(!stringAddU64(out_string, *((const u32*)elements[element_iterator++]))) {
                    return FALSE;
                }
                i += 3;
            }
            if(pattern->string[i + 1] == 'u' && pattern->string[i + 2] == '1' && pattern->string[i + 3] == '6') {
                if(!stringAddU64(out_string, *((const u16*)elements[element_iterator++]))) {
                    return FALSE;
                }
                i += 3;
            }
            if(pattern->string[i + 1] == 'u' && pattern->string[i + 2] == '8') {
                if(!stringAddU64(out_string, *((const u8*)elements[element_iterator++]))) {
                    return FALSE;
                }
                i += 2;
            }

            /* SIGNED */
            if(pattern->string[i + 1] == 'i' && pattern->string[i + 2] == '6' && pattern->string[i + 3] == '4') {
                if(!stringAddI64(out_string, *((const i64*)elements[element_iterator++]))) {
                    return FALSE;
                }
                i += 3;
            }
            if(pattern->string[i + 1] == 'i' && pattern->string[i + 2] == '3' && pattern->string[i + 3] == '2') {
                if(!stringAddI64(out_string, *((const i32*)elements[element_iterator++]))) {
                    return FALSE;
                }
                i += 3;
            }
            if(pattern->string[i + 1] == 'i' && pattern->string[i + 2] == '1' && pattern->string[i + 3] == '6') {
                if(!stringAddI64(out_string, *((const i16*)elements[element_iterator++]))) {
                    return FALSE;
                }
                i += 3;
            }
            if(pattern->string[i + 1] == 'i' && pattern->string[i + 2] == '8') {
                if(!stringAddI64(out_string, *((const i8*)elements[element_iterator++]))) {
                    return FALSE;
                }
                i += 2;
            }

            /* FLOATING POINT */
            if(pattern->string[i + 1] == 'f' && pattern->string[i + 2] == '6' && pattern->string[i + 3] == '4') {
                if(!stringAddF64(out_string, *((const f64*)elements[element_iterator++]))) {
                    return FALSE;
                }
                i += 3;
            }
            if(pattern->string[i + 1] == 'f' && pattern->string[i + 2] == '3' && pattern->string[i + 3] == '2') {
                if(!stringAddF64(out_string, *((const f32*)elements[element_iterator++]))) {
                    return FALSE;
                }
                i += 3;
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
    HANDLE file = CreateFile(path->string, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
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
