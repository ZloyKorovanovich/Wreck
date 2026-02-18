
#define INCLUDE_VULKAN_INTERNAL
#include "vulkan.h"


#ifdef _WIN32

    static void
    getMaxScreenSize(
        u32 *x, 
        u32 *y
    ) {
        *x = GetSystemMetrics(SM_CXFULLSCREEN);
        *y = GetSystemMetrics(SM_CYFULLSCREEN);
    }

    /* returns false if should close */
    static b32
    processWindow(
        Window window
    ) {
        /* 2nd param is NULL, it indicates that we peek 
                message from current Threads windows       */
        b32 is_running_window = TRUE;

        MSG win32_message = (MSG){0};
        while(PeekMessage(
            &win32_message, 
            NULL, 
            0, 
            0, 
            PM_REMOVE
        )) {
            is_running_window = (win32_message.message == WM_QUIT) ? FALSE : TRUE;
            TranslateMessage(&win32_message);
            DispatchMessage(&win32_message);
        }
        
        return is_running_window;
    }

#endif

/*  ===============================================================
        VULKAN MEMORY
    =============================================================== */

b32 
createVulkanMemory(
    const VulkanDevice *vulkan_device,
    VulkanMemory *vulkan_memory,
    MsgCallback_pfn msg_callback
) {
    *vulkan_memory = (VulkanMemory){.device = vulkan_device->device};
    vkGetPhysicalDeviceMemoryProperties(vulkan_device->physical_device, &vulkan_memory->memory_properties);
    return TRUE;
}

b32 
destroyVulkanMemory(
    const VulkanMemory *vulkan_memory,
    MsgCallback_pfn msg_callback
) {
    String log_str = STACK_STR(256);
    b32 result = TRUE;
    if(vulkan_memory->vram_allocation_count != 0) {
        stringPattern(
            &TRACED_STR("destroying vulkan memory struct, but active allocation count: %u32"),
            (const void *[]){&vulkan_memory->vram_allocation_count},
            &log_str
        );
        MSG_WARNING(msg_callback, &log_str);
        result = FALSE;
    }
    return result;
}

b32 allocateVram(
    VulkanMemory *vulkan_memory,
    const VramAllocationRequest *request,
    VramAllocation *allocation
) {
    const u32 memory_type_count = vulkan_memory->memory_properties.memoryTypeCount;
    VkMemoryType *memory_types = vulkan_memory->memory_properties.memoryTypes;
    VkMemoryHeap *memory_heaps = vulkan_memory->memory_properties.memoryHeaps;

    u32 memory_type_flags = 0;
    u32 memory_type_id = U32_MAX;
    u32 memory_heap_id = U32_MAX;
    for(u32 i = 0; i < memory_type_count; i++) {
        if(
            request->mandatory_flags ==  (memory_types[i].propertyFlags & request->mandatory_flags) &&
            !(memory_types[i].propertyFlags & request->restricted_flags) &&
            memory_heaps[memory_types[i].heapIndex].size > request->size
        ) {
            if(countBits_dword(memory_type_flags & request->negative_flags) >= countBits_dword(memory_types[i].propertyFlags & request->negative_flags)) {
                memory_type_id = i;
                memory_type_flags = memory_types[i].propertyFlags;
                memory_heap_id = memory_types[i].heapIndex;
                memory_heaps[memory_types[i].heapIndex].size -= request->size;
                continue;
            }
            if(countBits_qword(memory_type_flags & request->positive_flags) <= countBits_qword(memory_types[i].propertyFlags & request->positive_flags)) {
                memory_type_id = i;
                memory_type_flags = memory_types[i].propertyFlags;
                memory_heap_id = memory_types[i].heapIndex;
                memory_heaps[memory_types[i].heapIndex].size -= request->size;
                continue;
            }
        }
    }

    if(memory_type_id == U32_MAX) {
        return FALSE;
    }

    VkDeviceMemory device_memory = NULL;
    const VkMemoryAllocateInfo allocate_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = request->size,
        .memoryTypeIndex = memory_type_id
    };
    if(vkAllocateMemory(vulkan_memory->device, &allocate_info, NULL, &device_memory) != VK_SUCCESS) {
        return FALSE;
    }

    *allocation = (VramAllocation) {
        .memory = device_memory,
        .size = request->size,
        .heap_id = memory_heap_id,
        .type_id = memory_type_id
    };
    vulkan_memory->vram_allocation_count++;

    return TRUE;
}

b32 freeVram(
    VulkanMemory *vulkan_memory,
    VramAllocation *vram_allocation
) {
    if(!vram_allocation->memory) {
        return FALSE;
    }
    
    vkFreeMemory(vulkan_memory->device, vram_allocation->memory, NULL);
    vulkan_memory->memory_properties.memoryHeaps[vram_allocation->heap_id].size += vram_allocation->size;
    vulkan_memory->vram_allocation_count--;
    *vram_allocation = (VramAllocation){0};
    return TRUE;
}

/*  ===============================================================
        VULKAN SCREEN
    =============================================================== */
/* There are many things to say here. Most iportantly, why its dynamic ?
    Textures that we use for actual rendering (color and depth) are created once for biggest screen res,
    but swapchain needs to be recreated every time we resize the window.                                */
/* Rendering to separate texture and then transfering data into swapchain is crusial, because you ususally
    can not access surface texture as storage image or sampled image, so it is not possible to add any post-processing
    without rendering to a separate texture and then transfering to surface in someway. (potentially graphics pass blit will be most efficient) */

