#define VULKAN_INTERNAL
#include "gpu.h"


#define ADJUST_GPU_BUFFER_LOCATION(mem_req, loc, alloc_size, bits)  \
alloc_size = ALIGN(alloc_size, mem_req.alignment);                  \
loc = (GPUBufferLocation) {                                         \
    .offset = alloc_size,                                           \
    .size = mem_req.size                                            \
};                                                                  \
alloc_size += mem_req.size;                                         \
bits &= mem_req.memoryTypeBits;

/* DESCRIPTOR_SET_COUNT is indicating how many descriptors exist */
static b32 createDescriptors(
    VkDevice vk_device,
    MsgCallback_pfn msg_callback,
    VkDescriptorPool *descriptor_pool,
    VkDescriptorSetLayout *descriptor_layouts,
    VkDescriptorSet *descriptor_sets
) {
    const VkDescriptorPoolSize pool_sizes[] = {
        (VkDescriptorPoolSize) {
            .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1
        },
        (VkDescriptorPoolSize) {
            .type = VK_DESCRIPTOR_TYPE_SAMPLER,
            .descriptorCount = 4
        },
        (VkDescriptorPoolSize) {
            .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = MAX_DESCRIPTOR_BINDINGS
        },
        (VkDescriptorPoolSize) {
            .type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            .descriptorCount = 2 + MAX_DESCRIPTOR_BINDINGS
        },
        (VkDescriptorPoolSize) {
            .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .descriptorCount = 2 + MAX_DESCRIPTOR_BINDINGS
        }
    };

    const VkDescriptorPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = DESCRIPTOR_SET_COUNT,
        .poolSizeCount = ARRAY_SIZE(pool_sizes),
        .pPoolSizes = pool_sizes
    };
    if(vkCreateDescriptorPool(vk_device, &pool_info, NULL, descriptor_pool) != VK_SUCCESS) {
        MSG_ERROR(msg_callback, &TRACED_STR("failed to create vulkan descriptor pool"));
        return FALSE;
    }

    /* DESCRIPTOR SET LAYOUT 0 */ {
        const VkDescriptorSetLayoutBinding bindings[] = {
            (VkDescriptorSetLayoutBinding) {
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_ALL
            },
            (VkDescriptorSetLayoutBinding) {
                .binding = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
                .descriptorCount = 4,
                .stageFlags = VK_SHADER_STAGE_ALL
            },
            (VkDescriptorSetLayoutBinding) {
                .binding = 2,
                .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_ALL
            },
            (VkDescriptorSetLayoutBinding) {
                .binding = 3,
                .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_ALL
            },
            (VkDescriptorSetLayoutBinding) {
                .binding = 4,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_ALL
            },
            (VkDescriptorSetLayoutBinding) {
                .binding = 5,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_ALL
            }
        };
        const VkDescriptorSetLayoutCreateInfo layout_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = ARRAY_SIZE(bindings),
            .pBindings = bindings
        };

        if(vkCreateDescriptorSetLayout(vk_device, &layout_info, NULL, &descriptor_layouts[0]) != VK_SUCCESS) {
            MSG_ERROR(msg_callback, &TRACED_STR("failed to create vulkan descriptor set layout 0"));
            return FALSE;
        }
    }

    /* DESCRIPTOR SET LAYOUT 1 */ {
        const VkDescriptorSetLayoutBinding bindings[] = {
            (VkDescriptorSetLayoutBinding) {
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = MAX_DESCRIPTOR_BINDINGS,
                .stageFlags = VK_SHADER_STAGE_ALL
            }
        };
        const VkDescriptorSetLayoutCreateInfo layout_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = ARRAY_SIZE(bindings),
            .pBindings = bindings
        };

        if(vkCreateDescriptorSetLayout(vk_device, &layout_info, NULL, &descriptor_layouts[1]) != VK_SUCCESS) {
            MSG_ERROR(msg_callback, &TRACED_STR("failed to create vulkan descriptor set layout 1"));
            return FALSE;
        }
    }

    /* DESCRIPTOR SET LAYOUT */ {
        const VkDescriptorSetLayoutBinding bindings[] = {
            (VkDescriptorSetLayoutBinding) {
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                .descriptorCount = MAX_DESCRIPTOR_BINDINGS,
                .stageFlags = VK_SHADER_STAGE_ALL
            }
        };
        const VkDescriptorSetLayoutCreateInfo layout_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = ARRAY_SIZE(bindings),
            .pBindings = bindings
        };

        if(vkCreateDescriptorSetLayout(vk_device, &layout_info, NULL, &descriptor_layouts[2]) != VK_SUCCESS) {
            MSG_ERROR(msg_callback, &TRACED_STR("failed to create vulkan descriptor set layout 2"));
            return FALSE;
        }
    }

    VkDescriptorSetAllocateInfo descriptor_sets_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = *descriptor_pool,
        .descriptorSetCount = DESCRIPTOR_SET_COUNT,
        .pSetLayouts = descriptor_layouts
    };
    if(vkAllocateDescriptorSets(vk_device, &descriptor_sets_info, descriptor_sets) != VK_SUCCESS) {
        MSG_ERROR(msg_callback, &TRACED_STR("failed to allocate vulkan descriptor sets"));
        return FALSE;
    }

    return TRUE;
}

