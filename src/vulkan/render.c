#include "vulkan.h"



/* creates graphics pipeline inside render pipeline struct, resources are passed for format reference */
b32 createGraphicsPipeline(VkDevice device, RenderContext* render_context, RenderPipeline* pipeline) {
    /* shader descriptors array */
    VkPipelineShaderStageCreateInfo shader_stages[2] = {
        (VkPipelineShaderStageCreateInfo) {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pName = SHADER_ENTRY_VERTEX,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = pipeline->shaders[SHADER_GRAPHICS_VERTEX_ID]
        },
        (VkPipelineShaderStageCreateInfo) {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pName = SHADER_ENTRY_FRAGMENT,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = pipeline->shaders[SHADER_GRAPHICS_FRAGMENT_ID]
        }
    };

    VkFormat color_format = render_context->surface_data.color_format;
    VkFormat depth_format = render_context->surface_data.depth_format;

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
        .layout = render_context->full_pipeline_layout,
        .renderPass = NULL,
        .pNext = &rendering_create_info
    };

    if(vkCreateGraphicsPipelines(device, NULL, 1, &pipeline_info, NULL, &pipeline->pipeline) != VK_SUCCESS) {
        return FALSE;
    }
    return TRUE;
}


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


/* called when render loop notices window resize condition */
i32 renderOnWindowResize(const VulkanContext* vulkan_context, MsgCallback_pfn msg_callback, RenderContext* render_context) {
    vkDeviceWaitIdle(vulkan_context->device);
    
    /* DESTRUCTION */ {
        for(u32 i = 0; i < render_context->screen.swapchain_image_count; i++) {
            vkDestroyImageView(vulkan_context->device, render_context->screen.swapchain_image_views[i], NULL);
        }
        vkDestroyImageView(vulkan_context->device, render_context->screen.depth_image_view, NULL);
        vkDestroyImage(vulkan_context->device, render_context->screen.depth_image, NULL);
        vkDestroySwapchainKHR(vulkan_context->device, render_context->screen.swapchain, NULL);
    }

    /* vkGetPhysicalDeviceSurfaceCapabilitiesKHR it changes surface resolution somehow */
    VkSurfaceCapabilitiesKHR surface_capabilities = (VkSurfaceCapabilitiesKHR){0};
    do {
        glfwPollEvents();
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vulkan_context->physical_device, vulkan_context->surface, &surface_capabilities);
    } while (surface_capabilities.currentExtent.width == 0 && surface_capabilities.currentExtent.height == 0);

    /* get glfw size to clamp if surface resolution is invalid */
    i32 width, height;
    glfwGetFramebufferSize(vulkan_context->window, &width, &height);
    render_context->surface_data.extent = (VkExtent2D) {
        MIN((u32)width, surface_capabilities.currentExtent.width),
        MIN((u32)height, surface_capabilities.currentExtent.height)
    };
    if(MSG_IS_ERROR(createSwapchain(vulkan_context, msg_callback, render_context))) {
        MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_SWAPCHAIN_RECREATE, "failed to recreate swapchain");
    }

    return MSG_CODE_SUCCESS;
}

