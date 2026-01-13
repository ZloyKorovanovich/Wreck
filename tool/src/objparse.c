#define _CRT_SECURE_NO_DEPRECATE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef int i32;
typedef unsigned u32;
typedef unsigned long long u64;
typedef float f32;
typedef unsigned short u16;

typedef enum {
    OBJ_CODE_SUCCESS = 0,
    OBJ_CODE_ERROR_FAILED_TO_OPEN_FILE = -1,
    OBJ_CODE_ERROR_FAILED_TO_PARSE_LINE = -2,
    OBJ_CODE_ERROR_FAILED_TO_MALLOC = -3,
    OBJ_CODE_ERROR_UNEXPECTED_SPACE = -4,
    OBJ_CODE_ERROR_SYMBOL_PATTERN_NO_MATCH = -5,
    OBJ_CODE_ERROR_WRONG_ARGUMENTS = -7,
    OBJ_CODE_ERROR_FAILED_TO_READ_FILE_TO_BUFFER = -8,
    OBJ_CODE_ERROR_BAD_ARRAY_COUNTS = -9,
    OBJ_CODE_ERROR_FAILED_TO_WRITE_FILE = -10
} OBJ_CODES;

typedef struct {
    f32 x;
    f32 y;
    f32 z;
} Vec3;

typedef struct {
    f32 x;
    f32 y;
} Vec2;

typedef struct {
    u32 x;
    u32 y;
    u32 z;
} Uvec3;

typedef struct {
    f32 position[3];
    f32 normal[3];
    f32 uv[2];
} Vertex;

typedef struct {
    Vec3* positions;
    Vec3* normals;
    Uvec3* indices;

    u32 position_count;
    u32 position_capacity;
    u32 normal_count;
    u32 normal_capacity;
    u32 index_count;
    u32 index_capacity;
} RawMesh;

#define ARRAY_SIZE(a) (sizeof(a) /sizeof(a[0]))
#define MESH_ARRAY_GROWS (4096)

i32 parseLine(const char* begin, const char* end, RawMesh* mesh) {
    typedef enum {
        SYMBOL_TYPE_NONE = 0,
        SYMBOL_TYPE_VERTEX = 1,
        SYMBOL_TYPE_NORMAL = 2,
        SYMBOL_TYPE_INDEX = 3
    } SymbolType;
    typedef struct {
        const char* search;
        SymbolType type;
        u32 state;
    } SearchTarget;

    SearchTarget targets[] = {
        {.search = "v", .type = SYMBOL_TYPE_VERTEX},
        {.search = "vn", .type = SYMBOL_TYPE_NORMAL},
        {.search = "f", .type = SYMBOL_TYPE_INDEX}
    };

    if(*begin == '#') {
        return OBJ_CODE_SUCCESS;
    }
    while (*begin == ' ' || *begin == '\t' || *begin == '\r') {
        begin++;
    }
    
    u32 target_id = 0xffffffff;
    while (begin != end) {
        u32 possibilities = 0;
        for(u32 i = 0; i < ARRAY_SIZE(targets); i++) {
            if(targets[i].state == 0xffffffff) continue;
            /* check if symbol ended */
            if(*begin == ' ' || *begin == '\t' || *begin == '\r') {
                if(targets[i].search[targets[i].state] == 0) {
                    target_id = i;
                    break;
                }
                continue;
            }
            /* match non 0 characters */
            if(*begin == targets[i].search[targets[i].state]) {
                if(targets[i].search[targets[i].state] == 0) {
                    continue;
                }
                targets[i].state++;
                possibilities++;
            } else {
                targets[i].state = 0xffffffff;
            }
        }
        if(possibilities == 0) break;
        begin++;
    }
    if(target_id == 0xffffffff) {
        return OBJ_CODE_SUCCESS;
    }
    /* if parse vertex */
    if(targets[target_id].type == SYMBOL_TYPE_VERTEX) {
        f32 x = (f32)strtof(begin, (char**)&begin);
        f32 y = (f32)strtof(begin, (char**)&begin);
        f32 z = (f32)strtof(begin, (char**)&begin);
        if(mesh->position_count == mesh->position_capacity) {
            u32 new_capacity = mesh->position_capacity + MESH_ARRAY_GROWS;
            if(!(mesh->positions = realloc(mesh->positions, sizeof(Vec3) * (u64)new_capacity))) {
                return OBJ_CODE_ERROR_FAILED_TO_MALLOC;
            }
            mesh->position_capacity = new_capacity;
        }
        mesh->positions[mesh->position_count++] = (Vec3) {x, y, z};
    }
    /* if parse normal */
    if(targets[target_id].type == SYMBOL_TYPE_NORMAL) {
        f32 x = (f32)strtof(begin, (char**)&begin);
        f32 y = (f32)strtof(begin, (char**)&begin);
        f32 z = (f32)strtof(begin, (char**)&begin);
        if(mesh->normal_count == mesh->normal_capacity) {
            u32 new_capacity = mesh->normal_capacity + MESH_ARRAY_GROWS;
            if(!(mesh->normals = realloc(mesh->normals, sizeof(Vec3) * (u64)new_capacity))) {
                return OBJ_CODE_ERROR_FAILED_TO_MALLOC;
            }
            mesh->normal_capacity = new_capacity;
        }
        mesh->normals[mesh->normal_count++] = (Vec3) {x, y, z};
    }
    /* if parse index */
    if(targets[target_id].type == SYMBOL_TYPE_INDEX) {
        for(u32 i = 0; i < 3; i++) {
            u32 v = (u32)strtoul(begin, (char**)&begin, 10) - 1;
            u32 n = 0xffffffff;
            if(*begin == '/' && *(begin + 1) != '/') {
                n = (u32)strtoul(begin + 1, (char**)&begin, 10) - 1;
            }
            u32 t = 0xffffffff;
            if(*begin == '/' && *(begin + 1) != ' ') {
                t = (u32)strtoul(begin + 1, (char**)&begin, 10) - 1;
            }

            if(mesh->index_count == mesh->index_capacity) {
                u32 new_capacity = mesh->index_capacity + MESH_ARRAY_GROWS;
                if(!(mesh->indices = realloc(mesh->indices, sizeof(Uvec3) * (u64)new_capacity))) {
                    return OBJ_CODE_ERROR_FAILED_TO_MALLOC;
                }
                mesh->index_capacity = new_capacity;
            }
        
            mesh->indices[mesh->index_count++] = (Uvec3) {v, n, t};
            if(i == 3) break;
            /* skip space between indices */
            while (*begin == ' ' || *begin == '\t' || *begin == '\r') {
                begin++;
            }
        }
    }
    
    return OBJ_CODE_SUCCESS;
}

