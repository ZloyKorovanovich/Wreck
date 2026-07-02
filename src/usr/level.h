#ifndef _LEVEL_INCLUDED
#define _LEVEL_INCLUDED

#define PI         (3.14159265358979)
#define DEG_TO_RAD (PI/180.0)

/* we work in vulkan coordinate system:
   X+ right
   Y+ up
   Z- forward                           */

#define CGLM_FORCE_DEPTH_ZERO_TO_ONE

#include "../base.h"
#include <cglm/cglm.h>
#include <cglm/cam.h> 

#define LEVEL_CAMERA_MOVE_SPEED   (2.0)
#define LEVEL_CAMERA_ROTATE_SPEED (0.005)
#define LEVEL_CAMERA_FOV          (90.0)
#define LEVEL_CAMERA_NEAR_CLIP    (0.3)
#define LEVEL_CAMERA_FAR_CLIP     (1000.0)

i32 update_level(mat4 camera_vp, mat4 camera_inv_v, vec4 camera_pos, f64* out_time, f64* out_delta);

#endif