static b32
createSwapchain(
    const VulkanObjects *vulkan_objects,
    const VulkanDevice *device,
    MsgCallback_pfn msg_callback,
    VulkanScreen *vulkan_screen
) {
    u32 min_image_count = U32_MAX;
    VkExtent2D surface_extent = (VkExtent2D){0};
    VkSurfaceTransformFlagsKHR surface_transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;

    /* GET SURFACE INFO */ {
        VkSurfaceCapabilitiesKHR surface_capabilities = (VkSurfaceCapabilitiesKHR){0};
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device->physical_device, vulkan_objects->surface, &surface_capabilities);

        /* skip frames if size is 0 */
        while(surface_capabilities.currentExtent.width == 0 || surface_capabilities.currentExtent.height == 0) {
            /* dont forget to proccess events or everything will stall forever */
            if(!processWindow(vulkan_objects->window)) {
                MSG_LOG(msg_callback, &CONST_STRING("window closed while resizing"));
            }
            vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device->physical_device, vulkan_objects->surface, &surface_capabilities);
        }

        /* clamp image count */
        if(surface_capabilities.maxImageCount == 0) {
            min_image_count = MAX(4, surface_capabilities.minImageCount);
        } else {
            min_image_count = CLAMP(surface_capabilities.minImageCount, surface_capabilities.maxImageCount, 4);
        }

        /* check if image count is bigger than swaphcain image array */
        if(min_image_count > MAX_SWAPCHAIN_IMAGE_COUNT) {
            MSG_ERROR(msg_callback, &TRACED_STR("swapchain min image count is bigger than maximum possible swapchain images"));
            goto _preallocation_fail;
        }

        /* set surface info */
        surface_extent = surface_capabilities.currentExtent;
        surface_transform = surface_capabilities.currentTransform;
    }

    /* CREATE SWAPCHAIN */ {
        VkSwapchainKHR old_swapchain = vulkan_screen->swapchain;
        /* destroy old swapchain image views */
        if(old_swapchain) {
            const u32 swapchain_image_count = vulkan_screen->swapchain_image_count;
            VkImageView *swapchain_image_views = vulkan_screen->swapchain_views;
            for(u32 i = 0; i < swapchain_image_count; i++) {
                vkDestroyImageView(device->device, swapchain_image_views[i], NULL);
            }
        }

        /* create new swapchain using old if it existed */
        const VkSwapchainCreateInfoKHR swapchain_info = {
            .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .surface = vulkan_objects->surface,
            .imageFormat = device->screen_color_format,
            .imageColorSpace = device->screen_color_space,
            .presentMode = device->present_mode,
            .minImageCount = min_image_count,
            .imageExtent = surface_extent,
            .preTransform = surface_transform,
            .imageArrayLayers = 1,
            .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            .clipped = TRUE,
            .oldSwapchain = old_swapchain
        };
        if(vkCreateSwapchainKHR(device->device, &swapchain_info, NULL, & vulkan_screen->swapchain) != VK_SUCCESS) {
            MSG_ERROR(msg_callback, &TRACED_STR("failed to create swapchain"));
            goto _preallocation_fail;
        }

        /* detsroy old sapchain if it existed */
        if(old_swapchain) {
            vkDestroySwapchainKHR(device->device, old_swapchain, NULL);
        }
    }    

    /* CREATE IMAGES ARRAYS */ {
        /* made for simpler access */
        u32 swapchain_image_count = 0;
        VkImage *swapchain_images = vulkan_screen->swapchain_images;
        VkImageView *swapchain_views = vulkan_screen->swapchain_views;

        vkGetSwapchainImagesKHR(device->device, vulkan_screen->swapchain, &swapchain_image_count, NULL);
        /* check if actual swaphcain image count will not greater than what is allocated for array */
        if(swapchain_image_count > MAX_SWAPCHAIN_IMAGE_COUNT) {
            MSG_ERROR(msg_callback, &TRACED_STR("swapchain that was created has too many images"));
            goto _swapchain_images_fail;
        }
        
        /* write swaphcain image handles */
        vkGetSwapchainImagesKHR(device->device, vulkan_screen->swapchain, &swapchain_image_count, swapchain_images);
        /* dont forget to copy image count into segment struct */
        vulkan_screen->swapchain_image_count = swapchain_image_count;

        /* create image views */
        for(u32 i = 0; i < swapchain_image_count; i++) {
            const VkImageViewCreateInfo view_info = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .components.r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .components.g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .components.b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .components.a = VK_COMPONENT_SWIZZLE_IDENTITY,
                .subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .subresourceRange.baseMipLevel = 0,
                .subresourceRange.levelCount = 1,
                .subresourceRange.baseArrayLayer = 0,
                .subresourceRange.layerCount = 1,
                /* different part */
                .format = device->screen_color_format,
                .image = swapchain_images[i]
            };
            if(vkCreateImageView(device->device, &view_info, NULL, &swapchain_views[i]) != VK_SUCCESS) {
                MSG_ERROR(msg_callback, &TRACED_STR("failed to create swapchain image view"));
                swapchain_views[i] = NULL; /* mark it as empty, so that we can properly destroy prior to that */
                goto _swapchain_image_views_fail;
            }
        }
    }
    
    /* success */
    return TRUE;
    
    /* fails */
    _swapchain_image_views_fail: {
        VkImageView *swapchain_views = vulkan_screen->swapchain_views;
        for(u32 i = 0; i < MAX_SWAPCHAIN_IMAGE_COUNT; i++) {
            if(swapchain_views[i] == NULL) {
                break;
            }
            vkDestroyImageView(device->device, swapchain_views[i], NULL);
        }
    };
    _swapchain_images_fail: {
        vkDestroySwapchainKHR(device->device, vulkan_screen->swapchain, NULL);
    };
    return FALSE;

    _preallocation_fail: {
        return FALSE;
    };
}

