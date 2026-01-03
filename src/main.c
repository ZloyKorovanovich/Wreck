#include "main.h"

#ifdef _WIN32
#include <windows.h>
#endif

b32 msgCallback(i32 msg_code, const char* msg) {
    /* if message is error we stop */
    if(MSG_IS_ERROR(msg_code)) {
        if(msg) printf("!: %d %s\n", msg_code, msg);
        else printf("ERROR: %d\n", msg_code);
        return TRUE;
    }
    /* if message is warning, we just print it and continue */
    if(MSG_IS_WARNING(msg_code)) {
        if(msg) printf("?: %d %s\n", msg_code, msg);
        else printf("?: %d\n", msg_code);
        return FALSE;
    }
    /* log message */
    if(msg) printf(":: %s\n", msg);
    return FALSE;
}

typedef struct {
    f32 screen_params[4];
    f32 time_params[4];
} UniformBuffer;

typedef struct {
    f32 vertices[36 * 4];
} CubeBuffer;

typedef struct {
    f32 positions[16 * 4];
} GridBuffer;

void startCallback(VulkanCmdContext* cmd) {
    CubeBuffer* cube_buffer = cmdBeginWriteResource(cmd, &(RenderBinding){1, 0});
    *cube_buffer = (CubeBuffer) {
        .vertices =  {
            -1.0, -1.0,  1.0, 1.0,
            1.0, -1.0,  1.0, 1.0,
            1.0,  1.0,  1.0, 1.0,
            
            -1.0, -1.0,  1.0, 1.0,
            1.0,  1.0,  1.0, 1.0,
            -1.0,  1.0,  1.0, 1.0,

            1.0, -1.0, -1.0, 1.0,
            -1.0, -1.0, -1.0, 1.0,
            -1.0,  1.0, -1.0, 1.0,
            
            1.0, -1.0, -1.0, 1.0,
            -1.0,  1.0, -1.0, 1.0,
            1.0,  1.0, -1.0, 1.0,

            -1.0,  1.0,  1.0, 1.0,
            1.0,  1.0,  1.0, 1.0,
            1.0,  1.0, -1.0, 1.0,
            
            -1.0,  1.0,  1.0, 1.0,
            1.0,  1.0, -1.0, 1.0,
            -1.0,  1.0, -1.0, 1.0,
            
            -1.0, -1.0, -1.0, 1.0,
            1.0, -1.0, -1.0, 1.0,
            1.0, -1.0,  1.0, 1.0,
            
            -1.0, -1.0, -1.0, 1.0,
            1.0, -1.0,  1.0, 1.0,
            -1.0, -1.0,  1.0, 1.0,

            1.0, -1.0,  1.0, 1.0,
            1.0, -1.0, -1.0, 1.0,
            1.0,  1.0, -1.0, 1.0,
            
            1.0, -1.0,  1.0, 1.0,
            1.0,  1.0, -1.0, 1.0,
            1.0,  1.0,  1.0, 1.0,

            -1.0, -1.0, -1.0, 1.0,
            -1.0, -1.0,  1.0, 1.0,
            -1.0,  1.0,  1.0, 1.0,
            
            -1.0, -1.0, -1.0, 1.0,
            -1.0,  1.0,  1.0, 1.0,
            -1.0,  1.0, -1.0, 1.0
        }
    };
    cmdEndWriteResource(cmd);
}

void renderCallback(const RenderWindowContext* window_context, VulkanCmdContext* cmd) {
    static f64 s_delta_time = 0.0;
    static f64 s_time = 0.0;

#ifdef _WIN32
    static LARGE_INTEGER s_cpu_time = (LARGE_INTEGER){0};

    LARGE_INTEGER new_cpu_time = (LARGE_INTEGER){0};
    LARGE_INTEGER cpu_frequency = (LARGE_INTEGER){0};
    QueryPerformanceCounter(&new_cpu_time);
    QueryPerformanceFrequency(&cpu_frequency);
    s_delta_time = (f64)(new_cpu_time.QuadPart - s_cpu_time.QuadPart) / (f64)cpu_frequency.QuadPart;
    s_time += s_delta_time;
    s_cpu_time = new_cpu_time;
#endif

    UniformBuffer* uniform_buffer = cmdBeginWriteResource(cmd, &(RenderBinding){0, 0});
    *uniform_buffer = (UniformBuffer) {
        .screen_params = {
            (f32)window_context->x,
            (f32)window_context->y,
            1.0, -1.0
        },
        .time_params = {
            (f32)s_time,
            (f32)s_delta_time,
            0.0, 0.0
        }
    };
    cmdEndWriteResource(cmd);

    cmdCompute(cmd, 0, 1, 1, 1);
    cmdDraw(cmd, 1, 36, 16);
}