static VkPipeline createGraphicsPipeline(
    const GPUDevice *gpu_device,
    VkShaderModule vertex_shader,
    VkShaderModule fragment_shader,
    VkPipelineLayout pipeline_layout,
    GPUProgramFlags flags,
    u32 color_target_count,
    GPUFormat depth_target,
    const GPUFormat *color_targets,
    MsgCallback_pfn msg_callback
) {

    VkFormat depth_attachment = VK_FORMAT_UNDEFINED;
    VkFormat color_attachments[MAX_COLOR_TARGET_COUNT] = {0};

    /* ATTACHMENTS */ {
        if(color_target_count > MAX_COLOR_TARGET_COUNT) {
            MSG_ERROR(msg_callback, &TRACED_STR("too many color targets used in graphics pipeline"));
            return NULL;
        }
        /* fill formats for render targets */
        for(u32 i = 0; i < color_target_count; i++) {
            if(color_targets[i] == GPU_FORMAT_RGBA_COLOR) {
                color_attachments[i] = gpu_device->format_rgba_color;
            }
            if(color_targets[i] == GPU_RENDER_SURFACE_COLOR) {
                color_attachments[i] = gpu_device->format_surface_color;
            }
        }
        if(depth_target == GPU_FORMAT_DEPTH) {
            depth_attachment = gpu_device->format_depth;
        }
    }

    u32 vertex_binding_count = 0;
    u32 vertex_attribute_count = 0;
    VkVertexInputBindingDescription vertex_bindings[1] = {0};
    VkVertexInputAttributeDescription vertex_attributes[3] = {0};

    /* VERTEX INPUT */ {
        /* right now we have only 1 vertex binding */
        vertex_bindings[0] = (VkVertexInputBindingDescription) {
            .binding = 0,
            .stride = sizeof(GPUVertex),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
        };
        
        if(flags & GPU_PROGRAM_FLAG_VERTEX_POSITION) {
            vertex_binding_count = 1;
            vertex_attributes[vertex_attribute_count++] = (VkVertexInputAttributeDescription) {
                .binding = 0,
                .location = 0,
                .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                .offset = 0
            };
        }
        if(flags & GPU_PROGRAM_FLAG_VERTEX_NORMAL) {
            vertex_binding_count = 1;
            vertex_attributes[vertex_attribute_count++] = (VkVertexInputAttributeDescription) {
                .binding = 0,
                .location = 1,
                .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                .offset = 16
            };
        }
        if(flags & GPU_PROGRAM_FLAG_VERTEX_TEXCOORD) {
            vertex_binding_count = 1;
            vertex_attributes[vertex_attribute_count++] = (VkVertexInputAttributeDescription) {
                .binding = 0,
                .location = 2,
                .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                .offset = 32
            };
        }
    }

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
        .colorAttachmentCount = color_target_count,
        .pColorAttachmentFormats = color_attachments,
        .depthAttachmentFormat = depth_attachment
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
    if(vkCreateGraphicsPipelines(gpu_device->vk_device, NULL, 1, &pipeline_info, NULL, &pipeline) != VK_SUCCESS) {
        MSG_ERROR(msg_callback, &TRACED_STR("failed to create graphics pipeline"))
        return NULL;
    }
    return pipeline;
}

