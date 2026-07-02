#include "graphics.h"
#include "../../res/res.h"
#include "../../gpu/gpu.h"

#include "resources.h"
#include "pipelines.h"

static PipelineInfo pipeline_infos[PIPELINE_COUNT] = {0};

b32 generate_graphics_pipeline(
    CtxHandle        res_ctx,
    const char*      vertex_name,
    const char*      fragment_name,
    const GpuFormat* color_formats,
    u32              color_formats_count,
    u32              depth_format,
    PipelineInfo*    pipeline_info
) {
    void* vertex_shader_code   = NULL;
    void* fragment_shader_code = NULL;
    u64   vertex_shader_size   = 0;
    u64   fragment_shader_size = 0;

    vertex_shader_code   = res_load_shader(res_ctx, vertex_name, &vertex_shader_size);
    fragment_shader_code = res_load_shader(res_ctx, fragment_name, &fragment_shader_size);

    if(vertex_shader_code == NULL) {
        LOG_ERROR("failed to load vertex shader");
        goto fail;
    }
    if(fragment_shader_code == NULL) {
        LOG_ERROR("failed to load fragment shader");
        goto fail;
    }

    *pipeline_info = (PipelineInfo) {
        .type                = GPU_PIPELINE_TYPE_GRAPHICS,
        .vertex              = vertex_shader_code,
        .fragment            = fragment_shader_code,
        .vertex_size         = vertex_shader_size,
        .fragment_size       = fragment_shader_size,
        .color_formats       = color_formats,
        .color_formats_count = color_formats_count,
        .depth_format        = depth_format
    };

    return TRUE;

    fail: {
        return FALSE;
    }
}

b32 generate_compute_pipeline(
    CtxHandle     res_ctx,
    const char*   compute_name,
    PipelineInfo* pipeline_info
) {
    void* compute_shader_code = NULL;
    u64   compute_shader_size = 0;

    compute_shader_code = res_load_shader(res_ctx, compute_name, &compute_shader_size);
    
    if(compute_shader_code == NULL) {
        LOG_ERROR("failed to load compute shader file");
        goto fail;
    }

    *pipeline_info = (PipelineInfo) {
        .type         = GPU_PIPELINE_TYPE_COMPUTE,
        .compute      = compute_shader_code,
        .compute_size = compute_shader_size
    };

    return TRUE;

    fail: {
        return FALSE;
    }
}

b32 load_shaders(
    CtxHandle         res_ctx, 
    const ShaderData* shader_datas, 
    PipelineInfo*     pipeline_infos
) {
    for(u32 i = 0; i != PIPELINE_COUNT; i++) {
        const char* shader_name               = shader_datas[i].name;
        const u32*  color_formats             = shader_datas[i].color_formats;
        const u32   color_formats_count       = shader_datas[i].color_formats_count;
        const u32   depth_format              = shader_datas[i].depth_format;

        char        shader_path_vertex  [256] = {0};
        char        shader_path_fragment[256] = {0};
        char        shader_path_compute [256] = {0};

        /* invalid name */
        if(shader_name == NULL) {
            LOG_ERROR("invalid shader name id: %u", i);
            goto fail;
        }
        if(shader_name[0] == 0 || shader_name[1] != ':') {
            LOG_ERROR("invalid shader name format: \"%s\" id: %u", shader_name, i);
            goto fail;
        }

        switch(shader_name[0]) {
            case 'g':
                strcpy_s(shader_path_vertex  , sizeof(shader_path_vertex)  , shader_name + 2);
                strcat_s(shader_path_vertex  , sizeof(shader_path_vertex)  , "_v.spv"       );
                strcpy_s(shader_path_fragment, sizeof(shader_path_fragment), shader_name + 2);
                strcat_s(shader_path_fragment, sizeof(shader_path_fragment), "_f.spv"       );

                if(!generate_graphics_pipeline(
                    res_ctx,
                    shader_path_vertex,
                    shader_path_fragment,
                    color_formats,
                    color_formats_count,
                    depth_format,
                    &pipeline_infos[i]
                )) {
                    LOG_ERROR("failed to generate pipeline info name: \"%s\" id: %u", shader_name, i);
                    goto fail;
                }
            break;
            case 'c':
                strcpy_s(shader_path_compute, sizeof(shader_path_compute), shader_name + 2);
                strcat_s(shader_path_compute, sizeof(shader_path_compute), "_c.spv"       );

                if(!generate_compute_pipeline(
                    res_ctx,
                    shader_path_compute,
                    &pipeline_infos[i]
                )) {
                    LOG_ERROR("failed to generate pipeline info name: \"%s\" id: %u", shader_name, i);
                    goto fail;
                }
            break;
            default:
                LOG_ERROR("invalid shader type name: \"%s\" id: %u", shader_name, i);
            goto fail;
        }
    }

    return TRUE;

    fail: {
        return FALSE;
    }
}