static b32 
createScreenTextures(
    const VulkanObjects *vulkan_objects,
    const VulkanDevice *vulkan_device,
    VulkanMemory *vulkan_memory,
    MsgCallback_pfn msg_callback,
    VulkanScreen *screen_textures
) {
    String log_str = STACK_STR(256);

    *screen_textures = (VulkanScreen){0};
    
    u32 max_res_x = 0; 
    u32 max_res_y = 0;

    /* CALCULATE IMAGE SIZE */ {
        /* We allocate for the worst case, when the whole screen will be window.
            If size of windows is too big and its unsupported, screen textures size 
            will be clamped to maximum possible. Basically you will get rendering into
            smaller resolution and then transfer image to bigger swapchain texture.     */
        
        getMaxScreenSize(&max_res_x, &max_res_y);
        if(max_res_x == 0 || max_res_y == 0) {
            MSG_ERROR(msg_callback, &TRACED_STR("invalid system screen resolution x: 0, y: 0"));
            goto _prealloc_fail;
        }
        /* check if screen is scaled approperiately */
        if(max_res_x * max_res_y > SCREEN_MAX_PIXEL_COUNT) {
            stringPattern(
                &TRACED_STR("screen size is really big, render scale might be decreased x: %u32, y: %u32"), 
                (const void *[]){&max_res_x, &max_res_y},
                &log_str
            );
            MSG_WARNING(msg_callback, &log_str);
            max_res_x = MIN(MAX_RES_X, max_res_x);
            max_res_y = MIN(MAX_RES_Y, max_res_y);
        }
    }
    
    /* CREATE SCREEN IMAGES */ {
        /* We create images only once. If screen size differs from the image, we just use part of image in viewport */

        /* IMAGES */ {
            /* Keep in mind, that color_format is different from screen_color format. surface format might be (and likely is)
                not supported with tiling optimal and sampled/storage image.                                                    */
            const VkImageCreateInfo screen_color_info = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                .imageType = VK_IMAGE_TYPE_2D,
                .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                /* image data */
                .format = vulkan_device->color_format,
                .extent = (VkExtent3D) {
                    .width = max_res_x, 
                    .height = max_res_y, 
                    .depth = 1
                },
                /* additional */
                .tiling = VK_IMAGE_TILING_OPTIMAL,
                .mipLevels = 1,
                .arrayLayers = 1,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE
            };
            const VkImageCreateInfo screen_depth_info = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                .imageType = VK_IMAGE_TYPE_2D,
                .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                /* image data */
                .format = vulkan_device->depth_format,
                .extent = (VkExtent3D) {
                    .width = max_res_x, 
                    .height = max_res_y, 
                    .depth = 1
                },
                /* additional */
                .tiling = VK_IMAGE_TILING_OPTIMAL,
                .mipLevels = 1,
                .arrayLayers = 1,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE
            };

            /* create color image */
            if(vkCreateImage(vulkan_device->device, &screen_color_info, NULL, &screen_textures->color_image) != VK_SUCCESS) {
                MSG_ERROR(msg_callback, &TRACED_STR("failed to create screen color image"));
                goto _prealloc_fail;
            }
            /* create depth image */
            if(vkCreateImage(vulkan_device->device, &screen_depth_info, NULL, &screen_textures->depth_image) != VK_SUCCESS) {
                MSG_ERROR(msg_callback, &TRACED_STR("failed to create screen depth image"));
                goto _screen_depth_fail;
            }
        }

        /* ALLOCATE VRAM AND BIND IMAGES */ {
            VkMemoryRequirements screen_color_requirements = (VkMemoryRequirements){0};
            VkMemoryRequirements screen_depth_requirements = (VkMemoryRequirements){0};
            vkGetImageMemoryRequirements(vulkan_device->device, screen_textures->color_image, &screen_color_requirements);
            vkGetImageMemoryRequirements(vulkan_device->device, screen_textures->depth_image, &screen_depth_requirements);

            const u64 screen_color_offset = 0;
            const u64 screen_depth_offset = ALIGN(screen_color_requirements.size, screen_depth_requirements.alignment);

            VramAllocationRequest alloc_request = {
                .size = ALIGN(screen_color_requirements.size, screen_depth_requirements.alignment) + screen_depth_requirements.size,
                .memoryBits = screen_color_requirements.memoryTypeBits & screen_depth_requirements.memoryTypeBits
            };
            if(vulkan_device->memory_model == VULKAN_MEMORY_MODEL_HOST_DEVICE) {
                alloc_request.mandatory_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
                alloc_request.restricted_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
            }
            if(vulkan_device->memory_model == VULKAN_MEMORY_MODEL_FUSED_DEVICE) {
                alloc_request.mandatory_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
            }
            if(vulkan_device->memory_model == VULKAN_MEMORY_MODEL_FUSED_HOST) {
                alloc_request.mandatory_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
            }

            if(!allocateVram(vulkan_memory, &alloc_request, &screen_textures->vram_allocation)) {
                MSG_ERROR(msg_callback, &TRACED_STR("failed to allocate screen images vram"));
                goto _allocation_fail;
            }

            /* bind images to memory */
            if(vkBindImageMemory(vulkan_device->device, screen_textures->color_image, screen_textures->vram_allocation.memory, screen_color_offset) != VK_SUCCESS) {
                MSG_ERROR(msg_callback, &TRACED_STR("failed to bind screen color image memory"));
                goto _bind_fail;
            }
            if(vkBindImageMemory(vulkan_device->device, screen_textures->depth_image, screen_textures->vram_allocation.memory, screen_depth_offset) != VK_SUCCESS) {
                MSG_ERROR(msg_callback, &TRACED_STR("failed to bind screen color image memory"));
                goto _bind_fail;
            }
        }

        /* VIEWS */ {
            /* Create image views for textures only after binding memory. */

            const VkImageViewCreateInfo screen_color_view_info = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .image = screen_textures->color_image,
                .format = vulkan_device->color_format,
                .subresourceRange = (VkImageSubresourceRange) {
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT
                }
            };
            const VkImageViewCreateInfo screen_depth_view_info = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .image = screen_textures->depth_image,
                .format = vulkan_device->depth_format,
                .subresourceRange = (VkImageSubresourceRange) {
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                    .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT
                }
            };

            if(vkCreateImageView(vulkan_device->device, &screen_color_view_info, NULL, &screen_textures->color_view) != VK_SUCCESS) {
                MSG_ERROR(msg_callback, &TRACED_STR("failed to create screen color image view"));
                goto _screen_color_view_fail;
            }
            if(vkCreateImageView(vulkan_device->device, &screen_depth_view_info, NULL, &screen_textures->depth_view) != VK_SUCCESS) {
                MSG_ERROR(msg_callback, &TRACED_STR("failed to create screen depth image view"));
                goto _screen_depth_view_fail;
            }
        }        
    }
    
    /* CREATE SWAPCHAIN */
    if(!createSwapchain(vulkan_objects, vulkan_device, msg_callback, screen_textures)) {
        MSG_ERROR(msg_callback, &TRACED_STR("failed to cretae swapchain"));
        goto _swapchain_fail;
    }

    return TRUE;

    _swapchain_fail: {};
    vkDestroyImageView(vulkan_device->device, screen_textures->depth_view, NULL);
    _screen_depth_view_fail: {};
    vkDestroyImageView(vulkan_device->device, screen_textures->color_view, NULL);
    _bind_fail: {};
    freeVram(vulkan_memory, &screen_textures->vram_allocation);
    _allocation_fail: {};
    _screen_color_view_fail: {};
    vkDestroyImage(vulkan_device->device, screen_textures->depth_image, NULL);
    _screen_depth_fail: {};
    vkDestroyImage(vulkan_device->device, screen_textures->color_image, NULL);
    _prealloc_fail: {};
    return FALSE;
}


