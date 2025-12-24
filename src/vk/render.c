#include "vk.h"

#define MAX_SWAPCHAIN_IMAGE_COUNT 8
#define MAX_DESCRIPTOR_SETS 16
#define MAX_DESCRIPTOR_BINDING_COUNT 32

/* used for swapchain parameters selection 
    and accessing info about swapchain from outside */
typedef struct {
    VkSurfaceFormatKHR surface_format;
    VkPresentModeKHR present_mode;
    VkExtent2D extent;
    VkSurfaceTransformFlagsKHR surface_transform;
    u32 min_image_count;
    /* depth attachment */
    VkFormat depth_format;
} SwapchainParams;

/* local context available in scope of that file */
typedef struct {
    VkImage swapchain_images[MAX_SWAPCHAIN_IMAGE_COUNT];
    VkImageView swapchain_image_views[MAX_SWAPCHAIN_IMAGE_COUNT];
    VkSwapchainKHR swapchain;
    u32 swapchain_image_count;
    /*depth*/
    VkImage depth_image;
    VkImageView depth_view;

    /* info about swapchain format etc */
    SwapchainParams swapchain_params;
    VkDeviceMemory targets_allocation;
    /* commands */
    VkCommandPool command_pool;
    /* descriptors */
    VkDescriptorPool descirptor_pool;
    VkDescriptorSet descriptor_set;
    VkDescriptorSetLayout descriptor_set_layout;
} RenderContext;


/* selects optimal swapchain params */
b32 getSwapchainParams(VkSurfaceKHR surface, VkPhysicalDevice device, GLFWwindow* window, SwapchainParams* swapchain_params) {
    #define MAX_SURFACE_FORMATS_COUNT 512
    #define MAX_PRESENT_MODES_COUNT 256

    /* FORMAT */{
        VkSurfaceFormatKHR surface_formats[MAX_SURFACE_FORMATS_COUNT] = {0};
        u32 format_count = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count, NULL);
        format_count = MIN(format_count, MAX_SURFACE_FORMATS_COUNT);
        if(format_count == 0) return FALSE;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count, surface_formats);

        /* find best surface format */
        swapchain_params->surface_format = surface_formats[0];
        for(u32 i = 0; i < format_count; i++) {
            if(surface_formats[i].format == VK_FORMAT_B8G8R8A8_SRGB && surface_formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                swapchain_params->surface_format = (VkSurfaceFormatKHR) {
                    VK_FORMAT_B8G8R8A8_SRGB,
                    VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
                };
            }
        }
    }   

    /* PRESENT MODE */ {
        VkPresentModeKHR present_modes[MAX_PRESENT_MODES_COUNT] = {0};
        u32 present_mode_count = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_mode_count, NULL);
        present_mode_count = MIN(present_mode_count, MAX_PRESENT_MODES_COUNT);
        if(present_mode_count == 0) return FALSE;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_mode_count, present_modes);
        
        swapchain_params->present_mode = VK_PRESENT_MODE_FIFO_KHR;
        for(u32 i = 0; i < present_mode_count; i++) {
            if(present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
                swapchain_params->present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
            }
        }
    }

    /* CAPABILITIES */ {
        VkSurfaceCapabilitiesKHR surface_capabilities;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &surface_capabilities);

        swapchain_params->min_image_count = MIN(MAX_SWAPCHAIN_IMAGE_COUNT, surface_capabilities.minImageCount);
        swapchain_params->surface_transform = surface_capabilities.currentTransform;

        /* compute swapchain size */
        i32 window_width, window_height;
        glfwGetFramebufferSize(window, &window_width, &window_height);
        /* might return big values so we clamp that down , but it might be ub still */
        swapchain_params->extent.width = CLAMP(surface_capabilities.minImageExtent.width, surface_capabilities.maxImageExtent.width, (u32)window_width);
        swapchain_params->extent.height = CLAMP(surface_capabilities.minImageExtent.height, surface_capabilities.maxImageExtent.height, (u32)window_height);
    }

    /* DEPTH FORMAT */ {
        /* search options that we will be iterating trough */
        const u32 depth_formats_count = 3;
        const VkFormat depth_formats[3] = {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT};
        
        for(u32 i = 0; i < depth_formats_count; i++) {
            VkFormatProperties format_properties = (VkFormatProperties){0};
            vkGetPhysicalDeviceFormatProperties(device, depth_formats[i], &format_properties);
            /* check if it is suitable */
            if (format_properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
                swapchain_params->depth_format = depth_formats[i];
                goto _found_depth_format; 
            }
        }
        //_not_found_depth_format:
        return FALSE;
        _found_depth_format: {}
    }

    return TRUE;
}