/* FIX: null terminated array in case of error, just fill whole array with 0 before creation */
static b32 createPrograms(
    const GPUDevice *gpu_device,
    MsgCallback_pfn msg_callback,
    u32 program_count,
    const GPUProgramInfo *program_infos,
    VkPipelineLayout pipeline_layout,
    VkPipeline *pipelines
) {
    String log_str = STACK_STR(256);
    VkDevice vk_device = gpu_device->vk_device;

    /* create programs one by one */
    for(u32 i = 0; i < program_count; i++) {
        const char *program_name = program_infos[i].name ? program_infos[i].name : "";

        if(program_infos[i].type == GPU_PROGRAM_TYPE_GRAPHICS) {
            VkShaderModule vertex_module = NULL;
            VkShaderModule fragment_module = NULL;
            
            /* create shader modules infos */
            const VkShaderModuleCreateInfo vertex_module_info = {
                .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                .pCode = program_infos[i].vertex_spv,
                .codeSize = program_infos[i].vertex_size
            };
            const VkShaderModuleCreateInfo fragment_module_info = {
                .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                .pCode = program_infos[i].fragment_spv,
                .codeSize = program_infos[i].fragment_size
            };
            /* create vertex shader */
            if(vkCreateShaderModule(vk_device, &vertex_module_info, NULL, &vertex_module) != VK_SUCCESS) {
                stringPattern(
                    &TRACED_STR("failed to create vulkan vertex shader module name: \"%c\" id: %u32/%u32"),
                    (const void *[]){program_name, &i, &program_count},
                    &log_str
                );
                MSG_ERROR(msg_callback, &log_str);
                goto _graphics_pipeline_fail;
            }
            /* create fragment shader */
            if(vkCreateShaderModule(vk_device, &fragment_module_info, NULL, &fragment_module) != VK_SUCCESS) {
                stringPattern(
                    &TRACED_STR("failed to create vulkan fragment shader module name: \"%c\" id: %u32/%u32"),
                    (const void *[]){program_name, &i, &program_count},
                    &log_str
                );
                MSG_ERROR(msg_callback, &log_str);
                goto _graphics_pipeline_fail;
            }

            /* create pipeline */
            pipelines[i] = createGraphicsPipeline(
                gpu_device,
                vertex_module,
                fragment_module,
                pipeline_layout,
                program_infos[i].flags,
                program_infos[i].color_format_count,
                program_infos[i].depth_format,
                program_infos[i].color_formats,
                msg_callback
            );
            if(!pipelines[i]) {
                stringPattern(
                    &TRACED_STR("failed to create vulkan graphics pipeline: \"%c\" id: %u32/%u32"),
                    (const void *[]){program_name, &i, &program_count},
                    &log_str
                );
                MSG_ERROR(msg_callback, &log_str);
                goto _graphics_pipeline_fail;
            }

            /* detsroy shader modules, we dont need them after we have pipelines */
            vkDestroyShaderModule(vk_device, vertex_module, NULL);
            vkDestroyShaderModule(vk_device, fragment_module, NULL);

            /* success */
            stringPattern(
                &CONST_STRING("created vulkan graphics pipeline: \"%c\" id: %u32/%u32"),
                (const void *[]){program_name, &i, &program_count},
                &log_str
            );
            MSG_LOG(msg_callback, &log_str);
            continue;

            /* safe destruction of pipeline and modules */
            /* if fail, destroy current pipeline and set value to NULL */
            _graphics_pipeline_fail: {
                if(pipelines[i]) {
                    vkDestroyPipeline(vk_device, pipelines[i], NULL);
                }
                if(vertex_module) {
                    vkDestroyShaderModule(vk_device, vertex_module, NULL);
                }
                if(fragment_module) {
                    vkDestroyShaderModule(vk_device, fragment_module, NULL);
                }
                pipelines[i] = NULL;
                return FALSE;
            }
        }
        if(program_infos[i].type == GPU_PROGRAM_TYPE_COMPUTE) {
            
        }
    
        stringPattern(&TRACED_STR("undetected GPU program type name: \"%c\" id: %u32/%u32"),
            (const void *[]){program_name, &i, &program_count},
            &log_str
        );
        MSG_LOG(msg_callback, &log_str);
        return FALSE;
    }

    return TRUE;
}

