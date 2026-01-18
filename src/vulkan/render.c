#include "vulkan.h"

b32 configureRenderSettings(const VulkanContext *vulkan_context, Stack* stack, MsgCallback_pfn msg_callback, RenderSettings *settings) {
    pushStack(stack);
    
    /* SURFACE FORMAT */ {
        u32 surface_format_count = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(vulkan_context->physical_device, vulkan_context->surface, &surface_format_count, NULL);
        if(surface_format_count == 0) {
            MSG_ERROR(msg_callback, &TRACED_STR("no surface formats available"));
            return FALSE;
        }
        VkSurfaceFormatKHR *surface_formats = allocateStack(stack, surface_format_count * sizeof(VkSurfaceFormatKHR), 16);
        if(!surface_formats) {
            MSG_ERROR(msg_callback, &TRACED_STR("failed to allocate surface_formats format array"));
            return FALSE;
        }
        vkGetPhysicalDeviceSurfaceFormatsKHR(vulkan_context->physical_device, vulkan_context->surface, &surface_format_count, surface_formats);

        settings->color_space = surface_formats[0].colorSpace;
        settings->color_format = surface_formats[0].format;
        for(u32 i = 0; i < surface_format_count; i++) {
            if(surface_formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR && surface_formats[i].format == VK_FORMAT_B8G8R8A8_SRGB) {
                settings->color_space = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
                settings->color_format = VK_FORMAT_B8G8R8A8_SRGB;
                break;
            }
        }
    }

    /* DEPTH FORMAT */ {
        const VkFormat depth_formats[] = {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT};
        /* check which of formats are supported for depth */
        for(u32 i = 0; i < ARRAY_SIZE(depth_formats); i++) {
            VkFormatProperties format_properties = (VkFormatProperties){0};
            vkGetPhysicalDeviceFormatProperties(vulkan_context->physical_device, depth_formats[i], &format_properties);
            /* check if it is suitable */
            if (format_properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
                settings->depth_format = depth_formats[i];
                goto _found_depth_format; 
            }
        }
        /* if not found depth format */
        MSG_ERROR(msg_callback, &TRACED_STR("failed to find suitable depth format"));
        return FALSE;
        _found_depth_format: {};
    }

    /* PRESENT MODE */ {
        u32 present_mode_count = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(vulkan_context->physical_device, vulkan_context->surface, &present_mode_count, NULL);
        if(present_mode_count == 0) {
            MSG_ERROR(msg_callback, &TRACED_STR("no present modes avaliable"));
            return FALSE;
        }
        VkPresentModeKHR *present_modes = allocateStack(stack, present_mode_count * sizeof(VkPresentModeKHR), 16);
        if(!present_modes) {
            MSG_ERROR(msg_callback, &TRACED_STR("failed to allocate present_modes array"));
            return FALSE;
        }
        vkGetPhysicalDeviceSurfacePresentModesKHR(vulkan_context->physical_device, vulkan_context->surface, &present_mode_count, present_modes);
        /* fifo present mode is guaranteed always */
        settings->present_mode = VK_PRESENT_MODE_FIFO_KHR;
        for(u32 i = 0; i < present_mode_count; i++) {
            if(present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
                settings->present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
                break;
            }
        }
    }

    popStack(stack);
    return TRUE;
}

