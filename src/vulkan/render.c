#include "vulkan.h"

#define SHADER_ENTRY_VERTEX "vertexMain"
#define SHADER_ENTRY_FRAGMENT "fragmentMain"
#define SHADER_ENTRY_COMPUTE "computeMain"


i32 getSurfaceData(const VulkanContext* vulkan_context, MsgCallback_pfn msg_callback, SurfaceData* surface_data) {
    #define MAX_SURFACE_FORMATS_COUNT 1024
    #define MAX_PRESENT_MODES_COUNT 256

    *surface_data = (SurfaceData){0};

    /* FORMAT */{
        VkSurfaceFormatKHR surface_formats[MAX_SURFACE_FORMATS_COUNT] = {0};
        u32 format_count = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(vulkan_context->physical_device, vulkan_context->surface, &format_count, NULL);
        format_count = MIN(format_count, MAX_SURFACE_FORMATS_COUNT);
        if(format_count == 0) {
            MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_NO_SURFACE_FORMATS_AVAILABLE, "no surface formats are available");
        }
        vkGetPhysicalDeviceSurfaceFormatsKHR(vulkan_context->physical_device, vulkan_context->surface, &format_count, surface_formats);

        /* find best surface format */
        surface_data->color_format = surface_formats[0].format;
        for(u32 i = 0; i < format_count; i++) {
            if(surface_formats[i].format == VK_FORMAT_B8G8R8A8_SRGB && surface_formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                surface_data->color_format = VK_FORMAT_B8G8R8A8_SRGB;
                surface_data->color_space = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
                break;
            }
        }
    }   

    /* DEPTH FORMAT */ {
        /* search options that we will be iterating trough */
        const u32 depth_formats_count = 3;
        const VkFormat depth_formats[3] = {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT};
        
        for(u32 i = 0; i < depth_formats_count; i++) {
            VkFormatProperties format_properties = (VkFormatProperties){0};
            vkGetPhysicalDeviceFormatProperties(vulkan_context->physical_device, depth_formats[i], &format_properties);
            /* check if it is suitable */
            if (format_properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
                surface_data->depth_format = depth_formats[i];
                goto _found_depth_format; 
            }
        }
        //_not_found_depth_format:
        MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_NO_DEPTH_MODES_AVAILABLE, "device does not support depth formats");
        _found_depth_format: {}
    }

    /* PRESENT MODE */ {
        VkPresentModeKHR present_modes[MAX_PRESENT_MODES_COUNT] = {0};
        u32 present_mode_count = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(vulkan_context->physical_device, vulkan_context->surface, &present_mode_count, NULL);
        present_mode_count = MIN(present_mode_count, MAX_PRESENT_MODES_COUNT);
        if(present_mode_count == 0) {
            MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_NO_PRESENT_MODES_AVAILABLE, "no surface present modes available");
        }
        vkGetPhysicalDeviceSurfacePresentModesKHR(vulkan_context->physical_device, vulkan_context->surface, &present_mode_count, present_modes);
        
        surface_data->present_mode = VK_PRESENT_MODE_FIFO_KHR;
        for(u32 i = 0; i < present_mode_count; i++) {
            if(present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
                surface_data->present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
                break;
            }
        }
    }

    /* CAPABILITIES */ {
        VkSurfaceCapabilitiesKHR surface_capabilities;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vulkan_context->physical_device, vulkan_context->surface, &surface_capabilities);

        if(surface_capabilities.minImageCount > MAX_SWAPCHAIN_IMAGE_COUNT) {
            MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_SWAPCHAIN_TOO_MANY_IMAGES, "too minimum image count from surface capabilities is too big");
        }

        surface_data->max_image_count = surface_capabilities.maxImageCount;
        surface_data->min_image_count = surface_capabilities.minImageCount + 1;
        surface_data->transform = surface_capabilities.currentTransform;

        while(surface_capabilities.currentExtent.width == 0 || surface_capabilities.currentExtent.height == 0) {
            glfwPollEvents();
            vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vulkan_context->physical_device, vulkan_context->surface, &surface_capabilities);
        }

        /* might return big values so we clamp that down , but it might be ub still */
        surface_data->extent.width = CLAMP(surface_capabilities.minImageExtent.width, surface_capabilities.maxImageExtent.width, (u32)surface_capabilities.currentExtent.width);
        surface_data->extent.height = CLAMP(surface_capabilities.minImageExtent.height, surface_capabilities.maxImageExtent.height, (u32)surface_capabilities.currentExtent.height);
    }
    
    return MSG_CODE_SUCCESS;
}