/* FIX: null terminated array in case of error, just fill whole array with 0 before creation */
/* ADD: initial write of user data provided through pointer in info struct, memory mapping */
static b32 createBuffers(
    /* inputs */
    const GPUDevice *gpu_device,
    GPUMemoryAllocator *gpu_memory_allocator,
    MsgCallback_pfn msg_callback,
    u32 mutable_storage_buffer_count,
    u32 storage_buffer_count,
    const GPUBufferInfo *uniform_buffer_info,
    const GPUBufferInfo *storage_buffer_infos,
    /* outputs */
    VkBuffer *uniform_buffer,
    VkBuffer *storage_buffers,
    GPUBufferLocation *uniform_buffer_location,
    GPUBufferLocation *storage_buffer_locations,
    GPUMemory *mutable_memory,
    GPUMemory *immutable_memory
) {
    VkDevice vk_device = gpu_device->vk_device;
    u64 mutable_memory_size = 0;
    u64 immutable_memory_size = 0;
    u32 mutable_memory_bits = U32_MAX;
    u32 immutable_memory_bits = U32_MAX;

    String log_str = STACK_STR(256);

    VkBufferCreateInfo buffer_create_info = (VkBufferCreateInfo){0};
    VkMemoryRequirements memory_requirements = (VkMemoryRequirements){0};

    if(uniform_buffer_info) {
        buffer_create_info = (VkBufferCreateInfo) {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            .size = uniform_buffer_info->size
        };
        if(vkCreateBuffer(vk_device, &buffer_create_info, NULL, uniform_buffer) != VK_SUCCESS) {
            MSG_ERROR(msg_callback, &TRACED_STR("failed to create uniform buffer"));
            if(*uniform_buffer) {
                vkDestroyBuffer(vk_device, *uniform_buffer, NULL);
                *uniform_buffer = NULL;
            }
            return FALSE;
        }
    
        /* adjust allocation size and set buffer location */
        vkGetBufferMemoryRequirements(vk_device, *uniform_buffer, &memory_requirements);
        ADJUST_GPU_BUFFER_LOCATION(memory_requirements, *uniform_buffer_location, mutable_memory_size, mutable_memory_bits);

        MSG_LOG(msg_callback, &CONST_STRING("created vulkan uniform buffer"));
    }
    for(u32 i = 0; i < mutable_storage_buffer_count; i++) {
        buffer_create_info = (VkBufferCreateInfo) {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            .size = storage_buffer_infos[i].size
        };
        if(vkCreateBuffer(vk_device, &buffer_create_info, NULL, &storage_buffers[i]) != VK_SUCCESS) {
            stringPattern(
                &TRACED_STR("failed to create mutable storage buffer id: %u32/%u32"),
                (const void *[]){&i, &storage_buffer_count},
                &log_str
            );
            MSG_ERROR(msg_callback, &log_str);

            /* set current buffer to null in case of error */
            if(storage_buffers[i]) {
                vkDestroyBuffer(vk_device, storage_buffers[i], NULL);
                storage_buffers[i] = NULL;
            }
            return FALSE;
        }

        /* adjust allocation size and set buffer location */
        vkGetBufferMemoryRequirements(vk_device, storage_buffers[i], &memory_requirements);
        ADJUST_GPU_BUFFER_LOCATION(memory_requirements, storage_buffer_locations[i], mutable_memory_size, mutable_memory_bits);

        stringPattern(
            &CONST_STRING("created vulkan storage buffer id: %u32/%u32"), 
            (const void *[]){&i, &storage_buffer_count},
            &log_str
        );
        MSG_LOG(msg_callback, &log_str);
    }
    for(u32 i = mutable_storage_buffer_count; i < storage_buffer_count; i++) {
        buffer_create_info = (VkBufferCreateInfo) {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            .size = storage_buffer_infos[i].size
        };
        if(vkCreateBuffer(vk_device, &buffer_create_info, NULL, &storage_buffers[i]) != VK_SUCCESS) {
            stringPattern(
                &TRACED_STR("failed to create immutable storage buffer id: %u32/%u32"),
                (const void *[]){&i, &storage_buffer_count},
                &log_str
            );
            MSG_ERROR(msg_callback, &log_str);
            
            if(storage_buffers[i]) {
                vkDestroyBuffer(vk_device, storage_buffers[i], NULL);
                storage_buffers[i] = NULL;
            }
            return FALSE;
        }

        /* adjust allocation size and set buffer location */
        vkGetBufferMemoryRequirements(vk_device, storage_buffers[i], &memory_requirements);
        ADJUST_GPU_BUFFER_LOCATION(memory_requirements, storage_buffer_locations[i], immutable_memory_size, immutable_memory_bits);

        stringPattern(
            &CONST_STRING("created vulkan storage buffer id: %u32/%u32"), 
            (const void *[]){&i, &storage_buffer_count},
            &log_str
        );
        MSG_LOG(msg_callback, &log_str);
    }

    /* allocate muatble resouce memory */
    if(mutable_memory_size != 0) {
        if(!allocateGPUMemory(
            gpu_memory_allocator, 
            "static resources mutable allocation", 
            GPU_MEMORY_USE_HOST_TO_DEVICE,
            mutable_memory_bits,
            mutable_memory_size,
            mutable_memory
        )) {
            MSG_ERROR(msg_callback, &TRACED_STR("failed to allocate gpu static resources mutable memory"));
            return FALSE;
        }
    }
    /* allocate immutable resource memory */
    if(immutable_memory_size != 0) {
        if(!allocateGPUMemory(
            gpu_memory_allocator, 
            "static resources immutable allocation", 
            GPU_MEMORY_USE_DEVICE,
            immutable_memory_bits,
            immutable_memory_size,
            immutable_memory
        )) {
            MSG_ERROR(msg_callback, &TRACED_STR("failed to allocate gpu static resources immutable memory"));
            return FALSE;
        }
    }

    return TRUE;
}