static b32
destroyScreenTextures(
    const VulkanObjects *vulkan_objects,
    const VulkanDevice *vulkan_device,
    VulkanMemory *vulkan_memory,
    MsgCallback_pfn msg_callback,
    VulkanScreen *vulkan_screen
) {
    b32 result = TRUE;

    if(!vulkan_screen->swapchain) {
        MSG_WARNING(msg_callback, &TRACED_STR("trying to destroy swapchain, but pointer is null"));
        result = FALSE;
    } else {
        const u32 image_count = vulkan_screen->swapchain_image_count;
        VkImageView *image_views = vulkan_screen->swapchain_views;
        for(u32 i = 0; i < image_count; i++) {
            vkDestroyImageView(vulkan_device->device, image_views[i], NULL);
        }
        vkDestroySwapchainKHR(vulkan_device->device, vulkan_screen->swapchain, NULL);
    }

    if(!vulkan_screen->color_view) {
        MSG_WARNING(msg_callback, &TRACED_STR("trying to destroy screen color view, but pointer is null"));
        result = FALSE;
    } else {
        vkDestroyImageView(vulkan_device->device, vulkan_screen->color_view, NULL);
    }
    if(!vulkan_screen->depth_view) {
        MSG_WARNING(msg_callback, &TRACED_STR("trying to destroy screen depth view, but pointer is null"));
        result = FALSE;
    } else {
        vkDestroyImageView(vulkan_device->device, vulkan_screen->depth_view, NULL);
    }

    if(!freeVram(vulkan_memory, &vulkan_screen->vram_allocation)) {
        MSG_WARNING(msg_callback, &TRACED_STR("failed to free screen images vram"));
        result = FALSE;
    }
    
    if(!vulkan_screen->color_image) {
        MSG_WARNING(msg_callback, &TRACED_STR("trying to destroy screen color image, but pointer is null"));
        result = FALSE;
    } else {
        vkDestroyImage(vulkan_device->device, vulkan_screen->color_image, NULL);
    }
    if(!vulkan_screen->depth_image) {
        MSG_WARNING(msg_callback, &TRACED_STR("trying to destroy screen depth image, but pointer is null"));
        result = FALSE;
    } else {
        vkDestroyImage(vulkan_device->device, vulkan_screen->depth_image, NULL);
    }

    *vulkan_screen = (VulkanScreen){0};
    return result;
}


/*  ===============================================================
        VULKAN PIPELINES
    =============================================================== */