i32 createSwapchain(const VulkanContext* vulkan_context, MsgCallback_pfn msg_callback, RenderContext* render_context) {
    
    /* SWAPCHAIN */ {
        /* create swapchain based on selected params*/
        const VkSwapchainCreateInfoKHR swapchain_info = {
            .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .surface = vulkan_context->surface,
            .minImageCount = render_context->surface_data.min_image_count,
            .imageFormat = render_context->surface_data.color_format,
            .imageColorSpace = render_context->surface_data.color_space,
            .imageExtent = render_context->surface_data.extent,
            .presentMode = render_context->surface_data.present_mode,
            .preTransform = render_context->surface_data.transform,
            .imageArrayLayers = 1,
            .pQueueFamilyIndices = &vulkan_context->render_family_id,
            .queueFamilyIndexCount = 1,
            .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            .clipped = TRUE
        };
        if(vkCreateSwapchainKHR(vulkan_context->device, &swapchain_info, NULL, &render_context->screen.swapchain) != VK_SUCCESS) {
            MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_SWAPCHAIN_CREATE, "failed to create vulkan swapchain");
        }
        /* get images from swapchain to render into them later */
        vkGetSwapchainImagesKHR(vulkan_context->device, render_context->screen.swapchain, &render_context->screen.swapchain_image_count, NULL);
        if(render_context->screen.swapchain_image_count > MAX_SWAPCHAIN_IMAGE_COUNT) {
            MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_SWAPCHAIN_TOO_MANY_IMAGES, "too many images in vulkan swapchain");
        }
        vkGetSwapchainImagesKHR(vulkan_context->device, render_context->screen.swapchain, &render_context->screen.swapchain_image_count, render_context->screen.swapchain_images);

        /* create views to images */
        const u32 swapchain_image_count = render_context->screen.swapchain_image_count;
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
                .format = render_context->surface_data.color_format,
                .image = render_context->screen.swapchain_images[i]
            };

            if(vkCreateImageView(vulkan_context->device, &view_info, NULL, &render_context->screen.swapchain_image_views[i]) != VK_SUCCESS) {
                MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_IMAGE_VIEW_CREATE, "failed to create vulkan swapchain image view");
            }
        }
    }

    /* DEPTH */ {
        /* create depth image */
        const VkImageCreateInfo depth_image_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .extent = (VkExtent3D){
                .width = render_context->surface_data.extent.width, 
                .height = render_context->surface_data.extent.height, 
                .depth = 1
            },
            .format = render_context->surface_data.depth_format,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = 1,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE
        };
        if(vkCreateImage(vulkan_context->device, &depth_image_info, NULL, &render_context->screen.depth_image) != VK_SUCCESS) {
            MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_IMAGE_CREATE, "failed to create render depth image");
        }
        /* @(FIX): binding with zero offset, but that might change in the future */
        if(vkBindImageMemory(vulkan_context->device, render_context->screen.depth_image, render_context->screen.vram->memory, 0) != VK_SUCCESS) {
            MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_IMAGE_BIND_MEMORY, "failed to bind render depth image to memory");
        }

        /* create depth view */
        const VkImageViewCreateInfo depth_view_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .image = render_context->screen.depth_image,
            .format = render_context->surface_data.depth_format,
            .subresourceRange = (VkImageSubresourceRange) {
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
                .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT
            }
        };
        if(vkCreateImageView(vulkan_context->device, &depth_view_info, NULL, &render_context->screen.depth_image_view) != VK_SUCCESS) {
            MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_IMAGE_VIEW_CREATE, "failed to create render depth view");
        }
    }

    return MSG_CODE_SUCCESS;
}

i32 renderProcess(const VulkanContext* vulkan_context, RenderContext* render_context) {
    /* SETUP */ {

    }

    

    return MSG_CODE_SUCCESS;
}