static b32 createBackScreen(
    /* inputs */
    const GPUDevice *gpu_device,
    GPUMemoryAllocator *gpu_memory_allocator,
    MsgCallback_pfn msg_callback,
    /* outputs */
    VkImage *screen_color_image,
    VkImage *screen_depth_image,
    VkImageView *screen_color_image_view,
    VkImageView *screen_depth_image_view,
    GPUBufferLocation *screen_color_location,
    GPUBufferLocation *screen_depth_location,
    GPUMemory *screen_memory
) {
    VkDevice vk_device = gpu_device->vk_device;
    String log_str = STACK_STR(256);

    /* set to maximum supported resolution for back textures */
    u32 max_screen_x = 3840;
    u32 max_screen_y = 2160;

    #ifdef _WIN32
        DEVMODE dm = {0};
        dm.dmSize = sizeof(DEVMODE);
        
        DWORD win32_max_x = 0;
        DWORD win32_max_y = 0;
        DWORD mode_id = 0;
        
        /* get all display sizes */
        while (EnumDisplaySettings(NULL, mode_id, &dm)) {
            win32_max_x = (dm.dmPelsWidth > win32_max_x) ? dm.dmPelsWidth : win32_max_x;
            win32_max_y = (dm.dmPelsHeight > win32_max_y) ? dm.dmPelsHeight : win32_max_y;
            mode_id++;
        }
        
        /* clamp max display sizes */
        max_screen_x = MIN(max_screen_x, win32_max_x);
        max_screen_y = MIN(max_screen_y, win32_max_y);
    #endif

    /* CREATE IMAGES */ {
        const VkImageCreateInfo screen_color_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
            .extent = (VkExtent3D) {
                .width = max_screen_x,
                .height = max_screen_y,
                .depth = 1
            },
            .mipLevels = 1,
            .arrayLayers = 1,
            .format = gpu_device->format_rgba_color,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .samples = VK_SAMPLE_COUNT_1_BIT
        };
        const VkImageCreateInfo screen_depth_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT,
            .extent = (VkExtent3D) {
                .width = max_screen_x,
                .height = max_screen_y,
                .depth = 1
            },
            .mipLevels = 1,
            .arrayLayers = 1,
            .format = gpu_device->format_depth,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .samples = VK_SAMPLE_COUNT_1_BIT
        };
        if(vkCreateImage(vk_device, &screen_color_info, NULL, screen_color_image) != VK_SUCCESS) {
            MSG_ERROR(msg_callback, &TRACED_STR("failed to create screen color image"));
            return FALSE;
        }
        if(vkCreateImage(vk_device, &screen_depth_info, NULL, screen_depth_image) != VK_SUCCESS) {
            MSG_ERROR(msg_callback, &TRACED_STR("failed to create screen depth image"));
            return FALSE;
        }
    }

    u64 allocation_size = 0;
    u32 memory_bits = U32_MAX;

    /* ALLOCATE MEMORY */ {
        VkMemoryRequirements screen_color_requirements = (VkMemoryRequirements){0};
        VkMemoryRequirements screen_depth_requirements = (VkMemoryRequirements){0};

        vkGetImageMemoryRequirements(vk_device, *screen_color_image, &screen_color_requirements);
        vkGetImageMemoryRequirements(vk_device, *screen_depth_image, &screen_depth_requirements);

        ADJUST_GPU_BUFFER_LOCATION(screen_color_requirements, *screen_color_location, allocation_size, memory_bits);
        ADJUST_GPU_BUFFER_LOCATION(screen_depth_requirements, *screen_depth_location, allocation_size, memory_bits);

        if(!allocateGPUMemory(
            gpu_memory_allocator, 
            "screen images allocation", 
            GPU_MEMORY_USE_DEVICE,
            memory_bits,
            allocation_size,
            screen_memory
        )) {
            MSG_ERROR(msg_callback, &TRACED_STR("failed to allocate screen images memory"));
            return FALSE;
        }
    }

    /* BIND IMAGES AND CREATE VIEWS */ {
        /* bind color image memory */
        if(vkBindImageMemory(
            vk_device, 
            *screen_color_image, 
            screen_memory->memory, 
            screen_color_location->offset
        ) != VK_SUCCESS) {
            MSG_ERROR(msg_callback, &TRACED_STR("failed to bind screen color image memory"));
            return FALSE;
        }
        /* bind depth image memory */
        if(vkBindImageMemory(
            vk_device,
            *screen_depth_image,
            screen_memory->memory,
            screen_depth_location->offset
        ) != VK_SUCCESS) {
            MSG_ERROR(msg_callback, &TRACED_STR("failed to bind screen depth image memory"));
            return FALSE;
        }

        const VkImageViewCreateInfo screen_color_view_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .image = *screen_color_image,
            .format = gpu_device->format_rgba_color,
            .components = (VkComponentMapping) {
                .r = VK_COMPONENT_SWIZZLE_R,
                .g = VK_COMPONENT_SWIZZLE_G,
                .b = VK_COMPONENT_SWIZZLE_B,
                .a = VK_COMPONENT_SWIZZLE_A
            },
            .subresourceRange = (VkImageSubresourceRange) {
                .layerCount = 1,
                .levelCount = 1,
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT
            }
        };
        const VkImageViewCreateInfo screen_depth_view_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .image = *screen_depth_image,
            .format = gpu_device->format_depth,
            .components = (VkComponentMapping) {
                .r = VK_COMPONENT_SWIZZLE_R,
                .g = VK_COMPONENT_SWIZZLE_ZERO,
                .b = VK_COMPONENT_SWIZZLE_ZERO,
                .a = VK_COMPONENT_SWIZZLE_ZERO
            },
            .subresourceRange = (VkImageSubresourceRange) {
                .layerCount = 1,
                .levelCount = 1,
                .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT
            }
        };

        if(vkCreateImageView(vk_device, &screen_color_view_info, NULL, screen_color_image_view) != VK_SUCCESS) {
            MSG_ERROR(msg_callback, &TRACED_STR("failed to create screen color image view"));
            return FALSE;
        }
        if(vkCreateImageView(vk_device, &screen_depth_view_info, NULL, screen_depth_image_view) != VK_SUCCESS) {
            MSG_ERROR(msg_callback, &TRACED_STR("failed to create screen color image view"));
            return FALSE;
        }
    }

    stringPattern(
        &CONST_STRING("created back screen images of size {x: %u32 y: %u32, memory size: %u64}"),
        (const void *[]){&max_screen_x, &max_screen_y, &allocation_size},
        &log_str
    );
    MSG_LOG(msg_callback, &log_str);

    return TRUE;
}

