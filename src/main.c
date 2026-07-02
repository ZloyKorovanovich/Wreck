#include "base.h"

#include "gpu/gpu.h"
#include "res/res.h"
#include "usr/graphics/graphics.h"
#include "usr/level.h"

i32 main(i32 argc, char** argv) {
    /* startup */
    const GpuInfo gpu_info = {
        .window_name          = "Wreck",
        .frame_buffer_x       = 1920,
        .frame_buffer_y       = 1080,
        .pci_vendor_id        = 0x1002, /* 0x1002 0x10DE */
        .pci_device_id        = 0x1638, /* 0x1638 0x25E0 */
        .vulkan_debug_enabled = FALSE
    };
    CtxHandle gpu_ctx = NULL;
    CtxHandle res_ctx = NULL;

    gpu_ctx = gpu_start(&gpu_info);
    res_ctx = res_start();

    if(gpu_ctx == NULL) {
        LOG_ERROR("failed to start gpu");
        goto fail;
    }
    if(res_ctx == NULL) {
        LOG_ERROR("failed to start resources");
        goto fail;
    }

    if(!graphics_load(gpu_ctx, res_ctx)) {
        LOG_ERROR("failed to init graphics");
        goto fail;
    }

    while(1) {
        i32 update_result  = 0;
        i32 render_result  = 0;
        
        mat4 camera_vp       = {0};
        mat4 camera_inv_vp   = {0};
        mat4 camera_inv_v    = {0};
        vec4 camera_position = {0};
        vec4 sun_direction   = {0};

        f64 time  = 0.0;
        f64 delta = 0.0;

        update_result = update_level(
            camera_vp, 
            camera_inv_v,
            camera_position,
            &time, 
            &delta
        );
        if(update_result == 1) {
            LOG_ERROR("failed to update level");
            goto fail;
        }
        if(update_result == 2) {
            break;
        }
        glm_mat4_inv(
            camera_vp, 
            camera_inv_vp
        );

        f32 sun_state = time * 0.05;
        sun_state     = sun_state - floor(sun_state / PI) * PI;

        sun_direction[0] = cosf(sun_state);
        sun_direction[1] = sinf(sun_state);


        FrameData frame_data = {
            .camera_vp       = (f32*)camera_vp,
            .camera_inv_vp   = (f32*)camera_inv_vp,
            .camera_inv_v    = (f32*)camera_inv_v,
            .camera_position = (f32*)camera_position,
            .sun_direction   = (f32*)sun_direction,
            .time            = time,
            .delta           = delta
        };

        render_result = graphics_render_frame(gpu_ctx, &frame_data);
        if(render_result == 1) {
            LOG_ERROR("failed to render frame");
            goto fail;
        }
        if(render_result == 2) {
            break;
        }
    }
    
    graphics_unload(gpu_ctx);
    gpu_stop(gpu_ctx);
    res_stop(res_ctx);

    return 0;

    fail: {
        return 1;
    }
}


