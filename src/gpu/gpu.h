#ifndef _GPU_INCLUDED
#define _GPU_INCLUDED

#include "../base.h"

#define VRAM_SIZE_DEVICE_BUFFERS (256 * 1024 * 1024)
#define VRAM_SIZE_DEVICE_IMAGES  (512 * 1024 * 1024)
#define VRAM_SIZE_HOST_TRANSFER  (512 * 1024 * 1024)

#define GPU_SYNC_TRANSFER_SIZE   (         4 * 1024)

#define VRAM_SIZE_DEVICE_VULKAN  (VRAM_SIZE_DEVICE_BUFFERS + VRAM_SIZE_DEVICE_IMAGES)
#define VRAM_SIZE_HOST_VULKAN    (VRAM_SIZE_HOST_TRANSFER                           )

#define GPU_SHADER_ENTRY_VERTEX   "main_vertex"
#define GPU_SHADER_ENTRY_FRAGMENT "main_fragment"
#define GPU_SHADER_ENTRY_COMPUTE  "main_compute"

#define GPU_MAX_STATIC_BUFFERS          (32)
#define GPU_MAX_STATIC_IMAGES           (32)
#define GPU_MAX_COLOR_ATTACHMENTS       (8)
#define GPU_MAX_BINDINGS_PER_DESCRIPTOR (16)
#define GPU_PUSH_CONSTANTS_SIZE         (64)

#define GPU_SAMPLER_LINEAR_REPEAT_ID    (0)
#define GPU_SAMPLER_LINEAR_CLAMP_ID     (1)
#define GPU_SAMPLER_NEAREST_REPEAT_ID   (2)
#define GPU_SAMPLER_NEAREST_CLAMP_ID    (3)

#define GPU_IMAGE_SURFACE_ID            (0xFFFFFFFE)

typedef u32 GpuFormat;
typedef u32 GpuImageFlags;
typedef u32 GpuBufferFlags;
typedef u32 GpuPipelineType;
typedef u32 GpuDescriptorType;

enum GpuFormat {
    GPU_FORMAT_NONE                = 0,
    GPU_FORMAT_R32G32B32A32_SFLOAT = 1,
    GPU_FORMAT_R32G32_SFLOAT       = 2,
    GPU_FORMAT_R32_SFLOAT          = 3,
    GPU_FORMAT_D32_SFLOAT          = 4,
    GPU_FORMAT_R16G16B16A16_SFLOAT = 5,
    GPU_FORMAT_R16G16_SFLOAT       = 6,
    GPU_FORMAT_R16_SFLOAT          = 7,
    GPU_FORMAT_R8G8B8_UNORM        = 8,
    GPU_FORMAT_R8G8_UNORM          = 9,
    GPU_FORMAT_R8_UNORM            = 10,
    GPU_FORMAT_COUNT               = 11,
    GPU_FORMAT_SURFACE             = 0xFFFFFFFE
};

enum GpuImageFlags {
    GPU_IMAGE_FLAGS_NONE            = 0x0,
    GPU_IMAGE_FLAG_COLOR_ATTACHMENT = 0x1,
    GPU_IMAGE_FLAG_DEPTH_ATTACHMENT = 0x2,
    GPU_IMAGE_FLAG_SAMPLED          = 0x4,
    GPU_IMAGE_FLAG_STORAGE          = 0x8,
    GPU_IMAGE_FLAGS_MASK            = 0xF
};

enum GpuBufferFlags {
    GPU_BUFFER_FLAGS_NONE            = 0x0,
    GPU_BUFFER_FLAG_UNIFORM_BUFFER   = 0x1,
    GPU_BUFFER_FLAG_STORAGE_BUFFER   = 0x2,
    GPU_BUFFER_FLAGS_MASK            = 0x3
};

enum GpuPipelineType {
    GPU_PIPELINE_TYPE_NONE     = 0,
    GPU_PIPELINE_TYPE_GRAPHICS = 1,
    GPU_PIPELINE_TYPE_COMPUTE  = 2
};

enum GpuDescriptorType {
    GPU_DESCRIPTOR_TYPE_NONE           = 0,
    GPU_DESCRIPTOR_TYPE_UNIFORM_BUFFER = 1,
    GPU_DESCRIPTOR_TYPE_STORAGE_BUFFER = 2,
    GPU_DESCRIPTOR_TYPE_SAMPLED_IMAGE  = 3,
    GPU_DESCRIPTOR_TYPE_STORAGE_IMAGE  = 4,
    GPU_DESCRIPTOR_TYPE_SAMPLER        = 5,
    GPU_DESCRIPTOR_TYPE_COUNT          = 6
};

