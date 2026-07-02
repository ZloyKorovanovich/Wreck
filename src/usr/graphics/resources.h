#ifndef _GRAPHICS_RESOURCES_INCLUDED
#define _GRAPHICS_RESOURCES_INCLUDED

#include "../../gpu/gpu.h"

#define FRAME_BUFFER_SIZE_X (2560)
#define FRAME_BUFFER_SIZE_Y (1440)

enum Samplers {
    SAMPLER_LINEAR_REPEAT  = GPU_SAMPLER_LINEAR_REPEAT_ID,
    SAMPLER_LINEAR_CLAMP   = GPU_SAMPLER_LINEAR_CLAMP_ID,
    SAMPLER_NEAREST_REPEAT = GPU_SAMPLER_NEAREST_REPEAT_ID,
    SAMPLER_NEAREST_CLAMP  = GPU_SAMPLER_NEAREST_CLAMP_ID
};

enum Images {
    IMAGE_SCREEN_COLOR,
    IMAGE_SCREEN_DEPTH,
    IMAGE_COPY_COLOR,
    IMAGE_COPY_DEPTH,
    IMAGE_COUNT,
    IMAGE_SURFACE = GPU_IMAGE_SURFACE_ID
};

enum Buffers {
    BUFFER_GLOBAL,
    BUFFER_COUNT
};

const GpuDescriptorType set_0_bindings[] = {
    GPU_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    GPU_DESCRIPTOR_TYPE_SAMPLER,
    GPU_DESCRIPTOR_TYPE_SAMPLER,
    GPU_DESCRIPTOR_TYPE_SAMPLER,
    GPU_DESCRIPTOR_TYPE_SAMPLER,
    GPU_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
    GPU_DESCRIPTOR_TYPE_STORAGE_IMAGE,
    GPU_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
    GPU_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
    GPU_DESCRIPTOR_TYPE_SAMPLED_IMAGE
};

const BindingInfo binding_writes[] = {
    {0, 0, BUFFER_GLOBAL         },
    {0, 1, SAMPLER_LINEAR_REPEAT },
    {0, 2, SAMPLER_LINEAR_CLAMP  },
    {0, 3, SAMPLER_NEAREST_REPEAT},
    {0, 4, SAMPLER_NEAREST_CLAMP },
    {0, 5, IMAGE_SCREEN_COLOR    },
    {0, 6, IMAGE_SCREEN_COLOR    },
    {0, 7, IMAGE_SCREEN_DEPTH    },
    {0, 8, IMAGE_COPY_COLOR      },
    {0, 9, IMAGE_COPY_DEPTH      }
};

typedef struct {
    f32 screen_params  [4];
    f32 sun_direction  [4];
    f32 camera_position[4];
    f32 time           [4];
    f32 camera_vp      [4 * 4];
    f32 camera_inv_vp  [4 * 4];
    f32 camera_inv_v   [4 * 4];
} GlobalBuffer;

const BufferInfo buffer_infos[BUFFER_COUNT] = {
    [BUFFER_GLOBAL] = (BufferInfo) {
        .flags = GPU_BUFFER_FLAG_UNIFORM_BUFFER,
        .size  = sizeof(GlobalBuffer)
    }
};

const ImageInfo image_infos[IMAGE_COUNT] = {
    [IMAGE_SCREEN_COLOR] = (ImageInfo) {
        .flags  = GPU_IMAGE_FLAG_COLOR_ATTACHMENT | GPU_IMAGE_FLAG_SAMPLED | GPU_IMAGE_FLAG_STORAGE,
        .format = GPU_FORMAT_R16G16B16A16_SFLOAT,
        .size_x = FRAME_BUFFER_SIZE_X,
        .size_y = FRAME_BUFFER_SIZE_Y
    },
    [IMAGE_SCREEN_DEPTH] = (ImageInfo) {
        .flags  = GPU_IMAGE_FLAG_DEPTH_ATTACHMENT | GPU_IMAGE_FLAG_SAMPLED,
        .format = GPU_FORMAT_D32_SFLOAT,
        .size_x = FRAME_BUFFER_SIZE_X,
        .size_y = FRAME_BUFFER_SIZE_Y
    },
    [IMAGE_COPY_COLOR] = (ImageInfo) {
        .flags  = GPU_IMAGE_FLAG_COLOR_ATTACHMENT | GPU_IMAGE_FLAG_SAMPLED,
        .format = GPU_FORMAT_R16G16B16A16_SFLOAT,
        .size_x = FRAME_BUFFER_SIZE_X,
        .size_y = FRAME_BUFFER_SIZE_Y
    },
    [IMAGE_COPY_DEPTH] = (ImageInfo) {
        .flags  = GPU_IMAGE_FLAG_COLOR_ATTACHMENT | GPU_IMAGE_FLAG_SAMPLED,
        .format = GPU_FORMAT_R32_SFLOAT,
        .size_x = FRAME_BUFFER_SIZE_X,
        .size_y = FRAME_BUFFER_SIZE_Y
    }
};

#endif