static VkPipeline
createGraphicsPipeline(
    VkDevice device,
    VkShaderModule vertex_shader,
    VkShaderModule fragment_shader,
    VkPipelineLayout pipeline_layout,
    u32 color_format_count,
    const VkFormat *color_formats,
    const VkFormat depth_format,
    VulkanShaderFlags flags
) {
    VkPipelineShaderStageCreateInfo shader_stages[] = {
        (VkPipelineShaderStageCreateInfo) {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pName = SHADER_VERTEX_ENTRY,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vertex_shader
        },
        (VkPipelineShaderStageCreateInfo) {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pName = SHADER_FRAGMENT_ENTRY,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = fragment_shader
        }
    };

    /* these states will be changed on fly */
    VkPipelineDynamicStateCreateInfo dynamic_state = (VkPipelineDynamicStateCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2,
        .pDynamicStates = (const VkDynamicState[]){VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR}
    };

    u32 vertex_binding_count = 0;
    VkVertexInputBindingDescription vertex_bindings[1] = {
        (VkVertexInputBindingDescription) {
            .binding = 0,
            .stride = sizeof(Vertex),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
        }
    };

    u32 vertex_attribute_count = 0;
    VkVertexInputAttributeDescription vertex_attributes[3] = {0};

    if(flags & VULKAN_SHADER_FLAG_USE_VERTEX_POSITION) {
        vertex_binding_count = 1;
        vertex_attributes[vertex_attribute_count++] = (VkVertexInputAttributeDescription) {
            .binding = 0,
            .location = 0,
            .format = VK_FORMAT_R32G32B32A32_SFLOAT,
            .offset = 0
        };
    }
    if(flags & VULKAN_SHADER_FLAG_USE_VERTEX_NORMAL) {
        vertex_binding_count = 1;
        vertex_attributes[vertex_attribute_count++] = (VkVertexInputAttributeDescription) {
            .binding = 0,
            .location = 1,
            .format = VK_FORMAT_R32G32B32A32_SFLOAT,
            .offset = 16
        };
    }
    if(flags & VULKAN_SHADER_FLAG_USE_VERTEX_TEXCOORD) {
        vertex_binding_count = 1;
        vertex_attributes[vertex_attribute_count++] = (VkVertexInputAttributeDescription) {
            .binding = 0,
            .location = 2,
            .format = VK_FORMAT_R32G32B32A32_SFLOAT,
            .offset = 32
        };
    }

    VkPipelineVertexInputStateCreateInfo vertex_input_state = (VkPipelineVertexInputStateCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexAttributeDescriptionCount = vertex_attribute_count,
        .pVertexAttributeDescriptions = vertex_attributes,
        .vertexBindingDescriptionCount = vertex_binding_count,
        .pVertexBindingDescriptions = vertex_bindings
    };
    
    /* settings for gpu pipeline */
    VkPipelineInputAssemblyStateCreateInfo input_assembly_state = (VkPipelineInputAssemblyStateCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = FALSE
    };
    VkPipelineRasterizationStateCreateInfo rasterization_state = (VkPipelineRasterizationStateCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = FALSE,
        .rasterizerDiscardEnable = FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .lineWidth = 1.0f,
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_CLOCKWISE,
        .depthBiasEnable = FALSE,
        .depthBiasConstantFactor = 0.0f,
        .depthBiasClamp = 0.0f,
        .depthBiasSlopeFactor = 0.0f
    };

    VkPipelineMultisampleStateCreateInfo multisample_state = (VkPipelineMultisampleStateCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .sampleShadingEnable = FALSE,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .minSampleShading = 1.0f,
        .pSampleMask = NULL,
        .alphaToCoverageEnable = FALSE,
        .alphaToOneEnable = FALSE
    };
    VkPipelineColorBlendAttachmentState color_blend_attachment_state = (VkPipelineColorBlendAttachmentState) {
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        .blendEnable = VK_FALSE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp = VK_BLEND_OP_ADD
    };
    VkPipelineColorBlendStateCreateInfo color_blend_state = (VkPipelineColorBlendStateCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount = 1,
        .pAttachments = &color_blend_attachment_state,
        .blendConstants[0] = 0.0f,
        .blendConstants[1] = 0.0f,
        .blendConstants[2] = 0.0f,
        .blendConstants[3] = 0.0f
    };
    VkPipelineDepthStencilStateCreateInfo depth_stencil_state = (VkPipelineDepthStencilStateCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = TRUE,
        .depthWriteEnable = TRUE,
        .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL
    };
    VkPipelineRenderingCreateInfoKHR rendering_create_info = (VkPipelineRenderingCreateInfoKHR) {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
        .colorAttachmentCount = color_format_count,
        .pColorAttachmentFormats = color_formats,
        .depthAttachmentFormat = depth_format
    };
    /* dynamic, will be replaced */
    VkPipelineViewportStateCreateInfo viewport_state = (VkPipelineViewportStateCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1
    };

    VkGraphicsPipelineCreateInfo pipeline_info = (VkGraphicsPipelineCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .basePipelineHandle = NULL,
        .basePipelineIndex = -1,
        .stageCount = 2,
        .pStages = shader_stages,
        .pVertexInputState = &vertex_input_state,
        .pInputAssemblyState = &input_assembly_state,
        .pViewportState = &viewport_state,
        .pRasterizationState = &rasterization_state,
        .pMultisampleState = &multisample_state,
        .pDepthStencilState = &depth_stencil_state,
        .pColorBlendState = &color_blend_state,
        .pDynamicState = &dynamic_state,
        .layout = pipeline_layout,
        .renderPass = NULL,
        .pNext = &rendering_create_info
    };

    VkPipeline pipeline = NULL;
    if(vkCreateGraphicsPipelines(device, NULL, 1, &pipeline_info, NULL, &pipeline) != VK_SUCCESS) {
        return NULL;
    }
    return pipeline;
}

static VkPipeline
createComputePipeline(
    VkDevice device,
    VkShaderModule compute_shader,
    VkPipelineLayout pipeline_layout,
    VulkanShaderFlags flags
) {
    const VkComputePipelineCreateInfo compute_pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .basePipelineIndex = -1,
        .stage = (VkPipelineShaderStageCreateInfo) {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .module = compute_shader,
            .pName = SHADER_COMPUTE_ENTRY,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT
        },
        .layout = pipeline_layout
    };
    VkPipeline pipeline = NULL;
    if(vkCreateComputePipelines(device, NULL, 1, &compute_pipeline_info, NULL, &pipeline) != VK_SUCCESS) {
        return NULL;
    }
    return pipeline;
}

