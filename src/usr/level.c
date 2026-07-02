#include "level.h"
#include <windows.h>

#define KEY_PRESSED_FLAG 0x8000

static u64 old_counter   = 0;
static f64 time          = 0.0;

static i32 old_mouse_pos_x = 0;
static i32 old_mouse_pos_y = 0;

static f32  camera_x_rot    = 0.0f;
static f32  camera_y_rot    = 0.0f;
static vec4 camera_position = {0.0, 1.0, 0.0, 1.0};
static vec4 camera_rotation = {0.0, 0.0, 0.0, 1.0};

/* 0 = success
   1 = fail    
   2 = quit   */
i32 update_level(mat4 camera_vp, mat4 camera_inv_v, vec4 camera_pos, f64* out_time, f64* out_delta) {
    static b32 is_firt_frame = TRUE;

    f64 delta = 0.0;

    /* query performance */ {
    LARGE_INTEGER performance_counter = (LARGE_INTEGER){0};
    LARGE_INTEGER frequency_counter   = (LARGE_INTEGER){0};

    if(is_firt_frame) {
        if(!QueryPerformanceCounter(&performance_counter)) {
            LOG_ERROR("failed to query performance counter");
            goto fail;
        }
        old_counter = performance_counter.QuadPart;
    } else {
        delta = 0;

        if(!QueryPerformanceCounter(&performance_counter)) {
            LOG_ERROR("failed to query performance counter");
            goto fail;
        }

        if(!QueryPerformanceFrequency(&frequency_counter)) {
            LOG_ERROR("failed to query frequency counter");
            goto fail;
        }

        delta = (f64)(performance_counter.QuadPart - old_counter) / (f64)frequency_counter.QuadPart;

        time = time + delta;
        old_counter = performance_counter.QuadPart;
    }
    }

    i32 mouse_delta_x = 0;
    i32 mouse_delta_y = 0;

    /* process window events */
    MSG msg = (MSG){0};
    while(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        switch (msg.message) {
            case WM_QUIT:
                /* quit window */
            goto quit;
            case WM_INPUT: 
               RAWINPUT raw_input      = (RAWINPUT){0};
               UINT     raw_input_size = sizeof(RAWINPUT);
                
                GetRawInputData((HRAWINPUT)msg.lParam, RID_INPUT, &raw_input, &raw_input_size, sizeof(RAWINPUTHEADER));
                if(raw_input.header.dwType == RIM_TYPEMOUSE) {
                    mouse_delta_x += raw_input.data.mouse.lLastX;
                    mouse_delta_y += raw_input.data.mouse.lLastY;
                }
            break;
            default:
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            break;
        }
    }

    /* camera */ {
    f32 k = delta * LEVEL_CAMERA_MOVE_SPEED;

    vec4 forward  = { 0.0, 0.0,-1.0 * k, 0.0};
    vec4 backward = { 0.0, 0.0, 1.0 * k, 0.0};
    vec4 up       = { 0.0, 1.0 * k, 0.0, 0.0};
    vec4 down     = { 0.0,-1.0 * k, 0.0, 0.0};
    vec4 right    = { 1.0 * k, 0.0, 0.0, 0.0};
    vec4 left     = {-1.0 * k, 0.0, 0.0, 0.0};

    vec4 local_forward  = {0};
    vec4 local_backward = {0};
    vec4 local_up       = {0};
    vec4 local_down     = {0};
    vec4 local_right    = {0};
    vec4 local_left     = {0};

    glm_quat_rotatev(camera_rotation, forward, local_forward);
    glm_quat_rotatev(camera_rotation, backward, local_backward);
    glm_quat_rotatev(camera_rotation, up, local_up);
    glm_quat_rotatev(camera_rotation, down, local_down);
    glm_quat_rotatev(camera_rotation, right, local_right);
    glm_quat_rotatev(camera_rotation, left, local_left);

    if(GetKeyState('E') & KEY_PRESSED_FLAG) {
        glm_vec4_add(camera_position, local_forward, camera_position);
    }
    if(GetKeyState('D') & KEY_PRESSED_FLAG) {
        glm_vec4_add(camera_position, local_backward, camera_position);
    }
    if(GetKeyState('S') & KEY_PRESSED_FLAG) {
        glm_vec4_add(camera_position, local_left, camera_position);
    }
    if(GetKeyState('F') & KEY_PRESSED_FLAG) {
        glm_vec4_add(camera_position, local_right, camera_position);
    }
    if(GetKeyState(VK_CONTROL) & KEY_PRESSED_FLAG) {
        glm_vec4_add(camera_position, local_down, camera_position);
    }
    if(GetKeyState(VK_SPACE) & KEY_PRESSED_FLAG) {
        glm_vec4_add(camera_position, local_up, camera_position);
    }

    if(!is_firt_frame) {
        /* alternate controls */
        /* <> left-right */
        if(GetKeyState(VK_OEM_COMMA) & KEY_PRESSED_FLAG) {
            camera_x_rot = camera_x_rot + 1 * LEVEL_CAMERA_ROTATE_SPEED * delta;
        }
        if(GetKeyState(VK_OEM_PERIOD) & KEY_PRESSED_FLAG) {
            camera_x_rot = camera_x_rot - 1 * LEVEL_CAMERA_ROTATE_SPEED * delta;
        }
        /* :" up-down */
        if(GetKeyState(VK_OEM_1) & KEY_PRESSED_FLAG) {
            camera_y_rot = camera_y_rot + 1 * LEVEL_CAMERA_ROTATE_SPEED * delta;
        }
        if(GetKeyState(VK_OEM_7) & KEY_PRESSED_FLAG) {
            camera_y_rot = camera_y_rot - 1 * LEVEL_CAMERA_ROTATE_SPEED * delta;
        }

        camera_x_rot = camera_x_rot - (f32)mouse_delta_x * LEVEL_CAMERA_ROTATE_SPEED;
        camera_y_rot = camera_y_rot - (f32)mouse_delta_y * LEVEL_CAMERA_ROTATE_SPEED;
        camera_y_rot = glm_clamp(camera_y_rot, -PI/2.0, PI/2.0);
    }

    vec4 rotator_y = {0.0, sinf(camera_x_rot / 2.0), 0.0, cosf(camera_x_rot / 2.0)};
    vec4 rotator_x = {sinf(camera_y_rot / 2.0), 0.0, 0.0, cosf(camera_y_rot / 2.0)};

    glm_quat_mul(rotator_y, rotator_x, camera_rotation);

    mat4 transform = {
        {1.0, 0.0, 0.0, 0.0},
        {0.0, 1.0, 0.0, 0.0},
        {0.0, 0.0, 1.0, 0.0},
        {0.0, 0.0, 0.0, 1.0}
    };
    mat4 translate_mat = {
        {1.0, 0.0, 0.0, 0.0},
        {0.0, 1.0, 0.0, 0.0},
        {0.0, 0.0, 1.0, 0.0},
        {camera_position[0], camera_position[1], camera_position[2], 1.0}
    };
    mat4 rotate_mat  = {0};
    mat4 project_mat = {0};
    mat4 transform_1 = {0};

    glm_quat_mat4(camera_rotation, rotate_mat);
    glm_perspective(LEVEL_CAMERA_FOV * DEG_TO_RAD, 16.0 / 9.0, LEVEL_CAMERA_NEAR_CLIP, LEVEL_CAMERA_FAR_CLIP, project_mat);
    project_mat[1][1] = -project_mat[1][1];

    glm_mat4_mul(transform   , translate_mat, transform_1 );
    glm_mat4_mul(transform_1 , rotate_mat   , camera_inv_v);
    glm_mat4_inv(camera_inv_v, transform_1                );
    glm_mat4_mul(project_mat , transform_1  , camera_vp   );

    camera_pos[0] = camera_position[0];
    camera_pos[1] = camera_position[1];
    camera_pos[2] = camera_position[2];
    camera_pos[3] = camera_position[3];

    /*
    printf(
        "cam pos: {%f, %f, %f, %f}, cam rot: {%f, %f, %f, %f}, mouse_delta: {%d, %d}, time: %f, delta: %f\n",
        camera_position[0], camera_position[1], camera_position[2], camera_position[3],
        camera_rotation[0], camera_rotation[1], camera_rotation[2], camera_rotation[3],
        mouse_delta_x, mouse_delta_y,
        (f32)time, (f32)delta
    );*/
    

    is_firt_frame = FALSE;
    }

    *out_time  = time;
    *out_delta = delta;

    return 0;

    fail: {
        return 1;
    }

    quit: {
        return 2;
    }
}
