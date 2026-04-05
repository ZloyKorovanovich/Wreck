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
    RENDER_RESOURCE_TYPE_NONE             = 0,
    RENDER_RESOURCE_TYPE_COLOR_ATTACHMENT = 1,
    RENDER_RESOURCE_TYPE_DEPTH_ATTACHMENT = 2,
    RENDER_RESOURCE_TYPE_GENRIC_IMAGE     = 3,

    RENDER_RESOURCE_TYPE_UNIFORM_BUFFER   = 4,
    RENDER_RESOURCE_TYPE_STORAGE_BUFFER   = 5
} RenderResourceType;

typedef enum {
    RENDER_RESOURCE_FLAGS_NONE          = 0x0,
    RENDER_RESOURCE_FLAG_RENDER_QUEUE   = 0x1,
    RENDER_RESOURCE_FLAG_COMPUTE_QUEUE  = 0x2,
    RENDER_RESOURCE_FLAG_TRANSFER_QUEUE = 0x4
} RenderResourceFlags;

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
    RenderResourceType  type;
    RenderFormat        format;
    RenderResourceFlags flags;
    u32                 x;
    u32                 y;
    u32                 z;
    u32                 mip_levels;
    u32                 sampled_binding;
    u32                 storage_binding;
} RenderImage;

typedef struct {
    RenderResourceType  type;
    RenderResourceFlags flags;
    u64                 size;
} RenderBuffer;

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
    const RenderImage*  images;
    const RenderBuffer* buffers;
    u32                 image_count;
    u32                 buffer_count;
} LayoutRenderBindingsIn;

CtxHandle openRenderWindow(const OpenRenderWindowIn* in, void* page_address);
b32 closeRenderWindow(CtxHandle ctx);
b32 loadShaderPrograms(CtxHandle ctx, const LoadShaderProgramsIn* in);
b32 layoutRenderBindings(CtxHandle ctx, const LayoutRenderBindingsIn* in);

#endif