static b32 
createPipelines(
    const VulkanDevice *vulkan_device,
    u32 graphics_pipeline_count,
    u32 compute_pipeline_count,
    const VulkanGraphicsPipelineInfo *graphics_pipeline_infos,   
    const VulkanComputePipelineInfo *compute_pipeline_infos,
    MsgCallback_pfn msg_callback,
    VulkanPipelines *vulkan_pipelines,
    void *resource_shaders
) {
    String log_str = STACK_STR(512);
    const char *empty_name = " ";

    *vulkan_pipelines = (VulkanPipelines) {0};
    if(graphics_pipeline_count == 0 && compute_pipeline_count == 0) { 
        MSG_LOG(msg_callback, &CONST_STRING("no vulkan pipelines created"));
        goto _success;
    }
    if(graphics_pipeline_count != 0) {
        vulkan_pipelines->graphics_pipelines = (VkPipeline *)resource_shaders;
        vulkan_pipelines->graphics_pipeline_count = graphics_pipeline_count;
    }
    if(compute_pipeline_count != 0) {
        vulkan_pipelines->compute_pipelines = (VkPipeline *)resource_shaders + graphics_pipeline_count;
        vulkan_pipelines->compute_pipeline_count = compute_pipeline_count;
    }

    VkPipelineLayoutCreateInfo pipeline_layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO
    };
    if(vkCreatePipelineLayout(vulkan_device->device, &pipeline_layout_info, NULL, &vulkan_pipelines->pipeline_layout) != VK_SUCCESS) {
        MSG_ERROR(msg_callback, &TRACED_STR("failed to create pipeline layout"));
        goto _prealloc_fail;
    }

    u32 graphics_pipelines_created = 0;
    u32 compute_pipelines_created = 0;
    VkPipeline *graphics_pipelines = vulkan_pipelines->graphics_pipelines;
    VkPipeline *compute_pipelines = vulkan_pipelines->compute_pipelines;

    if(graphics_pipeline_count != 0) {
        for(u32 i = 0; i < graphics_pipeline_count; i++) {
            const char *shader_name = graphics_pipeline_infos[i].name;
            graphics_pipelines[i] = NULL;

            /* VALIDATION */ {
                if(!graphics_pipeline_infos[i].name) {
                    stringPattern(&TRACED_STR("unnamed graphics pipeline at id: %u32"), (const void *[]){&i}, &log_str);
                    MSG_WARNING(msg_callback, &log_str);
                    shader_name = empty_name;
                }
                if(
                    !graphics_pipeline_infos[i].vertex_spv             || 
                    !graphics_pipeline_infos[i].fragment_spv           || 
                    graphics_pipeline_infos[i].vertex_spv_size == 0    || 
                    graphics_pipeline_infos[i].fragment_spv_size == 0
                ) {
                    stringPattern(
                        &TRACED_STR(
                            "graphics pipeline has invalid data {\n"
                            "\tid: %u32\n"
                            "\tname: \"%c\"\n"
                            "\tvertex shader: %p\n"
                            "\tfragment shader: %p\n"
                            "\tvertex shader size: %u64\n"
                            "\tfragment shader size: %u64\n"
                            "}"
                        ),
                        (const void *[]) {
                            &i,
                            shader_name,
                            &graphics_pipeline_infos[i].vertex_spv,
                            &graphics_pipeline_infos[i].fragment_spv,
                            &graphics_pipeline_infos[i].vertex_spv_size,
                            &graphics_pipeline_infos[i].fragment_spv_size
                        },
                        &log_str
                    );
                    MSG_ERROR(msg_callback, &log_str);
                    goto _graphics_pipeline_validation_fail;
                }
            }

            VkShaderModule vertex_shader = NULL;
            VkShaderModule fragment_shader = NULL;

            /* COMPILE TO NATIVE */ {
                const VkShaderModuleCreateInfo vertex_shader_info = {
                    .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                    .pCode = graphics_pipeline_infos[i].vertex_spv,
                    .codeSize = graphics_pipeline_infos[i].vertex_spv_size
                };
                const VkShaderModuleCreateInfo fragment_shader_info = {
                    .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                    .pCode = graphics_pipeline_infos[i].fragment_spv,
                    .codeSize = graphics_pipeline_infos[i].fragment_spv_size
                };

                if(vkCreateShaderModule(vulkan_device->device, &vertex_shader_info, NULL, &vertex_shader) != VK_SUCCESS) {
                    stringPattern(
                        &TRACED_STR(
                            "failed to compile graphics vertex shader {\n"
                            "\tid: %u32\n"
                            "\tname: \"%c\"\n"
                            "}"
                        ),
                        (const void *[]) {&i, shader_name},
                        &log_str
                    );
                    MSG_ERROR(msg_callback, &log_str);

                    goto _vertex_shader_fail;
                }

                if(vkCreateShaderModule(vulkan_device->device, &fragment_shader_info, NULL, &fragment_shader) != VK_SUCCESS) {
                    stringPattern(
                        &TRACED_STR(
                            "failed to compile graphics fragment shader {\n"
                            "\tid: %u32\n"
                            "\tname: \"%c\"\n"
                            "}"
                        ),
                        (const void *[]) {&i, shader_name},
                        &log_str
                    );
                    MSG_ERROR(msg_callback, &log_str);

                    goto _fragment_shader_fail;
                }
            }
            
            /* CREATE PIPELINE */ {
                graphics_pipelines[i] = createGraphicsPipeline(
                    vulkan_device->device,
                    vertex_shader,
                    fragment_shader,
                    vulkan_pipelines->pipeline_layout,
                    1, &vulkan_device->color_format,
                    vulkan_device->depth_format,
                    graphics_pipeline_infos[i].flags
                );
                if(!graphics_pipelines[i]) {
                    stringPattern(
                        &TRACED_STR(
                            "failed to create graphics pipeline {\n"
                            "\tid: %u32\n"
                            "\tname: \"%c\"\n"
                            "\tflags: %u32\n"
                            "}"
                        ),
                        (const void *[]) {&i, shader_name, &graphics_pipeline_infos[i].flags},
                        &log_str
                    );
                    MSG_ERROR(msg_callback, &log_str);

                    goto _graphics_pipeline_fail;
                }
            }

            vkDestroyShaderModule(vulkan_device->device, vertex_shader, NULL);
            vkDestroyShaderModule(vulkan_device->device, fragment_shader, NULL);

            /* LOG */ {
                stringPattern(&CONST_STRING("created graphics pipeline name: \"%c\" id: %u32"), (const void *[]){shader_name, &i}, &log_str);
                MSG_LOG(msg_callback, &log_str);
            }

            /* all good, create next shader program */
            graphics_pipelines_created++;
            continue;
            
            _graphics_pipeline_fail: {
                vkDestroyShaderModule(vulkan_device->device, fragment_shader, NULL);
            }
            _fragment_shader_fail: {
                vkDestroyShaderModule(vulkan_device->device, vertex_shader, NULL);
            }
            _vertex_shader_fail: {};
            _graphics_pipeline_validation_fail: {
                goto _graphics_pipelines_creation_fail;
            }
        }
    } else {
        MSG_LOG(msg_callback, &CONST_STRING("no graphics pipelines created"));
    }

    if(compute_pipeline_count != 0) {
        for(u32 i = 0; i < compute_pipeline_count; i++) {
            const char *shader_name = compute_pipeline_infos[i].name;

            /* VALIDATE */ {
                if(!compute_pipeline_infos[i].name) {
                    stringPattern(
                        &TRACED_STR("unnamed compute pipeline at id: %u32"),
                        (const void *[]){&i},
                        &log_str
                    );
                    MSG_WARNING(msg_callback, &log_str);
                    shader_name = " ";
                }
                if(!compute_pipeline_infos[i].compute_spv || compute_pipeline_infos[i].compute_spv_size == 0) {
                    stringPattern(
                        &TRACED_STR(
                            "graphics pipeline has invalid data {\n"
                            "\tid: %u32\n"
                            "\tname: \"%c\"\n"
                            "\tcompute shader: %p\n"
                            "\tcompute shader size: %u64\n"
                            "}"
                        ),
                        (const void *[]){
                            &i,
                            shader_name,
                            &compute_pipeline_infos[i].compute_spv,
                            &compute_pipeline_infos[i].compute_spv_size
                        },
                        &log_str
                    );
                    goto _compute_pipeline_validation_fail;
                }
            }

            VkShaderModule compute_shader = NULL;
            /* COMPILE TO NATIVE */ {
                const VkShaderModuleCreateInfo shader_module_info = {
                    .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                    .pCode = compute_pipeline_infos[i].compute_spv,
                    .codeSize = compute_pipeline_infos[i].compute_spv_size
                };
                if(vkCreateShaderModule(vulkan_device->device, &shader_module_info, NULL, &compute_shader) != VK_SUCCESS) {
                    stringPattern(
                        &TRACED_STR(
                            "failed to compile compute shader {\n"
                            "\tid: %u32\n"
                            "\tname: \"%c\"\n"
                            "}"
                        ),
                        (const void *[]) {&i, shader_name},
                        &log_str
                    );
                    MSG_ERROR(msg_callback, &log_str);

                    goto _compute_shader_fail;
                }
            }

            /* CREATE PIPELINE */ {
                compute_pipelines[i] = createComputePipeline(
                    vulkan_device->device,
                    compute_shader,
                    vulkan_pipelines->pipeline_layout,
                    compute_pipeline_infos[i].flags
                );

                if(!compute_pipelines[i]) {
                    stringPattern(
                        &TRACED_STR(
                            "failed to create graphics pipeline {\n"
                            "\tid: %u32\n"
                            "\tname: \"%c\"\n"
                            "\tflags: %u32\n"
                            "}"
                        ),
                        (const void *[]) {&i, shader_name, &compute_pipeline_infos[i].flags},
                        &log_str
                    );
                    MSG_ERROR(msg_callback, &log_str);
                    goto _compute_pipeline_fail;
                }

            }
            
            _compute_pipeline_fail: {
                vkDestroyShaderModule(vulkan_device->device, compute_shader, NULL);
            }
            _compute_shader_fail: {};
            _compute_pipeline_validation_fail: {
                goto _compute_pipelines_creation_fail;
            }
        }
    } else {
        MSG_LOG(msg_callback, &CONST_STRING("no compute pipelines created"));
    }

    _success: {
        return TRUE;
    }

    _compute_pipelines_creation_fail: {
        for(u32 i = 0; i < compute_pipelines_created; i++) {
            vkDestroyPipeline(vulkan_device->device, compute_pipelines[i], NULL);
        }
    }
    _graphics_pipelines_creation_fail: {
        for(u32 i = 0; i < graphics_pipelines_created; i++) {
            vkDestroyPipeline(vulkan_device->device, graphics_pipelines[i], NULL);
        }
    }
    _prealloc_fail: {
        return FALSE;
    }
}