/* called only in renderCreateContext during initial swapchain creation and when window is resized (also creates depth buffer)*/
i32 renderCreateSwapchain(const VulkanContext* vulkan_context, msg_callback_pfn msg_callback, RenderContext* render_context) {

    /* COLOR */ {
        /* create swapchain based on selected params*/
        VkSwapchainCreateInfoKHR swapchain_info = {
            .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .surface = vulkan_context->surface,
            .minImageCount = render_context->swapchain_params.min_image_count,
            .imageFormat = render_context->swapchain_params.surface_format.format,
            .imageColorSpace = render_context->swapchain_params.surface_format.colorSpace,
            .imageExtent = render_context->swapchain_params.extent,
            .presentMode = render_context->swapchain_params.present_mode,
            .preTransform = render_context->swapchain_params.surface_transform,
            .imageArrayLayers = 1,
            .pQueueFamilyIndices = &vulkan_context->render_family_id,
            .queueFamilyIndexCount = 1,
            .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            .clipped = TRUE
        };
        if(vkCreateSwapchainKHR(vulkan_context->device, &swapchain_info, NULL, &render_context->swapchain) != VK_SUCCESS) {
            MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_SWAPCHAIN_CREATE, "failed to create vulkan swapchain");
        }
        /* get images from swapchain to render into them later */
        vkGetSwapchainImagesKHR(vulkan_context->device, render_context->swapchain, &render_context->swapchain_image_count, NULL);
        if(render_context->swapchain_image_count > MAX_SWAPCHAIN_IMAGE_COUNT) {
            MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_SWAPCHAIN_TOO_MANY_IMAGES, "too many images in vulkan swapchain");
        }
        vkGetSwapchainImagesKHR(vulkan_context->device, render_context->swapchain, &render_context->swapchain_image_count, render_context->swapchain_images);

        /* create views to images */
        for(u32 i = 0; i < render_context->swapchain_image_count; i++) {
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
                .format = render_context->swapchain_params.surface_format.format,
                .image = render_context->swapchain_images[i]
            };

            if(vkCreateImageView(vulkan_context->device, &view_info, NULL, render_context->swapchain_image_views + i) != VK_SUCCESS) {
                MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_IMAGE_VIEW_CREATE, "failed to create vulkan swapchain image view");
            }
        }
    }
    
    /* DEPTH */ {
        /* create depth image */
        VkImageCreateInfo depth_image_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .extent = (VkExtent3D){
                .width = render_context->swapchain_params.extent.width, 
                .height = render_context->swapchain_params.extent.height, 
                .depth = 1
            },
            .format = render_context->swapchain_params.depth_format,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = 1,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE
        };
        if(vkCreateImage(vulkan_context->device, &depth_image_info, NULL, &render_context->depth_image) != VK_SUCCESS) {
            MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_IMAGE_CREATE, "failed to create render depth image");
        }
        /* @(FIX): binding with zero offset, but that might change in the future */
        if(vkBindImageMemory(vulkan_context->device, render_context->depth_image, render_context->targets_allocation, 0) != VK_SUCCESS) {
            MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_IMAGE_BIND_MEMORY, "failed to bind render depth image to memory");
        }

        /* create depth view */
        VkImageViewCreateInfo depth_view_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .image = render_context->depth_image,
            .format = render_context->swapchain_params.depth_format,
            .subresourceRange = (VkImageSubresourceRange) {
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
                .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT
            }
        };
        if(vkCreateImageView(vulkan_context->device, &depth_view_info, NULL, &render_context->depth_view) != VK_SUCCESS) {
            MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_IMAGE_VIEW_CREATE, "failed to create render depth view");
        }
    }

    return MSG_CODE_SUCCESS;
}

