#include <base.h>

#define MODEL_VERSION (2)

typedef struct {
    f32 position[3];
    f32 normal[3];
    f32 texcoord[2];
} VertexV2;

typedef u16 IndexV2;

#define NODE_SIZE (1024 * 64)

typedef struct {
    VertexV2 *vertices;
    IndexV2 *indices;
    u32 vertex_count;
    u32 index_count;
} MeshData;

typedef union {
    struct {
        f32 components[3];
    };
    struct {
        f32 x;
        f32 y;
        f32 z;
    };
} Vec3;

typedef union {
    struct {
        f32 components[2];
    };
    struct {
        f32 x;
        f32 y;
    };
} Vec2;

typedef struct {
    u16 position;
    u16 normal;
    u16 texcoord;
} Index;

typedef struct {
    Vec3 *positions;
    Vec3 *normals;
    Vec2 *texcoords;
    Index *indices;

    u32 position_count;
    u32 normal_count;
    u32 texcoord_count;
    u32 index_count;
} RawMeshData;

#define IS_SPACE(c) (c == ' ' || c == '\t' || c == '\n' || c == '\r')

b32 parseObj(
    const char *obj_begin, const char *obj_end, 
    Arena *position_arena, Arena *normal_arena, Arena *texcoord_arena, Arena *index_arena, 
    RawMeshData *raw_mesh
) {
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

        if(obj[0] == 'v' && obj[1] == 'n') {
            obj++;
            if(*++obj != ' ') {
                goto _end;
            }

            if(raw_mesh->normal_count * sizeof(Vec3) > normal_arena->physical_size) {
                if(!allocateArena(normal_arena, 1024 * sizeof(Vec3), 4)) {
                    printConsole(&CONST_STRING("failed to reallocate normals array\n"));
                    return FALSE;
                }
            }

            Vec3 *normal = &raw_mesh->normals[raw_mesh->normal_count++];
            String str = (String){0};
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
                normal->components[i] = (f32)stringToF64(&str);
            }
            continue;
        }

        if(obj[0] == 'v' && obj[1] == 't') {
            obj++;
            if(*++obj != ' ') {
                goto _end;
            }

            if(raw_mesh->texcoord_count * sizeof(Vec2) > texcoord_arena->physical_size) {
                if(!allocateArena(texcoord_arena, 1024 * sizeof(Vec2), 4)) {
                    printConsole(&CONST_STRING("failed to reallocate texcoords array\n"));
                    return FALSE;
                }
            }

            Vec2 *texcoord = &raw_mesh->texcoords[raw_mesh->texcoord_count++];
            String str = (String){0};
            for(u32 i = 0; i < 2; i++) {
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
                texcoord->components[i] = (f32)stringToF64(&str);
            }
            continue;
        }

        if(obj[0] == 'v' && obj[1] == ' ') {
            obj++;

            if(raw_mesh->position_count * sizeof(Vec3) > position_arena->physical_size) {
                if(!allocateArena(position_arena, 1024 * sizeof(Vec3), 4)) {
                    printConsole(&CONST_STRING("failed to reallocate positions array\n"));
                    return FALSE;
                }
            }

            Vec3 *position = &raw_mesh->positions[raw_mesh->position_count++];
            String str = (String){0};
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
                position->components[i] = (f32)stringToF64(&str);
            }
            continue;
        }


        if(obj[0] == 'f' && obj[1] == ' ') {
            obj++;

            if(raw_mesh->index_count * sizeof(Index) > index_arena->physical_size) {
                if(!allocateArena(index_arena, 1024 * sizeof(Index) * 3, 2)) {
                    printConsole(&CONST_STRING("failed to reallocate index array\n"));
                    return FALSE;
                }
            }

            Index *indices = &raw_mesh->indices[raw_mesh->index_count];
            raw_mesh->index_count += 3;
            String str = (String){0};
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

                indices[i].position = (u16)stringToU64(&str) - 1;

                if(*obj++ == '/') {
                    str = (String){.string = (char *)obj};
                    while (*obj >= 48 && *obj < 58) {
                        str.size++;
                        obj++;
                    }

                    indices[i].texcoord = (u16)stringToU64(&str) - 1;
                }
                if(*obj++ == '/') {
                    str = (String){.string = (char *)obj};
                    while (*obj >= 48 && *obj < 58) {
                        str.size++;
                        obj++;
                    }

                    indices[i].normal = (u16)stringToU64(&str) - 1;
                }

                while (!IS_SPACE(*obj)) {
                    obj++;
                }
            }
            
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
    if(!createArena(&arena, (1024 * 1024 * 512), 1024 * 64)) {
        printConsole(&CONST_STRING("failed to create arena"));
        return -1;
    }

    /* ERRROR HERE */
    const String obj_file = CONST_STRING(argv[1]);
    const String model_file = CONST_STRING(argv[2]);

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
    }

    Arena normal_arena = (Arena){0};
    Arena position_arena = (Arena){0};
    Arena texcoord_arena = (Arena){0};
    Arena index_arena = (Arena){0};

    if(
        !createArena(&normal_arena, ALIGN(file_size, 1024 * 64), 1024 * 64)   ||
        !createArena(&position_arena, ALIGN(file_size, 1024 * 64), 1024 * 64) ||
        !createArena(&texcoord_arena, ALIGN(file_size, 1024 * 64), 1024 * 64) ||
        !createArena(&index_arena, ALIGN(file_size, 1024 * 64), 1024 * 64)
    ) {
        printConsole(&CONST_STRING("failed to allocate ra. mesh arenas\n"));
        return -1;
    }

    const char* read_end = (const char*)read_buffer.buffer + file_size - 1;
    while (*read_end != '\n' && read_end != read_buffer.buffer) {
        printConsole(&CONST_STRING("failed to find good file end\n"));
        read_end--;
    }

    RawMeshData raw_mesh = (RawMeshData){
        .positions = allocateArena(&position_arena, 1024 * 64, 4),
        .normals = allocateArena(&normal_arena, 1024 * 64, 4),
        .texcoords = allocateArena(&texcoord_arena, 1024 * 64, 4),
        .indices = allocateArena(&index_arena, 1024 * 64 * 3, 2),
    };

    if(!raw_mesh.positions || !raw_mesh.normals || !raw_mesh.texcoords || !raw_mesh.indices) {
        printConsole(&CONST_STRING("failed to allcoate raw mesh arrays\n"));
        return -1;
    }

    if(!parseObj(read_buffer.buffer, (const char*)read_end, &position_arena, &normal_arena, &texcoord_arena, &index_arena, &raw_mesh)) {
        printConsole(&CONST_STRING("failed to parse obj"));
        return -1;
    }

    clearArena(&arena);

    u32 vertex_count = MAX(raw_mesh.normal_count, MAX(raw_mesh.position_count, raw_mesh.texcoord_count));
    Buffer write_buffer = {
        .buffer = allocateArena(&arena, 16 + sizeof(VertexV2) * vertex_count + sizeof(IndexV2) * raw_mesh.index_count, 16),
        .size = 16 + sizeof(VertexV2) * vertex_count + sizeof(IndexV2) * raw_mesh.index_count
    };
    if(!write_buffer.buffer) {
        printConsole(&CONST_STRING("failed to allocate write buffer"));
        return -1;
    }

    *((u32 *)write_buffer.buffer) = MODEL_VERSION;
    *((u32 *)write_buffer.buffer + 1) = vertex_count;
    *((u32 *)write_buffer.buffer + 2) = raw_mesh.index_count;
    *((u32 *)write_buffer.buffer + 3) = 0;

    VertexV2 *vertices = (VertexV2 *)((u8 *)write_buffer.buffer + 16);
    IndexV2 *indices = (IndexV2 *)((u8 *)write_buffer.buffer + 16 + sizeof(VertexV2) * vertex_count);

    u16 vertex_id = 0;
    for(u32 i = 0; i < raw_mesh.index_count; i++) {
        for(u32 j = 0; j < i; j++) {
            if(
                raw_mesh.indices[i].position == raw_mesh.indices[j].position &&
                raw_mesh.indices[i].normal == raw_mesh.indices[j].normal &&
                raw_mesh.indices[i].texcoord == raw_mesh.indices[j].texcoord
            ) {
                indices[i] = indices[j];
                goto _next_index;
            }
        }
        indices[i] = vertex_id;
        vertices[vertex_id++] = (VertexV2) {
            .position = {
                raw_mesh.positions[raw_mesh.indices[i].position].x,
                raw_mesh.positions[raw_mesh.indices[i].position].y,
                raw_mesh.positions[raw_mesh.indices[i].position].z
            },
            .normal = {
                raw_mesh.normals[raw_mesh.indices[i].normal].x,
                raw_mesh.normals[raw_mesh.indices[i].normal].y,
                raw_mesh.normals[raw_mesh.indices[i].normal].z
            },
            .texcoord = {
                raw_mesh.texcoords[raw_mesh.indices[i].texcoord].x,
                raw_mesh.texcoords[raw_mesh.indices[i].texcoord].y
            }
        };

        _next_index: {};
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