i32 renderLoop(const VulkanContext* vulkan_context, MsgCallback_pfn msg_callback, RenderContext* render_context) {
    typedef struct {
        VkSemaphore image_submit_semaphores[MAX_SWAPCHAIN_IMAGE_COUNT];
        VkSemaphore image_available_semaphore;
        VkFence frame_fence;
    } SyncObjects;

    SyncObjects sync_objects = (SyncObjects){0};
    VulkanCmdContext cmd_context = (VulkanCmdContext) {
        .vulkan_context = vulkan_context,
        .render_context = render_context,
        .bound_pipeline_id = U32_MAX, 
        .render_image_id = U32_MAX
    };

    /* SETUP */ {
        /* command buffer allocation */
        VkCommandBufferAllocateInfo command_buffer_info = (VkCommandBufferAllocateInfo) {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandBufferCount = 1,
            .commandPool = render_context->command_pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY
        };
        if(vkAllocateCommandBuffers(vulkan_context->device, &command_buffer_info, &cmd_context.command_buffer) != VK_SUCCESS) {
            MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_COMMAND_BUFFER_ALLOCATE, "failed to allocate command buffer");
        }

        /* sync objects */
        VkSemaphoreCreateInfo semaphore_info = (VkSemaphoreCreateInfo) {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
        };
        VkFenceCreateInfo fence_info = (VkFenceCreateInfo) {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT
        };
        /* final submit semaphores */
        for(u32 i = 0; i < render_context->screen.swapchain_image_count; i++) {
            if(vkCreateSemaphore(vulkan_context->device, &semaphore_info, NULL, &sync_objects.image_submit_semaphores[i]) != VK_SUCCESS) {
                MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_SEMAPHORE_CREATE, "failed to create image submit semaphore");
            }
        }
        /* image available semaphore */
        if(vkCreateSemaphore(vulkan_context->device, &semaphore_info, NULL, &sync_objects.image_available_semaphore) != VK_SUCCESS) {
            MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_SEMAPHORE_CREATE, "failed to create image avaliable semaphore");
        };

        /* create frame fence */
        if(vkCreateFence(vulkan_context->device, &fence_info, NULL, &sync_objects.frame_fence) != VK_SUCCESS) {
            MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_FENCE_CREATE, "failed to create fence");
        }
    }

    while (!glfwWindowShouldClose(vulkan_context->window)) {
        glfwPollEvents();
        
        cmd_context.bound_pipeline_id = U32_MAX;
        cmd_context.render_image_id = U32_MAX;

        /* AQUIRE */ {
            /* wait for fences */
            vkWaitForFences(vulkan_context->device, 1, &sync_objects.frame_fence, VK_TRUE, U64_MAX);
            /* get s.apchain image id */
            VkResult image_acquire_result = vkAcquireNextImageKHR(vulkan_context->device, render_context->screen.swapchain, U64_MAX, sync_objects.image_available_semaphore, NULL, &cmd_context.render_image_id);
            /* check if frambuffer should resize */
            if(image_acquire_result == VK_ERROR_OUT_OF_DATE_KHR || image_acquire_result == VK_SUBOPTIMAL_KHR) {
                if(MSG_IS_ERROR(renderOnWindowResize(vulkan_context, msg_callback, render_context))) {
                    MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_RESIZE_FAIL, "error occured during window resize in the beginning of loop");
                }
            }
            vkResetFences(vulkan_context->device, 1, &sync_objects.frame_fence);
        }

        const VkCommandBufferBeginInfo command_buffer_begin_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
        };
        vkResetCommandBuffer(cmd_context.command_buffer, 0);
        vkBeginCommandBuffer(cmd_context.command_buffer, &command_buffer_begin_info);

        /* SCREEN IMAGE TOP TRANSITION */ {
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
                .image = render_context->screen.swapchain_images[cmd_context.render_image_id]
            };
            vkCmdPipelineBarrier(
                cmd_context.command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 
                0, 0, NULL, 0, NULL, 1, &image_top_barrier
            );
        }

        /* transfer control to user for drawing */
        if(render_context->render_callback) {
            render_context->render_callback(&cmd_context);
        }

        /* FINISH RESOURCE USAGE */ {
            if(cmd_context.bound_pipeline_id != U32_MAX) {
                const RenderPipeline* last_render_pipeline = &render_context->render_pipelines[cmd_context.bound_pipeline_id];
                if(last_render_pipeline->type == RENDER_PIPELINE_TYPE_GRAPHICS) {
                    vulkan_context->cmd_end_rendering_khr(cmd_context.command_buffer);
                }
            }
        }

        /* SCREEN IMAGE BOTTOM TRANSITION */ {
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
                .image = render_context->screen.swapchain_images[cmd_context.render_image_id]
            };
            vkCmdPipelineBarrier(
                cmd_context.command_buffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 
                0, 0, NULL, 0, NULL, 1, &image_bottom_barrier
            );
        }

        vkEndCommandBuffer(cmd_context.command_buffer);

        /* SUBMIT */ {
            /* sunmit work for render queue */
            VkSubmitInfo submit_info = {
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                .waitSemaphoreCount = 1,
                .pWaitSemaphores = &sync_objects.image_available_semaphore,
                .pWaitDstStageMask = (const VkPipelineStageFlags[]){VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT},
                .signalSemaphoreCount = 1,
                .pSignalSemaphores = &sync_objects.image_submit_semaphores[cmd_context.render_image_id],
                .commandBufferCount = 1,
                .pCommandBuffers = &cmd_context.command_buffer
            };
            vkQueueSubmit(vulkan_context->render_queue, 1, &submit_info, sync_objects.frame_fence);
            /* present image with render queue */
            VkPresentInfoKHR present_info = (VkPresentInfoKHR) {
                .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                .waitSemaphoreCount = 1,
                .pWaitSemaphores = &sync_objects.image_submit_semaphores[cmd_context.render_image_id],
                .swapchainCount = 1,
                .pSwapchains = &render_context->screen.swapchain,
                .pImageIndices = &cmd_context.render_image_id,
                .pResults = NULL
            };
            VkResult present_result = vkQueuePresentKHR(vulkan_context->render_queue, &present_info);
            if(present_result == VK_ERROR_OUT_OF_DATE_KHR || present_result == VK_SUBOPTIMAL_KHR) {
                if(MSG_IS_ERROR(renderOnWindowResize(vulkan_context, msg_callback, render_context))) {
                    MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_RESIZE_FAIL, "error occured during window resize in the beginning of loop");
                }
            }
        }
    }
    
    /* DESTRUCTION */ {
        vkDeviceWaitIdle(vulkan_context->device);
        for(u32 i = 0; i < render_context->screen.swapchain_image_count; i++) {
            vkDestroySemaphore(vulkan_context->device, sync_objects.image_submit_semaphores[i], NULL);
        }
        vkDestroySemaphore(vulkan_context->device, sync_objects.image_available_semaphore, NULL);
        vkDestroyFence(vulkan_context->device, sync_objects.frame_fence, NULL);
    }

    return MSG_CODE_SUCCESS;
}

