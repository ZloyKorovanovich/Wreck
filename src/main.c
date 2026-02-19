#include <base.h>
#include "vulkan/vulkan.h"

void 
msgCallback(
    i32 code, 
    const String *msg
) {
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

u64 
readShaderFile(
    const char *name, 
    Buffer *buffer
) {
    u64 file_size = fileToBuffer(&CONST_STRING(name), buffer);
    if(file_size > buffer->size || file_size == 0) {
        return 0;
    }
    return file_size;
}

b32
renderLoop(
    VulkanRenderCmd *cmd
) {
    /* SCREEN RENDER */ {
        const u32 color_attachments[] = {IMAGE_SCREEN_COLOR_ID};
        const u32 depth_attachment = IMAGE_SCREEN_DEPTH_ID;

        cmdBeginRendering(cmd, ARRAY_SIZE(color_attachments), color_attachments, depth_attachment);
        cmdEndRendering(cmd);
    }
    return TRUE;
}

i32 
main(
    i32 argc, 
    char **argv
) {
    void *virtual_allocation = VirtualAlloc(NULL, 1024 * 64, MEM_RESERVE, PAGE_READWRITE);
    if(!virtual_allocation) {
        MSG_ERROR(msgCallback, &TRACED_STR("failed to allocate virtual memory"));
        return -1;
    }

    Segment segment = {virtual_allocation, (byte *)virtual_allocation + 1024 * 64};

    CreateVulkanIn create_vulkan_in = {
        .msg_callback = msgCallback,
        .segment = &segment,
        .name = "Wreck 3D",
        .flags = VULKAN_IN_FLAG_DEBUG | VULKAN_IN_FLAG_RESIZE,
        .x = 800,
        .y = 600
    };

    Buffer vertex_shader = (Buffer){.buffer = (char[1024 * 4]){0}, .size = 1024 * 4};
    Buffer fragment_shader = (Buffer){.buffer = (char[1024 * 4]){0}, .size = 1024 * 4};
    u64 vertex_size = 0;
    u64 fragment_size = 0;
    /* TEST LOAD SHADERS */ {
        vertex_size = readShaderFile("out/data/triangle_v.spv", &vertex_shader);
        fragment_size = readShaderFile("out/data/triangle_f.spv", &fragment_shader);
        if(vertex_size == 0 || fragment_size == 0) {
            MSG_ERROR(msgCallback, &TRACED_STR("failed to load shaders to buffers"));
            return -1;
        }
    }
    CreateVulkanOut create_vulkan_out = (CreateVulkanOut){0};
    CreateVulkanDynamicIn create_vulkan_dynamic_in = {
        .graphics_shaders = (VulkanGraphicsPipelineInfo[]) {
            (VulkanGraphicsPipelineInfo) {
                .name = "traingle",
                .vertex_spv = vertex_shader.buffer,
                .fragment_spv = fragment_shader.buffer,
                .vertex_spv_size = vertex_size,
                .fragment_spv_size = fragment_size
            }
        },
        .graphics_shader_count = 1
    };

    VulkanHandle vulkan = createVulkan(&create_vulkan_in, &create_vulkan_out);
    if(!vulkan) {
        MSG_ERROR(msgCallback, &TRACED_STR("failed to create vulkan"));
        return -1;
    }

    if(!createVulkanDynamic(vulkan, &create_vulkan_dynamic_in)) {
        MSG_ERROR(msgCallback, &TRACED_STR("failed to create dynamic vulkan"));
        return -1;
    }

    if(!runVulkanLoop(vulkan, renderLoop, msgCallback)) {
        MSG_ERROR(msgCallback, &TRACED_STR("failed to run vulkan loop"));
        return -1;
    }

    if(!destroyVulkanDynamic(vulkan)) {
        MSG_ERROR(msgCallback, &TRACED_STR("failed to destroy dynamic vulkan"));
        return -1;
    }

    if(!destroyVulkan(vulkan)) {
        MSG_ERROR(msgCallback, &TRACED_STR("failed to destroy vulkan"));
        return -1;
    }

    if(!VirtualFree(virtual_allocation, 0, MEM_RELEASE)) {
        MSG_ERROR(msgCallback, &TRACED_STR("failed to free virtual memory"));
        return -1;
    }

    return 0;
}