static b32
destroyPipelines(
    const VulkanDevice *vulkan_device,
    VulkanPipelines *vulkan_pipelines,
    MsgCallback_pfn msg_callback
) {
    String log_str = STACK_STR(256);
    VkDevice device = vulkan_device->device;
    b32 result = TRUE;

    const u32 graphics_pipeline_count = vulkan_pipelines->graphics_pipeline_count;
    const u32 compute_pipeline_count = vulkan_pipelines->compute_pipeline_count;
    VkPipeline *graphics_pipelines = vulkan_pipelines->graphics_pipelines;
    VkPipeline *compute_pipelines = vulkan_pipelines->compute_pipelines;
    for(u32 i = 0; i < graphics_pipeline_count; i++) {
        if(graphics_pipelines[i]) {
            vkDestroyPipeline(device, graphics_pipelines[i], NULL);
        } else {
            stringPattern(&TRACED_STR("trying to destroy graphics pipeline that is null id: %u32"), (const void *[]){&i}, &log_str);
            MSG_WARNING(msg_callback, &log_str);
            result = FALSE;
        }
    }
    for(u32 i = 0; i < compute_pipeline_count; i++) {
        if(compute_pipelines[i]) {
            vkDestroyPipeline(device, compute_pipelines[i], NULL);
        } else {
            stringPattern(&TRACED_STR("trying to destroy compute pipeline that is null id: %u32"), (const void *[]){&i}, &log_str);
            MSG_WARNING(msg_callback, &log_str);
            result = FALSE;
        }
    }

    if(vulkan_pipelines->pipeline_layout) {
        vkDestroyPipelineLayout(device, vulkan_pipelines->pipeline_layout, NULL);
    } else {
        MSG_WARNING(msg_callback, &TRACED_STR("trying to detsroy vulkan pipeline layout, but it is null"));
        result = FALSE;
    }

    return result;
}

