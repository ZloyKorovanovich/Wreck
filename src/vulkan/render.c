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
    const VulkanContext *vulkan_context, const Vram *images_device_vram, MsgCallback_pfn msg_callback, 
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
        if(vkBindImageMemory(vulkan_context->device, screen_images->depth_image, images_device_vram->memory, screen_images->depth_vram_region.offset) != VK_SUCCESS) {
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

/* read buffer should end on the end of stack for easy reallocation */
VkShaderModule createShaderModule(VkDevice device, const String *file, MsgCallback_pfn msg_callback, Buffer *read_buffer, Stack *stack) {
    u64 file_size = fileToBuffer(file, read_buffer);
    /* if failed */
    if(file_size == 0) {
        MSG_ERROR(msg_callback, &TRACED_STR("invalid shader file"));
        return NULL;
    }
    /* if file size is bigger than buffer reallocate buffer */
    if(file_size > read_buffer->size) {
        /* allocate with aligment 1 to avoid issues with holes in buffer */
        if(!allocateStack(stack, file_size - read_buffer->size, 1)) {
            MSG_ERROR(msg_callback, &TRACED_STR("failed to reallocate read_buffer for shaders"));
            return NULL;
        }
        read_buffer->size = file_size;
        /* second read attempt */
        if(fileToBuffer(file, read_buffer) != file_size) {
            MSG_ERROR(msg_callback, &TRACED_STR("failed to read shader file after reallocation"));
            return NULL;
        }
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
                MSG_ERROR(render_context->msg_callback, &log_str);
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


/* read buffer should end on the end of stack for easy reallocation */
b32 createRawMesh(const String *file, MsgCallback_pfn msg_callback, Buffer *read_buffer, Stack *stack, Arena *mesh_arena, RawMesh *mesh) {
    /* try to read file to buffer */
    u64 file_size = fileToBuffer(file, read_buffer);
    /* if read unccuccessful */
    if(file_size == 0) {
        MSG_ERROR(msg_callback, &TRACED_STR("failed to read mesh file"));
        return FALSE;
    }
    /* if need to reallocate buffer for bigger file */
    if(file_size > read_buffer->size) {
        /* allocate more memory, buffer ends on the end of stack */
        if(!allocateStack(stack, file_size - read_buffer->size, 1)) {
            MSG_ERROR(msg_callback, &TRACED_STR("failed to reallocate read buffer for larger mesh file"));
            return FALSE;
        }
        /* read file to buffer 2 attempt */
        if(fileToBuffer(file, read_buffer) != file_size) {
            MSG_ERROR(msg_callback, &TRACED_STR("failed to read mesh file to buffer after reallocation"));
            return FALSE;
        }
    }

    u32 file_version = *((u32 *)read_buffer->buffer);
    /* first version of mesh */
    if(file_version == 1) {
        /* vertex v1 */
        typedef struct {
            f32 position[3];
        } VertexV1;
        typedef u16 IndexV1;

        /* get vertex and index count */
        u32 vertex_count = *(u32 *)((u8 *)read_buffer->buffer + 4);
        u32 index_count = *(u32 *)((u8 *)read_buffer->buffer + 8);
        if(vertex_count == 0 || index_count == 0) {
            MSG_ERROR(msg_callback, &TRACED_STR("mesh file contain 0 length arrays"));
            return FALSE;
        }

        /* vertex and index arrays in buffer */
        VertexV1 *vertices_v1 = (VertexV1 *)((u8 *)read_buffer->buffer + 16);
        IndexV1 *indices_v1 = (IndexV1 *)((u8 *)read_buffer->buffer + 16 + sizeof(VertexV1) * vertex_count);

        /* allocate raw mesh arrays */
        *mesh = (RawMesh) {
            .vertices = allocateArena(mesh_arena, sizeof(Vertex) * vertex_count, 16),
            .indices = allocateArena(mesh_arena, sizeof(u16) * index_count, 16),
            .vertex_count = vertex_count,
            .index_count = index_count
        };

        if(!mesh->vertices) {
            MSG_ERROR(msg_callback, &TRACED_STR("failed to allocate raw mesh vertices"));
            return FALSE;
        }
        if(!mesh->indices) {
            MSG_ERROR(msg_callback, &TRACED_STR("failed to allocate raw mesh indices"));
            return FALSE;
        }

        /* VERTEX IMPLEMENTATION DEPENDENT */ {
            for(u32 i = 0; i < vertex_count; i++) {
                mesh->vertices[i] = (Vertex) {
                    .position = {
                        vertices_v1[i].position[0],
                        vertices_v1[i].position[1],
                        vertices_v1[i].position[2]
                    }
                };
            }
            for(u32 i = 0; i < index_count; i++) {
                mesh->indices[i] = (u16)indices_v1[i];
            }
        }
       
        return TRUE;
    }
    /* failed to identify file version */
    MSG_ERROR(msg_callback, &TRACED_STR("mesh file has unknown version"));
    return FALSE;
}

/* requires temporary staging buffer and transfer submit */
b32 createMeshBuffers_DescreteModel(RenderContext *render_context, u32 mesh_count, const MeshInfo *mesh_infos, Stack *init_stack, VkFence fence) {
    pushStack(init_stack);
    String log_str = {
        .string = (char[256]){0},
        .capacity = 256
    };
    /* simple access pointers */
    VulkanContext *vulkan_context = render_context->vulkan_context;
    Meshes *meshes = &render_context->render_meshes;


    /* raw mesh array */
    RawMesh *raw_meshes = NULL;
    /* vk buffers */
    VramRegion *device_vertex_regions = NULL;
    VramRegion *device_index_regions = NULL;
    VramRegion *host_mesh_regions = NULL;
    /* combined vertex and index buffers */
    VkBuffer *host_mesh_buffers = NULL;

    Arena mesh_arena = (Arena){0};
    Buffer read_buffer = (Buffer){0};

    /* CPU ARRAYS ALLOCATION */ {
        /* arena is created because we need simontaneously allocate RawMesh structs and reallocate read_buffer */
        if(!createArena(&mesh_arena, 1024 * 1024 * 256, 1024 * 64)) {
            MSG_ERROR(render_context->msg_callback, &TRACED_STR("failed to create mesh_arena"));
            return FALSE;
        }

        /* raw mesh array for vertex and index pointers */
        raw_meshes = allocateStack(init_stack, sizeof(RawMesh) * mesh_count, 0);
        if(!raw_meshes) {
            MSG_ERROR(render_context->msg_callback, &TRACED_STR("failed to allocate raw_meshes array"));
            return FALSE;
        }

        /* 1 set for device vertex 1 for device index 1 for host mesh, total 3! */
        VramRegion *vram_regions = allocateStack(init_stack, sizeof(VramRegion) * mesh_count * 3, 0);
        if(!vram_regions) {
            MSG_ERROR(render_context->msg_callback, &TRACED_STR("failed to allocate vram_regions array for meshes"));
            return FALSE;
        }
        device_vertex_regions = vram_regions;
        device_index_regions = vram_regions + mesh_count;
        host_mesh_regions = vram_regions + mesh_count * 2;

        /* allocate space for host buffers */
        host_mesh_buffers = allocateStack(init_stack, sizeof(VkBuffer) * mesh_count, 0);

        /* allocate space for mesh resources */
        meshes->meshes_count = mesh_count;
        meshes->render_meshes = allocateArena(&render_context->resource_arena, sizeof(RenderMesh) * mesh_count, 0);
        if(!meshes->render_meshes) {
            MSG_ERROR(render_context->msg_callback, &TRACED_STR("failed to allocate render_meshes array on resource arena"));
            return FALSE;
        }

        /* read buffer should be allocated last on stack, otherwise reallocation will break memory continuity */
        read_buffer = (Buffer){
            .buffer = allocateStack(init_stack, 1024 * 64, 0),
            .size = 1024 * 64
        };
        if(!read_buffer.buffer) {
            MSG_ERROR(render_context->msg_callback, &TRACED_STR("failed to allocate read buffer for meshes"));
            return FALSE;
        }
    }

    VramInfo device_vram_info = {
        .mandatory_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        .restricted_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
        .memory_type_bits = U32_MAX, 
        .aligment = 1
    };
    VramInfo host_vram_info = {
        .mandatory_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
        .restricted_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        .memory_type_bits = U32_MAX, 
        .aligment = 1
    };

    /* create mesh structs */
    RenderMesh *render_meshes = meshes->render_meshes;
    for(u32 i = 0; i < mesh_count; i++) {
        /* read file to raw mesh */
        if(!createRawMesh(&mesh_infos[i].file, render_context->msg_callback, &read_buffer, init_stack, &mesh_arena, &raw_meshes[i])) {
            stringPattern(&TRACED_STR("failed to create raw mesh from file: %s"), (const void *[]){&mesh_infos[i].file}, &log_str);
            MSG_ERROR(render_context->msg_callback, &log_str);
            return FALSE;
        }

        /* DEVICE BUFFERS */ {
            render_meshes[i] = (RenderMesh){0};
            const VkBufferCreateInfo vertex_buffer_info = {
                .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                .size = sizeof(Vertex) * raw_meshes[i].vertex_count,
                .queueFamilyIndexCount = 1,
                .pQueueFamilyIndices = &vulkan_context->render_queue_id,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE
            };
            const VkBufferCreateInfo index_buffer_info = {
                .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                .size = sizeof(u16) * raw_meshes[i].index_count,
                .queueFamilyIndexCount = 1,
                .pQueueFamilyIndices = &vulkan_context->render_queue_id,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE
            };
            if(vkCreateBuffer(vulkan_context->device, &vertex_buffer_info, NULL, &render_meshes[i].vertex_buffer) != VK_SUCCESS) {
                stringPattern(&TRACED_STR("failed to create vertex buffer file: %s"), (const void *[]){&mesh_infos[i].file}, &log_str);
                MSG_ERROR(render_context->msg_callback, &log_str);
                return FALSE;
            }
            if(vkCreateBuffer(vulkan_context->device, &index_buffer_info, NULL, &render_meshes[i].index_buffer) != VK_SUCCESS) {
                stringPattern(&TRACED_STR("failed to create index buffer file: %s"), (const void *[]){&mesh_infos[i].file}, &log_str);
                MSG_ERROR(render_context->msg_callback, &log_str);
                return FALSE;
            }

            /* device memory requirements */
            VkMemoryRequirements vertex_buffer_requirements = (VkMemoryRequirements){0};
            VkMemoryRequirements index_buffer_requirements = (VkMemoryRequirements){0};
            vkGetBufferMemoryRequirements(vulkan_context->device, render_meshes[i].vertex_buffer, &vertex_buffer_requirements);
            vkGetBufferMemoryRequirements(vulkan_context->device, render_meshes[i].index_buffer, &index_buffer_requirements);

            /* align vram offset for vertex buffer */
            device_vram_info.size = ALIGN(device_vram_info.size, vertex_buffer_requirements.alignment);
            device_vertex_regions[i] = (VramRegion) {
                .offset = device_vram_info.size,
                .size = vertex_buffer_requirements.size
            };
            /* align vram offset again for index buffer, written after vertex buffer */
            device_vram_info.size = ALIGN((device_vram_info.size + vertex_buffer_requirements.size), index_buffer_requirements.alignment);
            device_index_regions[i] = (VramRegion) {
                .offset = device_vram_info.size,
                .size = index_buffer_requirements.size
            };

            /* adjust stats of vram info */
            device_vram_info.size += index_buffer_requirements.size;
            device_vram_info.memory_type_bits &= vertex_buffer_requirements.memoryTypeBits & index_buffer_requirements.memoryTypeBits;
            device_vram_info.aligment = MAX(MAX(vertex_buffer_requirements.alignment, index_buffer_requirements.alignment), device_vram_info.aligment);
        }
        
        /* HOST BUFFER */ {
            const VkBufferCreateInfo host_buffer_info = {
                .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                .size = sizeof(Vertex) * raw_meshes[i].vertex_count + sizeof(u16) * raw_meshes[i].index_count,
                .queueFamilyIndexCount = 1,
                .pQueueFamilyIndices = &vulkan_context->render_queue_id,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE
            };
            if(vkCreateBuffer(vulkan_context->device, &host_buffer_info, NULL, &host_mesh_buffers[i]) != VK_SUCCESS) {
                stringPattern(&TRACED_STR("failed to create host mesh buffer file: %s"), (const void *[]){&mesh_infos[i].file}, &log_str);
                MSG_ERROR(render_context->msg_callback, &log_str);
                return FALSE;
            } 

            /* host memory requirements */
            VkMemoryRequirements host_buffer_requirements = (VkMemoryRequirements){0};
            vkGetBufferMemoryRequirements(vulkan_context->device, host_mesh_buffers[i], &host_buffer_requirements);
            
            /* adjust vram info size and fill vram region */
            host_vram_info.size = ALIGN(host_vram_info.size, host_buffer_requirements.alignment);
            host_mesh_regions[i] = (VramRegion) {
                .offset = host_vram_info.size,
                .size = host_buffer_requirements.size
            };
            host_vram_info.size += host_buffer_requirements.size;
        }
    
    }
    
    /* allocate device vram block */
    if(!allocateVram(vulkan_context, &device_vram_info, &meshes->mesh_device_vram)) {
        MSG_ERROR(render_context->msg_callback, &TRACED_STR("failed to allocate device render meshes vram"));
        return FALSE;
    }
    /* allocate host vram block */
    Vram host_vram = (Vram){0};
    if(!allocateVram(vulkan_context, &host_vram_info, &host_vram)) {
        MSG_ERROR(render_context->msg_callback, &TRACED_STR("failed to allocate host render meshes vram"));
        return FALSE;
    }
    
    /* bind buffers to memory */
    for(u32 i = 0; i < mesh_count; i++) {
        if(vkBindBufferMemory(vulkan_context->device, render_meshes[i].vertex_buffer, meshes->mesh_device_vram.memory, device_vertex_regions[i].offset) != VK_SUCCESS) {
            MSG_ERROR(render_context->msg_callback, &TRACED_STR("failed to bind device vertex buffer memory"));
            return FALSE;
        }
        if(vkBindBufferMemory(vulkan_context->device, render_meshes[i].index_buffer, meshes->mesh_device_vram.memory, device_index_regions[i].offset) != VK_SUCCESS) {
            MSG_ERROR(render_context->msg_callback, &TRACED_STR("failed to bind device index buffer memory"));
            return FALSE;
        }
        if(vkBindBufferMemory(vulkan_context->device, host_mesh_buffers[i], host_vram.memory, host_mesh_regions[i].offset) != VK_SUCCESS) {
            MSG_ERROR(render_context->msg_callback, &TRACED_STR("failed to bind host mesh buffer memory"));
            return FALSE;
        }
    }

    /* RAW MESHES TO HOST BUFFERS */ {
        void* mapped_memory = NULL;
        if(vkMapMemory(vulkan_context->device, host_vram.memory, 0, host_vram.size, 0, &mapped_memory) != VK_SUCCESS) {
            MSG_ERROR(render_context->msg_callback, &TRACED_STR("failed to map host mesh memory"));
            return FALSE;
        }
        
        for(u32 i = 0; i < mesh_count; i++) {
            /* make data hopefuly closer to us and mark it as constant */
            const u32 vertex_count = raw_meshes[i].vertex_count;
            const u32 index_count = raw_meshes[i].index_count;
            const Vertex* src_vertices = raw_meshes[i].vertices;
            const u16* src_indices = raw_meshes[i].indices;

            /* copy vertices */
            Vertex *dst_vertices = (Vertex *)((u8*)mapped_memory + host_mesh_regions[i].offset);
            for(u32 i = 0; i < vertex_count; i++) {
                dst_vertices[i] = src_vertices[i];
            }
            /* copy indices */
            u16 *dst_indices = (u16 *)((u8*)mapped_memory + host_mesh_regions[i].offset + sizeof(Vertex) * vertex_count);
            for(u32 i = 0; i < index_count; i++) {
                dst_indices[i] = src_indices[i];
            }
        }

        vkUnmapMemory(vulkan_context->device, host_vram.memory);
    }

    /* HOST BUFFERS TO DEVICE */ {
        VkCommandBuffer command_buffer = NULL;
        const VkCommandBufferAllocateInfo command_buffer_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = render_context->command_pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1
        };
        if(vkAllocateCommandBuffers(vulkan_context->device, &command_buffer_info, &command_buffer) != VK_SUCCESS) {
            MSG_ERROR(render_context->msg_callback, &TRACED_STR("failed to allocate command_buffer for mesh transfer"));
            return FALSE;
        }

        const VkCommandBufferBeginInfo begin_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
        };
        if(vkBeginCommandBuffer(command_buffer, &begin_info) != VK_SUCCESS) {
            MSG_ERROR(render_context->msg_callback, &TRACED_STR("failed to begin mesh tramsfer command buffer"));
            return FALSE;
        }

        for(u32 i = 0; i < mesh_count; i++) {
            /* regions to copy, we copy from 1 buffer into 2 buffers */
            const VkBufferCopy vertex_buffer_copy = {
                .size = raw_meshes[i].vertex_count * sizeof(Vertex)
            };
            const VkBufferCopy index_buffer_copy = {
                .srcOffset = raw_meshes[i].vertex_count * sizeof(Vertex),
                .size = raw_meshes[i].index_count * sizeof(u16)
            };
            /* cipy buffers */
            vkCmdCopyBuffer(command_buffer, host_mesh_buffers[i], render_meshes[i].vertex_buffer, 1, &vertex_buffer_copy);
            vkCmdCopyBuffer(command_buffer, host_mesh_buffers[i], render_meshes[i].index_buffer, 1, &index_buffer_copy);
        }

        if(vkEndCommandBuffer(command_buffer) != VK_SUCCESS) {
            MSG_ERROR(render_context->msg_callback, &TRACED_STR("failed to end mesh tramsfer command buffer"));
            return FALSE;
        }

        /* submit to queue */
        const VkSubmitInfo submit_info = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .commandBufferCount = 1,
            .pCommandBuffers = &command_buffer
        };
        if(vkQueueSubmit(vulkan_context->render_queue, 1, &submit_info, fence) != VK_SUCCESS) {
            MSG_ERROR(render_context->msg_callback, &TRACED_STR("failed to submit mesh transfer"));
            return FALSE;
        }
    }

    freeArena(&mesh_arena);
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

    /* fence is needed to sync transfer operations on gpu, its not needed on integrated device model */
    VkFence transfer_fence = NULL;
    if(vulkan_context->device_model == DEVICE_MODEL_DESCRETE) {
        const VkFenceCreateInfo transfer_fence_info = {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT /* so that wait for fences will not stall if fence is not used */
        };
        if(vkCreateFence(vulkan_context->device, &transfer_fence_info, NULL, &transfer_fence) != VK_SUCCESS) {
            MSG_ERROR(context->msg_callback, &TRACED_STR("failed to create transfer fence"));
            return FALSE;
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
            if(!allocateVram(vulkan_context, &images_alloc_info, &context->images_device_vram)) {
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
        if(!createScreenTextures(vulkan_context, &context->images_device_vram, context->msg_callback, &context->render_settings, &context->screen_images)) {
            MSG_ERROR(context->msg_callback, &TRACED_STR("failed to create screen textures"));
            return NULL;
        }
    }

    /* MESHES */
    if(info->mesh_count != 0) {
        if(vulkan_context->device_model == DEVICE_MODEL_DESCRETE) {
            vkResetFences(vulkan_context->device, 1, &transfer_fence);
            if(!createMeshBuffers_DescreteModel(context, info->mesh_count, info->meshes, &init_stack, transfer_fence)) {
                MSG_ERROR(context->msg_callback, &TRACED_STR("failed to create mesh buffers"));
                return FALSE;
            }
        }
        if(vulkan_context->device_model == DEVICE_MODEL_INTEGRATED) {
            MSG_ERROR(context->msg_callback, &TRACED_STR("failed to create mesh buffers"));
            return FALSE;
        }
    }

    /* PROGRAMS */
    if(info->program_count != 0) {
        if(!createShaderPrograms(context, info->program_count, info->programs, &init_stack)) {
            MSG_ERROR(context->msg_callback, &TRACED_STR("failed to create shader shader_programs"));
            return NULL;
        }
    }

    if(transfer_fence) {
        /* first wait for it then destroy */
        vkWaitForFences(vulkan_context->device, 1, &transfer_fence, TRUE, U64_MAX);
        vkDestroyFence(vulkan_context->device, transfer_fence, NULL);
    }

    return context;
}

void destroyRenderContext(RenderContext *context) {
    VulkanContext* vulkan_context = context->vulkan_context;

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

    /* MESHES */ {
        const u32 mesh_count = context->render_meshes.meshes_count;
        RenderMesh* render_meshes = context->render_meshes.render_meshes;
        for(u32 i = 0; i < mesh_count; i++) {
            vkDestroyBuffer(vulkan_context->device, render_meshes[i].vertex_buffer, NULL);
            vkDestroyBuffer(vulkan_context->device, render_meshes[i].index_buffer, NULL);
        }
        freeVram(vulkan_context, &context->render_meshes.mesh_device_vram);
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
        freeVram(vulkan_context, &context->images_device_vram);
        freeArena(&context->resource_arena);
    }

    /* COMMAND POOL */ {
        vkDestroyCommandPool(vulkan_context->device, context->command_pool, NULL);
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

    RenderCmd *render_cmd = NULL;
    UpdateInfo *update_info = NULL;

    /* allocate everything for update if needed */
    if(update_callback) {
        render_cmd = allocateArena(&render_context->resource_arena, sizeof(RenderCmd), 0);
        if(!render_cmd) {
            MSG_ERROR(render_context->msg_callback, &TRACED_STR("failed to allocate render_cmd"));
            return FALSE;
        }
        update_info = allocateArena(&render_context->resource_arena, sizeof(UpdateInfo), 0);
        if(!update_info) {
            MSG_ERROR(render_context->msg_callback, &TRACED_STR("failed to allocate update_info"));
            return FALSE;
        }
    }

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
                if(!createScreenTextures(vulkan_context, &render_context->images_device_vram, render_context->msg_callback, &render_context->render_settings, &render_context->screen_images)) {
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
        const VkImageView screen_color_view = render_context->screen_images.swapchain_image_views[render_image_id];
        const VkImageView screen_depth_view = render_context->screen_images.depth_image_view;

        /* @(FIX): this is temporary we should handle that in begin/end rendering and submit */
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

        /* transfer control to user */
        if(update_callback) {
            /* setup context */
            *update_info = (UpdateInfo) {
                .res_x = render_context->render_settings.extent.width,
                .res_y = render_context->render_settings.extent.height
            };
            *render_cmd = (RenderCmd) {
                .command_buffer = command_buffer,
                .render_context = render_context,
                .screen_color_view = screen_color_view,
                .screen_depth_view = screen_depth_view
            };
            /* transfer control */
            update_callback(update_info, render_cmd);
        }
        
        /* @(FIX): this is temporary */
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
                if(!createScreenTextures(vulkan_context, &render_context->images_device_vram, render_context->msg_callback, &render_context->render_settings, &render_context->screen_images)) {
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


void beginRendering(RenderCmd *cmd, u32 color_count, u32 *color_ids, u32 depth_id) {
    u32 res_x = 0;
    u32 res_y = 0;

    /* COLOR TARGETS */ {
        cmd->color_attachment_count = color_count;
        for(u32 i = 0; i < color_count; i++) {
            /* if screen color target */
            if(color_ids[i] == RENDER_ATTACHMENT_SCREEN_COLOR_ID) {
                cmd->color_attachments[i] = (VkRenderingAttachmentInfoKHR) {
                    .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
                    .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR,
                    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                    .imageView = cmd->screen_color_view
                };
                res_x = MAX(res_x, cmd->render_context->render_settings.extent.width);
                res_y = MAX(res_y, cmd->render_context->render_settings.extent.height);
                continue;
            }
        }
    }

    /* DEPTH TARGET */ {
        cmd->use_depth_atachment = FALSE;
        if(depth_id == RENDER_ATTACHMENT_SCREEN_DEPTH_ID) {
            cmd->depth_attachment = (VkRenderingAttachmentInfoKHR) {
                .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
                .clearValue = (VkClearValue) {
                    .depthStencil = (VkClearDepthStencilValue) {
                        .depth = 1.0
                    }
                },
                .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .imageView = cmd->screen_depth_view
            };
            cmd->use_depth_atachment = TRUE;
            res_x = MAX(res_x, cmd->render_context->render_settings.extent.width);
            res_y = MAX(res_y, cmd->render_context->render_settings.extent.height);
        }
    }

    /* put attchemnt info into that struct */
    const VkRenderingInfoKHR rendering_info = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,
        .colorAttachmentCount = cmd->color_attachment_count,
        .pColorAttachments = cmd->color_attachments,
        .layerCount = 1,
        .pDepthAttachment = cmd->use_depth_atachment ? &cmd->depth_attachment : NULL,
        .renderArea = (VkRect2D) {
            .offset = {0, 0},
            .extent = (VkExtent2D) {res_x, res_y}
        }
    };
    const VkViewport viewport = {
        .x = 0,
        .y = 0,
        .minDepth = 0.0,
        .maxDepth = 1.0,
        .width = (f32)res_x,
        .height = (f32)res_y
    };
    /* begin rendering KHR call */
    cmd->render_context->vulkan_context->cmd_begin_rendering(cmd->command_buffer, &rendering_info);
    vkCmdSetViewport(cmd->command_buffer, 0, 1, &viewport);
    vkCmdSetScissor(cmd->command_buffer, 0, 1, &rendering_info.renderArea);
}

void endRendering(RenderCmd *cmd) {
    cmd->render_context->vulkan_context->cmd_end_rendering(cmd->command_buffer);
    cmd->color_attachment_count = 0;
    cmd->use_depth_atachment = FALSE;
}

void drawProcedural(RenderCmd *cmd, u32 program_id, u32 vertex_count, u32 instance_count) {
    const RenderContext *render_context = cmd->render_context;

    ShaderProgram *last_program = cmd->last_shader_program;
    ShaderProgram *new_program = &render_context->shader_programs.shader_programs[program_id];
    /* if pipeline changed, bind new pipeline */
    if(last_program != new_program) {
        vkCmdBindPipeline(cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, new_program->pipeline);
        cmd->last_shader_program = new_program;
    }
    /* draw call */
    vkCmdDraw(cmd->command_buffer, vertex_count, instance_count, 0 , 0);
}
