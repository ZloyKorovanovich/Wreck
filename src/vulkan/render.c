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

b32 createScreenTextures(
    const VulkanContext *vulkan_context, const Vram *images_vram, MsgCallback_pfn msg_callback, 
    RenderSettings *render_settings, ScreenImages *screen_images
) {
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
        MSG_ERROR(msg_callback, &TRACED_STR("swapchain min image count is too big"));
        return FALSE;
    }

    /* clamp resolution and set screen settings */
    u32 res_x = CLAMP(surface_capabilities.minImageExtent.width, surface_capabilities.maxImageExtent.width, surface_capabilities.currentExtent.width);
    u32 res_y = CLAMP(surface_capabilities.minImageExtent.height, surface_capabilities.maxImageExtent.height, surface_capabilities.currentExtent.height);
    render_settings->extent = (VkExtent2D){res_x, res_y};

    /* SWAPCHAIN */ {
        /* old swapchain will cointain old swapchain if its not NULL, then its first creation, dont forget destroy it after recreation */
        const VkSwapchainCreateInfoKHR swapchain_info = {
            .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .surface = vulkan_context->surface,
            .minImageCount = min_image_count,
            .imageFormat = render_settings->color_format,
            .imageColorSpace = render_settings->color_space,
            .imageExtent = render_settings->extent,
            .presentMode = render_settings->present_mode,
            .preTransform =surface_capabilities.currentTransform,
            .imageArrayLayers = 1,
            .pQueueFamilyIndices = &vulkan_context->render_queue_id,
            .queueFamilyIndexCount = 1,
            .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            .clipped = TRUE,
            .oldSwapchain = screen_images->swapchain
        };
        if(vkCreateSwapchainKHR(vulkan_context->device, &swapchain_info, NULL, &screen_images->swapchain) != VK_SUCCESS) {
            MSG_ERROR(msg_callback, &TRACED_STR("failed to create swapchain"));
            return FALSE;
        }

        /* check if swapchain is recreated or created for the first time */
        if(swapchain_info.oldSwapchain) {
            /* if its not a first swapchain, just recreate images and views */
            for(u32 i = 0; i < screen_images->swapchain_image_count; i++) {
                vkDestroyImageView(vulkan_context->device, screen_images->swapchain_image_views[i], NULL);
            }
            vkDestroySwapchainKHR(vulkan_context->device, swapchain_info.oldSwapchain, NULL);
            vkGetSwapchainImagesKHR(vulkan_context->device, screen_images->swapchain, &screen_images->swapchain_image_count, NULL);
            vkGetSwapchainImagesKHR(vulkan_context->device, screen_images->swapchain, &screen_images->swapchain_image_count, screen_images->swapchain_images);
        } else {
            /* if swapchain is created for the first time, we need to allocate arrays of images and views */
            vkGetSwapchainImagesKHR(vulkan_context->device, screen_images->swapchain, &screen_images->swapchain_image_count, NULL);
            vkGetSwapchainImagesKHR(vulkan_context->device, screen_images->swapchain, &screen_images->swapchain_image_count, screen_images->swapchain_images);
        }

        /* create image views for swapchain */
        const u32 swapchain_image_count = screen_images->swapchain_image_count;
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
                .format = render_settings->color_format,
                .image = screen_images->swapchain_images[i]
            };
            if(vkCreateImageView(vulkan_context->device, &view_info, NULL, &screen_images->swapchain_image_views[i]) != VK_SUCCESS) {
                MSG_ERROR(vulkan_context->msg_callback, &TRACED_STR("failed to create swapchain image view"));
                return FALSE;
            }
        }
    }

    /* DEPTH BUFFER */ {
        /* if depth image is already created we need to destroy it and its view */
        if(screen_images->depth_image) {
            /* if recreating depth image */
            vkDestroyImageView(vulkan_context->device, screen_images->depth_image_view, NULL);
            vkDestroyImage(vulkan_context->device, screen_images->depth_image, NULL);
        }

        VkImageCreateInfo depth_image_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .extent = (VkExtent3D){
                .width = render_settings->extent.width, 
                .height = render_settings->extent.height, 
                .depth = 1
            },
            .format = render_settings->depth_format,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = 1,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE
        };
        if(vkCreateImage(vulkan_context->device,&depth_image_info, NULL, &screen_images->depth_image) != VK_SUCCESS) {
            MSG_ERROR(msg_callback, &TRACED_STR("failed to create depth image"));
            return FALSE;
        }
        /* bind memory to depth image before creating view */
        if(vkBindImageMemory(vulkan_context->device, screen_images->depth_image, images_vram->memory, screen_images->depth_vram_region.offset) != VK_SUCCESS) {
            MSG_ERROR(msg_callback, &TRACED_STR("failed to bind depth image memory"));
            return FALSE;
        }

        /* create image view*/
        const VkImageViewCreateInfo depth_view_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .image = screen_images->depth_image,
            .format = render_settings->depth_format,
            .subresourceRange = (VkImageSubresourceRange) {
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
                .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT
            }
        };
        if(vkCreateImageView(vulkan_context->device, &depth_view_info, NULL, &screen_images->depth_image_view) != VK_SUCCESS) {
            MSG_ERROR(msg_callback, &TRACED_STR("failed to create render depth image view"));
            return FALSE;
        }
    }

    return TRUE;
}


