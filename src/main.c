#include "main.h"
#include "vk/vk.h"

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

i32 main(i32 argc, char** argv) {
    const RenderSettings render_settings = {
        .bindings = (RenderBinding[]){ 
            (RenderBinding) {0, 0, RENDER_BINDING_TYPE_UNIFORM_BUFFER}
        },
        .binding_count = 1,
        .nodes = (RenderNode[]) {
            (RenderNode) {
                .type = RENDER_NODE_TYPE_GRAPHICS,
                .vertex_shader = "out/data/triangle_v.spv",
                .fragment_shader = "out/data/triangle_f.spv"
            }
        },
        .node_count = 1
    };
    /* fill everything */
    const VulkanInfo vulkan_info = {
        .msg_callback = &msgCallback,
        .name = "Wreck Demo",
        .x = 800,
        .y = 600,
        .flags = VULKAN_FLAG_DEBUG,
        .version = MAKE_VERSION(0, 1, 0),
        .render_settings = &render_settings
    };
    /* run vulkan rendering! */
    if(MSG_IS_ERROR(vulkanRun(&vulkan_info))) {
        return -1;
    }
    return 0;
}

