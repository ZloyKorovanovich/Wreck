#include "vk.h"

#define MAX_SWAPCHAIN_IMAGE_COUNT 8
#define MAX_DESCRIPTOR_SETS 16
#define MAX_DESCRIPTOR_BINDING_COUNT 32

#define SHADER_VERTEX_ENTRY "vertexMain"
#define SHADER_FRAGMENT_ENTRY "fragmentMain"
#define SHADER_COMPUTE_ENTRY "computeMain"

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

/* local Render node analog */
typedef struct {
    RenderNodeType type;
    VkPipeline pipeline;
    VkPipelineLayout pipeline_layout; /* is a sencond read-only reference to pipeline layout */
    VkShaderModule vertex_shader;
    VkShaderModule fragment_shader;
    VkShaderModule compute_shader;
} PipelineNode;

typedef struct {
    RenderBindingType type;
    void* host_resource;
    void* device_resource;
    u64 host_offset; /* offset in vram allocation */
    u64 device_offset; /* offset in vram allocation */
    RenderMemoryWrite_pfn init_batch;
    RenderMemoryWrite_pfn frame_batch;
    u64 size;
} ResourceNode;

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
    VkPipelineLayout pipeline_layout;
    /* pipelines and resources */
    PipelineNode* pipeline_nodes;
    ResourceNode* resource_nodes;
    u32 pipeline_node_count;
    u32 resource_node_count;
    VkDeviceMemory device_resource_memory;
    VkDeviceMemory host_resource_memory;
} RenderContext;


/* remove shaders and compile directly to Pipeline node */
VkShaderModule createShaderModule(VkDevice device, const char* shader_path, ByteBuffer* read_buffer) {
    VkShaderModule shader_module = NULL;
    /* open file */
    FILE* file = fopen(shader_path, "rb");
    /* find file size */
    fseek(file, 0, SEEK_END);
    u64 shader_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    /* reallocate buffer if needed */
    if(shader_size > read_buffer->size) {
        read_buffer->size = MAX(shader_size, read_buffer->size + 4096);
        if(!(read_buffer->buffer = realloc(read_buffer->buffer, read_buffer->size))) {
            return NULL;
        }
    }
    /* read code from file */
    if(fread(read_buffer->buffer, 1, shader_size, file) != shader_size) {
        return NULL;
    }

    /* create module */
    VkShaderModuleCreateInfo shader_module_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pCode = (u32*)read_buffer->buffer,
        .codeSize = shader_size
    };
    if(vkCreateShaderModule(device, &shader_module_info, NULL, &shader_module) != VK_SUCCESS) {
        return NULL;
    }
    return shader_module;
}

/*@(FIX): add comments*/
VkBuffer createBuffer(VkDevice device, u64 size, u32 queue_family, u32 usage) {
    VkBuffer buffer = NULL;
    VkBufferCreateInfo buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .usage = usage,
        .size = size,
        .pQueueFamilyIndices = &queue_family,
        .queueFamilyIndexCount = 1,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    if(vkCreateBuffer(device, &buffer_info, NULL, &buffer) != VK_SUCCESS) {
        return NULL;
    }
    return buffer;
}


/* modiffies pipeline nodes */
b32 createGraphicsPipeline(VkDevice device, VkFormat color_format, VkFormat depth_format, PipelineNode* node) {
    /* shader descriptors array */
    VkPipelineShaderStageCreateInfo shader_stages[2] = {
        (VkPipelineShaderStageCreateInfo) {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pName = SHADER_VERTEX_ENTRY,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = node->vertex_shader
        },
        (VkPipelineShaderStageCreateInfo) {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pName = SHADER_FRAGMENT_ENTRY,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = node->fragment_shader
        }
    };

    /* dynamic states, changed on fly */
    VkPipelineDynamicStateCreateInfo dynamic_state = (VkPipelineDynamicStateCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2,
        .pDynamicStates = (const VkDynamicState[]){VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR}
    };
    /* settings for gpu pipeline */
    VkPipelineVertexInputStateCreateInfo vertex_input_state = (VkPipelineVertexInputStateCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexAttributeDescriptionCount = 0,
        .vertexBindingDescriptionCount = 0,
        .pVertexAttributeDescriptions = NULL,
        .pVertexBindingDescriptions = NULL
    };
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
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &color_format,
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
        .layout = node->pipeline_layout,
        .renderPass = NULL,
        .pNext = &rendering_create_info
    };

    if(vkCreateGraphicsPipelines(device, NULL, 1, &pipeline_info, NULL, &node->pipeline) != VK_SUCCESS) {
        return FALSE;
    }
    return TRUE;
}

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