VkShaderModule createShaderModule(VkDevice device, const String *file, MsgCallback_pfn msg_callback, Buffer *read_buffer, Stack *stack) {
    u64 file_size = fileToBuffer(file, read_buffer);
    /* if failed */
    if(file_size == 0) {
        MSG_ERROR(msg_callback, &TRACED_STR("invalid shader file"));
        return NULL;
    }
    /* if file size is bigger than buffer reallocate buffer */
    if(file_size > read_buffer->size) {
        if(!allocateStack(stack, file_size - read_buffer->size, 0)) {
            MSG_ERROR(msg_callback, &TRACED_STR("failed to reallocate read_buffer for shaders"));
            return NULL;
        }
        read_buffer->size = file_size;
    }
    /* second read attempt */
    if(fileToBuffer(file, read_buffer) != file_size) {
        MSG_ERROR(msg_callback, &TRACED_STR("failed to read shader file after reallocation"));
        return NULL;
    }

    /* create vulkan shader module, compile shader */
    VkShaderModule module = NULL;
    VkShaderModuleCreateInfo module_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pCode = read_buffer->buffer,
        .codeSize = file_size
    };
    if(vkCreateShaderModule(device, &module_info, NULL, &module) != VK_SUCCESS) {
        MSG_ERROR(msg_callback, &TRACED_STR("failed to create shader module"));
        return NULL;
    }
    return module;
}

