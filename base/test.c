#include "base.h"


void arenaTest(void) {
    String string_buffer = (String) {
        .string = (char[256]){0},
        .size = 0,
        .capacity = 256
    };
    printConsole(&CONST_STRING("BASE TEST!\n"));

    Arena arena = (Arena){0};
    if(createArena(&arena, 0, 4096)) {
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
    if(allocateArena(&arena, 1024, 4)) {
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

    String some_shit = CONST_STRING("who the fuck are you");

    u64 test_val = 10;
    u64 other_val = 2314;

    if(!stringPattern(&CONST_STRING("this is pattern %u %c world %s %u\n"), (const void*[]){&test_val, "hello", &some_shit, &other_val}, &print_string)) {
        printConsole(&CONST_STRING("NO WAY!\n"));
    }

    printConsole(&print_string);
}

i32 main(i32 argc, char** argv) {

    stringPatternTest();

    return 0;
}