void cmdDraw(VulkanCmdContext* cmd, u32 pipeline_id, u32 vertex_count, u32 instance_count) {
    const VulkanContext* vulkan_context = cmd->vulkan_context;
    const RenderContext* render_context = cmd->render_context;
    /* check if pipeline that is drawn is new */
    if(cmd->bound_pipeline_id != pipeline_id) {
        /* get render pipeline */
        const RenderPipeline* new_render_pipeline = &render_context->render_pipelines[pipeline_id];
        /* if pipeline is graphics */
        if(new_render_pipeline->type == RENDER_PIPELINE_TYPE_GRAPHICS) {
            /* if previous wasnt graphics, we need to do begin rendering */
            if(cmd->bound_pipeline_id == U32_MAX || render_context->render_pipelines[cmd->bound_pipeline_id].type != RENDER_PIPELINE_TYPE_GRAPHICS) {
                /* @(FIX): this is screen targets, replace them when implement offscreen rendering */
                const VkRenderingAttachmentInfoKHR screen_color_attachment = {
                    .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
                    .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR,
                    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                    .imageView = render_context->screen.swapchain_image_views[cmd->render_image_id]
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
                    .imageView = render_context->screen.depth_image_view
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
                        .extent = render_context->surface_data.extent
                    }
                };
                /* begin rendering */
                vulkan_context->cmd_begin_rendering_khr(cmd->command_buffer, &rendering_info);
                /* set dynamic states */
                const VkViewport viewport_state = {
                    .x = 0.0,
                    .y = 0.0,
                    .width = (float)render_context->surface_data.extent.width,
                    .height = (float)render_context->surface_data.extent.height,
                    .minDepth = 0.0,
                    .maxDepth = 1.0
                };
                vkCmdSetViewport(cmd->command_buffer, 0, 1, &viewport_state);
                vkCmdSetScissor(cmd->command_buffer, 0, 1, &rendering_info.renderArea);
            }
            /* bind pipeline */
            cmd->bound_pipeline_id = pipeline_id;
            vkCmdBindPipeline(cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, new_render_pipeline->pipeline);
        }
    }
    /* draw */
    vkCmdDraw(cmd->command_buffer, vertex_count, instance_count, 0, 0);
}


