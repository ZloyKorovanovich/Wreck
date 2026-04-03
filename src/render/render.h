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

typedef enum {
    RENDER_FORMAT_NONE                = 0,
    RENDER_FORMAT_R32G32B32A32_SFLOAT = 1
} RenderFormat;

typedef enum {
    RENDER_BINDING_TYPE_NONE             = 0,
    RENDER_BINDING_TYPE_STORAGE_IMAGE    = 1,
    RENDER_BINDING_TYPE_COLOR_ATTACHMENT = 2,
    RENDER_BINDING_TYPE_DEPTH_ATTACHMENT = 3,
    RENDER_BINDING_TYPE_UNIFORM_BUFFER   = 4,
    RENDER_BINDING_TYPE_STORAGE_BUFFER   = 5
} RenderBindingType;

#define IMAGE_SURFACE_COLOR_ID (0xfffffffe)
#define IMAGE_SCREEN_COLOR_ID  (0xfffffffd)
#define IMAGE_SCREEN_DEPTH_ID  (0xfffffffc)

#define MAX_RENDER_ATTACHMENT_COUNT (8)


typedef struct {
    ShaderProgramType  type;
    ShaderProgramFlags flags;
    union {
        struct {
            void*      vertex;
            void*      fragment;
            u32        vertex_size;
            u32        fragment_size;
            const u32* color_attachment_ids;
            u32        depth_attachment_id;
            u32        color_attachment_count;
        };
        struct {
            void* compute;
            u32   compute_size;
        };
    };
} ShaderProgram;

typedef struct {
    RenderBindingType type;
    union {
        struct {
            RenderFormat image_format;
            u32          image_x;
            u32          image_y;
            u32          image_z;
            u32          image_sampled_id;
            u32          image_storage_id;
        };
        struct {
            u32 buffer_id;
            u64 buffer_size;
        };
    };
} RenderBinding;

typedef struct {
    const char* name;
    u32         window_x;
    u32         window_y;
} OpenRenderWindowIn;

typedef struct {
    const ShaderProgram* programs;
    u32                  program_count;
} LoadShaderProgramsIn;

typedef struct {
    const RenderBinding* bindings;
    u32                  binding_count;
} LayoutRenderBindingsIn;

CtxHandle openRenderWindow(const OpenRenderWindowIn* in, void* page_address);
b32 closeRenderWindow(CtxHandle ctx);
b32 loadShaderPrograms(CtxHandle ctx, const LoadShaderProgramsIn* in);
b32 layoutRenderBindings(CtxHandle ctx, const LayoutRenderBindingsIn* in);

#endif
