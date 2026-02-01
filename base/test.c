#include "base.h"


void arenaTest(void) {
    String string_buffer = (String) {
        .string = (char[256]){0},
        .size = 0,
        .capacity = 256
    };
    printConsole(&CONST_STRING("BASE TEST!\n"));

    Arena arena = (Arena){0};
    if(createArena(&arena, 4096, 4096)) {
        printConsole(&CONST_STRING("created arena\n"));
    } else {
        printConsole(&CONST_STRING("failed to create arena!\n"));
    }
    if(allocateArena(&arena, 3324, 4)) {
        stringAddCstring(&string_buffer, "allocated arena 1 physical: ");
        stringAddU64(&string_buffer, arena.physical_size);
        stringAddCstring(&string_buffer, " virtual: ");
        stringAddU64(&string_buffer, arena.virtual_size);
        stringAddCstring(&string_buffer, "\n");

        printConsole(&string_buffer);
        stringZero(&string_buffer);
    } else {
        printConsole(&CONST_STRING("SHIT 2!\n"));
    }
    if(allocateArena(&arena, 1239281, 4)) {
        stringAddCstring(&string_buffer, "allocated arena 2 physical: ");
        stringAddU64(&string_buffer, arena.physical_size);
        stringAddCstring(&string_buffer, " virtual: ");
        stringAddU64(&string_buffer, arena.virtual_size);
        stringAddCstring(&string_buffer, "\n");

        printConsole(&string_buffer);
        stringZero(&string_buffer);
    } else {
        printConsole(&CONST_STRING("failed to make second allocation!\n"));
    }

    clearArena(&arena);
    if(allocateArena(&arena, 4096, 4)) {
        stringAddCstring(&string_buffer, "allocated arena 3 after clear physical: ");
        stringAddU64(&string_buffer, arena.physical_size);
        stringAddCstring(&string_buffer, " virtual: ");
        stringAddU64(&string_buffer, arena.virtual_size);
        stringAddCstring(&string_buffer, "\n");

        printConsole(&string_buffer);
        stringZero(&string_buffer);
    } else {
        printConsole(&CONST_STRING("failed to make allocation after clear!\n"));
    }

    if(freeArena(&arena)) {
        stringAddCstring(&string_buffer, "free arena physical: ");
        stringAddU64(&string_buffer, arena.physical_size);
        stringAddCstring(&string_buffer, " virtual: ");
        stringAddU64(&string_buffer, arena.virtual_size);
        stringAddCstring(&string_buffer, "\n");

        printConsole(&string_buffer);
        stringZero(&string_buffer);
    } else {
        printConsole(&CONST_STRING("failed to free arena!\n"));
    }
}

void stringPatternTest(void) {
    String print_string = {
        .string = (char[512]){0},
        .capacity = 512
    };

    String val_string = CONST_STRING("labubu");
    const char* val_cstring = "hello world";

    u64 val_u64 = 10;
    u32 val_u32 = 12314;
    u16 val_u16 = 636;
    u8 val_u8 = 214;

    i64 val_i64 = -2947;
    i32 val_i32 = 21312;
    i16 val_i16 = 124;
    i8 val_i8 = -98;

    f64 val_f64 = 2.0812;
    f32 val_f32 = 2131.5;

    if(!stringPattern(
        &CONST_STRING("pattern string: %s %c %u64 %u32 %u16 %u8 %i64 %i32 %i16 %i8 %f64 %f32\n"), 
        (const void*[]){
            &val_string, val_cstring,
            &val_u64, &val_u32, &val_u16, &val_u8,
            &val_i64, &val_i32, &val_i16, &val_i8,
            &val_f64, &val_f32
        }, &print_string
    )) {
        printConsole(&CONST_STRING("pattern string fail\n"));
    }
    printConsole(&print_string);

    stringZero(&print_string);
    stringAddI64(&print_string, stringToI64(&CONST_STRING("-231")));
    stringAddChar(&print_string, '\n');
    printConsole(&print_string);

    stringZero(&print_string);
    stringAddU64(&print_string, stringToU64(&CONST_STRING("2312")));
    stringAddChar(&print_string, '\n');
    printConsole(&print_string);

    stringZero(&print_string);
    stringAddF64(&print_string, stringToF64(&CONST_STRING("52312.931")));
    stringAddChar(&print_string, '\n');
    printConsole(&print_string);
}

void asmTest(void) {
    u32 array_dst[15] = {0};
    u32 array_src[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    asmCopyMemoryDwordAtomicW(array_dst, array_src, 10);
    asmCopyMemoryQwordAtomicW(array_dst, array_src + 4, 2);

    String str = STACK_STR(256);
    for(u32 i = 0; i < 10; i++) {
        stringAddU64(&str, array_dst[i]);
        stringAddChar(&str, ' ');
    }
    printConsole(&str);
}

i32 main(i32 argc, char **argv) {
    arenaTest();
    stringPatternTest();
    asmTest();

    return 0;
}
