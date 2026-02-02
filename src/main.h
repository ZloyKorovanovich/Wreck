#ifndef MAIN_INCLUDED
#define MAIN_INCLUDED

#include <base.h>

/*======================================================================
    MSG
  ======================================================================*/

typedef enum {
    MSG_CODE_SUCCESS = 0,
    MSG_CODE_ERROR = -1,
    MSG_CODE_WARNING = 1
} MSG_TYPE;

typedef void (*MsgCallback_pfn)(i32 type, const String *msg);

#define MSG_LOG(callback, msg) if(callback) {callback(MSG_CODE_SUCCESS, msg);}
#define MSG_ERROR(callback, msg) if(callback) {callback(MSG_CODE_ERROR, msg);}
#define MSG_WARNING(callback, msg) if(callback) {callback(MSG_CODE_WARNING, msg);}

/* These stringfy look ugly, but thats the only way around :( */
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
#define LOCATION_TRACE " *** " __FILE__ ":" TOSTRING(__LINE__)

#define TRACED_STR(str) CONST_STRING(str LOCATION_TRACE)

/*======================================================================
    VULKAN
  ======================================================================*/

typedef struct VulkanContext VulkanContext;

typedef enum {
    VULKAN_FLAG_RESIZABLE = 0x1,
    VULKAN_FLAG_DEBUG = 0x2
} VulkanFlags;

typedef struct {
    String name;
    VulkanFlags flags;
    u32 resolution_x;
    u32 resolution_y;
    MsgCallback_pfn msg_callback;
} VulkanContextInfo;

VulkanContext *createVulkanContext(Allocate_pfn context_allocate, const VulkanContextInfo *info);
void destroyVulkanContext(VulkanContext *context);

/*======================================================================
    RENDER
  ======================================================================*/

typedef struct RenderContext RenderContext;
typedef struct RenderCmd RenderCmd;

typedef struct {
    u32 res_x;
    u32 res_y;
} UpdateInfo;

typedef void (*RenderUpdate_pfn) (UpdateInfo *info, RenderCmd *render_cmd);

typedef enum {
    SHADER_PROGRAM_FLAGS_NONE = 0,
    SHADER_PRORGAM_FLAG_USE_VERTEX_POSITION = 0x1,
    SHADER_PRORGAM_FLAG_USE_VERTEX_NORMAL = 0x2,
    SHADER_PRORGAM_FLAG_USE_VERTEX_UV = 0x4,
    SHADER_PROGRAM_FLAG_LINE_MODE = 0x8,
    SHADER_PROGRAM_FLAG_POINT_MODE = 0x10,
    SHADER_PROGRAM_FLAG_EMPTY_LAYOUT = 0x20
} ShaderProgramFlags;

typedef struct {
    ShaderProgramFlags flags;
    const String vertex_shader; /* set if graphics */
    const String fragment_shader; /* set if graphics */
    const String compute_shader; /* set if compute */
} ShaderProgramInfo;

typedef struct {
    const String file;
} MeshInfo;

/* global uniform buffer that is used during update */
typedef struct {
    void *data;
    u64 size;
} UniformBufferInfo;

typedef struct {
    void *data;
    u64 size;
} StorageBufferInfo;

typedef struct {
    VulkanContext *vulkan_context;
    /* resources */
    const UniformBufferInfo *uniform_buffer; /* used for common data like projection matrix, time values, screen resolution, etc. */

    const ShaderProgramInfo *programs; /* program ids will be preserved for access */
    const MeshInfo *meshes;
    const StorageBufferInfo *storage_buffers;
    u32 program_count;
    u32 mesh_count; 
    u32 storage_buffer_count; /* total storage buffer count */
    u32 storage_host_mutable_buffer_count; /* should be not greater than storage_buffer_count*/

    u64 push_constants_size;
    /* callbacks */
    MsgCallback_pfn msg_callback;
} RenderContextInfo;

RenderContext* createRenderContext(Allocate_pfn context_allocate, const RenderContextInfo *info);
void destroyRenderContext(RenderContext *context);
b32 runRenderLoop(RenderContext *render_context, RenderUpdate_pfn update_callback);

#define RENDER_ATTACHMENT_SCREEN_COLOR_ID (U32_MAX - 1)
#define RENDER_ATTACHMENT_SCREEN_DEPTH_ID (U32_MAX - 2)

void *cmdWriteHostUniformBuffer(RenderCmd *cmd);
void cmdTransferUniformBuffer(RenderCmd *cmd, u64 size);
void cmdPushContsants(RenderCmd *cmd, const void *constants, u64 size);

void cmdBeginRendering(RenderCmd *cmd, u32 color_count, u32 *color_ids, u32 depth_id);
void cmdEndRendering(RenderCmd *cmd);
void cmdDrawProcedural(RenderCmd *cmd, u32 program_id, u32 vertex_count, u32 instance_count);
void cmdDrawMesh(RenderCmd *cmd, u32 program_id, u32 mesh_id, u32 instance_count);

#endif
