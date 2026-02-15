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
        .flags = VULKAN_IN_FLAG_DEBUG | VULKAN_IN_FLAG_RESIZE | VULKAN_IN_PROTECT_MEMORY,
        .x = 800,
        .y = 600
    };
    CreateVulkanOut create_vulkan_out = (CreateVulkanOut){0};

    VulkanHandle vulkan = createVulkan(&create_vulkan_in, &create_vulkan_out);
    if(!vulkan) {
        MSG_ERROR(msgCallback, &TRACED_STR("failed to create vulkan"));
        return -1;
    }
    
    if(!createVulkanDynamic(vulkan)) {
        MSG_ERROR(msgCallback, &TRACED_STR("failed to create dynamic vulkan"));
        return -1;
    }

    if(!runVulkanLoop(vulkan)) {
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