/* called when render loop notices window resize condition */
i32 renderOnWindowResize(const VulkanContext* vulkan_context, msg_callback_pfn msg_callback, RenderContext* render_context) {
    vkDeviceWaitIdle(vulkan_context->device);
    
    /* DESTRUCTION */ {
        for(u32 i = 0; i < render_context->swapchain_image_count; i++) {
            vkDestroyImageView(vulkan_context->device, render_context->swapchain_image_views[i], NULL);
        }
        vkDestroyImageView(vulkan_context->device, render_context->depth_view, NULL);
        vkDestroyImage(vulkan_context->device, render_context->depth_image, NULL);
        vkDestroySwapchainKHR(vulkan_context->device, render_context->swapchain, NULL);
    }

    /* vkGetPhysicalDeviceSurfaceCapabilitiesKHR it changes surface resolution somehow */
    VkSurfaceCapabilitiesKHR surface_capabilities = (VkSurfaceCapabilitiesKHR){0};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vulkan_context->physical_device, vulkan_context->surface, &surface_capabilities);
    /* get glfw size to clamp if surface resolution is invalid */
    i32 width, height;
    glfwGetFramebufferSize(vulkan_context->window, &width, &height);

    render_context->swapchain_params.extent = (VkExtent2D) {
        MIN(surface_capabilities.currentExtent.width, (u32)width),
        MIN(surface_capabilities.currentExtent.height, (u32)height)
    };
    if(MSG_IS_ERROR(renderCreateSwapchain(vulkan_context, msg_callback, render_context))) {
        MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_SWAPCHAIN_RECREATE, "failed to recreate swapchain");
    }

    return MSG_CODE_SUCCESS;
}

