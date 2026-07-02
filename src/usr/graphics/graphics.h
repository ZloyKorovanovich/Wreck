#ifndef _GRAPHICS_INCLUDED
#define _GRAPHICS_INCLUDED

#include "../../base.h"

typedef struct {
    const f32* camera_vp;
    const f32* camera_inv_vp;
    const f32* camera_inv_v;
    const f32* camera_position;
    const f32* sun_direction;
    f64        time;
    f64        delta;
} FrameData;

b32  graphics_load(CtxHandle gpu_ctx, CtxHandle res_ctx);
void graphics_unload(CtxHandle gpu_ctx);
i32  graphics_render_frame(CtxHandle gpu_ctx, const FrameData* frame_data);

#endif