/* runs main render loop */
i32 renderLoop(const VulkanContext* vulkan_context, msg_callback_pfn msg_callback, const RenderContext* render_context) {
    typedef struct {
        VkSemaphore image_submit_semaphores[MAX_SWAPCHAIN_IMAGE_COUNT];
        VkSemaphore image_available_semaphore;
        VkSemaphore image_finished_semaphore;
        VkFence frame_fence;
        VkImageMemoryBarrier image_top_barrier;
        VkImageMemoryBarrier image_bottom_barrier;
    } SyncObjects;
    typedef struct {
        VkRenderingAttachmentInfoKHR screen_color;
        VkRenderingAttachmentInfoKHR screen_depth;
    } Attachments;

    SyncObjects sync_objects = (SyncObjects){0};
    Attachments attachments = (Attachments){0};
    VkCommandBuffer command_buffer = NULL;


    /* COMMAND BUFFER */ {
        VkCommandBufferAllocateInfo cmbuffers_info = (VkCommandBufferAllocateInfo) {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandBufferCount = 1,
            .commandPool = render_context->command_pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY
        };
        if(vkAllocateCommandBuffers(vulkan_context->device, &cmbuffers_info, &command_buffer) != VK_SUCCESS) {
            MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_COMMAND_BUFFER_ALLOCATE, "failed to allocate command buffer");
        }
    }

    /* SYNC OBJECTS */ {
        VkSemaphoreCreateInfo semaphore_info = (VkSemaphoreCreateInfo) {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
        };
        VkFenceCreateInfo fence_info = (VkFenceCreateInfo) {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT
        };
        /* final submit semaphores */
        for(u32 i = 0; i < MAX_SWAPCHAIN_IMAGE_COUNT; i++) {
            if(vkCreateSemaphore(vulkan_context->device, &semaphore_info, NULL, sync_objects.image_submit_semaphores + i) != VK_SUCCESS) {
                MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_SEMAPHORE_CREATE, "failed to create image submit semaphore");
            }
        }
        /* image available image finished */
        if(vkCreateSemaphore(vulkan_context->device, &semaphore_info, NULL, &sync_objects.image_available_semaphore) != VK_SUCCESS) {
            MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_SEMAPHORE_CREATE, "failed to create image avaliable semaphore");
        };
        if(vkCreateSemaphore(vulkan_context->device, &semaphore_info, NULL, &sync_objects.image_finished_semaphore) != VK_SUCCESS) {
            MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_SEMAPHORE_CREATE, "failed to create image finished semaphore");
        };

        /* fence for gpu cpu sync */
        if(vkCreateFence(vulkan_context->device, &fence_info, NULL, &sync_objects.frame_fence) != VK_SUCCESS) {
            MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_FENCE_CREATE, "failed to create image finished semaphore");
        }

        /* barriers */
        sync_objects.image_top_barrier = (VkImageMemoryBarrier) {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            }
        };
        sync_objects.image_bottom_barrier = (VkImageMemoryBarrier) {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            }
        };
    }

    /* ATTACHMENTS */ {
        attachments.screen_color = (VkRenderingAttachmentInfoKHR) {
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
            .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE
        };

        attachments.screen_depth = (VkRenderingAttachmentInfoKHR) {
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
            .clearValue = (VkClearValue) {
                .depthStencil = (VkClearDepthStencilValue){
                    .depth = 1.0
                }
            },
            .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE
        };
    }

    /* render loop itself */
    while (!glfwWindowShouldClose(vulkan_context->window)) {
        glfwPollEvents();
        
        VkRenderingInfoKHR rendering_info = {
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,
            .colorAttachmentCount = 1,
            .pColorAttachments = &attachments.screen_color,
            .layerCount = 1,
            .pDepthAttachment = &attachments.screen_depth
        };
    }

    /* SYNC OBJECTS DESTROY */ {
        for(u32 i = 0; i < MAX_SWAPCHAIN_IMAGE_COUNT; i++) {
            vkDestroySemaphore(vulkan_context->device, sync_objects.image_submit_semaphores[i], NULL);
        }
        vkDestroySemaphore(vulkan_context->device, sync_objects.image_available_semaphore, NULL);
        vkDestroySemaphore(vulkan_context->device, sync_objects.image_finished_semaphore, NULL);
        vkDestroyFence(vulkan_context->device, sync_objects.frame_fence, NULL);
        sync_objects = (SyncObjects){0};
    }
    
    return MSG_CODE_SUCCESS;
}