i32 createRenderContext(VulkanContext* vulkan_context, const RenderContextInfo* render_info, MsgCallback_pfn msg_callback, RenderContext** render_context) {
    char msg_log[256] = {0};

    RenderContext* context = malloc(sizeof(RenderContext));
    if(!context) {
        MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_BUFFER_MALLOC_FAIL, "failed to allocate render context");
    }
    *context = (RenderContext){0};

    if(MSG_IS_ERROR(getSurfaceData(vulkan_context, msg_callback, &context->surface_data))) {
        MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_SURFACE_STATS_NOT_SUITABLE, "surface is not suitable for the use of application");
    }

    /* ALLOCATE SCREEN TARGETS */ {
        /* get max possible screen resolution, in order to make a constant allocation that can be reused in any case */

        i32 monitor_count = 0;
        GLFWmonitor** monitors = glfwGetMonitors(&monitor_count);
        i32 max_width = 0;
        i32 max_height = 0;
        for(i32 i = 0; i < monitor_count; i++) {
            const GLFWvidmode* video_mode = glfwGetVideoMode(monitors[i]);
            max_width = MAX(max_width, video_mode->width);
            max_height = MAX(max_height, video_mode->height);
        }

        /* create dummy depth texture for memory requirements */
        VkImage depth_prototype = NULL;
        const VkImageCreateInfo depth_prototype_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .extent = (VkExtent3D){
                .width = (u32)max_width, 
                .height = (u32)max_height, 
                .depth = 1
            },
            .format = context->surface_data.depth_format,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = 1,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE
        };
        if(vkCreateImage(vulkan_context->device, &depth_prototype_info, NULL, &depth_prototype) != VK_SUCCESS) {
            MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_IMAGE_CREATE, "failed to create depth image prototype for allocating memory");
        }
        VkMemoryRequirements depth_prototype_memory = (VkMemoryRequirements){0};
        vkGetImageMemoryRequirements(vulkan_context->device, depth_prototype, &depth_prototype_memory);

        /* allocate memory for biggest resolution */
        VramAllocationInfo alloc_info = {
            .size = depth_prototype_memory.size,
            .memory_type_flags = depth_prototype_memory.memoryTypeBits
        };
        if(vulkan_context->device_type == DEVICE_TYPE_DESCRETE) {
            alloc_info.positive_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
            alloc_info.negative_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
        }
        if(vulkan_context->device_type == DEVICE_TYPE_INTEGRATED) {
            alloc_info.positive_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        }

        if(!(context->screen.vram = allocateVram(vulkan_context, &alloc_info))) {
            MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_ALLOCATE_VRAM, "failed to allocate memory for depth image");
        }
        /* dont forget to delete prototypes */
        vkDestroyImage(vulkan_context->device, depth_prototype, NULL);
    }

    if(MSG_IS_ERROR(createSwapchain(vulkan_context, msg_callback, context))) {
        MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_SWAPCHAIN_CREATE, "failed to create screen targets");
    }

    /* CREATE DESCRIPTOR POOL */ {
        const VkDescriptorPoolSize descriptor_pool_sizes[] ={
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_DESCRIPTOR_SETS * MAX_BINDINGS_PER_SET},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, MAX_DESCRIPTOR_SETS * MAX_BINDINGS_PER_SET},
            {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, MAX_DESCRIPTOR_SETS * MAX_BINDINGS_PER_SET},
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, MAX_DESCRIPTOR_SETS * MAX_BINDINGS_PER_SET}
        };
        const VkDescriptorPoolCreateInfo descriptor_pool_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .maxSets = MAX_DESCRIPTOR_SETS,
            .poolSizeCount = ARRAY_COUNT(descriptor_pool_sizes),
            .pPoolSizes = descriptor_pool_sizes
        };
        if(vkCreateDescriptorPool(vulkan_context->device, &descriptor_pool_info, NULL, &context->descriptor_pool) != VK_SUCCESS) {
            MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_DESCRIPTOR_POOL_CREATE, "failed to create descriptor pool");
        };
    }

    if(render_info->resource_count == 0) goto _skip_resource_creation;

    /* CREATE RESOURCES */ {        
        const u32 resource_count = render_info->resource_count;
        const RenderResourceInfo* resource_infos = render_info->resource_infos;
        const DeviceType device_type = vulkan_context->device_type;

        /* validation */
        for(u32 i = 0; i < resource_count; i++) {
            /* check resource type */
            if(resource_infos[i].type != RENDER_RESOURCE_TYPE_STORAGE_BUFFER && resource_infos[i].type != RENDER_RESOURCE_TYPE_UNIFORM_BUFFER) {
                strcpy(msg_log, "resource info has invalid type: ");
                strcat_u32(msg_log, resource_infos[i].type);
                strcat(msg_log, "{binding: ");
                strcat_u32(msg_log, resource_infos[i].binding);
                strcat(msg_log, " set: ");
                strcat_u32(msg_log, resource_infos[i].set);
                strcat(msg_log, "}" LOCATION_TRACE);
                
                MSG_CALLBACK_NO_TRACE(msg_callback, MSG_CODE_ERROR_VK_RESOURCE_INFO_INVALID, msg_log);
            }
            /* check set binding location */
            for(u32 j = 0; j < i; j++) {
                if(resource_infos[i].set == resource_infos[j].set && resource_infos[i].binding == resource_infos[j].binding) {
                    strcpy(msg_log, "2 or more resource infos have common location {binding: ");
                    strcat_u32(msg_log, resource_infos[i].binding);
                    strcat(msg_log, " set: ");
                    strcat_u32(msg_log, resource_infos[i].set);
                    strcat(msg_log, "}" LOCATION_TRACE);
                    
                    MSG_CALLBACK_NO_TRACE(msg_callback, MSG_CODE_ERROR_VK_RESOURCE_INFO_INVALID, msg_log);
                }
            }
            /* check for usage flags */
            if(resource_infos[i].mutability != RENDER_RESOURCE_HOST_IMMUTABLE && resource_infos[i].mutability != RENDER_RESOURCE_HOST_MUTABLE) {
                strcpy(msg_log, "resource info invalid usage flag {binding: ");
                strcat_u32(msg_log, resource_infos[i].binding);
                strcat(msg_log, " set: ");
                strcat_u32(msg_log, resource_infos[i].set);
                strcat(msg_log, "}" LOCATION_TRACE);
                
                MSG_CALLBACK_NO_TRACE(msg_callback, MSG_CODE_ERROR_VK_RESOURCE_INFO_INVALID, msg_log);
            }
        }

        /* only used if device model is descrete */
        VramAllocationInfo host_allocation_info = {.memory_type_flags = U32_MAX};
        /* used anyway (if not 0 size )*/
        VramAllocationInfo device_allocation_info = {.memory_type_flags = U32_MAX};

        /* create resources */
        for(u32 i = 0; i < resource_count; i++) {
            RenderResource* resource = &context->resources[resource_infos[i].set][resource_infos[i].binding];
            context->resources_plain[context->resource_count++] = resource;
            /* if buffer */
            if(resource_infos[i].type == RENDER_RESOURCE_TYPE_UNIFORM_BUFFER || resource_infos[i].type == RENDER_RESOURCE_TYPE_STORAGE_BUFFER) {
                *resource = (RenderResource) {
                    .type = resource_infos[i].type,
                    .size = resource_infos[i].size
                };

                u32 main_usage = 0;
                if(resource_infos[i].type == RENDER_RESOURCE_TYPE_UNIFORM_BUFFER) main_usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
                if(resource_infos[i].type == RENDER_RESOURCE_TYPE_STORAGE_BUFFER) main_usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

                /* if immutable create in device memory only */
                if(resource_infos[i].mutability == RENDER_RESOURCE_HOST_IMMUTABLE) {
                    const VkBufferCreateInfo buffer_info = {
                        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                        .usage = main_usage,
                        .pQueueFamilyIndices = &vulkan_context->render_family_id,
                        .queueFamilyIndexCount = 1,
                        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                        .size = resource_infos[i].size
                    };
                    if(vkCreateBuffer(vulkan_context->device, &buffer_info, NULL, (VkBuffer*)(&resource->device_resource)) != VK_SUCCESS) {
                        strcpy(msg_log, "failed to create device uniform buffer, resource {binding: ");
                        strcat_u32(msg_log, resource_infos[i].binding);
                        strcat(msg_log, " set :");
                        strcat_u32(msg_log, resource_infos[i].set);
                        strcat(msg_log, "}" LOCATION_TRACE);
                        MSG_CALLBACK_NO_TRACE(msg_callback, MSG_CODE_ERROR_VK_BUFFER_CREATE, msg_log);
                    }

                    VkMemoryRequirements device_buffer_requirements = (VkMemoryRequirements){0};
                    vkGetBufferMemoryRequirements(vulkan_context->device, (VkBuffer)resource->device_resource, &device_buffer_requirements);

                    device_allocation_info.size = resource->device_offset = ALIGN(device_allocation_info.size, device_buffer_requirements.alignment);
                    device_allocation_info.size += device_buffer_requirements.size;
                    continue;
                }
                /* next different on different device types */
                if(device_type == DEVICE_TYPE_DESCRETE) {
                    if(resource_infos[i].mutability == RENDER_RESOURCE_HOST_MUTABLE) {
                        /* create two buffers one transfer src and one uniform and transfer dst */
                        const VkBufferCreateInfo device_buffer_info = {
                            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                            .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | main_usage,
                            .pQueueFamilyIndices = &vulkan_context->render_family_id,
                            .queueFamilyIndexCount = 1,
                            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                            .size = resource_infos[i].size
                        };
                        const VkBufferCreateInfo host_buffer_info = {
                            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                            .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                            .pQueueFamilyIndices = &vulkan_context->render_family_id,
                            .queueFamilyIndexCount = 1,
                            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                            .size = resource_infos[i].size
                        };
                        /* create device buffer */
                        if(vkCreateBuffer(vulkan_context->device, &device_buffer_info, NULL, (VkBuffer*)(&resource->device_resource)) != VK_SUCCESS) {
                            strcpy(msg_log, "failed to create device uniform buffer, resource {binding: ");
                            strcat_u32(msg_log, resource_infos[i].binding);
                            strcat(msg_log, " set: ");
                            strcat_u32(msg_log, resource_infos[i].set);
                            strcat(msg_log, "}" LOCATION_TRACE);
                            MSG_CALLBACK_NO_TRACE(msg_callback, MSG_CODE_ERROR_VK_BUFFER_CREATE, msg_log);
                        }
                        /* create host buffer */
                        if(vkCreateBuffer(vulkan_context->device, &host_buffer_info, NULL, (VkBuffer*)(&resource->host_resource)) != VK_SUCCESS) {
                            strcpy(msg_log, "failed to create host uniform buffer, resource {binding: ");
                            strcat_u32(msg_log, resource_infos[i].binding);
                            strcat(msg_log, " set: ");
                            strcat_u32(msg_log, resource_infos[i].set);
                            strcat(msg_log, "}" LOCATION_TRACE);
                            MSG_CALLBACK_NO_TRACE(msg_callback, MSG_CODE_ERROR_VK_BUFFER_CREATE, msg_log);
                        }

                        VkMemoryRequirements device_buffer_requirements = (VkMemoryRequirements){0};
                        VkMemoryRequirements host_buffer_requirements = (VkMemoryRequirements){0};
                        vkGetBufferMemoryRequirements(vulkan_context->device, (VkBuffer)resource->device_resource, &device_buffer_requirements);
                        vkGetBufferMemoryRequirements(vulkan_context->device, (VkBuffer)resource->host_resource, &host_buffer_requirements);

                        device_allocation_info.size = resource->device_offset = ALIGN(device_allocation_info.size, device_buffer_requirements.alignment);
                        device_allocation_info.size += device_buffer_requirements.size;
                        device_allocation_info.memory_type_flags &= device_buffer_requirements.memoryTypeBits;

                        host_allocation_info.size = resource->host_offset = ALIGN(host_allocation_info.size, host_buffer_requirements.alignment);
                        host_allocation_info.size += host_buffer_requirements.size;
                        host_allocation_info.memory_type_flags &= host_buffer_requirements.memoryTypeBits;
                    }
                    continue;
                }
                /* integrated gpu operates on host visible heap */
                if(device_type == DEVICE_TYPE_INTEGRATED) {
                    if(resource_infos[i].mutability == RENDER_RESOURCE_HOST_MUTABLE) {
                        /* create one device buffer because host memory heap is used */
                        const VkBufferCreateInfo device_buffer_info = {
                            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                            .usage = main_usage,
                            .pQueueFamilyIndices = &vulkan_context->render_family_id,
                            .queueFamilyIndexCount = 1,
                            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                            .size = resource_infos[i].size
                        };
                        /* create device buffer */
                        if(vkCreateBuffer(vulkan_context->device, &device_buffer_info, NULL, (VkBuffer*)(&resource->device_resource)) != VK_SUCCESS) {
                            strcpy(msg_log, "failed to create device uniform buffer, resource {binding: ");
                            strcat_u32(msg_log, resource_infos[i].binding);
                            strcat(msg_log, " set: ");
                            strcat_u32(msg_log, resource_infos[i].set);
                            strcat(msg_log, "}" LOCATION_TRACE);
                            MSG_CALLBACK_NO_TRACE(msg_callback, MSG_CODE_ERROR_VK_BUFFER_CREATE, msg_log);
                        }

                        VkMemoryRequirements device_buffer_requirements = (VkMemoryRequirements){0};
                        vkGetBufferMemoryRequirements(vulkan_context->device, (VkBuffer)resource->device_resource, &device_buffer_requirements);

                        device_allocation_info.size = resource->device_offset = ALIGN(device_allocation_info.size, device_buffer_requirements.alignment);
                        device_allocation_info.size += device_buffer_requirements.size;
                        device_allocation_info.memory_type_flags &= device_buffer_requirements.memoryTypeBits;
                    }
                    continue;
                }
            }
            /* if storage buffer */
            if(resource_infos[i].set == RENDER_RESOURCE_TYPE_STORAGE_BUFFER) {

            }
        }

        /* allocate memory */
        if(host_allocation_info.size != 0) {
            if(!(context->resource_host_allocation = allocateVram(vulkan_context, &host_allocation_info))) {
                MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_ALLOCATE_VRAM, "faield to allocate vram for host side resources");
            }
        }
        if(device_allocation_info.size != 0) {
            if(!(context->resource_device_allocation = allocateVram(vulkan_context, &device_allocation_info))) {
                MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_ALLOCATE_VRAM, "faield to allocate vram for device side resources");
            }
        }

        /* bind memory to resources */
        const u32 bind_resource_count = context->resource_count;
        const RenderResource** bind_resources = (const RenderResource**)context->resources_plain;
        for(u32 i = 0; i < bind_resource_count; i++) {
            if(bind_resources[i]->type == RENDER_RESOURCE_TYPE_UNIFORM_BUFFER || bind_resources[i]->type == RENDER_RESOURCE_TYPE_STORAGE_BUFFER) {
                if(vkBindBufferMemory(vulkan_context->device, (VkBuffer)bind_resources[i]->device_resource, context->resource_device_allocation->memory, bind_resources[i]->device_offset) != VK_SUCCESS) {
                    MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_BIND_RESOURCE_MEMORY, "failed to bind device buffer memory");
                }
                if(bind_resources[i]->host_resource) {
                    if(vkBindBufferMemory(vulkan_context->device, (VkBuffer)bind_resources[i]->host_resource, context->resource_host_allocation->memory, bind_resources[i]->host_offset) != VK_SUCCESS) {
                        MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_BIND_RESOURCE_MEMORY, "failed to bind device buffer memory");
                    }
                }
            }
        }

        /* LOG */ {
            strcpy(msg_log, "created resources count: ");
            strcat_u32(msg_log, context->resource_count);
            MSG_CALLBACK_NO_TRACE(msg_callback, MSG_CODE_SUCCESS, msg_log);
        }
    }
   
    /* DESCRIPTOR SETS */ {
        typedef struct {
            VkDescriptorBufferInfo buffer_infos [MAX_DESCRIPTOR_SETS * MAX_BINDINGS_PER_SET];
            VkWriteDescriptorSet descriptor_writes[MAX_DESCRIPTOR_SETS * MAX_BINDINGS_PER_SET];
            u32 descriptor_write_set_ids[MAX_DESCRIPTOR_SETS * MAX_BINDINGS_PER_SET];
            VkDescriptorSetLayoutBinding bindings [MAX_BINDINGS_PER_SET];
        } DescriptorTempBuffer;
        
        u32 buffer_info_count = 0;
        u32 descriptor_write_count = 0;

        DescriptorTempBuffer* temp_buffer = malloc(sizeof(DescriptorTempBuffer));
        if(!temp_buffer) {
            MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_BUFFER_MALLOC_FAIL, "failed to allocate temporary descriptor buffer");
        }
        *temp_buffer = (DescriptorTempBuffer){0};

        for(u32 i = 0; i < MAX_DESCRIPTOR_SETS; i++) {
            u32 binding_count = 0;
            /* generate VkDescriptorSetLayoutBinding and VkWriteDescriptorSet infos */
            for(u32 j = 0; j < MAX_BINDINGS_PER_SET; j++) {
                const RenderResource* resource = &context->resources[i][j];
                /* slots that are not used marked as none, so we dont count them */
                if(resource->type == RENDER_RESOURCE_TYPE_NONE) continue;
                /* detect type */
                u32 descriptor_type = U32_MAX;
                VkDescriptorBufferInfo* buffer_info = NULL;
                /* if uniform buffer */
                if(resource->type == RENDER_RESOURCE_TYPE_UNIFORM_BUFFER) {
                    descriptor_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

                    buffer_info = &temp_buffer->buffer_infos[buffer_info_count++];
                    *buffer_info = (VkDescriptorBufferInfo) {
                        .buffer = resource->device_resource,
                        .offset = 0,
                        .range = resource->size
                    };
                }
                /* if storage buffer */
                if(resource->type == RENDER_RESOURCE_TYPE_STORAGE_BUFFER) {
                    descriptor_type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

                    buffer_info = &temp_buffer->buffer_infos[buffer_info_count++];
                    *buffer_info = (VkDescriptorBufferInfo) {
                        .buffer = resource->device_resource,
                        .offset = 0,
                        .range = resource->size
                    };
                }
                /* set binding info */
                temp_buffer->bindings[binding_count++] = (VkDescriptorSetLayoutBinding) {
                    .binding = j,
                    .descriptorCount = 1,
                    .descriptorType = descriptor_type,
                    .stageFlags = VK_SHADER_STAGE_ALL
                };
                /* create descriptor write info */
                temp_buffer->descriptor_write_set_ids[descriptor_write_count] = i;
                temp_buffer->descriptor_writes[descriptor_write_count++] = (VkWriteDescriptorSet) {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstBinding = j,
                    .descriptorCount = 1,
                    .descriptorType = descriptor_type,
                    .pBufferInfo = buffer_info
                };
            }
            /* skip if no bindings */
            if(binding_count == 0) continue;
            /* create descriptor set layout */
            VkDescriptorSetLayoutCreateInfo layout_info = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                .pBindings = temp_buffer->bindings,
                .bindingCount = binding_count
            };
            if(vkCreateDescriptorSetLayout(vulkan_context->device, &layout_info, NULL, &context->descriptor_set_layouts[context->descriptor_set_count++]) != VK_SUCCESS) {
                MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_CREATE_DESCRIPTOR_SET_LAYOUT, "failed to create descriptor set layout");
            };
        }
        
        /* create descriptor sets */
        const VkDescriptorSetAllocateInfo allocate_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = context->descriptor_pool,
            .descriptorSetCount = context->descriptor_set_count,
            .pSetLayouts = context->descriptor_set_layouts
        };
        if(vkAllocateDescriptorSets(vulkan_context->device, &allocate_info, context->descriptor_sets) != VK_SUCCESS) {
            MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_ALLOCATE_DESCRIPTOR_SETS, "failed to create descriptor sets");
        }
        
        /* write descriptors */
        for(u32 i = 0; i < descriptor_write_count; i++) {
            temp_buffer->descriptor_writes[i].dstSet = context->descriptor_sets[temp_buffer->descriptor_write_set_ids[i]];
        }
        vkUpdateDescriptorSets(vulkan_context->device, descriptor_write_count, temp_buffer->descriptor_writes, 0, NULL);

        free(temp_buffer);

        /* LOG */ {
            strcpy(msg_log, "created descriptors set count: ");
            strcat_u32(msg_log, context->descriptor_set_count);
            MSG_CALLBACK_NO_TRACE(msg_callback, MSG_CODE_SUCCESS, msg_log);
        }
    }

    _skip_resource_creation: {}

    /* CREATE PIPELINE LAYOUTS */ {
        /* create empty descriptor set layout */
        const VkDescriptorSetLayoutCreateInfo empty_set_layout_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO
        };
        if(vkCreateDescriptorSetLayout(vulkan_context->device, &empty_set_layout_info, NULL, &context->empty_set_layout) != VK_SUCCESS) {
            MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_CREATE_DESCRIPTOR_SET_LAYOUT, "failed to create empty descriptor set layout");
        }
        /* create empty pipeline layout */
        const VkPipelineLayoutCreateInfo empty_pipeline_layout_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 1,
            .pSetLayouts = &context->empty_set_layout
        };
        if(vkCreatePipelineLayout(vulkan_context->device, &empty_pipeline_layout_info, NULL, &context->empty_pipeline_layout) != VK_SUCCESS) {
            MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_CREATE_DESCRIPTOR_SET_LAYOUT, "failed to create empty pipeline layout");
        }

        /* if layout sets are present (resources werent skipped), create full pipeline layout */
        if(context->descriptor_set_count != 0) {
            /* create empty pipeline layout */
            const VkPipelineLayoutCreateInfo full_pipeline_layout_info = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                .setLayoutCount = context->descriptor_set_count,
                .pSetLayouts = context->descriptor_set_layouts
            };
            if(vkCreatePipelineLayout(vulkan_context->device, &full_pipeline_layout_info, NULL, &context->full_pipeline_layout) != VK_SUCCESS) {
                MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_CREATE_DESCRIPTOR_SET_LAYOUT, "failed to create empty pipeline layout");
            }
        }
    }

    /* CREATE PIPELINES */ {
        const u32 pipeline_count = render_info->pipeline_count;
        const RenderPipelineInfo* pipeline_infos = render_info->pipeline_infos;
        /* validation */
        for(u32 i = 0; i < pipeline_count; i++) {
            /* check type */
            if(pipeline_infos[i].type != RENDER_PIPELINE_TYPE_GRAPHICS && pipeline_infos[i].type != RENDER_PIPELINE_TYPE_COMPUTE) {
                strcpy(msg_log, "render pipeline has invalid type id: ");
                strcat_u32(msg_log, i);
                strcat(msg_log, " name: \"");
                if(pipeline_infos[i].name) {
                    strcat(msg_log, pipeline_infos[i].name);
                }
                strcat(msg_log, "\"" LOCATION_TRACE);
                MSG_CALLBACK_NO_TRACE(msg_callback, MSG_CODE_ERROR_VK_RENDER_PIPELINE_INVALID, msg_log);
            }
            /* graphics pipeline contains only vertex and fragment shaders */
            if(pipeline_infos[i].type == RENDER_PIPELINE_TYPE_GRAPHICS && (!pipeline_infos[i].vertex_shader || !pipeline_infos[i].fragment_shader || pipeline_infos[i].compute_shader)) {
                strcpy(msg_log, "render graphics pipeline has invalid shaders id: ");
                strcat_u32(msg_log, i);
                strcat(msg_log, " name: \"");
                if(pipeline_infos[i].name) {
                    strcat(msg_log, pipeline_infos[i].name);
                }
                strcat(msg_log, "\"" LOCATION_TRACE);
                MSG_CALLBACK_NO_TRACE(msg_callback, MSG_CODE_ERROR_VK_RENDER_PIPELINE_INVALID, msg_log);
            }
            /* compute pipeline should only have compute shader */
            if(pipeline_infos[i].type == RENDER_PIPELINE_TYPE_GRAPHICS && (!pipeline_infos[i].compute_shader || pipeline_infos[i].vertex_shader || pipeline_infos[i].fragment_shader)) {
                strcpy(msg_log, "render compute pipeline has invalid shaders id: ");
                strcat_u32(msg_log, i);
                strcat(msg_log, " name: \"");
                if(pipeline_infos[i].name) {
                    strcat(msg_log, pipeline_infos[i].name);
                }
                strcat(msg_log, "\"" LOCATION_TRACE);
                MSG_CALLBACK_NO_TRACE(msg_callback, MSG_CODE_ERROR_VK_RENDER_PIPELINE_INVALID, msg_log);
            }
        }
        
        /* create pipelines */
        for(u32 i = 0; i < pipeline_count; i++) {

        }
    }

    /* CREATE COMMAND POOL */ {
        VkCommandPoolCreateInfo comman_pool_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .queueFamilyIndex = vulkan_context->render_family_id
        };
        if(vkCreateCommandPool(vulkan_context->device, &comman_pool_info, NULL, &context->command_pool)) {
            MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_COMMAND_POOL_CREATE, "failed to create render command pool");
        }
    }

    *render_context = context;
    MSG_CALLBACK_NO_TRACE(msg_callback, MSG_CODE_SUCCESS, "created render context");

    return MSG_CODE_SUCCESS;
}