b32 createGPUStaticResources(
    GPU *gpu, 
    const CreateGPUStaticResourcesIn *input, 
    CreateGPUStaticResourcesOut *output
) {
    const GPUDevice *device = &gpu->device;
    GPUStaticResources *static_resources = &gpu->static_resources;
    MsgCallback_pfn msg_callback = gpu->msg_callback;
    *static_resources = (GPUStaticResources){0};

    if(!createDescriptors(
        device->vk_device,
        msg_callback,
        &static_resources->descriptor_pool,
        static_resources->descriptor_layouts,
        static_resources->descriptor_sets
    )) {
        MSG_ERROR(msg_callback, &TRACED_STR("failed to create vulkan descriptors"));
        return FALSE;
    }

    /* PIPELINE LAYOUT */ {
        VkPipelineLayoutCreateInfo pipeline_layout_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = DESCRIPTOR_SET_COUNT,
            .pSetLayouts = static_resources->descriptor_layouts,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = (VkPushConstantRange[]) {
                (VkPushConstantRange) {
                    .offset = 0,
                    .size = PUSH_CONSTANT_RANGE_SIZE,
                    .stageFlags = VK_SHADER_STAGE_ALL
                }
            }
        };
        if(vkCreatePipelineLayout(
            device->vk_device, 
            &pipeline_layout_info, 
            NULL, 
            &static_resources->pipeline_layout
        ) != VK_SUCCESS) {
            MSG_ERROR(msg_callback, &TRACED_STR("failed to create vulkan pipeline layout"));
            return FALSE;
        }
    }
    
    /* ALLOCATION */ {
        u64 allocation_size = sizeof(VkPipeline) * input->program_count + (sizeof(VkBuffer) + sizeof(GPUBufferLocation)) * input->storage_buffer_count;
        /* allocate if size is non zero */
        if(allocation_size != 0) {
            void *allocation = allocateStaticResources(
                &gpu->resource_allocator,
                allocation_size,
                0
            );
            if(!allocation) {
                MSG_ERROR(msg_callback, &TRACED_STR("failed to allocate gpu static resources"));
                return FALSE;
            }

            static_resources->pipelines                 = (VkPipeline *)allocation;
            static_resources->storage_buffers           = (VkBuffer *)((u8 *)allocation + sizeof(VkPipeline) * input->program_count);
            static_resources->storage_buffer_locations  = (GPUBufferLocation *)((u8 *)allocation + sizeof(VkPipeline) * input->program_count + sizeof(VkBuffer) * input->storage_buffer_count);
        }
    }

    /* PIPELINES */ {
        if(input->program_count != 0) {
            static_resources->pipeline_count = input->program_count;
            /* create pipelines (gpu programs) */
            if(!createPrograms(
                device,
                msg_callback,
                input->program_count,
                input->programs,
                static_resources->pipeline_layout,
                static_resources->pipelines
            )) {
                MSG_ERROR(msg_callback, &TRACED_STR("failed to create vulkan gpu programs"));
                return FALSE;
            }
        }
    }

    /* BUFFERS */ {
        if(input->storage_buffer_count != 0 || input->uniform_buffer) {
            static_resources->mutable_storage_buffer_count = input->mutable_storage_buffer_count;
            static_resources->storage_buffer_count = input->storage_buffer_count;
            /* create buffers */
            if(!createBuffers(
                device,
                &gpu->memory_allocator,
                msg_callback,
                input->mutable_storage_buffer_count,
                input->storage_buffer_count,
                input->uniform_buffer,
                input->storage_buffers,
                &static_resources->uniform_buffer,
                static_resources->storage_buffers,
                &static_resources->uniform_buffer_location,
                static_resources->storage_buffer_locations,
                &static_resources->mutable_memory,
                &static_resources->immutable_memory
            )) {
                MSG_ERROR(msg_callback, &TRACED_STR("failed to create static resources buffers"))
            }
        }
    }

    /* BACK SCREEN */ {
        if(!createBackScreen(
            device,
            &gpu->memory_allocator,
            msg_callback,
            &static_resources->screen_color_image,
            &static_resources->screen_depth_image,
            &static_resources->screen_color_view,
            &static_resources->screen_depth_view,
            &static_resources->screen_color_location,
            &static_resources->screen_depth_location,
            &static_resources->screen_memory
        )) {
            MSG_ERROR(msg_callback, &TRACED_STR("failed to create vulkan back screen textures"));
            return FALSE;
        }
    }

    MSG_LOG(msg_callback, &CONST_STRING("created vulkan static resources"));
    return TRUE;
}