/* creates render objects */
i32 renderCreateContext(const VulkanContext* vulkan_context, const RenderSettings* settings, msg_callback_pfn msg_callback, RenderContext* render_context) {
    /* select swapchain params */
    if(!getSwapchainParams(vulkan_context->surface, vulkan_context->physical_device, vulkan_context->window, &render_context->swapchain_params)) {
        MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_SURFACE_STATS_NOT_SUITABLE, "vulkan surface stats not suitable");
    }

    /* ALLOCATIONS */ {
        /* get max possible screen resolution, in order to make a constant allocation that can be reused in any case */
        const GLFWvidmode* video_mode = glfwGetVideoMode(glfwGetPrimaryMonitor());

        /* create dummy depth texture for memory requirements */
        VkImage depth_prototype = NULL;
        VkImageCreateInfo depth_prototype_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .extent = (VkExtent3D){
                .width = video_mode->width, 
                .height = video_mode->height, 
                .depth = 1
            },
            .format = render_context->swapchain_params.depth_format,
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

        /* allocate vram memory for prototypes */
        VkMemoryAllocateInfo targets_allocation_info = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize = depth_prototype_memory.size,
            .memoryTypeIndex = 0xffffffff
        };
        /* find suitable memory type */
        /* @(FIX): gpu memory allocation should be tracked, so its reasonable to put into some other file */
        VkPhysicalDeviceMemoryProperties memory_properties = (VkPhysicalDeviceMemoryProperties){0};
        vkGetPhysicalDeviceMemoryProperties(vulkan_context->physical_device, &memory_properties);
        for(u32 i = 0; i < memory_properties.memoryTypeCount; i++) {
            VkMemoryType memory_type = memory_properties.memoryTypes[i];
            if(
                (memory_type.propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) && 
                !(memory_type.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) &&
                (memory_type.propertyFlags & depth_prototype_memory.memoryTypeBits) &&
                memory_properties.memoryHeaps[memory_type.heapIndex].size > depth_prototype_memory.size
            ) {
                targets_allocation_info.memoryTypeIndex = i;
            }
        }
        /* allocation call */
        if(vkAllocateMemory(vulkan_context->device, &targets_allocation_info, NULL, &render_context->targets_allocation) != VK_SUCCESS) {
            MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_ALLOCATE_VRAM, "failed to allocate vram");
        }
        /* dont forget to delete prototypes */
        vkDestroyImage(vulkan_context->device, depth_prototype, NULL);
    }

    /* create swapchain and depth texture */
    if(MSG_IS_ERROR(renderCreateSwapchain(vulkan_context, msg_callback, render_context))) {
        MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_SWAPCHAIN_CREATE, "vulkan surface stats not suitable");
    }

    /* DESCRIPTORS */ {
        /* get enough space in pool for descriptors */
        VkDescriptorPoolCreateInfo descriptor_pool_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .maxSets = MAX_DESCRIPTOR_SETS,
            .pPoolSizes = (VkDescriptorPoolSize[]) {
                (VkDescriptorPoolSize) {
                    .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                    .descriptorCount = 128
                },
                (VkDescriptorPoolSize) {
                    .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                    .descriptorCount = 128
                }
            },
            .poolSizeCount = 2
        };
        if(vkCreateDescriptorPool(vulkan_context->device, &descriptor_pool_info, NULL, &render_context->descirptor_pool) != VK_SUCCESS) {
            MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_DESCRIPTOR_POOL_CREATE, "failed to create render descriptor pool");
        }

        /* if settings are null then nobody specified bindings */
        if(settings) {
            /* allocate descriptor binding array */
            VkDescriptorSetLayoutBinding descriptor_bindings[MAX_DESCRIPTOR_BINDING_COUNT] = {0};
            if(settings->binding_count > MAX_DESCRIPTOR_BINDING_COUNT) {
                MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_DESCRIPTOR_TOO_MANY_BINDINGS, "too many descriptor bindings");
            }

            /* @(FIX): add different stageFlags and different descriptor sets */
            for(u32 i = 0; i < settings->binding_count; i++) {
                descriptor_bindings[i] = (VkDescriptorSetLayoutBinding) {
                    .binding = settings->bindings[i].binding,
                    .stageFlags = VK_SHADER_STAGE_ALL,
                    .descriptorCount = 1
                };
                switch (settings->bindings[i].type) {
                    case RENDER_BINDING_TYPE_UNIFORM_BUFFER:
                        descriptor_bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                        break;
                    case RENDER_BIDNING_TYPE_STORAGE_BUFFER:
                        descriptor_bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                        break;
                    default:
                        MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_INVALID_BINDING_TYPE, "render binding type invalid");
                }
            }

            /* create layout set */
            VkDescriptorSetLayoutCreateInfo layout_create_info = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                .pBindings = descriptor_bindings,
                .bindingCount = settings->binding_count
            };
            if(vkCreateDescriptorSetLayout(vulkan_context->device, &layout_create_info, NULL, &render_context->descriptor_set_layout) != VK_SUCCESS) {
                MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_CREATE_DESCRIPTOR_SET_LAYOUT, "failed to create vulkan descriptor set layout");
            }

            /* allocate descriptor set based on layout */
            VkDescriptorSetAllocateInfo descriptor_set_info = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                .descriptorPool = render_context->descirptor_pool,
                .descriptorSetCount = 1,
                .pSetLayouts = &render_context->descriptor_set_layout
            };
            if(vkAllocateDescriptorSets(vulkan_context->device, &descriptor_set_info, &render_context->descriptor_set)) {
                MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_CREATE_DESCRIPTOR_SET, "failed to create descriptor set");
            }
        }
    }

    /* COMMAND BUFFERS */ {
        VkCommandPoolCreateInfo command_pool_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .queueFamilyIndex = vulkan_context->render_family_id,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
        };
        if(vkCreateCommandPool(vulkan_context->device, &command_pool_info, NULL, &render_context->command_pool) != VK_SUCCESS) {
            MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_COMMAND_POOL_CREATE, "failed to create render command pool");
        }
    }

    return MSG_CODE_SUCCESS;
}

