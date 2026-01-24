#include <base.h>

const u16 indices[] = {
    0, 1, 2,
    0, 2, 3
};

const f32 positions[][3] = {
    {-0.5,-0.5, 0.0},
    {-0.5, 0.5, 0.0},
    { 0.5, 0.5, 0.0},
    { 0.5,-0.5, 0.0}
};

int main(int argc, char **argv) {
    Buffer write_buffer = {
        .buffer = (char[16 + sizeof(positions) + sizeof(indices)]){0},
        .size = 16 + ARRAY_SIZE(positions) + ARRAY_SIZE(indices)
    };

    *((u32 *)write_buffer.buffer) = 1;
    *((u32 *)write_buffer.buffer + 1) = ARRAY_SIZE(positions);
    *((u32 *)write_buffer.buffer + 2) = ARRAY_SIZE(indices);
    *((u32 *)write_buffer.buffer + 3) = 0;
    
    u64 version = (u64)*((u32 *)write_buffer.buffer);
    String str = (String) {
        .string = (char[256]){0},
        .capacity = 256
    };

    stringPattern(&CONST_STRING("version: %u\n"), (const void *[]){&version}, &str);
    printConsole(&str);

    for(u32 i = 0; i < ARRAY_SIZE(positions); i++) {
        f32* position = (f32 *)((u8*)write_buffer.buffer + 16 + sizeof(f32) * 3 * i);
        position[0] = positions[i][0];
        position[1] = positions[i][1];
        position[2] = positions[i][2];
    }

    for(u32 i = 0; i < ARRAY_SIZE(indices); i++) {
        u16* index = (u16 *)((u8*)write_buffer.buffer + 16 + ARRAY_SIZE(positions) + sizeof(u16) * i);
        *index = indices[i];
    }

    if(!bufferToFile(&CONST_STRING(argv[1]), &write_buffer)) {
        printConsole(&CONST_STRING("failed to write buffer to file"));
    }

    if(!fileToBuffer(&CONST_STRING(argv[1]), &write_buffer)) {
        printConsole(&CONST_STRING("failed to read from file"));
    }
    
    version = (u64)*((u32 *)write_buffer.buffer);
    stringPattern(&CONST_STRING("read version: %u\n"), (const void *[]){&version}, &str);
    printConsole(&str);

    return 0;
}