/*  ===============================================================
        EXTERN INTERFACE
    =============================================================== */

b32 createVulkanDynamic(
    VulkanHandle vulkan, 
    const CreateVulkanDynamicIn *input
) {
    const VulkanSegment *vulkan_segment = (VulkanSegment *)vulkan;
    const VulkanObjects *vulkan_objects = (VulkanObjects *)vulkan_segment->vulkan_objects;
    const VulkanDevice *vulkan_device = (VulkanDevice *)vulkan_segment->vulkan_device;
    MsgCallback_pfn msg_callback = vulkan_objects->msg_callback;

    VulkanMemory *vulkan_memory = (VulkanMemory *)vulkan_segment->vulkan_memory;
    VulkanScreen *vulkan_screen = (VulkanScreen *)vulkan_segment->vulkan_screen;

    if(!createVulkanMemory(
        vulkan_device,
        vulkan_memory,
        msg_callback
    )) {
        MSG_ERROR(msg_callback, &TRACED_STR("failed to create vulkan memory"));
        goto _critical_fail;
    }

    if(!createScreenTextures(
        vulkan_objects, 
        vulkan_device,
        vulkan_memory,
        msg_callback, 
        vulkan_screen
    )) {
        MSG_ERROR(msg_callback, &TRACED_STR("failed to create vulkan screen textures"));
        goto _critical_fail;
    }

    if(!createPipelines(
        vulkan_device,
        input->graphics_shader_count,
        input->compute_shader_count,
        input->graphics_shaders,
        input->compute_shaders,
        msg_callback,
        vulkan_segment->vulkan_pipelines,
        vulkan_segment->resource_shaders
    )) {
        MSG_ERROR(msg_callback, &TRACED_STR("failed to create vulkan pipelines"));
        goto _critical_fail;
    }

    return TRUE;

    _critical_fail: {
        return FALSE;
    }
}

b32 destroyVulkanDynamic(
    VulkanHandle vulkan
) {
    const VulkanSegment *vulkan_segment = (VulkanSegment *)vulkan;
    const VulkanObjects *vulkan_objects = (VulkanObjects *)vulkan_segment->vulkan_objects;
    const VulkanDevice *vulkan_device = (VulkanDevice *)vulkan_segment->vulkan_device;
    MsgCallback_pfn msg_callback = vulkan_objects->msg_callback;

    VulkanMemory *vulkan_memory = (VulkanMemory *)vulkan_segment->vulkan_memory;
    VulkanScreen *vulkan_screen = (VulkanScreen *)vulkan_segment->vulkan_screen;
    VulkanPipelines *vulkan_pipelines = (VulkanPipelines *)vulkan_segment->vulkan_pipelines;
    b32 result = TRUE;

    if(!destroyPipelines(
        vulkan_device,
        vulkan_pipelines,
        msg_callback
    )) {
        MSG_WARNING(msg_callback, &TRACED_STR("failed to destroy vulkan pipelines"));
    }

    if(!destroyScreenTextures(
        vulkan_objects, 
        vulkan_device, 
        vulkan_memory,
        msg_callback, 
        vulkan_screen
    )) {
        MSG_WARNING(msg_callback, &TRACED_STR("failed to destroy screen vulkan screen textures"));
        result = FALSE;
    }

    if(!destroyVulkanMemory(
        vulkan_memory,
        msg_callback
    )) {
        MSG_WARNING(msg_callback, &TRACED_STR("failed to destroy vulkan memory"));
        result = FALSE;
    }

    return result;
}

b32
runVulkanLoop(
    VulkanHandle vulkan
) {
    const VulkanSegment *vulkan_segment = (VulkanSegment *)vulkan;
    const VulkanObjects *vulkan_objects = (VulkanObjects *)vulkan_segment->vulkan_objects;

    while(processWindow(vulkan_objects->window)) {
        
    }

    return TRUE;
}
