#include "main.h"

void msgCallback(i32 code, const String *msg) {
    String print_str = (String) {
        .string = (char[512]){0},
        .capacity = 512
    };

    if(!msg) {
        printConsole(&CONST_STRING("got bad message"));
        return;
    }

    /* log message */
    if(code == 0) {
        stringPattern(&CONST_STRING(":: %s\n"), (const void *[]){msg}, &print_str);
        printConsole(&print_str);
    }
    /* warning message */
    if(code > 0) {
        stringPattern(&CONST_STRING(":? %s\n"), (const void *[]){msg}, &print_str);
        printConsole(&print_str);
    }
    /* error message */
    if(code < 0) {
        stringPattern(&CONST_STRING(":! %s\n"), (const void *[]){msg}, &print_str);
        errorConsole(&print_str);
    }
}

static Arena s_context_arena = (Arena){0};

void *allocateContext(u64 size, u64 aligment) {
    return allocateArena(&s_context_arena, size, aligment);
}

i32 main(i32 argc, char **argv) {
    if(!createArena(&s_context_arena, 1024 * 1024 * 64, 1024 * 1024 * 4)) {
        MSG_ERROR(msgCallback, &TRACED_STR("failed to create s_context_arena allocator"));
        return -1;
    }

    VulkanContextInfo vulkan_info = {
        .name = CONST_STRING("Wreck"),
        .resolution_x = 800,
        .resolution_y = 600,
        .flags = VULKAN_FLAG_RESIZABLE | VULKAN_FLAG_DEBUG,
        .msg_callback = &msgCallback
    };
    VulkanContext *vulkan_context = createVulkanContext(&allocateContext, &vulkan_info);

    RenderContextInfo render_info = {
        .vulkan_context = vulkan_context,
        .msg_callback = &msgCallback
    };
    RenderContext *render_context = createRenderContext(&allocateContext, &render_info);

    destroyRenderContext(render_context);
    destroyVulkanContext(vulkan_context);

    if(!freeArena(&s_context_arena)) {
        MSG_ERROR(msgCallback, &TRACED_STR("failed to free s_context_arena allcoator"))
    }
}
