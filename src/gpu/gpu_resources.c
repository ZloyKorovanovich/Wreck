#include "gpu_internal.h"

/* note that GPU_FORMAT_SURFACE is not in the list, because its selected dynamically */
const VkFormat format_conversion_table[GPU_FORMAT_COUNT] = {
    [GPU_FORMAT_NONE               ] = VK_FORMAT_UNDEFINED,
    [GPU_FORMAT_R32G32B32A32_SFLOAT] = VK_FORMAT_R32G32B32A32_SFLOAT,
    [GPU_FORMAT_R32G32_SFLOAT      ] = VK_FORMAT_R32G32_SFLOAT,
    [GPU_FORMAT_R32_SFLOAT         ] = VK_FORMAT_R32_SFLOAT,
    [GPU_FORMAT_D32_SFLOAT         ] = VK_FORMAT_D32_SFLOAT,
    [GPU_FORMAT_R16G16B16A16_SFLOAT] = VK_FORMAT_R16G16B16A16_SFLOAT,
    [GPU_FORMAT_R16G16_SFLOAT      ] = VK_FORMAT_R16G16_SFLOAT,
    [GPU_FORMAT_R16_SFLOAT         ] = VK_FORMAT_R16_SFLOAT,
    [GPU_FORMAT_R8G8B8_UNORM       ] = VK_FORMAT_R8G8B8_UNORM,
    [GPU_FORMAT_R8G8_UNORM         ] = VK_FORMAT_R8G8_UNORM,
    [GPU_FORMAT_R8_UNORM           ] = VK_FORMAT_R8_UNORM
};

b32 create_buffers(
    VkDevice          device,
    GpuMemorySection* memory,
    const BufferInfo* buffer_infos,
    u32               buffer_infos_count,
    GpuBuffer*        buffers
) {
    if(buffer_infos_count > GPU_MAX_STATIC_BUFFERS) {
        LOG_ERROR("too many static buffers: %u/%u", buffer_infos_count, GPU_MAX_STATIC_BUFFERS);
        goto fail;
    }

    for(u32 i = 0; i != buffer_infos_count; i++) {
        /* buffer usage */
        VkBufferUsageFlags buffer_usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        if(buffer_infos[i].flags & ~GPU_BUFFER_FLAGS_MASK) {
            LOG_ERROR(
                "invalid buffer flags id: %u/%u mask: %06x/%06x", 
                i, buffer_infos_count,  buffer_infos[i].flags, GPU_BUFFER_FLAGS_MASK
            );
            goto fail;
        }
        if(buffer_infos[i].flags & GPU_BUFFER_FLAG_UNIFORM_BUFFER) {
            buffer_usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        }
        if(buffer_infos[i].flags & GPU_BUFFER_FLAG_STORAGE_BUFFER) {
            buffer_usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        }

        /* create buffer */
        const VkBufferCreateInfo buffer_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .usage = buffer_usage,
            .size  = buffer_infos[i].size
        };

        VkBuffer buffer = NULL;
        if(vkCreateBuffer(device, &buffer_info, NULL, &buffer) != VK_SUCCESS) {
            LOG_ERROR("failed to create buffer id: %u/%u", i, buffer_infos_count);
            goto fail;
        }
        
        /* get buffer requirements and bind buffer */
        VkMemoryRequirements buffer_requirements = (VkMemoryRequirements){0};
        vkGetBufferMemoryRequirements(device, buffer, &buffer_requirements);

        const u64 buffer_allocation_offset = ALIGN(memory->offset, buffer_requirements.alignment);
        const u64 buffer_allocation_size   = buffer_requirements.size;

        if(buffer_allocation_offset + buffer_allocation_size > memory->limit) {
            LOG_ERROR("ran out of buffers memory: %llu/%llu", buffer_allocation_offset + buffer_allocation_size, memory->limit);
            goto fail;
        }
        if(vkBindBufferMemory(device, buffer, memory->memory, buffer_allocation_offset) != VK_SUCCESS) {
            LOG_ERROR("failed to bind buffer memory id: %u/%u", i, buffer_infos_count);
            goto fail;
        }

        memory->offset = buffer_allocation_offset + buffer_allocation_size;
        buffers[i]     = (GpuBuffer) {
            .usage             = buffer_usage,
            .allocation_offset = buffer_allocation_offset,
            .allocation_size   = buffer_allocation_size,
            .used_size         = buffer_infos[i].size,
            .buffer            = buffer
        };
    }

    return TRUE;

    fail: {
        return FALSE;
    }
}

