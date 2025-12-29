#include "main.h"
#include "vk/vk.h"

typedef struct {
    f32 screen_params[4];
} UniformBuffer;
typedef struct {
    f32 positions[3 * 3];
} VertexBuffer;

b32 msgCallback(i32 msg_code, const char* msg) {
    /* if message is error we stop */
    if(MSG_IS_ERROR(msg_code)) {
        if(msg) printf("ERROR: %d %s\n", msg_code, msg);
        else printf("ERROR: %d\n", msg_code);
        return TRUE;
    }
    /* if message is warning, we just print it and continue */
    if(MSG_IS_WARNING(msg_code)) {
        if(msg) printf("WARNING: %d %s\n", msg_code, msg);
        else printf("WARNING: %d\n", msg_code);
    }
    /* it is not likely that someone sends message with success code, 
        but it might happen, may be he is happy */
    return FALSE;
}

static float s_screen_x = 0.0;
static float s_screen_y = 0.0;

void renderUpdate(const RenderUpdateContext* render_context) {
    s_screen_x = (float)render_context->screen_x;
    s_screen_y = (float)render_context->screen_y;
}

void uniformBufferWrite(void* ptr) {
    *((UniformBuffer*)ptr) = (UniformBuffer) {
        .screen_params = {100.0 / s_screen_x, 100.0 / s_screen_y, 0.0, 0.0}
    };
}

void uniformBuffer1Write(void* ptr) {
    *((UniformBuffer*)ptr) = (UniformBuffer) {
        .screen_params = {500.0 / s_screen_x, 500.0 / s_screen_y, 0.0, 0.0}
    };
}

void drawTriangleProcedural(RenderDrawInfo** infos, u32* count) {
    static RenderDrawInfo s_draw_infos[1] = {
        (RenderDrawInfo) {
            .instance_count = 1,
            .vertex_count = 3
        }
    };
    *infos = s_draw_infos;
    *count = 1;
}


i32 main(i32 argc, char** argv) {
    const RenderSettings render_settings = {
        .bindings = (RenderBinding[]){ 
            (RenderBinding) {
                .binding = 0, .set = 0, 
                .type = RENDER_BINDING_TYPE_UNIFORM_BUFFER,
                .size = sizeof(UniformBuffer),
                .frame_batch = &uniformBufferWrite
            },
            (RenderBinding) {
                .binding = 1, .set = 0, 
                .type = RENDER_BINDING_TYPE_UNIFORM_BUFFER,
                .size = sizeof(UniformBuffer),
                .frame_batch = &uniformBuffer1Write
            }
        },
        .binding_count = 2,
        .nodes = (RenderNode[]) {
            (RenderNode) {
                .type = RENDER_NODE_TYPE_GRAPHICS,
                .vertex_shader = "out/data/triangle_v.spv",
                .fragment_shader = "out/data/triangle_f.spv",
                .draw_callback = &drawTriangleProcedural
            },
            (RenderNode) {
                .type = RENDER_NODE_TYPE_GRAPHICS,
                .vertex_shader = "out/data/triangle_flip_v.spv",
                .fragment_shader = "out/data/triangle_f.spv",
                .draw_callback = &drawTriangleProcedural
            }
        },
        .node_count = 2,
        .update_callback = &renderUpdate
    };
    /* fill everything */
    const VulkanInfo vulkan_info = {
        .msg_callback = &msgCallback,
        .name = "Wreck Demo",
        .x = 800,
        .y = 600,
        .flags = VULKAN_FLAG_WIN_RESIZE | VULKAN_FLAG_DEBUG,
        .version = MAKE_VERSION(0, 1, 0),
        .render_settings = &render_settings
    };
    /* run vulkan rendering! */
    if(MSG_IS_ERROR(vulkanRun(&vulkan_info))) {
        return -1;
    }
    return 0;
}