/* remove shaders and compile directly to Pipeline node */
i32 createShaderModule(VkDevice device, const char* shader_path, MsgCallback_pfn msg_callback, ByteBuffer* read_buffer, VkShaderModule* shader_module) {
    char msg_log[256] = {0};
    *shader_module = NULL;
    /* open file */
    FILE* file = fopen(shader_path, "rb");
    if(!file) {
        strcpy(msg_log, "failed to open shader file path: \"");
        strcat(msg_log, shader_path ? shader_path : "");
        strcat(msg_log, "\"" LOCATION_TRACE);
        MSG_CALLBACK_NO_TRACE(msg_callback, MSG_CODE_ERROR_VK_OPEN_FILE, msg_log);
    }
    /* find file size */
    fseek(file, 0, SEEK_END);
    u64 shader_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    /* reallocate buffer if needed */
    if(shader_size > read_buffer->size) {
        read_buffer->size = MAX(shader_size, read_buffer->size + 4096);
        if(!(read_buffer->buffer = realloc(read_buffer->buffer, read_buffer->size))) {
            strcpy(msg_log, "failed to to reallocate shader read buffer shader path: \"");
            strcat(msg_log, shader_path ? shader_path : "");
            strcat(msg_log, "\" new size: ");
            strcat_u64(msg_log, shader_size);
            strcat(msg_log, LOCATION_TRACE);
            MSG_CALLBACK_NO_TRACE(msg_callback, MSG_CODE_ERROR_VK_BUFFER_MALLOC_FAIL, msg_log);
        }
    }
    /* read code from file */
    if(fread(read_buffer->buffer, 1, shader_size, file) != shader_size) {
        strcpy(msg_log, "failed to read shader file to buffer path: \"");
        strcat(msg_log, shader_path ? shader_path : "");
        strcat(msg_log, "\"" LOCATION_TRACE);
        MSG_CALLBACK_NO_TRACE(msg_callback, MSG_CODE_ERROR_VK_READ_FILE_TO_BUFFER, msg_log);
    }

    /* create module */
    VkShaderModuleCreateInfo shader_module_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pCode = (u32*)read_buffer->buffer,
        .codeSize = shader_size
    };
    if(vkCreateShaderModule(device, &shader_module_info, NULL, shader_module) != VK_SUCCESS) {
        strcpy(msg_log, "failed to create module from shader path: \"");
        strcat(msg_log, shader_path ? shader_path : "");
        strcat(msg_log, "\"" LOCATION_TRACE);
        MSG_CALLBACK_NO_TRACE(msg_callback, MSG_CODE_ERROR_VK_SHADER_MODULE_CREATE, msg_log);
    }

    return MSG_CODE_SUCCESS;
}