enum GpuDescriptorSets {
    GPU_DESCRIPTOR_SET_0     = 0,
    GPU_DESCRIPTOR_SET_1     = 1,
    GPU_DESCRIPTOR_SET_2     = 2,
    GPU_DESCRIPTOR_SET_3     = 3,
    GPU_DESCRIPTOR_SET_COUNT = 4
};

typedef struct {
    GpuImageFlags flags;
    GpuFormat     format;
    u32           size_x;
    u32           size_y;
} ImageInfo;

typedef struct {
    GpuBufferFlags flags;
    u64            size;
} BufferInfo;

typedef struct {
    GpuPipelineType  type;
    const void*      vertex;
    const void*      fragment;
    const void*      compute;
    u64              vertex_size;
    u64              fragment_size;
    u64              compute_size;
    const GpuFormat* color_formats;
    GpuFormat        depth_format;
    u32              color_formats_count;
} PipelineInfo;

typedef struct {
    const GpuDescriptorType* bindings;
    u32                      bindings_count;
} DescriptorSetInfo;

typedef struct {
    const char* window_name;
    u32         frame_buffer_x;
    u32         frame_buffer_y;
    u16         pci_vendor_id;
    u16         pci_device_id;
    b32         vulkan_debug_enabled;
} GpuInfo;

typedef struct {
    const ImageInfo*  image_infos;
    const BufferInfo* buffer_infos;
    u32               image_infos_count;
    u32               buffer_infos_count;
} ResourcesInfo;

typedef struct {
    DescriptorSetInfo   descriptor_set_infos[GPU_DESCRIPTOR_SET_COUNT];
    const PipelineInfo* pipeline_infos;
    u32                 pipeline_infos_count;
} ShadersInfo;

/* depth will be cleared to max depth value */
typedef struct {
    b32        do_not_clear;
    u32        offset_x;
    u32        offset_y;
    u32        size_x;
    u32        size_y;
    f32        min_depth;
    f32        max_depth;
    const u32* attachments_color; 
    const u32* images_read;
    const u32* buffers_read;
    u32        attachment_depth;
    u32        attachments_color_count;
    u32        images_read_count;
    u32        buffers_read_count;
} DrawingInfo;

typedef struct {
    u32 set_id;
    u32 binding_id;
    u32 resource_id;
} BindingInfo;

typedef struct {
    const u32* images_read_write;
    const u32* buffers_read_write;
    const u32* images_read_only;
    const u32* buffers_read_only;
    u32        images_read_write_count;
    u32        buffers_read_write_count;
    u32        images_read_only_count;
    u32        buffers_read_only_count;
} ComputeInfo;

CtxHandle gpu_start(const GpuInfo* gpu_info);
void      gpu_stop(CtxHandle ctx);

b32  gpu_allocate_resources(CtxHandle ctx, const ResourcesInfo* resources_info);
void gpu_release_resources(CtxHandle ctx);

b32  gpu_compile_shaders(CtxHandle ctx, const ShadersInfo* shaders_info);
void gpu_release_shaders(CtxHandle ctx);
b32  gpu_write_bindings(CtxHandle ctx, const BindingInfo* binding_infos, u32 binding_infos_count);

b32  gpu_render_init(CtxHandle ctx);
void gpu_render_terminate(CtxHandle ctx);
/* 0 = success; 1 = fail; 2 = window_terminated */
i32  gpu_render_frame_begin(CtxHandle ctx, u32* screen_x, u32* screen_y);
i32  gpu_render_frame_end(CtxHandle ctx);
/* drawing */
void gpu_render_begin_drawing(CtxHandle ctx, const DrawingInfo* drawing_info);
void gpu_render_end_drawing(CtxHandle ctx);
void gpu_render_bind_graphics_pipeline(CtxHandle ctx, u32 pipeline_id);
void gpu_render_push_constants(CtxHandle ctx, const void* constants, u64 size);
void gpu_render_draw(CtxHandle ctx, i32 instance_count, i32 vertex_count);
/* sync transfer */
void gpu_render_write_buffer(CtxHandle ctx, u32 buffer_id, const void* data, u64 offset, u64 size);
/* compute */
void gpu_render_compute_barrier(CtxHandle ctx, const ComputeInfo* compute_info);
void gpu_render_bind_compute_pipeline(CtxHandle ctx, u32 pipeline_id);
void gpu_render_dispatch(CtxHandle ctx, u32 groups_x, u32 groups_y, u32 groups_z);

#endif