i32 objToRaw(const char* str, RawMesh* raw_mesh) {
    const char* line_begin = str;
    const char* line_end = NULL;
    while (*str) {
        if(*str == '\n') {
            line_end = str;
            if(parseLine(line_begin, line_end, raw_mesh) != 0) {
                return OBJ_CODE_ERROR_FAILED_TO_PARSE_LINE;
            }
            line_begin = line_end + 1;
            line_end = NULL;
        }
        str++;
    }
    return 0;
}


#define VERSION (0x1)

i32 main(i32 argc, char** argv) {
    if(argc != 3) {
        printf("arg count should be 2 (3 counting first call ./exe)\n");
        return OBJ_CODE_ERROR_WRONG_ARGUMENTS;
    }
    
    FILE* obj_file = fopen(argv[1], "r");
    if(!obj_file) {
        printf("failed to open obj file\n");
        return OBJ_CODE_ERROR_FAILED_TO_OPEN_FILE;
    }

    fseek(obj_file, 0, SEEK_END);
    u64 obj_size = ftell(obj_file);
    fseek(obj_file, 0, SEEK_SET);
    char* read_buffer = malloc(obj_size + 1);
    if(!read_buffer) {
        printf("failed to allocate read buffer\n");
        return OBJ_CODE_ERROR_FAILED_TO_MALLOC;
    }

    if(fread(read_buffer, 1, obj_size, obj_file) != obj_size) {
        printf("failed to read file to buffer");
        return OBJ_CODE_ERROR_FAILED_TO_READ_FILE_TO_BUFFER;
    }
    read_buffer[obj_size] = 0; /* zero terminate */
    fclose(obj_file);

    RawMesh raw_mesh = (RawMesh) {
        .normals = malloc(sizeof(Vec3) * MESH_ARRAY_GROWS),
        .normal_capacity = MESH_ARRAY_GROWS,
        .positions = malloc(sizeof(Vec3) * MESH_ARRAY_GROWS),
        .position_capacity = MESH_ARRAY_GROWS,
        .indices = malloc(sizeof(Uvec3) * MESH_ARRAY_GROWS),
        .index_capacity = MESH_ARRAY_GROWS
    };
    if(!raw_mesh.normals || !raw_mesh.positions) {
        printf("failed to allocate mesh buffer\n");
        return OBJ_CODE_ERROR_FAILED_TO_MALLOC;
    }

    i32 parse_result = objToRaw(read_buffer, &raw_mesh);
    free(read_buffer);
    if(parse_result != OBJ_CODE_SUCCESS) {
        printf("failed to parse obj");
        return parse_result;
    }

    const u32 vertex_count = raw_mesh.index_count;
    Vertex* vertex_array = malloc(sizeof(Vertex) * raw_mesh.index_count);
    if(!vertex_array) {
        printf("failed to allocate vertex array\n");
        return OBJ_CODE_ERROR_FAILED_TO_MALLOC;
    }
    for(u32 i = 0; i < vertex_count; i++) {
        const Vec3* position = &raw_mesh.positions[raw_mesh.indices[i].x];
        const Vec3* normal = &raw_mesh.normals[raw_mesh.indices[i].z];

        vertex_array[i] = (Vertex) {
            .position = {position->x, position->y, position->z},
            .normal = {normal->x, normal->y, normal->z}
        };
    }

    FILE* bin_file = fopen(argv[2], "wb");
    const u32 head[] = {VERSION, vertex_count}; /* head of file */
    if(fwrite(head, sizeof(head[0]), ARRAY_SIZE(head), bin_file) != ARRAY_SIZE(head)) {
        printf("failed to write head to file\n");
        return OBJ_CODE_ERROR_FAILED_TO_WRITE_FILE;
    }
    if(fwrite(vertex_array, sizeof(Vertex), raw_mesh.index_count, bin_file) != raw_mesh.index_count) {
        printf("failed to write mesh to file\n");
        return OBJ_CODE_ERROR_FAILED_TO_WRITE_FILE;
    }
    fclose(bin_file);
    free(vertex_array);

    printf("successfully generated binary file\n");

    return OBJ_CODE_SUCCESS;
}