b32 createScreenTextures(RenderContext *render_context) {
    VulkanContext *vulkan_context = render_context->vulkan_context;

    /* requrest surface capabilities */
    VkSurfaceCapabilitiesKHR surface_capabilities = (VkSurfaceCapabilitiesKHR){0};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vulkan_context->physical_device, vulkan_context->surface, &surface_capabilities);
    /* if capabilities have invalid size, for example window is hidden, wait in a loop */
    while(surface_capabilities.currentExtent.width == 0 || surface_capabilities.currentExtent.height == 0) {
        /* dont forget to proccess events or everything will stall forever */
        glfwPollEvents();
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vulkan_context->physical_device, vulkan_context->surface, &surface_capabilities);
    }

    /* min image count calculation */
    u32 min_image_count = MAX(surface_capabilities.minImageCount, 1);
    if(surface_capabilities.maxImageCount != 0) {
        min_image_count = MIN(min_image_count, surface_capabilities.maxImageCount);
    }

    /* if too many images should be used we cant fit them into array that we have */
    if(min_image_count > MAX_SWAPCHAIN_IMAGES) {
        MSG_ERROR(render_context->msg_callback, &TRACED_STR("swapchain min image count is too big"));
        return FALSE;
    }

    /* clamp resolution and set screen settings */
    u32 res_x = CLAMP(surface_capabilities.minImageExtent.width, surface_capabilities.maxImageExtent.width, surface_capabilities.currentExtent.width);
    u32 res_y = CLAMP(surface_capabilities.minImageExtent.height, surface_capabilities.maxImageExtent.height, surface_capabilities.currentExtent.height);
    render_context->screen_settings = (ScreenSettings) {
        .extent = (VkExtent2D) {
            .width = res_x,
            .height = res_y
        },
        .viewport = (VkViewport) {
            .width = (f32)res_x,
            .height = (f32)res_y,
            .minDepth = 0.0,
            .maxDepth = 1.0
        }
    };

    /* SWAPCHAIN */ {
        /* old swapchain will cointain old swapchain if its not NULL, then its first creation, dont forget destroy it after recreation */
        const VkSwapchainCreateInfoKHR swapchain_info = {
            .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .surface = vulkan_context->surface,
            .minImageCount = min_image_count,
            .imageFormat = render_context->render_settings.color_format,
            .imageColorSpace = render_context->render_settings.color_space,
            .imageExtent = render_context->screen_settings.extent,
            .presentMode = render_context->render_settings.present_mode,
            .preTransform =surface_capabilities.currentTransform,
            .imageArrayLayers = 1,
            .pQueueFamilyIndices = &vulkan_context->render_queue_id,
            .queueFamilyIndexCount = 1,
            .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            .clipped = TRUE,
            .oldSwapchain = render_context->swapchain
        };
        if(vkCreateSwapchainKHR(vulkan_context->device, &swapchain_info, NULL, &render_context->swapchain) != VK_SUCCESS) {
            MSG_ERROR(render_context->msg_callback, &TRACED_STR("failed to create swapchain"));
            return FALSE;
        }

        /* check if swapchain is recreated or created for the first time */
        if(swapchain_info.oldSwapchain) {
            /* if its not a first swapchain, just recreate images and views */
            for(u32 i = 0; i < render_context->swapchain_image_count; i++) {
                vkDestroyImageView(vulkan_context->device, render_context->swapchain_image_views[i], NULL);
            }
            vkDestroySwapchainKHR(vulkan_context->device, swapchain_info.oldSwapchain, NULL);
            vkGetSwapchainImagesKHR(vulkan_context->device, render_context->swapchain, &render_context->swapchain_image_count, NULL);
            vkGetSwapchainImagesKHR(vulkan_context->device, render_context->swapchain, &render_context->swapchain_image_count, render_context->swapchain_images);
        } else {
            /* if swapchain is created for the first time, we need to allocate arrays of images and views */
            vkGetSwapchainImagesKHR(vulkan_context->device, render_context->swapchain, &render_context->swapchain_image_count, NULL);
            vkGetSwapchainImagesKHR(vulkan_context->device, render_context->swapchain, &render_context->swapchain_image_count, render_context->swapchain_images);
        }

        /* create image views for swapchain */
        const u32 swapchain_image_count = render_context->swapchain_image_count;
        for(u32 i = 0; i < swapchain_image_count; i++) {
            VkImageViewCreateInfo view_info = {
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
                .format = render_context->render_settings.color_format,
                .image = render_context->swapchain_images[i]
            };
            if(vkCreateImageView(vulkan_context->device, &view_info, NULL, &render_context->swapchain_image_views[i]) != VK_SUCCESS) {
                MSG_ERROR(vulkan_context->msg_callback, &TRACED_STR("failed to create swapchain image view"));
                return FALSE;
            }
        }
    }

    /* DEPTH BUFFER */ {
        /* if depth image is already created we need to destroy it and its view */
        if(render_context->depth_image) {
            /* if recreating depth image */
            vkDestroyImageView(vulkan_context->device, render_context->depth_image_view, NULL);
            vkDestroyImage(vulkan_context->device, render_context->depth_image, NULL);
        }

        VkImageCreateInfo depth_image_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .extent = (VkExtent3D){
                .width = render_context->screen_settings.extent.width, 
                .height = render_context->screen_settings.extent.height, 
                .depth = 1
            },
            .format = render_context->render_settings.depth_format,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = 1,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE
        };
        if(vkCreateImage(vulkan_context->device,&depth_image_info, NULL, &render_context->depth_image) != VK_SUCCESS) {
            MSG_ERROR(render_context->msg_callback, &TRACED_STR("failed to create depth image"));
            return FALSE;
        }
        /* bind memory to depth image before creating view */
        if(vkBindImageMemory(vulkan_context->device, render_context->depth_image, render_context->images_vram.memory, render_context->depth_vram.offset) != VK_SUCCESS) {
            MSG_ERROR(render_context->msg_callback, &TRACED_STR("failed to bind depth image memory"));
            return FALSE;
        }

        /* create image view*/
        const VkImageViewCreateInfo depth_view_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .image = render_context->depth_image,
            .format = render_context->render_settings.depth_format,
            .subresourceRange = (VkImageSubresourceRange) {
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
                .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT
            }
        };
        if(vkCreateImageView(vulkan_context->device, &depth_view_info, NULL, &render_context->depth_image_view) != VK_SUCCESS) {
            MSG_ERROR(render_context->msg_callback, &TRACED_STR("failed to create render depth image view"));
            return FALSE;
        }
    }

    return TRUE;
}