i32 createRenderContext(VulkanContext* vulkan_context, const RenderContextInfo* render_info, MsgCallback_pfn msg_callback, RenderContext** render_context) {
    char msg_log[256] = {0};

    RenderContext* context = malloc(sizeof(RenderContext));
    if(!context) {
        MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_BUFFER_MALLOC_FAIL, "failed to allocate render context");
    }
    *context = (RenderContext) {
        .update_callback = render_info->update_callback,
        .render_callback = render_info->render_callback
    };

    
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
                strcpy(msg_log, "resource info invalid mutability {binding: ");
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
            /* initialize basic info */
            *resource = (RenderResource) {
                .type = resource_infos[i].type,
                .size = resource_infos[i].size
            };
            /* if buffer */
            if(resource_infos[i].type == RENDER_RESOURCE_TYPE_UNIFORM_BUFFER || resource_infos[i].type == RENDER_RESOURCE_TYPE_STORAGE_BUFFER) {
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

    if(render_info->pipeline_count == 0) goto _skip_pipeline_creation;

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
            if(pipeline_infos[i].type == RENDER_PIPELINE_TYPE_COMPUTE && (!pipeline_infos[i].compute_shader || pipeline_infos[i].vertex_shader || pipeline_infos[i].fragment_shader)) {
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
        
        if(!(context->render_pipelines = malloc(sizeof(RenderPipeline) * pipeline_count))) {
            MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_BUFFER_MALLOC_FAIL, "failed to allocate render pipeline array");
        }
        context->render_pipeline_count = pipeline_count;
        /* allocate buffer for reading shaders (spirv) */
        ByteBuffer read_buffer = {
            .buffer = malloc(4096 * 4),
            .size = 4096 * 4
        };
        if(!read_buffer.buffer) {
            MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_BUFFER_MALLOC_FAIL, "failed to allocate shader read buffer");
        }

        /* create pipelines */
        RenderPipeline* render_pipelines = context->render_pipelines;
        for(u32 i = 0; i < pipeline_count; i++) {
            render_pipelines[i] = (RenderPipeline) { 
                .type = pipeline_infos[i].type 
            };
            /* if graphics */
            if(pipeline_infos[i].type == RENDER_PIPELINE_TYPE_GRAPHICS) {
                /* compile vertex */
                if(MSG_IS_ERROR(createShaderModule(vulkan_context->device, pipeline_infos[i].vertex_shader, msg_callback, &read_buffer, &render_pipelines[i].shaders[SHADER_GRAPHICS_VERTEX_ID]))) {
                    strcpy(msg_log, "failed to compile graphics pipeline vertex shader id: ");
                    strcat_u32(msg_log, i);
                    strcat(msg_log, " name: \"");
                    if(pipeline_infos[i].name) {
                        strcat(msg_log, pipeline_infos[i].name);
                    }
                    strcat(msg_log, "\"" LOCATION_TRACE);
                    MSG_CALLBACK_NO_TRACE(msg_callback, MSG_CODE_ERROR_VK_SHADER_MODULE_CREATE, msg_log);
                }
                /* compile fragment */
                if(MSG_IS_ERROR(createShaderModule(vulkan_context->device, pipeline_infos[i].fragment_shader, msg_callback, &read_buffer, &render_pipelines[i].shaders[SHADER_GRAPHICS_FRAGMENT_ID]))) {
                    strcpy(msg_log, "failed to compile graphics pipeline fragment shader id: ");
                    strcat_u32(msg_log, i);
                    strcat(msg_log, " name: \"");
                    if(pipeline_infos[i].name) {
                        strcat(msg_log, pipeline_infos[i].name);
                    }
                    strcat(msg_log, "\"" LOCATION_TRACE);
                    MSG_CALLBACK_NO_TRACE(msg_callback, MSG_CODE_ERROR_VK_SHADER_MODULE_CREATE, msg_log);
                }
                /* create pipeline */
                if(!createGraphicsPipeline(vulkan_context->device, context, &render_pipelines[i])) {
                    strcpy(msg_log, "failed to create graphics pipeline id: ");
                    strcat_u32(msg_log, i);
                    strcat(msg_log, " name: \"");
                    if(pipeline_infos[i].name) {
                        strcat(msg_log, pipeline_infos[i].name);
                    }
                    strcat(msg_log, "\"" LOCATION_TRACE);
                    MSG_CALLBACK_NO_TRACE(msg_callback, MSG_CODE_ERROR_VK_PIPELINE_CREATE, msg_log);
                }
            }
        }

        free(read_buffer.buffer);

        /* LOG */ {
            strcpy(msg_log, "created render pipelines count: ");
            strcat_u32(msg_log, context->render_pipeline_count);
            MSG_CALLBACK_NO_TRACE(msg_callback, MSG_CODE_SUCCESS, msg_log);
        }
    }

    _skip_pipeline_creation: {}

    /* CREATE COMMAND POOL */ {
        VkCommandPoolCreateInfo comman_pool_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .queueFamilyIndex = vulkan_context->render_family_id,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
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

    /* DESTROY PIPELINES */ {
        if(render_context->render_pipeline_count == 0) goto _skip_pipelines;

        const u32 render_pipeline_count = render_context->render_pipeline_count;
        const RenderPipeline* render_pipelines = render_context->render_pipelines;
        for(u32 i = 0; i < render_pipeline_count; i++) {
            vkDestroyPipeline(vulkan_context->device, render_pipelines[i].pipeline, NULL);
            for(u32 j = 0; j < MAX_SHADER_MODULE_COUNT; j++) {
                if(render_pipelines[i].shaders[j]) {
                    vkDestroyShaderModule(vulkan_context->device, render_pipelines[i].shaders[j], NULL);
                }
            }
        }
        /* free heap allocation */
        free(render_context->render_pipelines);

        _skip_pipelines: {}
    }
    
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