void destroyGPUStaticResources(
    GPU *gpu
) {
    VkDevice vk_device = gpu->device.vk_device;
    GPUStaticResources *static_resources = &gpu->static_resources;
    MsgCallback_pfn msg_callback = gpu->msg_callback;

    /* BACK SCREEN */ {
        if(static_resources->screen_color_view) {
            vkDestroyImageView(vk_device, static_resources->screen_color_view, NULL);
        }
        if(static_resources->screen_depth_view) {
            vkDestroyImageView(vk_device, static_resources->screen_depth_view, NULL);
        }

        if(static_resources->screen_color_image) {
            vkDestroyImage(vk_device, static_resources->screen_color_image, NULL);
        }
        if(static_resources->screen_depth_image) {
            vkDestroyImage(vk_device, static_resources->screen_depth_image, NULL);
        }

        if(static_resources->screen_memory.memory) {
            if(!freeGPUMemory(&gpu->memory_allocator, &static_resources->screen_memory)) {
                MSG_WARNING(msg_callback, &TRACED_STR("failed to free screen images memory"));
            }
        }
    }
    
    /* BUFFERS */ {
        /* uniform buffer */
        if(static_resources->uniform_buffer) {
            vkDestroyBuffer(vk_device, static_resources->uniform_buffer, NULL);
        }

        /* storage buffers */
        const u32 storage_buffer_count = static_resources->storage_buffer_count;
        const VkBuffer *storage_buffers = static_resources->storage_buffers;
        /* if error appeared during creation last buffer set to NULL */
        for(u32 i = 0; i < storage_buffer_count && storage_buffers[i]; i++) {
            vkDestroyBuffer(vk_device, storage_buffers[i], NULL);
        }

        if(static_resources->mutable_memory.memory) {
            if(!freeGPUMemory(&gpu->memory_allocator, &static_resources->mutable_memory)) {
                MSG_WARNING(msg_callback, &TRACED_STR("failed to free static resources mutable gpu memory"));
            }
        }
        if(static_resources->immutable_memory.memory) {
            if(!freeGPUMemory(&gpu->memory_allocator, &static_resources->immutable_memory)) {
                MSG_WARNING(msg_callback, &TRACED_STR("failed to free static resources immutable gpu memory"));
            }
        }
    }

    /* PIPELINES (GPU PROGRAMS)*/ {
        const u32 pipeline_created_count = static_resources->pipeline_count;
        const VkPipeline *pipelines = static_resources->pipelines;
        /* in case of error on createGPUStaticResources last program (with error is indentified as NULL) */
        for(u32 i = 0; i < pipeline_created_count && pipelines[i]; i++) {
            vkDestroyPipeline(vk_device, pipelines[i], NULL);
        }
    }

    /* destroy pipeline layout */
    vkDestroyPipelineLayout(vk_device, static_resources->pipeline_layout, NULL);

    /* unlike other resources this one destroys everything 
        with check for null no stop, because array is always small */
    VkDescriptorSetLayout *layouts = static_resources->descriptor_layouts;
    VkDescriptorSet *sets = static_resources->descriptor_sets;
    for(u32 i = 0; i < DESCRIPTOR_SET_COUNT; i++) {
        if(layouts[i]) {
            vkDestroyDescriptorSetLayout(vk_device, layouts[i], NULL);
        }
        layouts[i] = NULL;
        sets[i] = NULL;
    }
    vkDestroyDescriptorPool(vk_device, static_resources->descriptor_pool, NULL);

    MSG_LOG(msg_callback, &CONST_STRING("destroyed vulkan static resources"));
}