/* runs main render loop */
i32 renderLoop(const VulkanContext* vulkan_context, msg_callback_pfn msg_callback, RenderContext* render_context) {
    typedef struct {
        VkSemaphore image_submit_semaphores[MAX_SWAPCHAIN_IMAGE_COUNT];
        VkSemaphore image_available_semaphore;
        VkSemaphore image_finished_semaphore;
        VkFence frame_fence;
        VkImageMemoryBarrier image_top_barrier;
        VkImageMemoryBarrier image_bottom_barrier;
    } SyncObjects;

    SyncObjects sync_objects = (SyncObjects){0};
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

    /* @(FIX): add comments */
    /* UPDATE DESCRIPTORS */ {
        typedef struct {
            VkWriteDescriptorSet write_sets[MAX_DESCRIPTOR_BINDING_COUNT];
            VkDescriptorBufferInfo buffer_infos[MAX_DESCRIPTOR_BINDING_COUNT];
            u32 write_set_count;
            u32 buffer_info_count;
        } ResourceDescriptorWriteBuffer;

        if(render_context->resource_node_count == 0) goto _skip_descriptors;

        ResourceDescriptorWriteBuffer* resource_descriptor_write = malloc(sizeof(ResourceDescriptorWriteBuffer));
        if(!resource_descriptor_write) {
            MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_BUFFER_MALLOC_FAIL, "failed to allocate descriptor buffer info array");
        }
        *resource_descriptor_write = (ResourceDescriptorWriteBuffer){0};

        for(u32 i = 0; i < render_context->resource_node_count; i++) {
            const ResourceNode* resource_node = &render_context->resource_nodes[i];
            if(resource_node->type == RENDER_BINDING_TYPE_UNIFORM_BUFFER) {
                resource_descriptor_write->buffer_infos[resource_descriptor_write->buffer_info_count] = (VkDescriptorBufferInfo) {
                    .buffer = resource_node->device_resource,
                    .offset = 0,
                    .range = resource_node->size
                };
                resource_descriptor_write->write_sets[resource_descriptor_write->write_set_count] = (VkWriteDescriptorSet) {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = render_context->descriptor_set,
                    .dstBinding = 0,
                    .dstArrayElement = 0,
                    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                    .descriptorCount = 1,
                    .pBufferInfo = &resource_descriptor_write->buffer_infos[resource_descriptor_write->buffer_info_count],
                    .pImageInfo = NULL,
                    .pTexelBufferView = NULL
                };
                resource_descriptor_write->buffer_info_count++;
                resource_descriptor_write->write_set_count++;
                continue;
            }

            /* @(FIX): add different render binding types */
        }
        vkUpdateDescriptorSets(vulkan_context->device, resource_descriptor_write->write_set_count, resource_descriptor_write->write_sets, 0, NULL);

        free(resource_descriptor_write);
        _skip_descriptors: {}
    }

    /* render loop itself */
    while (!glfwWindowShouldClose(vulkan_context->window)) {
        glfwPollEvents();

        u32 image_id = U32_MAX;
        /* AQUIRE */ {
            /* wait untill previous frame finishes */
            vkWaitForFences(vulkan_context->device, 1, &sync_objects.frame_fence, VK_TRUE, U64_MAX);
            VkResult image_acquire_result = vkAcquireNextImageKHR(vulkan_context->device, render_context->swapchain, U64_MAX, sync_objects.image_available_semaphore, NULL, &image_id);

            /* check if frambuffer should resize */
            if(image_acquire_result == VK_ERROR_OUT_OF_DATE_KHR || image_acquire_result == VK_SUBOPTIMAL_KHR) {
                if(MSG_IS_ERROR(renderOnWindowResize(vulkan_context, msg_callback, render_context))) {
                    MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_RESIZE_FAIL, "error occured during window resize in the beginning of loop");
                }
                continue;
            }
            vkResetFences(vulkan_context->device, 1, &sync_objects.frame_fence);
        }
        
        const VkCommandBufferBeginInfo command_buffer_begin_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
        };
        vkResetCommandBuffer(command_buffer, 0);
        vkBeginCommandBuffer(command_buffer, &command_buffer_begin_info);

        const VkImageMemoryBarrier image_top_barrier = {
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
            },
            .image = render_context->swapchain_images[image_id]
        };
        const VkImageMemoryBarrier image_bottom_barrier = {
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
            },
            .image = render_context->swapchain_images[image_id]
        };

        /* image transition to rendering */
        
        vkCmdPipelineBarrier(
            command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 
            0, 0, NULL, 0, NULL, 1, &image_top_barrier
        );

        /* SCREEN GRAPHICS */ {
            /* screen attachments */
            const VkRenderingAttachmentInfoKHR screen_color_attachment = {
                .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
                .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .imageView = render_context->swapchain_image_views[image_id]
            };
            const VkRenderingAttachmentInfoKHR screen_depth_attachment = {
                .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
                .clearValue = (VkClearValue) {
                    .depthStencil = (VkClearDepthStencilValue){
                        .depth = 1.0
                    }
                },
                .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .imageView = render_context->depth_view
            };
            /* begin screen rendering */
            const VkRenderingInfoKHR rendering_info = {
                .sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,
                .colorAttachmentCount = 1,
                .pColorAttachments = &screen_color_attachment,
                .layerCount = 1,
                .pDepthAttachment = &screen_depth_attachment,
                .renderArea = (VkRect2D) {
                    .offset = {0, 0},
                    .extent = render_context->swapchain_params.extent
                }
            };
            vulkan_context->cmd_begin_rendering_khr(command_buffer, &rendering_info);
            
            /* set screen viewport & scissor */
            VkViewport viewport = {
                .width = render_context->swapchain_params.extent.width,
                .height = render_context->swapchain_params.extent.height,
                .minDepth = 0.0,
                .maxDepth = 1.0,
                .x = 0,
                .y = 0
            };
            vkCmdSetViewport(command_buffer, 0, 1, &viewport);
            vkCmdSetScissor(command_buffer, 0, 1, &rendering_info.renderArea);
        
            for(u32 i = 0; i < render_context->pipeline_node_count; i++) {
                const PipelineNode* pipeline_node = &render_context->pipeline_nodes[i];
                //const RenderNodeType next_node_type = (i < render_context->pipeline_node_count) ? render_context->pipeline_nodes[i + 1].type : U32_MAX;
                if(pipeline_node->type == RENDER_NODE_TYPE_GRAPHICS) {
                    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_node->pipeline);
                    vkCmdDraw(command_buffer, 3, 1, 0, 0);
                    continue;
                }
            }

            vulkan_context->cmd_end_rendering_khr(command_buffer);
        }

        /* image transition to presentation */
        vkCmdPipelineBarrier(
            command_buffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 
            0, 0, NULL, 0, NULL, 1, &image_bottom_barrier
        );

        vkEndCommandBuffer(command_buffer);
        
        /* SUBMIT */ {
            /* sunmit work for render queue */
            VkSubmitInfo submit_info = {
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                .waitSemaphoreCount = 1,
                .pWaitSemaphores = &sync_objects.image_available_semaphore,
                .pWaitDstStageMask = (const VkPipelineStageFlags[]){VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT},
                .signalSemaphoreCount = 1,
                .pSignalSemaphores = &sync_objects.image_submit_semaphores[image_id],
                .commandBufferCount = 1,
                .pCommandBuffers = &command_buffer
            };
            vkQueueSubmit(vulkan_context->render_queue, 1, &submit_info, sync_objects.frame_fence);
            /* present image with render queue */
            VkPresentInfoKHR present_info = (VkPresentInfoKHR) {
                .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                .waitSemaphoreCount = 1,
                .pWaitSemaphores = &sync_objects.image_submit_semaphores[image_id],
                .swapchainCount = 1,
                .pSwapchains = &render_context->swapchain,
                .pImageIndices = &image_id,
                .pResults = NULL
            };
            VkResult present_result = vkQueuePresentKHR(vulkan_context->render_queue, &present_info);
            if(present_result == VK_ERROR_OUT_OF_DATE_KHR || present_result == VK_SUBOPTIMAL_KHR) {
                if(MSG_IS_ERROR(renderOnWindowResize(vulkan_context, msg_callback, render_context))) {
                    MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_RESIZE_FAIL, "error occured during window resize in the end of loop");
                }
                continue;
            }
        }
    }

    vkDeviceWaitIdle(vulkan_context->device);

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

    /* @(FIX): add comments */
    /* TARGETS ALLOCATION */ {
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

        if(!(render_context->targets_allocation = vramAllocate(&depth_prototype_memory, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT))) {
            MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_ALLOCATE_VRAM, "failed to allocate memory for depth image");
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
        if(settings && settings->binding_count != 0) {
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

            VkPipelineLayoutCreateInfo pipeline_layout_info = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                .pSetLayouts = &render_context->descriptor_set_layout,
                .setLayoutCount = 1
            };
            if(vkCreatePipelineLayout(vulkan_context->device, &pipeline_layout_info, NULL, &render_context->pipeline_layout) != VK_SUCCESS) {
                MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_PIPELINE_LAYOUT_CREATE, "failed to create pipeline layout for descriptor");
            };
        }
    }

    /* @(FIX): simplify, add comments */
    /* RESOURCE NODES */ {
        if(!settings) goto _no_bindings;
        if(settings->binding_count == 0) goto _no_bindings;

        if(!(render_context->resource_nodes = malloc(sizeof(ResourceNode) * settings->binding_count))) {
            MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_BUFFER_MALLOC_FAIL, "failed to allocate resource nodes buffer");
        }

        VkMemoryRequirements host_resource_requirements = (VkMemoryRequirements){.memoryTypeBits = U32_MAX};
        VkMemoryRequirements device_resource_requirements = (VkMemoryRequirements){.memoryTypeBits = U32_MAX};

        render_context->resource_node_count = 0;
        for(u32 i = 0; i < settings->binding_count; i++) {
            const RenderBinding* binding = &settings->bindings[0];
            ResourceNode* node = &render_context->resource_nodes[render_context->resource_node_count];

            VkMemoryRequirements device_buffer_memory = (VkMemoryRequirements){0};
            VkMemoryRequirements host_buffer_memory = (VkMemoryRequirements){0};

            /* skip none nodes */
            if(binding->type == RENDER_BINDING_TYPE_NONE) continue;
            /* uniform buffer node */
            if(binding->type == RENDER_BINDING_TYPE_UNIFORM_BUFFER) {
                *node = (ResourceNode) {
                    .type = RENDER_BINDING_TYPE_UNIFORM_BUFFER,
                    .size = binding->size
                };

                /* if usage device only, should be no batches */    
                if(binding->usage == RENDER_BINDING_USAGE_DEVICE_ONLY) {
                    /* batch validation */
                    if(binding->initial_batch || binding->frame_batch) {
                        MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_BINDING_USAGE_INVALID, "can not use memory write batches for device only buffers");
                    }
                    /* create buffer */
                    if(!(node->device_resource = createBuffer(vulkan_context->device, binding->size, vulkan_context->render_family_id, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT))) {
                        MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_CREATE_RESOURCE, "failed to create device uniform buffer");
                    }
                    /* adjust memory requirements */
                    vkGetBufferMemoryRequirements(vulkan_context->device, (VkBuffer)node->device_resource, &device_buffer_memory);
                    device_resource_requirements.memoryTypeBits &= device_buffer_memory.memoryTypeBits;
                    device_resource_requirements.alignment = MAX(device_resource_requirements.alignment, device_buffer_memory.alignment);
                    node->device_offset = ALIGN(device_resource_requirements.size, device_buffer_memory.alignment); /* dont forget to set offset */
                    device_resource_requirements.size = node->device_offset + device_buffer_memory.size;
                }
                /* if usage device and host, batches required */ 
                else if(binding->usage == RENDER_BINDING_USAGE_HOST_DEVICE) {
                    /* batch validation */
                    if(!binding->initial_batch && !binding->frame_batch) {
                        MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_BINDING_USAGE_INVALID, "neither initial batch nor frame batch are speciffied, but usage is host device");
                    } 
                    /* create buffer on device */
                    if(!(node->device_resource = createBuffer(vulkan_context->device, binding->size, vulkan_context->render_family_id, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT))) {
                        MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_CREATE_RESOURCE, "failed to create device uniform buffer");
                    }
                    /* create buffer on host */
                    if(!(node->host_resource = createBuffer(vulkan_context->device, binding->size, vulkan_context->render_family_id, VK_BUFFER_USAGE_TRANSFER_SRC_BIT))) {
                        MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_CREATE_RESOURCE, "failed to create host uniform buffer");
                    }
                    /* adjust device memory requirements */
                    vkGetBufferMemoryRequirements(vulkan_context->device, (VkBuffer)node->device_resource, &device_buffer_memory);
                    device_resource_requirements.alignment = MAX(device_resource_requirements.alignment, device_buffer_memory.alignment);
                    device_resource_requirements.memoryTypeBits &= device_buffer_memory.memoryTypeBits;
                    node->device_offset = ALIGN(device_resource_requirements.size, device_buffer_memory.alignment); /* dont forget to set offset */
                    device_resource_requirements.size = node->device_offset + device_buffer_memory.size;
                    /* adjust host memory requirements */
                    vkGetBufferMemoryRequirements(vulkan_context->device, (VkBuffer)node->host_resource, &host_buffer_memory);
                    host_resource_requirements.memoryTypeBits &= host_buffer_memory.memoryTypeBits;
                    host_resource_requirements.alignment = MAX(host_resource_requirements.alignment, host_buffer_memory.alignment);
                    node->host_offset = ALIGN(host_resource_requirements.size, host_buffer_memory.alignment); /* dont forget to set offset */
                    host_resource_requirements.size = node->host_offset + host_buffer_memory.size;
                }
                /* usage unspecified */
                else {
                    MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_BINDING_USAGE_INVALID, "render binding usage unspecified");
                }

                render_context->resource_node_count++;
                continue;
            }

            MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_BINDING_TYPE_INVALID, "render binding usage unspecified");
        }
        if(render_context->resource_node_count == 0) goto _no_bindings;

        /* allocating memory on arena */
        if(!(render_context->device_resource_memory = vramAllocate(&device_resource_requirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT))) {
            MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_ALLOCATE_VRAM, "failed to allocate device resource memory");
        }
        if(!(render_context->host_resource_memory = vramAllocate(&host_resource_requirements, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))) {
            MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_ALLOCATE_VRAM, "failed to allocate host resource memory");
        }

        /* bind memory, no validation here */
        for(u32 i = 0; i < render_context->resource_node_count; i++) {
            const ResourceNode* resource_node = &render_context->resource_nodes[i];
            if(resource_node->type == RENDER_BINDING_TYPE_UNIFORM_BUFFER || resource_node->type == RENDER_BIDNING_TYPE_STORAGE_BUFFER) {
                if(resource_node->host_resource) {
                    if(vkBindBufferMemory(vulkan_context->device, (VkBuffer)resource_node->host_resource, render_context->host_resource_memory, resource_node->host_offset) != VK_SUCCESS) {
                        MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_BIND_RESOURCE_MEMORY, "failed to bind host buffer to memory");
                    }
                }
                if(resource_node->host_resource) {
                    if(vkBindBufferMemory(vulkan_context->device, (VkBuffer)resource_node->device_resource, render_context->device_resource_memory, resource_node->device_offset) != VK_SUCCESS) {
                        MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_BIND_RESOURCE_MEMORY, "failed to bind device buffer to memory");
                    }
                }
            }
        }

        _no_bindings: {}
    }

    /* PIPELINE NODES */ {
        /* @(FIX): weird way of handling settings dont exist */
        if(!settings) goto _no_shaders; 
        if(settings->node_count == 0) goto _no_shaders;

        /* create shader read buffer */
        ByteBuffer read_buffer = {
            .buffer = malloc(4096 * 4),
            .size = 4096 * 4
        };
        if(!read_buffer.buffer) {
            MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_BUFFER_MALLOC_FAIL, "failed to allocate shader read buffer");
        }
        /* allocate pipeline nodes buffer */
        if(!(render_context->pipeline_nodes = malloc(sizeof(PipelineNode) * settings->node_count))) {
            MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_BUFFER_MALLOC_FAIL, "failed to allocate pipeline nodes buffer");
        }

        /* transform render nodes to pipeline nodes and validate them */
        render_context->pipeline_node_count = 0;
        for(u32 i = 0; i < settings->node_count; i++) {
            const RenderNode* node = &settings->nodes[i];
            PipelineNode* pipeline_node = &render_context->pipeline_nodes[render_context->pipeline_node_count];
            /* none nodes are just skipped */
            if(node->type == RENDER_NODE_TYPE_NONE) continue;
            /* if graphics node */
            if(node->type == RENDER_NODE_TYPE_GRAPHICS) {
                *pipeline_node = (PipelineNode) {
                    .type = RENDER_NODE_TYPE_GRAPHICS,
                    .pipeline_layout = render_context->pipeline_layout
                };
                /* validate shader types */
                if(!node->vertex_shader || !node->fragment_shader || node->compute_shader) {
                    MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_RENDER_NODE_INVALID_SHADERS, "invalid shaders in graphics render node");
                }
                /* compile shader modules */
                if(!(pipeline_node->vertex_shader = createShaderModule(vulkan_context->device, node->vertex_shader, &read_buffer))) {
                    MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_CREATE_SHADER_MODULE, "failed to create vertex shader module");
                }
                if(!(pipeline_node->fragment_shader = createShaderModule(vulkan_context->device, node->fragment_shader, &read_buffer))) {
                    MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_CREATE_SHADER_MODULE, "failed to create vertex shader module");
                }

                if(!createGraphicsPipeline(vulkan_context->device, render_context->swapchain_params.surface_format.format, render_context->swapchain_params.depth_format, pipeline_node)) {
                    MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_PIPELINE_CREATE, "failed to create graphics pipeline");
                }
                render_context->pipeline_node_count++;
                continue;
            }
            MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_INVALID_RENDER_NODE_TYPE, "invalid render node type detected");
        }

        _no_shaders: {}
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

    /* @(FIX): add comments */
    /* PIPELINE NODES */ {
        if(render_context->pipeline_node_count == 0) goto _skip_pipeline_nodes;

        for(u32 i = 0; i < render_context->pipeline_node_count; i++) {
            if(render_context->pipeline_nodes[i].type == RENDER_NODE_TYPE_GRAPHICS) {
                vkDestroyPipeline(vulkan_context->device, render_context->pipeline_nodes[i].pipeline, NULL);
                vkDestroyShaderModule(vulkan_context->device, render_context->pipeline_nodes[i].vertex_shader, NULL);
                vkDestroyShaderModule(vulkan_context->device, render_context->pipeline_nodes[i].fragment_shader, NULL);
            }
            if(render_context->pipeline_nodes[i].type == RENDER_NODE_TYPE_COMPUTE) {
                vkDestroyPipeline(vulkan_context->device, render_context->pipeline_nodes[i].pipeline, NULL);
                vkDestroyShaderModule(vulkan_context->device, render_context->pipeline_nodes[i].compute_shader, NULL);
            }
        }
        free(render_context->pipeline_nodes);

        _skip_pipeline_nodes: {}
    }

    /* @(FIX): add comments */
    /* RESOURCE NODES */ {
        if(render_context->resource_node_count == 0) goto _skip_resource_nodes;

        const u32 resource_node_count = render_context->resource_node_count;
        for(u32 i = 0; i < resource_node_count; i++) {
            ResourceNode* resource = &render_context->resource_nodes[i];
            if(resource->type == RENDER_BINDING_TYPE_UNIFORM_BUFFER || resource->type == RENDER_BIDNING_TYPE_STORAGE_BUFFER) {
                if(resource->host_resource) {
                    vkDestroyBuffer(vulkan_context->device, (VkBuffer)resource->host_resource, NULL);
                }
                if(resource->device_resource) {
                    vkDestroyBuffer(vulkan_context->device, (VkBuffer)resource->device_resource, NULL);
                }
            }
        }
        free(render_context->resource_nodes);

        _skip_resource_nodes: {}
    }

    /* DESCRIPTORS */ {
        vkDestroyPipelineLayout(vulkan_context->device, render_context->pipeline_layout, NULL);
        if(render_context->descriptor_set_layout) {
            vkDestroyDescriptorSetLayout(vulkan_context->device, render_context->descriptor_set_layout, NULL);
        }
        if(render_context->descirptor_pool) {
            vkDestroyDescriptorPool(vulkan_context->device, render_context->descirptor_pool, NULL);
        }
    }

    /* SWAPCHAIN IMAGES */ {
        for(u32 i = 0; i < render_context->swapchain_image_count; i++) {
            vkDestroyImageView(vulkan_context->device, render_context->swapchain_image_views[i], NULL);
        }
        vkDestroyImageView(vulkan_context->device, render_context->depth_view, NULL);
        vkDestroyImage(vulkan_context->device, render_context->depth_image, NULL);
    }

    vkDestroySwapchainKHR(vulkan_context->device, render_context->swapchain, NULL);
    *render_context = (RenderContext){0};
}

/* render main func */
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
