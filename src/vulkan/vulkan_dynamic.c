
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
        SCREEN TEXTURES
    =============================================================== */
/* There are many things to say here. Most iportantly, why its dynamic ?
    Textures that we use for actual rendering (color and depth) are created once for biggest screen res,
    but swapchain needs to be recreated every time we resize the window.                                */

static b32
createSwapchain(
    const VulkanObjects *vulkan_objects,
    const VulkanDevice *device,
    MsgCallback_pfn msg_callback,
    VulkanScreen *vulkan_textures
) {
    u32 min_image_count = U32_MAX;
    VkExtent2D surface_extent = (VkExtent2D){0};
    VkSurfaceTransformFlagsKHR surface_transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;

    /* GET SURFACE INFO */ {
        VkSurfaceCapabilitiesKHR surface_capabilities = (VkSurfaceCapabilitiesKHR){0};
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device->physical_device, vulkan_objects->surface, &surface_capabilities);

        while(surface_capabilities.currentExtent.width == 0 || surface_capabilities.currentExtent.height == 0) {
            /* dont forget to proccess events or everything will stall forever */
            if(!processWindow(vulkan_objects->window)) {
                MSG_LOG(msg_callback, &CONST_STRING("window closed while resizing"));
            }
            vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device->physical_device, vulkan_objects->surface, &surface_capabilities);
        }

        min_image_count = MAX(surface_capabilities.minImageCount, 1);
        if(surface_capabilities.maxImageCount != 0) {
            min_image_count = MIN(min_image_count, surface_capabilities.maxImageCount);
        }
        if(min_image_count > MAX_SWAPCHAIN_IMAGE_COUNT) {
            MSG_ERROR(msg_callback, &TRACED_STR("swapchain min image count is bigger than maximum possible swapchain images"));
            goto _preallocation_fail;
        }

        surface_extent = surface_capabilities.currentExtent;
        surface_transform = surface_capabilities.currentTransform;
    }

    /* CREATE SWAPCHAIN */ {
        VkSwapchainKHR old_swapchain = vulkan_textures->swapchain;
        /* destroy old swapchain objects */
        if(old_swapchain) {
            const u32 swapchain_image_count = vulkan_textures->swapchain_image_count;
            VkImageView *swapchain_image_views = vulkan_textures->swapchain_views;
            for(u32 i = 0; i < swapchain_image_count; i++) {
                vkDestroyImageView(device->device, swapchain_image_views[i], NULL);
            }
        }

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

        if(vkCreateSwapchainKHR(device->device, &swapchain_info, NULL, & vulkan_textures->swapchain) != VK_SUCCESS) {
            MSG_ERROR(msg_callback, &TRACED_STR("failed to create swapchain"));
            goto _preallocation_fail;
        }

        if(old_swapchain) {
            vkDestroySwapchainKHR(device->device, old_swapchain, NULL);
        }
    }    

    /* CREATE IMAGES ARRAYS */ {
        u32 swapchain_image_count = 0;
        VkImage *swapchain_images = vulkan_textures->swapchain_images;
        VkImageView *swapchain_views = vulkan_textures->swapchain_views;

        vkGetSwapchainImagesKHR(device->device, vulkan_textures->swapchain, &swapchain_image_count, NULL);
        if(swapchain_image_count > MAX_SWAPCHAIN_IMAGE_COUNT) {
            MSG_ERROR(msg_callback, &TRACED_STR("swapchain that was created has too many images"));
            goto _swapchain_images_fail;
        }
        
        vkGetSwapchainImagesKHR(device->device, vulkan_textures->swapchain, &swapchain_image_count, swapchain_images);
        vulkan_textures->swapchain_image_count = swapchain_image_count;

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
        VkImageView *swapchain_views = vulkan_textures->swapchain_views;
        for(u32 i = 0; i < MAX_SWAPCHAIN_IMAGE_COUNT; i++) {
            if(swapchain_views[i] == NULL) {
                break;
            }
            vkDestroyImageView(device->device, swapchain_views[i], NULL);
        }
    };
    _swapchain_images_fail: {
        vkDestroySwapchainKHR(device->device, vulkan_textures->swapchain, NULL);
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
    MsgCallback_pfn msg_callback,
    VulkanScreen *screen_textures
) {
    String log_str = STACK_STR(256);

    *screen_textures = (VulkanScreen){0};
    
    u32 max_res_x = 0; 
    u32 max_res_y = 0;
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

    /* CREATE SCREEN IMAGES */ {
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

        if(vkCreateImage(vulkan_device->device, &screen_color_info, NULL, &screen_textures->color_image) != VK_SUCCESS) {
            MSG_ERROR(msg_callback, &TRACED_STR("failed to create screen color image"));
            goto _prealloc_fail;
        }
        if(vkCreateImage(vulkan_device->device, &screen_depth_info, NULL, &screen_textures->depth_image) != VK_SUCCESS) {
            MSG_ERROR(msg_callback, &TRACED_STR("failed to create screen depth image"));
            goto _screen_depth_fail;
        }

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
    MsgCallback_pfn msg_callback,
    VulkanScreen *vulkan_textures
) {
    b32 result = TRUE;

    if(!vulkan_textures->swapchain) {
        MSG_WARNING(msg_callback, &TRACED_STR("trying to destroy swapchain, but pointer is null"));
        result = FALSE;
    } else {
        const u32 image_count = vulkan_textures->swapchain_image_count;
        VkImageView *image_views = vulkan_textures->swapchain_views;
        for(u32 i = 0; i < image_count; i++) {
            vkDestroyImageView(vulkan_device->device, image_views[i], NULL);
        }
        vkDestroySwapchainKHR(vulkan_device->device, vulkan_textures->swapchain, NULL);
    }

    if(!vulkan_textures->color_view) {
        MSG_WARNING(msg_callback, &TRACED_STR("trying to destroy screen color view, but pointer is null"));
        result = FALSE;
    } else {
        vkDestroyImageView(vulkan_device->device, vulkan_textures->color_view, NULL);
    }
    if(!vulkan_textures->depth_view) {
        MSG_WARNING(msg_callback, &TRACED_STR("trying to destroy screen depth view, but pointer is null"));
        result = FALSE;
    } else {
        vkDestroyImageView(vulkan_device->device, vulkan_textures->depth_view, NULL);
    }

    if(!vulkan_textures->color_image) {
        MSG_WARNING(msg_callback, &TRACED_STR("trying to destroy screen color image, but pointer is null"));
        result = FALSE;
    } else {
        vkDestroyImage(vulkan_device->device, vulkan_textures->color_image, NULL);
    }
    if(!vulkan_textures->depth_image) {
        MSG_WARNING(msg_callback, &TRACED_STR("trying to destroy screen depth image, but pointer is null"));
        result = FALSE;
    } else {
        vkDestroyImage(vulkan_device->device, vulkan_textures->depth_image, NULL);
    }

    *vulkan_textures = (VulkanScreen){0};
    return result;
}


/*  ===============================================================
        EXTERN INTERFACE
    =============================================================== */

b32
createVulkanDynamic(
    VulkanHandle vulkan
) {
    const VulkanSegment *vulkan_segment = (VulkanSegment *)vulkan;
    const VulkanObjects *vulkan_objects = (VulkanObjects *)vulkan_segment->vulkan_objects_begin;
    const VulkanDevice *vulkan_device = (VulkanDevice *)vulkan_segment->vulkan_objects_end_device_begin;
    MsgCallback_pfn msg_callback = vulkan_objects->msg_callback;

    VulkanScreen *vulkan_textures = (VulkanScreen *)vulkan_segment->vulkan_textures_begin;

    if(!createScreenTextures(
        vulkan_objects, 
        vulkan_device, 
        msg_callback, 
        vulkan_textures
    )) {
        MSG_ERROR(msg_callback, &TRACED_STR("failed to create screen screen_textures"));
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
    const VulkanObjects *vulkan_objects = (VulkanObjects *)vulkan_segment->vulkan_objects_begin;
    const VulkanDevice *vulkan_device = (VulkanDevice *)vulkan_segment->vulkan_objects_end_device_begin;
    MsgCallback_pfn msg_callback = vulkan_objects->msg_callback;

    VulkanScreen *vulkan_textures = (VulkanScreen *)vulkan_segment->vulkan_textures_begin;
    b32 result = TRUE;

    if(!destroyScreenTextures(
        vulkan_objects, 
        vulkan_device, 
        msg_callback, 
        vulkan_textures
    )) {
        MSG_WARNING(msg_callback, &TRACED_STR("failed to destroy screen screen_textures"));
        result = FALSE;
    }

    return result;
}

b32
runVulkanLoop(
    VulkanHandle vulkan
) {
    const VulkanSegment *vulkan_segment = (VulkanSegment *)vulkan;
    const VulkanObjects *vulkan_objects = (VulkanObjects *)vulkan_segment->vulkan_objects_begin;

    while(processWindow(vulkan_objects->window)) {
        
    }

    return TRUE;
}
