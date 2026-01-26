#include <base.h>

#define MODEL_VERSION (1)

typedef struct {
    f32 position[3];
} VertexV1;

typedef u16 IndexV1;

#define NODE_SIZE (1024 * 64)

typedef struct {
    VertexV1 *vertices;
    IndexV1 *indices;
    u32 vertex_count;
    u32 index_count;
} MeshData;

#define IS_SPACE(c) (c == ' ' || c == '\t' || c == '\n' || c == '\r')

b32 parseObj(const char *obj_begin, const char *obj_end, Arena *arena, MeshData *mesh_data) {
    const char* obj = obj_begin;
    while (obj != obj_end) {
        while (IS_SPACE(*obj) && obj != obj_end) {
            obj++;
            continue;
        }

        if(obj == obj_end) {
            break;
        }

        if(*obj == '#') {
            while (*obj++ != '\n' && obj != obj_end);
            continue;
        }

        if(*obj == 'v') {
            if(*++obj != ' ') {
                goto _end;
            }

            VertexV1 *vertex = &mesh_data->vertices[mesh_data->vertex_count++];
            String str = (String){0};
            String shit = STACK_STR(256);
            for(u32 i = 0; i < 3; i++) {
                while (*obj == ' ' || *obj == '\t' || *obj == '\r') {
                    obj++;
                }
                if(obj == obj_end) {
                    goto _end;
                }
                
                str = (String){.string = (char*)obj};
                while ((*obj >= 48 && *obj < 58) || (*obj == '.') || (*obj == '-')) {
                    str.size++;
                    obj++;
                }
                vertex->position[i] = (f32)stringToF64(&str);
                stringZero(&shit);
                stringAddF64(&shit, vertex->position[i]);
                stringAddChar(&shit, ' ');
                printConsole(&shit);
            }
            printConsole(&CONST_STRING("\n"));
            continue;
        }

        if(*obj == 'f') {
            if(*++obj != ' ') {
                goto _end;
            }

            IndexV1 *indices = &mesh_data->indices[mesh_data->index_count];
            mesh_data->index_count += 3;
            String str = (String){0};
            String shit = STACK_STR(256);
            for(u32 i = 0; i < 3; i++) {
                while (*obj == ' ' || *obj == '\t' || *obj == '\r') {
                    obj++;
                }
                
                if(obj == obj_end) {
                    continue;
                }
                    
                str = (String){.string = (char *)obj};
                while (*obj >= 48 && *obj < 58) {
                    str.size++;
                    obj++;
                }

                indices[i] = (u16)stringToU64(&str) - 1;
                stringAddU64(&shit, indices[i]);
                stringAddChar(&shit, ' ');

                while (!IS_SPACE(*obj)) {
                    obj++;
                }
            }
            stringAddChar(&shit, '\n');
            printConsole(&shit);
            
            continue;
        }

        _end: {};
        while(*obj != '\n') {
            obj++;
        }
    }
    
    return TRUE;
}

i32 main(i32 argc, char **argv) {
    if(argc != 3) {
        printConsole(&CONST_STRING("wrong argument count, should be: <path to application>(implicit) <srource obj> <destination .model>"));
        return -1;
    }
    
    Arena arena = (Arena){0};
    if(!createArena(&arena, 1024 * 1024 * 256, 1024 * 64)) {
        printConsole(&CONST_STRING("failed to create arena"));
        return -1;
    }

    /* ERRROR HERE */
    const String obj_file = CONST_STRING(argv[1]);
    const String model_file = CONST_STRING(argv[2]);
    MeshData mesh_data = (MeshData){0};

    u64 file_size = 0;
    Buffer read_buffer = (Buffer) {
        .buffer = allocateArena(&arena, 1024 * 64, 16),
        .size = 1024 * 64
    };

    /* READ FILE */ {

        if(!read_buffer.buffer) {
            printConsole(&CONST_STRING("failed to allocate read buffer"));
            return -1;
        }
        file_size = fileToBuffer(&obj_file, &read_buffer);
        if(file_size == 0) {
            printConsole(&CONST_STRING("failed to read obj file"));
            return -1;
        }
        if(file_size > read_buffer.size) {
            if(!allocateArena(&arena, file_size - read_buffer.size, 1)) {
                printConsole(&CONST_STRING("failed to reallocate read buffer"));
                return -1;
            }
            read_buffer.size = file_size;
            if(fileToBuffer(&obj_file, &read_buffer) != file_size) {
                printConsole(&CONST_STRING("failed to read file after buffer reallocation"));
                return -1;
            }
        }

        mesh_data = (MeshData) {
            .vertices = allocateArena(&arena, file_size, 16),
            .indices = allocateArena(&arena, file_size, 16)
        };
        if(!mesh_data.vertices) {
            printConsole(&CONST_STRING("failed to allocate vertices array"));
            return -1;
        }
        if(!mesh_data.indices) {
            printConsole(&CONST_STRING("failed to allocate indices array"));
            return -1;
        }
    }

    
    const char* read_end = (const char*)read_buffer.buffer + file_size - 1;
    while (*read_end != '\n' && read_end != read_buffer.buffer) {
        printConsole(&CONST_STRING("FUCK YOU\n"));
        read_end--;
    }
    

    if(!parseObj(read_buffer.buffer, (const char*)read_end, &arena, &mesh_data)) {
        printConsole(&CONST_STRING("failed to parse obj"));
        return -1;
    }

    Buffer write_buffer = {
        .buffer = allocateArena(&arena, 16 + sizeof(VertexV1) * mesh_data.vertex_count + sizeof(IndexV1) * mesh_data.index_count, 16),
        .size = 16 + sizeof(VertexV1) * mesh_data.vertex_count + sizeof(IndexV1) * mesh_data.index_count
    };
    if(!write_buffer.buffer) {
        printConsole(&CONST_STRING("failed to allocate write buffer"));
        return -1;
    }

    *((u32 *)write_buffer.buffer) = MODEL_VERSION;
    *((u32 *)write_buffer.buffer + 1) = mesh_data.vertex_count;
    *((u32 *)write_buffer.buffer + 2) = mesh_data.index_count;
    *((u32 *)write_buffer.buffer + 3) = 0;

    VertexV1 *vertices = (VertexV1 *)((u8 *)write_buffer.buffer + 16);
    IndexV1 *indices = (IndexV1 *)((u8 *)write_buffer.buffer + 16 + sizeof(VertexV1) * mesh_data.vertex_count);

    for(u32 i = 0; i < mesh_data.vertex_count; i++) {
        vertices[i] = mesh_data.vertices[i];   
    }
    for(u32 i = 0; i < mesh_data.index_count; i++) {
        indices[i] = mesh_data.indices[i];   
    }

    String shit = STACK_STR(256);

    if(!bufferToFile(&model_file, &write_buffer)) {
        printConsole(&CONST_STRING("failed to write model file"));
        return -1;
    }

    if(fileToBuffer(&model_file, &write_buffer) != write_buffer.size) {
        printConsole(&CONST_STRING("failed to read model file"));
        return -1;
    }

    stringAddU64(&shit, *(u32 *)write_buffer.buffer);
    printConsole(&shit);

    freeArena(&arena);
    return 0;
}
