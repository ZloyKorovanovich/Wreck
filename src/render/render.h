#ifndef _RENDER_INCLUDED
#define _RENDER_INCLUDED

#include "../base.h"

#define SHADER_ENTRY_VERTEX   "mainVertex"
#define SHADER_ENTRY_FRAGMENT "mainFragment"
#define SHADER_ENTRY_COMPUTE  "compute"

/* defined format of mesh:
    2  bytes index
    80 bytes vertex        */
typedef u16 RenderMeshIndex;
typedef struct {
    f32 position[4];
    f32 normal  [4];
    f32 texcoord[4];
    f32 weights [4];
    u32 bones   [4];
} RenderMeshVertex;

typedef struct {
    RenderMeshVertex* vertices;
    RenderMeshIndex*  indices;
} RenderMesh;

typedef enum {
    SHADER_PROGRAM_FLAGS_NONE           = 0x0,
    SHADER_PROGRAM_FLAG_VERTEX_POSITION = 0x1,
    SHADER_PROGRAM_FLAG_VERTEX_NORMAL   = 0x2,
    SHADER_PROGRAM_FLAG_VERTEX_TEXCOORD = 0x4,
    SHADER_PROGRAM_FLAG_VERTEX_WEIGHTS  = 0x8,
    SHADER_PROGRAM_FLAG_VERTEX_BONES    = 0x10
} ShaderProgramFlags;

typedef enum {
    SHADER_PROGRAM_TYPE_NONE     = 0,
    SHADER_PROGRAM_TYPE_GRAPHICS = 1,
    SHADER_PROGRAM_TYPE_COMPUTE  = 2
} ShaderProgramType;

typedef struct {
    ShaderProgramType  type;
    ShaderProgramFlags flags;
    union {
        struct {
            void* vertex;
            void* fragment;
            u32   vertex_size;
            u32   fragment_size;
        };
        struct {
            void* compute;
            u32   compute_size;
        };
    };
} ShaderProgram;

typedef struct {
    const char* name;
    u32         window_x;
    u32         window_y;
} OpenWindowRenderIn;

typedef struct {
    const ShaderProgram* programs;
    u32                  program_count;
} LoadShaderProgramsIn;

CtxHandle openRenderWindow(const OpenWindowRenderIn* in, const AllocationCallbacks* allocator);
b32 closeRenderWindow(CtxHandle ctx, const AllocationCallbacks* allocator);
b32 loadShaderPrograms(CtxHandle ctx, const LoadShaderProgramsIn* in, const AllocationCallbacks* allocator);

#endif
