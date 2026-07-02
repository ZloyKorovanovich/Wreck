#ifndef _GRAPHICS_PIPELINES_INCLUDED
#define _GRAPHICS_PIPELINES_INCLUDED

#include "../../gpu/gpu.h"

typedef struct {
    const char* name;
    const u32*  color_formats;
    u32         color_formats_count;
    u32         depth_format;
} ShaderData;

enum Pipelines {
    PIPELINE_SURFACE_BLIT,
    PIPELINE_COLOR_BLIT,
    PIPELINE_DEPTH_BLIT,
    PIPELINE_SKYBOX,
    PIPELINE_WATER_SURFACE,
    PIPELINE_UNDERWATER,
    PIPELINE_COUNT
};

const ShaderData shader_table [PIPELINE_COUNT] = {
    [PIPELINE_SURFACE_BLIT ] = {"g:res/spv/copy_color"      , (u32[]){GPU_FORMAT_SURFACE            }, 1, GPU_FORMAT_NONE      },
    [PIPELINE_COLOR_BLIT   ] = {"g:res/spv/copy_color"      , (u32[]){GPU_FORMAT_R16G16B16A16_SFLOAT}, 1, GPU_FORMAT_NONE      },
    [PIPELINE_DEPTH_BLIT   ] = {"g:res/spv/copy_depth"      , (u32[]){GPU_FORMAT_R32_SFLOAT         }, 1, GPU_FORMAT_NONE      },
    [PIPELINE_SKYBOX       ] = {"g:res/spv/skybox"          , (u32[]){GPU_FORMAT_R16G16B16A16_SFLOAT}, 1, GPU_FORMAT_D32_SFLOAT},
    [PIPELINE_WATER_SURFACE] = {"g:res/spv/water_surface"   , (u32[]){GPU_FORMAT_R16G16B16A16_SFLOAT}, 1, GPU_FORMAT_D32_SFLOAT},
    [PIPELINE_UNDERWATER   ] = {"g:res/spv/water_underwater", (u32[]){GPU_FORMAT_R16G16B16A16_SFLOAT}, 1, GPU_FORMAT_NONE      }
};

#endif