RenderContext *createRenderContext(Allocate_pfn context_allocate, const RenderContextInfo *info) {
    /* VALIDATION */ {
        if(!info) {
            return NULL;
        }
        if(!context_allocate) {
            MSG_ERROR(info->msg_callback, &TRACED_STR("invalid allocator procedure"));
            return NULL;
        }
        if(!info->vulkan_context) {
            MSG_ERROR(info->msg_callback, &TRACED_STR("invalid vulkan context"));
            return NULL;
        }
    }

    RenderContext *context = context_allocate(sizeof(RenderContext), 16);
    if(!context) {
        MSG_ERROR(info->msg_callback, &TRACED_STR("failed to allocate render context"));
        return NULL;
    }

    *context = (RenderContext) {
        .vulkan_context = info->vulkan_context,
        .msg_callback = info->msg_callback,
        .update_callback = info->update_callback
    };

    VulkanContext *vulkan_context = info->vulkan_context;

    /* allocate arena for resources, this arena has same lifetime as render context that is created */
    if(!createArena(&context->resource_arena, 1024 * 1024 * 256, 1024 * 64)) {
        MSG_ERROR(context->msg_callback, &TRACED_STR("failed to allocate resource arena"));
        return NULL;
    }

    /* create stack allocator, which is used during init (this function scope) */
    Stack init_stack = (Stack){0};
    if(!createStack(&init_stack, 1024 * 1024 * 256, 1024 * 64)) {
        MSG_ERROR(context->msg_callback, &TRACED_STR("failed to create render init_stack"));
        return NULL;
    }

    /* configure render formats for depth color etc */
    if(!configureRenderSettings(context->vulkan_context, &init_stack, context->msg_callback, &context->render_settings)) {
        MSG_ERROR(context->msg_callback, &TRACED_STR("failed to confiure render settings"));
        return NULL;
    }

    /* IMAGES MEMORY LAYOUT */ {
        VramInfo images_alloc_info = {.memory_type_bits = U32_MAX};
        
        if(vulkan_context->device_model == DEVICE_MODEL_DESCRETE) {
            images_alloc_info.mandatory_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
            images_alloc_info.restricted_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
        }
        if(vulkan_context->device_model == DEVICE_MODEL_INTEGRATED) {
            images_alloc_info.mandatory_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        }

        i32 max_screen_res_x = 0;
        i32 max_screen_res_y = 0;

        /* DEPTH IMAGE */ {
            /* get monitors and find their max size */
            i32 monitor_count = 0;
            GLFWmonitor **monitors = glfwGetMonitors(&monitor_count);
            for(i32 i = 0; i < monitor_count; i++) {
                const GLFWvidmode *video_mode = glfwGetVideoMode(monitors[i]);
                max_screen_res_x = MAX(max_screen_res_x, video_mode->width);
                max_screen_res_y = MAX(max_screen_res_y, video_mode->height);
            }

            /* create depth image with max resolution */
            VkImage depth_prototype = NULL;
            VkImageCreateInfo depth_prototype_info = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                .imageType = VK_IMAGE_TYPE_2D,
                .extent = (VkExtent3D){
                    .width = max_screen_res_x, 
                    .height = max_screen_res_y, 
                    .depth = 1
                },
                .format = context->render_settings.depth_format,
                .tiling = VK_IMAGE_TILING_OPTIMAL,
                .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                .mipLevels = 1,
                .arrayLayers = 1,
                .samples = 1,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE
            };
            if(vkCreateImage(vulkan_context->device, &depth_prototype_info, NULL, &depth_prototype) != VK_SUCCESS) {
                MSG_ERROR(context->msg_callback, &TRACED_STR("failed to create depth image prototype for max resolution"));
                return NULL;
            }

            VkMemoryRequirements depth_memory_requirements = (VkMemoryRequirements){0};
            vkGetImageMemoryRequirements(vulkan_context->device, depth_prototype, &depth_memory_requirements);
            /* destroy prototype image, because its not real */
            vkDestroyImage(vulkan_context->device, depth_prototype, NULL);

            u64 start_offset = ALIGN(images_alloc_info.size, depth_memory_requirements.alignment);

            /* fill memory descriptor struct */
            context->depth_vram = (VramRegion) {
                .offset = start_offset,
                .size = depth_memory_requirements.size
            };

            images_alloc_info.size = start_offset + depth_memory_requirements.size;
            images_alloc_info.memory_type_bits &= depth_memory_requirements.memoryTypeBits;
        }
        
        if(!images_alloc_info.memory_type_bits) {
            MSG_ERROR(context->msg_callback, &TRACED_STR("incorrect resource groupping, memory type bits became 0"));
            return NULL;
        }

        /* CALL TO ALLOCATOR */ {
            if(!allocateVram(vulkan_context, &images_alloc_info, &context->images_vram)) {
                MSG_ERROR(context->msg_callback, &TRACED_STR("failed to allocate images vram"));
                return NULL;
            }
        }
    }

    /* MESHES MEMORY LAYOUT */ {
        
    }

    /* SCREEN TEXTURES */ {
        /* allocate space on resource arena for VkImages and VkImageViews of swapchain, they are handles of resources (pointers) */
        void *swapchain_image_buffer = allocateArena(&context->resource_arena, MAX_SWAPCHAIN_IMAGES * 2 * sizeof(void *), 8);
        if(!swapchain_image_buffer) {
            MSG_ERROR(context->msg_callback, &TRACED_STR("failed to allocate swapchain_image_buffer"));
            return NULL;
        }
        /* offset pointers, beacuse we allocated space for 2 arrays at once */
        context->swapchain_images = (VkImage *)swapchain_image_buffer;
        context->swapchain_image_views = (VkImageView *)((void *)swapchain_image_buffer + MAX_SWAPCHAIN_IMAGES);

        /* create screen textures */
        if(!createScreenTextures(context)) {
            MSG_ERROR(context->msg_callback, &TRACED_STR("failed to create screen textures"));
            return NULL;
        }
    }

    return context;
}

void destroyRenderContext(RenderContext *context) {
    VulkanContext* vulkan_context = context->vulkan_context;

    /* SCREEN IMAGES */ {
        for(u32 i = 0; i < context->swapchain_image_count; i++) {
            vkDestroyImageView(vulkan_context->device, context->swapchain_image_views[i], NULL);
        }
        vkDestroySwapchainKHR(vulkan_context->device, context->swapchain, NULL);
        vkDestroyImageView(vulkan_context->device, context->depth_image_view, NULL);
        vkDestroyImage(vulkan_context->device, context->depth_image, NULL);
    }

    /* FREE MEMORY */ {
        freeVram(vulkan_context, &context->images_vram);
        freeArena(&context->resource_arena);
    }

    *context = (RenderContext){0};
}