/* FIX: refactor critical was bug found */
b32 create_images(
    VkDevice          device,
    GpuMemorySection* memory,
    const ImageInfo*  image_infos,
    u32               image_infos_count,
    GpuImage*         images
) {
    if(image_infos_count > GPU_MAX_STATIC_IMAGES) {
        LOG_ERROR("too many static images: %u/%u", image_infos_count, GPU_MAX_STATIC_IMAGES);
        goto fail;
    }

    for(u32 i = 0; i != image_infos_count; i++) {
        /* image format and usage */
        VkFormat           image_format = VK_FORMAT_UNDEFINED;
        VkImageUsageFlags  image_usage  = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        
        if(image_infos[i].format >= GPU_FORMAT_COUNT || image_infos[i].format == GPU_FORMAT_NONE) {
            LOG_ERROR("invalid image format id: %u/%u format: %u", i, image_infos_count, image_infos[i].format);
            goto fail;
        } else {
            image_format = format_conversion_table[image_infos[i].format];
        }

        if(image_infos[i].flags & ~GPU_IMAGE_FLAGS_MASK) {
            LOG_ERROR(
                "invalid image flags id: %u/%u flags: %06x/%06x", 
                i, image_infos_count, image_infos[i].flags, GPU_IMAGE_FLAGS_MASK
            );
            goto fail;
        }
        if(image_infos[i].flags & GPU_IMAGE_FLAG_COLOR_ATTACHMENT) {
            image_usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        }
        if(image_infos[i].flags & GPU_IMAGE_FLAG_DEPTH_ATTACHMENT) {
            image_usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        }
        if(image_infos[i].flags & GPU_IMAGE_FLAG_SAMPLED) {
            image_usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
        }
        if(image_infos[i].flags & GPU_IMAGE_FLAG_STORAGE) {
            image_usage |= VK_IMAGE_USAGE_STORAGE_BIT;
        }

        /* create image */
        const VkImageCreateInfo image_info = {
            .sType       = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType   = VK_IMAGE_TYPE_2D,
            .tiling      = VK_IMAGE_TILING_OPTIMAL,
            .samples     = VK_SAMPLE_COUNT_1_BIT,
            .format      = image_format,
            .usage       = image_usage,
            .mipLevels   = 1,
            .arrayLayers = 1,
            .extent      = (VkExtent3D) {
                .width  = image_infos[i].size_x,
                .height = image_infos[i].size_y,
                .depth  = 1
            }  
        };

        VkImage image = NULL;
        if(vkCreateImage(device, &image_info, NULL, &image) != VK_SUCCESS) {
            LOG_ERROR("failed to create image id: %u/%u", i, image_infos_count);
            goto fail;
        }

        /* image requirements */
        VkMemoryRequirements image_requirements = (VkMemoryRequirements){0};
        vkGetImageMemoryRequirements(device, image, &image_requirements);

        const u64 image_allocation_offset = ALIGN(memory->offset, image_requirements.alignment);
        const u64 image_allocation_size   = image_requirements.size;

        if(image_allocation_offset + image_allocation_size > memory->limit) {
            LOG_ERROR("ran out of images memory: %llu/%llu", image_allocation_offset + image_allocation_size, memory->limit);
            goto fail;
        }
        if(vkBindImageMemory(device, image, memory->memory, image_allocation_offset) != VK_SUCCESS) {
            LOG_ERROR("failed to bind image memory id: %u/%u", i, image_infos_count);
            goto fail;
        }

        /* FIX: image aspect selection */
        /* create image view */
        const VkImageAspectFlags    image_aspect    = (image_usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
        const VkImageViewCreateInfo image_view_info = {
            .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .format           = image_format,
            .image            = image,
            .viewType         = VK_IMAGE_VIEW_TYPE_2D,
            .components       = (VkComponentMapping) {
                .r = VK_COMPONENT_SWIZZLE_R,
                .g = VK_COMPONENT_SWIZZLE_G,
                .b = VK_COMPONENT_SWIZZLE_B,
                .a = VK_COMPONENT_SWIZZLE_A
            },
            .subresourceRange = (VkImageSubresourceRange) {
                .aspectMask     = image_aspect,
                .baseArrayLayer = 0,
                .baseMipLevel   = 0,
                .layerCount     = 1,
                .levelCount     = 1
            }
        };

        VkImageView image_view = NULL;
        if(vkCreateImageView(device, &image_view_info, NULL, &image_view) != VK_SUCCESS) {
            LOG_ERROR("failed to create image view id: %u/%u", i, image_infos_count);
            goto fail;
        }

        memory->offset = image_allocation_offset + image_allocation_size;
        images[i]      = (GpuImage) {
            .usage             = image_usage,
            .format            = image_format,
            .allocation_offset = image_allocation_offset,
            .allocation_size   = image_allocation_size,
            .size_x            = image_infos[i].size_x,
            .size_y            = image_infos[i].size_y,
            .image             = image,
            .view              = image_view,
            .aspect            = image_aspect
        };
    }

    return TRUE;

    fail: {
        return FALSE;
    }
}

/* FIX: refactor */
b32 create_transfer_buffers(
    VkDevice          device,
    GpuMemorySection* memory,
    GpuBuffer*        sync_transfer
) {
    const VkBufferCreateInfo sync_transfer_buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .size  = GPU_SYNC_TRANSFER_SIZE
    };

    VkBuffer sync_transfer_buffer = NULL;

    if(vkCreateBuffer(device, &sync_transfer_buffer_info, NULL, &sync_transfer_buffer) != VK_SUCCESS) {
        LOG_ERROR("failed to create sync transfer buffer");
        goto fail;
    }

    VkMemoryRequirements sync_transfer_requirements = (VkMemoryRequirements){0};

    vkGetBufferMemoryRequirements(device, sync_transfer_buffer, &sync_transfer_requirements);

    u64 allocation_end             = memory->offset;
    const u64 sync_transfer_offset = ALIGN(allocation_end, sync_transfer_requirements.alignment);
    const u64 sync_transfer_size   = sync_transfer_requirements.size;
    allocation_end                 = sync_transfer_offset + sync_transfer_size;

    if(allocation_end > memory->limit) {
        LOG_ERROR(
            "exceed memory limit: (%llu+%llu)/%llu", 
            sync_transfer_offset, sync_transfer_size, memory->limit
        );
        goto fail;
    }

    if(vkBindBufferMemory(device, sync_transfer_buffer, memory->memory, sync_transfer_offset) != VK_SUCCESS) {
        LOG_ERROR("failed to bind sync transfer buffer memory");
        goto fail;
    }

    *sync_transfer = (GpuBuffer) {
        .usage             = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .buffer            = sync_transfer_buffer,
        .allocation_offset = sync_transfer_offset,
        .allocation_size   = sync_transfer_size,
        .used_size         = GPU_SYNC_TRANSFER_SIZE
    };

    return TRUE;

    fail: {
        return FALSE;
    }
}