b32 graphics_load(
    CtxHandle gpu_ctx, 
    CtxHandle res_ctx
) {

    /* compile pipelines */ {
        if(!load_shaders(res_ctx, shader_table, pipeline_infos)) {
            LOG_ERROR("failed to load shaders");
            goto fail;
        }
        const ShadersInfo shaders_info = {
            .descriptor_set_infos = {
                [GPU_DESCRIPTOR_SET_0] = (DescriptorSetInfo) {
                    .bindings       = set_0_bindings,
                    .bindings_count = ARRAY_SIZE(set_0_bindings)
                }
            },
            .pipeline_infos       = pipeline_infos,
            .pipeline_infos_count = PIPELINE_COUNT
        };
        if(!gpu_compile_shaders(gpu_ctx, &shaders_info)) {
            LOG_ERROR("failed to compile shaders");
            goto fail;
        }
        res_free_shaders(res_ctx);
    }    

    /* allocate resources */ {
        const ResourcesInfo resources_info = {
            .buffer_infos       = buffer_infos,
            .buffer_infos_count = BUFFER_COUNT,
            .image_infos        = image_infos,
            .image_infos_count  = IMAGE_COUNT
        };
        if(!gpu_allocate_resources(gpu_ctx, &resources_info)) {
            LOG_ERROR("failed to allocate resources");
            goto fail;
        }
    }

    if(!gpu_write_bindings(gpu_ctx, binding_writes, ARRAY_SIZE(binding_writes))) {
        LOG_ERROR("failed to write bindings");
        goto fail;
    }
    if(!gpu_render_init(gpu_ctx)) {
        LOG_ERROR("failed to init render");
        goto fail;
    }

    return TRUE;

    fail: {
        return FALSE;
    }
}

void graphics_unload(
    CtxHandle gpu_ctx
) {
    gpu_render_terminate(gpu_ctx);
    gpu_release_shaders(gpu_ctx);
    gpu_release_resources(gpu_ctx);
}