i32 main(i32 argc, char** argv) {
    VulkanContext* vulkan_context = NULL;
    RenderContext* render_context = NULL;
    /* fill everything */
    const VulkanContextInfo vulkan_info = {
        .name = "Wreck Demo",
        .x = 800,
        .y = 600,
        .flags = VULKAN_FLAG_WIN_RESIZE,
        .version = MAKE_VERSION(0, 1, 0)
    };

    RenderResourceInfo resource_infos[] = {
        (RenderResourceInfo) {
            .binding = 0, .set = 0,
            .mutability = RENDER_RESOURCE_HOST_MUTABLE,
            .type = RENDER_RESOURCE_TYPE_UNIFORM_BUFFER,
            .size = sizeof(UniformBuffer)
        },
        (RenderResourceInfo) {
            .binding = 1, .set = 0,
            .mutability = RENDER_RESOURCE_HOST_MUTABLE,
            .type = RENDER_RESOURCE_TYPE_STORAGE_BUFFER,
            .size = sizeof(CubeBuffer)
        },
        (RenderResourceInfo) {
            .binding = 2, .set = 0,
            .mutability = RENDER_RESOURCE_HOST_IMMUTABLE,
            .type = RENDER_RESOURCE_TYPE_STORAGE_BUFFER,
            .size = sizeof(GridBuffer)
        },
        (RenderResourceInfo) {
            .binding = 3, .set = 0,
            .mutability = RENDER_RESOURCE_HOST_IMMUTABLE,
            .type = RENDER_RESOURCE_TYPE_STORAGE_BUFFER,
            .size = sizeof(GridBuffer)
        }
    };

    RenderPipelineInfo pipeline_infos[] = {
        [0] = (RenderPipelineInfo) {
            .type = RENDER_PIPELINE_TYPE_COMPUTE,
            .name = "grid",
            .compute_shader = "data/grid_c.spv",
            .resources_access = (RenderResourceAccess[]){
                {0, 0, RENDER_RESOURCE_ACCESS_TYPE_READ},
                {2, 0, RENDER_RESOURCE_ACCESS_TYPE_WRITE}
            },
            .resource_access_count = 2
        },
        [1] = (RenderPipelineInfo) {
            .type = RENDER_PIPELINE_TYPE_GRAPHICS,
            .name = "cube",
            .vertex_shader = "data/cube_v.spv",
            .fragment_shader = "data/cube_f.spv",
            .resources_access = (RenderResourceAccess[]){
                {0, 0, RENDER_RESOURCE_ACCESS_TYPE_READ}
            },
            .resource_access_count = 1
        }
    };

    const RenderContextInfo render_info = (RenderContextInfo) {
        .resource_count = ARRAY_COUNT(resource_infos),
        .resource_infos = resource_infos,
        .pipeline_count = ARRAY_COUNT(pipeline_infos),
        .pipeline_infos = pipeline_infos,
        .update_callback = &renderCallback,
        .start_callback = &startCallback
    };
    /* run vulkan rendering! */
    if(MSG_IS_ERROR(createVulkanContext(&vulkan_info, &msgCallback, &vulkan_context))) return -1;
    if(MSG_IS_ERROR(createRenderContext(vulkan_context, &render_info, &msgCallback, &render_context))) return -2;
    if(MSG_IS_ERROR(renderLoop(vulkan_context, &msgCallback, render_context))) return -3;
    if(MSG_IS_ERROR(destroyRenderContext(vulkan_context, msgCallback, render_context))) return -3;
    if(MSG_IS_ERROR(destroyVulkanContext(&msgCallback, vulkan_context))) return -4;
}
