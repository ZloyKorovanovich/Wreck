#include "main.h"

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
    float screen_params[4];
    float time_params[4];
} UniformBuffer;

i32 main(i32 argc, char** argv) {
    VulkanContext* vulkan_context = NULL;
    RenderContext* render_context = NULL;
    /* fill everything */
    const VulkanContextInfo vulkan_info = {
        .name = "Wreck Demo",
        .x = 800,
        .y = 600,
        .flags = VULKAN_FLAG_WIN_RESIZE | VULKAN_FLAG_DEBUG,
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
            .type = RENDER_RESOURCE_TYPE_UNIFORM_BUFFER,
            .size = sizeof(UniformBuffer)
        },
        (RenderResourceInfo) {
            .binding = 1, .set = 1,
            .mutability = RENDER_RESOURCE_HOST_IMMUTABLE,
            .type = RENDER_RESOURCE_TYPE_UNIFORM_BUFFER,
            .size = sizeof(UniformBuffer)
        }
    };

    const RenderContextInfo render_info = (RenderContextInfo) {
        .resource_count = ARRAY_COUNT(resource_infos),
        .resource_infos = resource_infos
    };
    /* run vulkan rendering! */
    if(MSG_IS_ERROR(createVulkanContext(&vulkan_info, &msgCallback, &vulkan_context))) return -1;
    if(MSG_IS_ERROR(createRenderContext(vulkan_context, &render_info, &msgCallback, &render_context))) return -2;
    if(MSG_IS_ERROR(destroyRenderContext(vulkan_context, msgCallback, render_context))) return -3;
    if(MSG_IS_ERROR(destroyVulkanContext(&msgCallback, vulkan_context))) return -4;
}