i32 graphics_render_frame(
    CtxHandle        gpu_ctx, 
    const FrameData* frame_data
) {
    u32 screen_x = 0;
    u32 screen_y = 0;

    
    /* begin frame */
    i32 frame_begin_result = gpu_render_frame_begin(gpu_ctx, &screen_x, &screen_y);
    if(frame_begin_result == 1) {
        LOG_ERROR("failed to begin frame rendering");
        goto fail;
    }
    if(frame_begin_result == 2) {
        goto close;
    }

    /* transfer sync buffers */ {
        const f32* sun_direction = frame_data->sun_direction;
        const f32* cam_position  = frame_data->camera_position;
        const f32* cam_vp        = frame_data->camera_vp;
        const f32* cam_inv_vp    = frame_data->camera_inv_vp;
        const f32* cam_inv_v     = frame_data->camera_inv_v;

        const GlobalBuffer global_buffer = {
            .screen_params   = {(f32)screen_x, (f32)screen_y, (f32)FRAME_BUFFER_SIZE_X, (f32)FRAME_BUFFER_SIZE_Y},
            .sun_direction   = {sun_direction[0], sun_direction[1], sun_direction[2], sun_direction[3]},
            .camera_position = {cam_position[0], cam_position[1], cam_position[2], cam_position[3]},
            .time            = {frame_data->time, frame_data->delta, 0.0, 0.0},
            .camera_vp       = {
                cam_vp[0 ], cam_vp[1 ], cam_vp[2 ], cam_vp[3 ],
                cam_vp[4 ], cam_vp[5 ], cam_vp[6 ], cam_vp[7 ],
                cam_vp[8 ], cam_vp[9 ], cam_vp[10], cam_vp[11],
                cam_vp[12], cam_vp[13], cam_vp[14], cam_vp[15]
            },
            .camera_inv_vp   = {
                cam_inv_vp[0 ], cam_inv_vp[1 ], cam_inv_vp[2 ], cam_inv_vp[3 ],
                cam_inv_vp[4 ], cam_inv_vp[5 ], cam_inv_vp[6 ], cam_inv_vp[7 ],
                cam_inv_vp[8 ], cam_inv_vp[9 ], cam_inv_vp[10], cam_inv_vp[11],
                cam_inv_vp[12], cam_inv_vp[13], cam_inv_vp[14], cam_inv_vp[15]
            },
            .camera_inv_v    = {
                cam_inv_v[0 ], cam_inv_v[1 ], cam_inv_v[2 ], cam_inv_v[3 ],
                cam_inv_v[4 ], cam_inv_v[5 ], cam_inv_v[6 ], cam_inv_v[7 ],
                cam_inv_v[8 ], cam_inv_v[9 ], cam_inv_v[10], cam_inv_v[11],
                cam_inv_v[12], cam_inv_v[13], cam_inv_v[14], cam_inv_v[15]
            }
        };

        gpu_render_write_buffer(gpu_ctx, BUFFER_GLOBAL, &global_buffer, 0, sizeof(GlobalBuffer));
    }

    /* skybox */ {
        const DrawingInfo skybox_drawing_info = (DrawingInfo) {
            .offset_x                = 0,
            .offset_y                = 0,
            .size_x                  = screen_x,
            .size_y                  = screen_y,
            .min_depth               = 0.0,
            .max_depth               = 1.0,
            .buffers_read_count      = 1,
            .buffers_read            = (u32[]) {
                BUFFER_GLOBAL
            },
            .attachments_color_count = 1,
            .attachments_color       = (u32[]) {
                IMAGE_SCREEN_COLOR
            },
            .attachment_depth        = IMAGE_SCREEN_DEPTH
        };
        
        gpu_render_begin_drawing(gpu_ctx, &skybox_drawing_info);
        gpu_render_bind_graphics_pipeline(gpu_ctx, PIPELINE_SKYBOX);
        gpu_render_draw(gpu_ctx, 1, 6);
        gpu_render_end_drawing(gpu_ctx);
    }

    /* copy color */ {
        const DrawingInfo copy_color_drawing_info = (DrawingInfo) {
            .offset_x                = 0,
            .offset_y                = 0,
            .size_x                  = screen_x,
            .size_y                  = screen_y,
            .min_depth               = 0.0,
            .max_depth               = 1.0,
            .buffers_read_count      = 1,
            .buffers_read            = (u32[]) {
                BUFFER_GLOBAL
            },
            .images_read_count       = 1,
            .images_read             = (u32[]) {
                IMAGE_SCREEN_COLOR
            },
            .attachments_color_count = 1,
            .attachments_color       = (u32[]) {
                IMAGE_COPY_COLOR
            },
            .attachment_depth        = U32_MAX
        };
        
        gpu_render_begin_drawing(gpu_ctx, &copy_color_drawing_info);
        gpu_render_bind_graphics_pipeline(gpu_ctx, PIPELINE_COLOR_BLIT);
        gpu_render_draw(gpu_ctx, 1, 6);
        gpu_render_end_drawing(gpu_ctx);
    }

    /* water */ {
        const DrawingInfo water_drawing_info = (DrawingInfo) {
            .do_not_clear            = TRUE,
            .offset_x                = 0,
            .offset_y                = 0,
            .size_x                  = screen_x,
            .size_y                  = screen_y,
            .min_depth               = 0.0,
            .max_depth               = 1.0,
            .buffers_read_count      = 1,
            .buffers_read            = (u32[]) {
                BUFFER_GLOBAL
            },
            .images_read_count       = 1,
            .images_read             = (u32[]) {
                IMAGE_COPY_COLOR
            },
            .attachments_color_count = 1,
            .attachments_color       = (u32[]) {
                IMAGE_SCREEN_COLOR
            },
            .attachment_depth        = IMAGE_SCREEN_DEPTH
        };

        gpu_render_begin_drawing(gpu_ctx, &water_drawing_info);
        gpu_render_bind_graphics_pipeline(gpu_ctx, PIPELINE_WATER_SURFACE);
        gpu_render_draw(gpu_ctx, 1, (600*600 + (4) * 8) * 6);
        gpu_render_end_drawing(gpu_ctx);
    }

    /* copy color */ {
        const DrawingInfo copy_color_drawing_info = (DrawingInfo) {
            .offset_x                = 0,
            .offset_y                = 0,
            .size_x                  = screen_x,
            .size_y                  = screen_y,
            .min_depth               = 0.0,
            .max_depth               = 1.0,
            .buffers_read_count      = 1,
            .buffers_read            = (u32[]) {
                BUFFER_GLOBAL
            },
            .images_read_count       = 1,
            .images_read             = (u32[]) {
                IMAGE_SCREEN_COLOR
            },
            .attachments_color_count = 1,
            .attachments_color       = (u32[]) {
                IMAGE_COPY_COLOR
            },
            .attachment_depth        = U32_MAX
        };
        
        gpu_render_begin_drawing(gpu_ctx, &copy_color_drawing_info);
        gpu_render_bind_graphics_pipeline(gpu_ctx, PIPELINE_COLOR_BLIT);
        gpu_render_draw(gpu_ctx, 1, 6);
        gpu_render_end_drawing(gpu_ctx);
    }

    /* underwater */ {
        const DrawingInfo underwater_drawing_info = (DrawingInfo) {
            .do_not_clear            = TRUE,
            .offset_x                = 0,
            .offset_y                = 0,
            .size_x                  = screen_x,
            .size_y                  = screen_y,
            .min_depth               = 0.0,
            .max_depth               = 1.0,
            .buffers_read_count      = 1,
            .buffers_read            = (u32[]) {
                BUFFER_GLOBAL
            },
            .images_read_count       = 2,
            .images_read             = (u32[]) {
                IMAGE_COPY_COLOR,
                IMAGE_SCREEN_DEPTH
            },
            .attachments_color_count = 1,
            .attachments_color       = (u32[]) {
                IMAGE_SCREEN_COLOR
            },
            .attachment_depth        = U32_MAX
        };
        
        gpu_render_begin_drawing(gpu_ctx, &underwater_drawing_info);
        gpu_render_bind_graphics_pipeline(gpu_ctx, PIPELINE_UNDERWATER);
        gpu_render_draw(gpu_ctx, 1, 32 * 32 * 6);
        gpu_render_end_drawing(gpu_ctx);
    }

    /* surface blit */ {
        const DrawingInfo surface_blit_drawing_info = (DrawingInfo) {
            .offset_x                = 0,
            .offset_y                = 0,
            .size_x                  = screen_x,
            .size_y                  = screen_y,
            .buffers_read_count      = 1,
            .buffers_read            = (u32[]) {
                BUFFER_GLOBAL
            },
            .images_read_count       = 1,
            .images_read             = (u32[]) {
                IMAGE_SCREEN_COLOR
            },
            .attachments_color_count = 1,
            .attachments_color       = (u32[]) {
                IMAGE_SURFACE
            },
            .attachment_depth        = U32_MAX
        };

        gpu_render_begin_drawing(gpu_ctx, &surface_blit_drawing_info);
        gpu_render_bind_graphics_pipeline(gpu_ctx, PIPELINE_SURFACE_BLIT);
        gpu_render_draw(gpu_ctx, 1, 6);
        gpu_render_end_drawing(gpu_ctx);
    }

    /* end frame */
    i32 frame_end_result = gpu_render_frame_end(gpu_ctx);
    if(frame_end_result == 1) {
        LOG_ERROR("failed to end frame rendering");
        goto fail;
    }
    if(frame_end_result == 2) {
        goto close;
    }
    

    return 0;

    fail: {
        return 1;
    }
    close: {
        return 2;
    }
}