/* destroys render objects and invalidates context struct */
void renderDestroyContext(const VulkanContext* vulkan_context, RenderContext* render_context) {

    /* COMMAND BUFFERS */ {
        vkDestroyCommandPool(vulkan_context->device, render_context->command_pool, NULL);
    }

    /* DESCRIPTORS */ {
        vkDestroyDescriptorSetLayout(vulkan_context->device, render_context->descriptor_set_layout, NULL);
        vkDestroyDescriptorPool(vulkan_context->device, render_context->descirptor_pool, NULL);
    }

    /* SWAPCHAIN IMAGES */ {
        for(u32 i = 0; i < render_context->swapchain_image_count; i++) {
            vkDestroyImageView(vulkan_context->device, render_context->swapchain_image_views[i], NULL);
        }
        vkDestroyImageView(vulkan_context->device, render_context->depth_view, NULL);
        vkDestroyImage(vulkan_context->device, render_context->depth_image, NULL);
    }

    /* FREE ALLOCATIONS */ {
        /* @(FIX) should be done by different module that manages memory */
        vkFreeMemory(vulkan_context->device, render_context->targets_allocation, NULL);
    }

    vkDestroySwapchainKHR(vulkan_context->device, render_context->swapchain, NULL);
    *render_context = (RenderContext){0};
}

i32 renderRun(const VulkanContext* vulkan_context, const RenderSettings* settings, msg_callback_pfn msg_callback) {
    RenderContext render_context = (RenderContext){0};
    if(MSG_IS_ERROR(renderCreateContext(vulkan_context, settings, msg_callback, &render_context))) {
        MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_CREATE_RENDER_CONTEXT, "failed to create render context");
    }

    if(MSG_IS_ERROR(renderLoop(vulkan_context, msg_callback, &render_context))) {
        MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_RENDER_LOOP_FAIL, "vulkan render loop error");
    }

    renderDestroyContext(vulkan_context, &render_context);
    return MSG_CODE_SUCCESS;
}