b32 create_samplers(
    VkDevice   device,
    VkSampler* sampler_linear_repeat,
    VkSampler* sampler_linear_clamp,
    VkSampler* sampler_nearest_repeat,
    VkSampler* sampler_nearest_clamp
) {
    const VkSamplerCreateInfo linear_repeat_info = {
        .sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter               = VK_FILTER_LINEAR,
        .minFilter               = VK_FILTER_LINEAR,
        .mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU            = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV            = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW            = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .mipLodBias              = 0.0f,
        .anisotropyEnable        = VK_FALSE,
        .maxAnisotropy           = 1.0f,
        .compareEnable           = VK_FALSE,
        .compareOp               = VK_COMPARE_OP_NEVER,
        .minLod                  = 0.0f,
        .maxLod                  = VK_LOD_CLAMP_NONE,
        .borderColor             = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
        .unnormalizedCoordinates = VK_FALSE
    };
    const VkSamplerCreateInfo linear_clamp_info = {
        .sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter               = VK_FILTER_LINEAR,
        .minFilter               = VK_FILTER_LINEAR,
        .mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .mipLodBias              = 0.0f,
        .anisotropyEnable        = VK_FALSE,
        .maxAnisotropy           = 1.0f,
        .compareEnable           = VK_FALSE,
        .compareOp               = VK_COMPARE_OP_NEVER,
        .minLod                  = 0.0f,
        .maxLod                  = VK_LOD_CLAMP_NONE,
        .borderColor             = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
        .unnormalizedCoordinates = VK_FALSE
    };
    const VkSamplerCreateInfo nearest_repeat_info = {
        .sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter               = VK_FILTER_NEAREST,
        .minFilter               = VK_FILTER_NEAREST,
        .mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU            = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV            = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW            = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .mipLodBias              = 0.0f,
        .anisotropyEnable        = VK_FALSE,
        .maxAnisotropy           = 1.0f,
        .compareEnable           = VK_FALSE,
        .compareOp               = VK_COMPARE_OP_NEVER,
        .minLod                  = 0.0f,
        .maxLod                  = VK_LOD_CLAMP_NONE,
        .borderColor             = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
        .unnormalizedCoordinates = VK_FALSE
    };
    const VkSamplerCreateInfo nearest_clamp_info = {
        .sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter               = VK_FILTER_NEAREST,
        .minFilter               = VK_FILTER_NEAREST,
        .mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .mipLodBias              = 0.0f,
        .anisotropyEnable        = VK_FALSE,
        .maxAnisotropy           = 1.0f,
        .compareEnable           = VK_FALSE,
        .compareOp               = VK_COMPARE_OP_NEVER,
        .minLod                  = 0.0f,
        .maxLod                  = VK_LOD_CLAMP_NONE,
        .borderColor             = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
        .unnormalizedCoordinates = VK_FALSE
    };

    if(vkCreateSampler(device, &linear_repeat_info, NULL, sampler_linear_repeat) != VK_SUCCESS) {
        LOG_ERROR("failed to create linear repeat sampler");
        goto fail;
    }
    if(vkCreateSampler(device, &linear_clamp_info, NULL, sampler_linear_clamp) != VK_SUCCESS) {
        LOG_ERROR("failed to create linear clamp sampler");
        goto fail;
    }
    if(vkCreateSampler(device, &nearest_repeat_info, NULL, sampler_nearest_repeat) != VK_SUCCESS) {
        LOG_ERROR("failed to create nearest repeat sampler");
        goto fail;
    }
    if(vkCreateSampler(device, &nearest_clamp_info, NULL, sampler_nearest_clamp) != VK_SUCCESS) {
        LOG_ERROR("failed to create nearest clamp sampler");
        goto fail;
    }

    return TRUE;

    fail: {
        return FALSE;
    }
}