i32 destroyRenderContext(VulkanContext* vulkan_context, MsgCallback_pfn msg_callback, RenderContext* render_context) {
    /* destroy command pool */
    vkDestroyCommandPool(vulkan_context->device, render_context->command_pool, NULL);
    
    /* DESTROY PIPELINE LAYOUTS */ {
        vkDestroyPipelineLayout(vulkan_context->device, render_context->empty_pipeline_layout, NULL);
        vkDestroyDescriptorSetLayout(vulkan_context->device, render_context->empty_set_layout, NULL);

        if(render_context->full_pipeline_layout) {
            vkDestroyPipelineLayout(vulkan_context->device, render_context->full_pipeline_layout, NULL);
        }
    }

    /* DESTROY RESOURCES AND DESCRIPTORS */ {
        if(render_context->descriptor_set_count == 0 || render_context->resource_count == 0) goto _skip_resources;

        for(u32 i = 0; i < MAX_DESCRIPTOR_SETS; i++) {
            for(u32 j = 0; j < MAX_BINDINGS_PER_SET; j++) {
                RenderResource* resource = &render_context->resources[i][j];
                if(resource->type == RENDER_RESOURCE_TYPE_UNIFORM_BUFFER || resource->type == RENDER_RESOURCE_TYPE_STORAGE_BUFFER) {
                    if(resource->device_resource) {
                        vkDestroyBuffer(vulkan_context->device, (VkBuffer)resource->device_resource, NULL);
                    }
                    if(resource->host_resource) {
                        vkDestroyBuffer(vulkan_context->device, (VkBuffer)resource->host_resource, NULL);
                    }
                }
            }
            vkDestroyDescriptorSetLayout(vulkan_context->device, render_context->descriptor_set_layouts[i], NULL);
        }
        /* destroy descriptor pool with descriptor sets */
        vkDestroyDescriptorPool(vulkan_context->device, render_context->descriptor_pool, NULL);

        _skip_resources: {}
    }

    /* SWAPCHAIN AND DEPTH */ {
        vkDestroyImageView(vulkan_context->device, render_context->screen.depth_image_view, NULL);
        vkDestroyImage(vulkan_context->device, render_context->screen.depth_image, NULL);

        const u32 swapchain_image_count = render_context->screen.swapchain_image_count;
        for(u32 i = 0; i < swapchain_image_count; i++) {
            vkDestroyImageView(vulkan_context->device, render_context->screen.swapchain_image_views[i], NULL);
        }
        vkDestroySwapchainKHR(vulkan_context->device, render_context->screen.swapchain, NULL);
    }
    
    free(render_context);

    MSG_CALLBACK_NO_TRACE(msg_callback, MSG_CODE_SUCCESS, "destroyed render context");
    return MSG_CODE_SUCCESS;
}