VkPipeline createGraphicsPipeline(
    VkDevice device, VkShaderModule vertex_shader, VkShaderModule fragment_shader, VkPipelineLayout pipeline_layout, 
    const VkFormat *color_formats, u32 color_format_count, VkFormat depth_format
) {
    VkPipelineShaderStageCreateInfo shader_stages[] = {
        (VkPipelineShaderStageCreateInfo) {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pName = SHADER_ENTRY_VERTEX,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vertex_shader
        },
        (VkPipelineShaderStageCreateInfo) {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pName = SHADER_ENTRY_FRAGMENT,
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

/* modifies programs struct of render_context */
b32 createShaderPrograms(RenderContext *render_context, u32 program_count, const ShaderProgramInfo *program_infos, Stack *init_stack) {
    pushStack(init_stack);
    
    String log_str = {
        .string = (char[256]){0},
        .capacity = 256
    };
    VulkanContext *vulkan_context = render_context->vulkan_context;
    /* struct to fill */
    Programs *programs = &render_context->shader_programs;

    /* PIPELINE LAYOUT */ {
        VkPipelineLayoutCreateInfo pipeline_layout_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO
        };
        if(vkCreatePipelineLayout(vulkan_context->device, &pipeline_layout_info, NULL, &programs->pipeline_layout) != VK_SUCCESS) {
            MSG_ERROR(render_context->msg_callback, &TRACED_STR("failed to create pipeline layout"));
        }
    }

    /* following part is a little bit tough, we need to read shader files into a buffer,
        then compile them to modules and make pipelines from them depending on type of programs assigned */

    /* allocate buffer for reading shaders */
    Buffer read_buffer = {
        .buffer = allocateStack(init_stack, 1024 * 64, 16),
        .size = 1024 * 64
    };
    if(!read_buffer.buffer) {
        MSG_ERROR(render_context->msg_callback, &TRACED_STR("failed to allocate read_buffer"));
        return FALSE;
    }

    /* allocate space for shader_programs */
    programs->shader_programs = allocateArena(&render_context->resource_arena, sizeof(ShaderProgram) * program_count, 0);
    if(!programs->shader_programs) {
        MSG_ERROR(render_context->msg_callback, &TRACED_STR("failed to allocate shader_programs array"));
        return FALSE;
    }
    programs->program_count = program_count;

    /* create shader programs */
    ShaderProgram *shader_programs = programs->shader_programs;
    for(u32 i = 0; i < program_count; i++) {
        /* if graphics */
        if(!IS_EMPTY_STR(program_infos[i].vertex_shader) && !IS_EMPTY_STR(program_infos[i].fragment_shader) && IS_EMPTY_STR(program_infos[i].compute_shader)) {
            shader_programs[i] = (ShaderProgram) {
                .type = SHADER_PROGRAM_TYPE_GRAPHICS,
                .vertex_shader = createShaderModule(vulkan_context->device, &program_infos[i].vertex_shader, render_context->msg_callback, &read_buffer, init_stack),
                .fragment_shader = createShaderModule(vulkan_context->device, &program_infos[i].fragment_shader, render_context->msg_callback, &read_buffer, init_stack)
            };
            /* check if shader module creation was successful */
            if(shader_programs[i].vertex_shader == NULL) {
                stringPattern(&TRACED_STR("invalid vertex shader: \"%s\""), (const void *[]){ &program_infos[i].vertex_shader}, &log_str);
                MSG_ERROR(render_context->msg_callback, &log_str);
                return FALSE;
            }
            if(shader_programs[i].fragment_shader == NULL) {
                stringPattern(&TRACED_STR("invalid fragment shader: \"%s\""), (const void *[]){ &program_infos[i].fragment_shader}, &log_str);
                MSG_ERROR(render_context->msg_callback, &log_str);
                return FALSE;
            }

            /* create pipeline */
            shader_programs[i].pipeline = createGraphicsPipeline(
                vulkan_context->device, shader_programs[i].vertex_shader, shader_programs[i].fragment_shader, programs->pipeline_layout,
                &render_context->render_settings.color_format, 1, render_context->render_settings.depth_format
            );
            if(!shader_programs[i].pipeline) {
                stringPattern(
                    &TRACED_STR("failed to create graphics pipeline: { vertex: \"%s\" fragment: \"%s\" compute: \"%s\" }"), 
                    (const void *[]){ &program_infos[i].vertex_shader, &program_infos[i].fragment_shader, &program_infos[i].compute_shader },
                    &log_str
                );
                return FALSE;
            }
            continue;
        }
        /* if compute */
        if(!IS_EMPTY_STR(program_infos[i].compute_shader) && IS_EMPTY_STR(program_infos[i].vertex_shader) && IS_EMPTY_STR(program_infos[i].fragment_shader)) {
            shader_programs[i] = (ShaderProgram) {
                .type = SHADER_PROGRAM_TYPE_COMPUTE,
                .compute_shader = createShaderModule(vulkan_context->device, &program_infos[i].compute_shader, render_context->msg_callback, &read_buffer, init_stack)
            };
            /* check if shader module creation was successful */
            if(shader_programs[i].compute_shader == NULL) {
                stringPattern(&TRACED_STR("invalid compute shader: \"%s\""), (const void *[]){ &program_infos[i].compute_shader}, &log_str);
                MSG_ERROR(render_context->msg_callback, &log_str);
                return FALSE;
            }
            continue;
        }
        /* invalid combination of shaders */
        stringPattern(
            &TRACED_STR("invalid shader combination { vertex: \"%s\" fragment: \"%s\" compute: \"%s\" }"), 
            (const void *[]){ &program_infos[i].vertex_shader, &program_infos[i].fragment_shader, &program_infos[i].compute_shader },
            &log_str
        );
        MSG_ERROR(render_context->msg_callback, &log_str);
        return FALSE;
    }

    popStack(init_stack);
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

    /* 
    String msg_string = {
        .string = (char[256]){0},
        .capacity = 256 
    }; */

    RenderContext *context = context_allocate(sizeof(RenderContext), 16);
    if(!context) {
        MSG_ERROR(info->msg_callback, &TRACED_STR("failed to allocate render context"));
        return NULL;
    }

    *context = (RenderContext) {
        .vulkan_context = info->vulkan_context,
        .msg_callback = info->msg_callback
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
            context->screen_images.depth_vram_region = (VramRegion) {
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

    /* SCREEN TEXTURES */ {
        /* allocate space on resource arena for VkImages and VkImageViews of swapchain, they are handles of resources (pointers) */
        void *swapchain_image_buffer = allocateArena(&context->resource_arena, MAX_SWAPCHAIN_IMAGES * 2 * sizeof(void *), 8);
        if(!swapchain_image_buffer) {
            MSG_ERROR(context->msg_callback, &TRACED_STR("failed to allocate swapchain_image_buffer"));
            return NULL;
        }
        /* offset pointers, beacuse we allocated space for 2 arrays at once */
        context->screen_images.swapchain_images = (VkImage *)swapchain_image_buffer;
        context->screen_images.swapchain_image_views = (VkImageView *)((void *)swapchain_image_buffer + MAX_SWAPCHAIN_IMAGES);

        /* create screen textures */
        if(!createScreenTextures(vulkan_context, &context->images_vram, context->msg_callback, &context->render_settings, &context->screen_images)) {
            MSG_ERROR(context->msg_callback, &TRACED_STR("failed to create screen textures"));
            return NULL;
        }
    }

    if(info->render_program_count != 0) {
        if(!createShaderPrograms(context, info->render_program_count, info->render_programs, &init_stack)) {
            MSG_ERROR(context->msg_callback, &TRACED_STR("failed to create shader shader_programs"));
            return NULL;
        }
    }

    /* COMMAND POOL */ {
        const VkCommandPoolCreateInfo command_pool_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = vulkan_context->render_queue_id
        };
        if(vkCreateCommandPool(vulkan_context->device, &command_pool_info, NULL, &context->command_pool) != VK_SUCCESS) {
            MSG_ERROR(context->msg_callback, &TRACED_STR("failed to create render command pool"));
            return NULL;
        }
    }

    return context;
}

void destroyRenderContext(RenderContext *context) {
    VulkanContext* vulkan_context = context->vulkan_context;

    /* COMMAND POOL */ {
        vkDestroyCommandPool(vulkan_context->device, context->command_pool, NULL);
    }

    /* SHADER PROGRAMS */ {
        const u32 program_count = context->shader_programs.program_count;
        ShaderProgram *shader_programs = context->shader_programs.shader_programs;
        for(u32 i = 0; i < program_count; i++) {
            if(shader_programs[i].type == SHADER_PROGRAM_TYPE_GRAPHICS) {
                vkDestroyShaderModule(vulkan_context->device, shader_programs[i].vertex_shader, NULL);
                vkDestroyShaderModule(vulkan_context->device, shader_programs[i].fragment_shader, NULL);
            }
            if(shader_programs[i].type == SHADER_PROGRAM_TYPE_COMPUTE) {
                vkDestroyShaderModule(vulkan_context->device, shader_programs[i].compute_shader, NULL);
            }
            vkDestroyPipeline(vulkan_context->device, shader_programs[i].pipeline, NULL);
        }
        vkDestroyPipelineLayout(vulkan_context->device, context->shader_programs.pipeline_layout, NULL);
    }

    /* SCREEN IMAGES */ {
        for(u32 i = 0; i < context->screen_images.swapchain_image_count; i++) {
            vkDestroyImageView(vulkan_context->device, context->screen_images.swapchain_image_views[i], NULL);
        }
        vkDestroySwapchainKHR(vulkan_context->device, context->screen_images.swapchain, NULL);
        vkDestroyImageView(vulkan_context->device, context->screen_images.depth_image_view, NULL);
        vkDestroyImage(vulkan_context->device, context->screen_images.depth_image, NULL);
    }

    /* FREE MEMORY */ {
        freeVram(vulkan_context, &context->images_vram);
        freeArena(&context->resource_arena);
    }

    *context = (RenderContext){0};
}

b32 runRenderLoop(RenderContext *render_context, RenderUpdate_pfn update_callback) {
    typedef struct {
        VkSemaphore image_available_semaphore;
        VkSemaphore image_submit_semaphores[MAX_SWAPCHAIN_IMAGES];
        VkFence frame_fence;    
    } SyncObjects;

    VulkanContext *vulkan_context = render_context->vulkan_context;
    VkCommandBuffer command_buffer = NULL;
    SyncObjects sync_objects = (SyncObjects){0};

    /* CREATE SYNC OBJECTS */ {
        /* create semaphores */
        const VkSemaphoreCreateInfo semaphore_info = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
        };
        if(vkCreateSemaphore(vulkan_context->device, &semaphore_info, NULL, &sync_objects.image_available_semaphore) != VK_SUCCESS) {
            MSG_ERROR(render_context->msg_callback, &TRACED_STR("failed to create image available semaphore"));
            return FALSE;
        }
        for(u32 i = 0; i < render_context->screen_images.swapchain_image_count; i++) {
            if(vkCreateSemaphore(vulkan_context->device, &semaphore_info, NULL, &sync_objects.image_submit_semaphores[i]) != VK_SUCCESS) {
                MSG_ERROR(render_context->msg_callback, &TRACED_STR("failed to create image submit semaphore"));
                return FALSE;
            }
        }
        
        /* create frame fence */
        const VkFenceCreateInfo fence_info = {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT
        };
        if(vkCreateFence(vulkan_context->device, &fence_info, NULL, &sync_objects.frame_fence) != VK_SUCCESS) {
            MSG_ERROR(render_context->msg_callback, &TRACED_STR("failed to create frame fence"));
            return FALSE;
        }
    }

    /* CREATE COMMAND BUFFER */ {
        VkCommandBufferAllocateInfo command_buffer_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = render_context->command_pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1
        };
        if(vkAllocateCommandBuffers(vulkan_context->device, &command_buffer_info, &command_buffer) != VK_SUCCESS) {
            MSG_ERROR(render_context->msg_callback, &TRACED_STR("failed to allocate command buffer"));
            return FALSE;
        }
    }

    while (!glfwWindowShouldClose(vulkan_context->window)) {
        /* run glfw events */
        glfwPollEvents();
        /* wait for previous frame to finish */
        vkWaitForFences(vulkan_context->device, 1, &sync_objects.frame_fence, TRUE, U64_MAX);

        u32 render_image_id = 0;

        /* AQUIRE */ {
            VkResult aquire_result = vkAcquireNextImageKHR(vulkan_context->device, render_context->screen_images.swapchain, U64_MAX, sync_objects.image_available_semaphore, NULL, &render_image_id);
            /* check if should resize right now */
            if(aquire_result == VK_ERROR_OUT_OF_DATE_KHR) {
                /* recreate screen textures */
                vkDeviceWaitIdle(vulkan_context->device);
                if(!createScreenTextures(vulkan_context, &render_context->images_vram, render_context->msg_callback, &render_context->render_settings, &render_context->screen_images)) {
                    MSG_ERROR(render_context->msg_callback, &TRACED_STR("failed to recreate swapchain"));
                    return FALSE;
                }
                continue;
            }
            /* check for errors */ 
            else if(aquire_result != VK_SUCCESS) {
                MSG_ERROR(render_context->msg_callback, &TRACED_STR("failed to aquire swapchain image"));
                return FALSE;
            }
        }

        vkResetFences(vulkan_context->device, 1, &sync_objects.frame_fence);
        const VkImage screen_color = render_context->screen_images.swapchain_images[render_image_id];

        /* BEGIN RENDERING */ {
            /* begin command buffer recording */
            const VkCommandBufferBeginInfo command_buffer_begin_info = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
            };
            if(vkResetCommandBuffer(command_buffer, 0) != VK_SUCCESS || vkBeginCommandBuffer(command_buffer, &command_buffer_begin_info) != VK_SUCCESS ) {
                MSG_ERROR(render_context->msg_callback, &TRACED_STR("failed to begin command buffer"));
                return FALSE;
            }

            /* transition screen image state */
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
                .image = screen_color
            };
            vkCmdPipelineBarrier(
                command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 
                0, 0, NULL, 0, NULL, 1, &image_top_barrier
            );
        }

        /* END RENDERING */ {
            /* transition image to present */
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
                .image = screen_color
            };
            vkCmdPipelineBarrier(
                command_buffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 
                0, 0, NULL, 0, NULL, 1, &image_bottom_barrier
            );
            /* end command buffer */
            if(vkEndCommandBuffer(command_buffer) != VK_SUCCESS) {
                MSG_ERROR(render_context->msg_callback, &TRACED_STR("failed to end render command buffer"));
                return FALSE;
            }
        }

        /* SUBMIT */ {
            /* sunmit work for render queue */
            VkSubmitInfo submit_info = {
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                .waitSemaphoreCount = 1,
                .pWaitSemaphores = &sync_objects.image_available_semaphore,
                .pWaitDstStageMask = (const VkPipelineStageFlags []){VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT},
                .signalSemaphoreCount = 1,
                .pSignalSemaphores = &sync_objects.image_submit_semaphores[render_image_id],
                .commandBufferCount = 1,
                .pCommandBuffers = &command_buffer
            };
            vkQueueSubmit(vulkan_context->render_queue, 1, &submit_info, sync_objects.frame_fence);
            /* present image with render queue */
            VkPresentInfoKHR present_info = (VkPresentInfoKHR) {
                .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                .waitSemaphoreCount = 1,
                .pWaitSemaphores = &sync_objects.image_submit_semaphores[render_image_id],
                .swapchainCount = 1,
                .pSwapchains = &render_context->screen_images.swapchain,
                .pImageIndices = &render_image_id,
                .pResults = NULL
            };
            VkResult present_result = vkQueuePresentKHR(vulkan_context->render_queue, &present_info);
            if(present_result == VK_ERROR_OUT_OF_DATE_KHR || present_result == VK_SUBOPTIMAL_KHR) {
                vkDeviceWaitIdle(vulkan_context->device);
                /* recreate screen textures */
                if(!createScreenTextures(vulkan_context, &render_context->images_vram, render_context->msg_callback, &render_context->render_settings, &render_context->screen_images)) {
                    MSG_ERROR(render_context->msg_callback, &TRACED_STR("failed to recreate swapchain"));
                    return FALSE;
                }
            } 
            else if(present_result != VK_SUCCESS) {
                MSG_ERROR(render_context->msg_callback, &TRACED_STR("failed to present render image"));
                return FALSE;
            }
        }
    }

    vkDeviceWaitIdle(vulkan_context->device);

    /* DESTROY SYNC OBJECTS */ {
        vkDestroySemaphore(vulkan_context->device, sync_objects.image_available_semaphore, NULL);
        for(u32 i = 0; i < render_context->screen_images.swapchain_image_count; i++) {
            vkDestroySemaphore(vulkan_context->device, sync_objects.image_submit_semaphores[i], NULL);
        }
        vkDestroyFence(vulkan_context->device, sync_objects.frame_fence, NULL);
    }
    
    return TRUE;
}