/*  0 = success
    1 = fail
    2 = window_closed */
i32 create_swapchain(
    VkDevice            device,
    VkPhysicalDevice    physical_device,
    VkSurfaceKHR        surface,
    VkFormat            surface_format,
    VkColorSpaceKHR     surface_color_space,
    VkSwapchainKHR*     swapchain,
    u32*                swapchain_images_count,
    u32*                swapchain_x,
    u32*                swapchain_y,
    VkImage*            swapchain_images,
    VkImageView*        swapchain_image_views
) {
    VkSurfaceCapabilitiesKHR surface_capabilities = (VkSurfaceCapabilitiesKHR){0}; 
    while(1) {
        /* get surface capabilities */
        if(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &surface_capabilities) != VK_SUCCESS) {
            LOG_ERROR("faied to get surface capabilities");
            goto fail;
        }

        if(surface_capabilities.currentExtent.width != 0 && surface_capabilities.currentExtent.height != 0) {
            break;
        }

        /* process window (helps to not stuck when its hidden) */
        MSG msg = (MSG){0};
        while(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if(msg.message == WM_QUIT) {
                goto quit;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    /* destroy old images */
    const VkSwapchainKHR old_swapchain              = *swapchain;
    const u32            swapchain_old_images_count = *swapchain_images_count;
    for(u32 i = 0; i != swapchain_old_images_count; i++) {
        vkDestroyImageView(device, swapchain_image_views[i], NULL);
        swapchain_images[i]      = NULL;
        swapchain_image_views[i] = NULL;
    }
    
    /* create swapchain */
    const u32 size_x = surface_capabilities.currentExtent.width;
    const u32 size_y = surface_capabilities.currentExtent.height;
    const VkSwapchainCreateInfoKHR swapchain_info = {
        .sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface          = surface,
        .oldSwapchain     = *swapchain,
        .minImageCount    = (surface_capabilities.maxImageCount == 0) ? GPU_OPTIMAL_SWAPCHAIN_IMAGES : CLAMP(surface_capabilities.minImageCount, surface_capabilities.maxImageCount, GPU_OPTIMAL_SWAPCHAIN_IMAGES),
        .imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageFormat      = surface_format,
        .imageColorSpace  = surface_color_space,
        .preTransform     = surface_capabilities.currentTransform,
        .compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .imageArrayLayers = 1,
        .imageExtent      = (VkExtent2D) {
            .width  = size_x,
            .height = size_y
        }
    };

    if(vkCreateSwapchainKHR(device, &swapchain_info, NULL, swapchain) != VK_SUCCESS) {
        LOG_ERROR("failed to create swapchain");
        goto fail;
    }

    if(old_swapchain != NULL) {
        vkDestroySwapchainKHR(device, old_swapchain, NULL);
    }

    /* get swapchain images */
    u32 swapchain_new_image_count = 0;
    /* get image count */
    vkGetSwapchainImagesKHR(device, *swapchain, &swapchain_new_image_count, NULL);
    if(swapchain_new_image_count == 0) {
        LOG_ERROR("no swapchain images found");
        goto fail;
    }
    if(swapchain_new_image_count > GPU_MAX_SWAPCHAIN_IMAGES) {
        LOG_ERROR("too many swapchain images created: %u/%u", swapchain_new_image_count, GPU_MAX_SWAPCHAIN_IMAGES);
        goto fail;
    }
    /* get images */
    *swapchain_images_count = swapchain_new_image_count;
    *swapchain_x            = size_x;
    *swapchain_y            = size_y;
    vkGetSwapchainImagesKHR(device, *swapchain, &swapchain_new_image_count, swapchain_images);

    /* create swapchain image views */
    for(u32 i = 0; i != swapchain_new_image_count; i++) {
        const VkImageViewCreateInfo image_view_info = {
            .sType      = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image      = swapchain_images[i],
            .viewType   = VK_IMAGE_VIEW_TYPE_2D,
            .format     = surface_format,
            .components = (VkComponentMapping) {
                VK_COMPONENT_SWIZZLE_R,
                VK_COMPONENT_SWIZZLE_G,
                VK_COMPONENT_SWIZZLE_B,
                VK_COMPONENT_SWIZZLE_A
            },
            .subresourceRange = (VkImageSubresourceRange) {
                .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel   = 0,
                .baseArrayLayer = 0,
                .layerCount     = 1,
                .levelCount     = 1
            }
        };

        if(vkCreateImageView(device, &image_view_info, NULL, &swapchain_image_views[i]) != VK_SUCCESS) {
            LOG_ERROR("failed to create vulkan swapchain image view %u/%u", i, swapchain_new_image_count);
        }
    }

    return 0;

    fail: {
        return 1;
    }

    quit: {
        return 2;
    }
}

/* GLOBAL INTERFACE */

b32 gpu_allocate_resources(
    CtxHandle            ctx, 
    const ResourcesInfo* resources_info
) {
    if(ctx == NULL || resources_info == NULL) {
        LOG_ERROR("input params are NULL");
        goto fail;
    }

    GpuContext*          gpu_ctx          = (GpuContext*)ctx;
    const VulkanObjects* vulkan_objects   = &gpu_ctx->vulkan_objects;
    const VulkanDevice*  vulkan_device    = &gpu_ctx->vulkan_device;
    VulkanResources*     vulkan_resources = &gpu_ctx->vulkan_resources;

    /* swapchain */
    if(create_swapchain(
        vulkan_device->device,
        vulkan_device->adapter->physical_device,
        vulkan_objects->surface,
        vulkan_device->adapter->surface_format,
        vulkan_device->adapter->surface_color_space,
        &vulkan_resources->swapchain,
        &vulkan_resources->swapchain_images_count,
        &vulkan_resources->swapchain_x,
        &vulkan_resources->swapchain_y,
        vulkan_resources->swapchain_images,
        vulkan_resources->swapchain_views
    ) != 0) {
        LOG_ERROR("failed to create vulkan swapchain");
        goto fail;
    }

    /* memory sections */
    GpuMemorySection device_buffers_memory = (GpuMemorySection) {
        .memory = vulkan_device->video_memory_device_buffers.device_memory,
        .offset = 0,
        .limit  = VRAM_SIZE_DEVICE_BUFFERS
    };
    GpuMemorySection device_images_memory = (GpuMemorySection) {
        .memory = vulkan_device->video_memory_device_images.device_memory,
        .offset = 0,
        .limit  = VRAM_SIZE_DEVICE_IMAGES
    };
    GpuMemorySection host_transfer_memory = (GpuMemorySection) {
        .memory = vulkan_device->video_memory_host_transfer.device_memory,
        .offset = 0,
        .limit  = VRAM_SIZE_HOST_TRANSFER
    };

    /* buffers */
    if(resources_info->buffer_infos_count != 0) {
        vulkan_resources->buffers_count = resources_info->buffer_infos_count;
        if(!create_buffers(
            vulkan_device->device, 
            &device_buffers_memory, 
            resources_info->buffer_infos, 
            resources_info->buffer_infos_count, 
            vulkan_resources->buffers
        )) {
            LOG_ERROR("failed to create buffers");
            goto fail;
        }
    }
    /* images */
    if(resources_info->image_infos_count != 0) {
        vulkan_resources->images_count = resources_info->image_infos_count;
        if(!create_images(
            vulkan_device->device,
            &device_images_memory,
            resources_info->image_infos,
            resources_info->image_infos_count,
            vulkan_resources->images
        )) {
            LOG_ERROR("failed to create images");
            goto fail;
        }
    }
    /* FIX: transfer */
    if(vulkan_device->video_memory_host_transfer.device_memory != NULL) {
        if(!create_transfer_buffers(
            vulkan_device->device,
            &host_transfer_memory,
            &vulkan_resources->buffer_sync_transfer
        )) {
            LOG_ERROR("failed to create transfer buffers");
        }
    }
    /* samplers */
    if(!create_samplers(
        vulkan_device->device,
        &vulkan_resources->sampler_linear_repeat,
        &vulkan_resources->sampler_linear_clamp,
        &vulkan_resources->sampler_nearest_repeat,
        &vulkan_resources->sampler_nearest_clamp
    )) {
        LOG_ERROR("failed to create samplers");
        goto fail;
    }

    return TRUE;

    fail: {
        return FALSE;
    }
}

void gpu_release_resources(
    CtxHandle ctx
) {
    if(ctx == NULL) {
        LOG_ERROR("input params are NULL");
        goto fail;
    }

    GpuContext*          gpu_ctx          = (GpuContext*)ctx;
    VulkanResources*     vulkan_resources = &gpu_ctx->vulkan_resources;
    const VkDevice       device           = gpu_ctx->vulkan_device.device;

    /* samplers */
    vkDestroySampler(device, vulkan_resources->sampler_linear_repeat , NULL);
    vkDestroySampler(device, vulkan_resources->sampler_linear_clamp  , NULL);
    vkDestroySampler(device, vulkan_resources->sampler_nearest_repeat, NULL);
    vkDestroySampler(device, vulkan_resources->sampler_nearest_clamp , NULL);

    /* transfer buffers */
    if(vulkan_resources->buffer_sync_transfer.buffer != NULL) {
        vkDestroyBuffer(device, vulkan_resources->buffer_sync_transfer.buffer, NULL);
    }

    /* buffers */
    const GpuBuffer* buffers       = vulkan_resources->buffers;
    const u32        buffers_count = vulkan_resources->buffers_count;
    for(u32 i = 0; i != buffers_count; i++) {
        vkDestroyBuffer(device, buffers[i].buffer, NULL);
    }

    /* images */
    const GpuImage* images       = vulkan_resources->images;
    const u32       images_count = vulkan_resources->images_count;
    for(u32 i = 0; i != images_count; i++) {
        vkDestroyImageView(device, images[i].view, NULL);
        vkDestroyImage(device, images[i].image, NULL);
    }

    /* swapchain */
    const VkImageView* swapchain_image_views       = vulkan_resources->swapchain_views;
    const u32          swapchain_image_views_count = vulkan_resources->swapchain_images_count;
    for(u32 i = 0; i != swapchain_image_views_count; i++) {
        vkDestroyImageView(device, swapchain_image_views[i], NULL);
    }
    vkDestroySwapchainKHR(device, vulkan_resources->swapchain, NULL);

    *vulkan_resources = (VulkanResources){0};

    fail: {};
}
