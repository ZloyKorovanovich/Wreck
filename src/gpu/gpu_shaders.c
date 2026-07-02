#include "gpu_internal.h"

extern const VkFormat format_conversion_table[GPU_FORMAT_COUNT];

const VkDescriptorPoolSize descriptor_pool_sizes[] = {
    {VK_DESCRIPTOR_TYPE_SAMPLER       , GPU_DESCRIPTOR_SET_COUNT + 4},
    {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, GPU_MAX_STATIC_BUFFERS      },
    {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, GPU_MAX_STATIC_BUFFERS      },
    {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,  GPU_MAX_STATIC_IMAGES       },
    {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,  GPU_MAX_STATIC_IMAGES       }
};

const VkDescriptorType descriptor_type_conversion_table[GPU_DESCRIPTOR_TYPE_COUNT] = {
    [GPU_DESCRIPTOR_TYPE_NONE          ] = VK_DESCRIPTOR_TYPE_MAX_ENUM,
    [GPU_DESCRIPTOR_TYPE_UNIFORM_BUFFER] = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    [GPU_DESCRIPTOR_TYPE_STORAGE_BUFFER] = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
    [GPU_DESCRIPTOR_TYPE_SAMPLED_IMAGE ] = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
    [GPU_DESCRIPTOR_TYPE_STORAGE_IMAGE ] = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
    [GPU_DESCRIPTOR_TYPE_SAMPLER       ] = VK_DESCRIPTOR_TYPE_SAMPLER
};

VkPipeline create_grpahics_pipeline(
    VkDevice         device,
    VkFormat         surface_format,
    VkPipelineLayout pipeline_layout,
    VkShaderModule   module_vertex,
    VkShaderModule   module_fragment,
    const GpuFormat* color_formats,
    u32              color_formats_count,
    GpuFormat        depth_format
) {
    /* convert attachment formats */
    VkFormat  attachments_color[GPU_MAX_COLOR_ATTACHMENTS] = {0};
    VkFormat  attachment_depth                             = 0;
    const u32 attachments_color_count                      = color_formats_count;

    for(u32 i = 0; i != attachments_color_count; i++) {
        if(color_formats[i] == GPU_FORMAT_SURFACE) {
            attachments_color[i] = surface_format;
        }
        else if(color_formats[i] < GPU_FORMAT_COUNT) {
            attachments_color[i] = format_conversion_table[color_formats[i]];
        }
        else {
            LOG_ERROR("invalid pipeline color format: %u", color_formats[i]);
            goto fail;
        }
    }

    if(depth_format < GPU_FORMAT_COUNT) {
        attachment_depth = format_conversion_table[depth_format];
    }
    else {
        LOG_ERROR("invalid pipeline depth format: %u", depth_format);
        goto fail;
    }

    /* create pipeline */
    const VkPipelineShaderStageCreateInfo shader_stages[2] = {
        (VkPipelineShaderStageCreateInfo) {
            .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pName  = GPU_SHADER_ENTRY_VERTEX,
            .stage  = VK_SHADER_STAGE_VERTEX_BIT,
            .module = module_vertex
        },
        (VkPipelineShaderStageCreateInfo) {
            .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pName  = GPU_SHADER_ENTRY_FRAGMENT,
            .stage  = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = module_fragment
        }
    };

    /* rendering info */
    const VkPipelineRenderingCreateInfoKHR rendering_create_info = (VkPipelineRenderingCreateInfoKHR) {
        .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
        .colorAttachmentCount    = attachments_color_count,
        .pColorAttachmentFormats = attachments_color,
        .depthAttachmentFormat   = attachment_depth
    };


    /* dynamic states */
    const VkDynamicState graphics_pipeline_dynamic_states[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    const VkPipelineDynamicStateCreateInfo dynamic_state = (VkPipelineDynamicStateCreateInfo) {
        .sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = ARRAY_SIZE(graphics_pipeline_dynamic_states),
        .pDynamicStates    = graphics_pipeline_dynamic_states
    };
    const VkPipelineViewportStateCreateInfo viewport_state = (VkPipelineViewportStateCreateInfo) {
        .sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount  = 1
    };

    /* static states */
    const VkPipelineVertexInputStateCreateInfo vertex_input_state = (VkPipelineVertexInputStateCreateInfo) {
        .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexAttributeDescriptionCount = 0,
        .vertexBindingDescriptionCount   = 0,
        .pVertexAttributeDescriptions    = NULL,
        .pVertexBindingDescriptions      = NULL
    };
    const VkPipelineInputAssemblyStateCreateInfo input_assembly_state = (VkPipelineInputAssemblyStateCreateInfo) {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = FALSE
    };
    const VkPipelineRasterizationStateCreateInfo rasterization_state = (VkPipelineRasterizationStateCreateInfo) {
        .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable        = FALSE,
        .rasterizerDiscardEnable = FALSE,
        .polygonMode             = VK_POLYGON_MODE_FILL,
        .lineWidth               = 1.0f,
        .cullMode                = VK_CULL_MODE_NONE,
        .frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable         = FALSE,
        .depthBiasConstantFactor = 0.0,
        .depthBiasClamp          = 0.0,
        .depthBiasSlopeFactor    = 0.0
    };
    const VkPipelineMultisampleStateCreateInfo multisample_state = (VkPipelineMultisampleStateCreateInfo) {
        .sType                 = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .sampleShadingEnable   = FALSE,
        .rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT,
        .minSampleShading      = 1.0f,
        .pSampleMask           = NULL,
        .alphaToCoverageEnable = FALSE,
        .alphaToOneEnable      = FALSE
    };
    const VkPipelineColorBlendAttachmentState color_blend_attachment_state = (VkPipelineColorBlendAttachmentState) {
        .colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        .blendEnable         = FALSE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
        .colorBlendOp        = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp        = VK_BLEND_OP_ADD
    };
    const VkPipelineColorBlendStateCreateInfo color_blend_state = (VkPipelineColorBlendStateCreateInfo) {
        .sType             = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable     = VK_FALSE,
        .logicOp           = VK_LOGIC_OP_COPY,
        .attachmentCount   = 1,
        .pAttachments      = &color_blend_attachment_state,
        .blendConstants[0] = 0.0f,
        .blendConstants[1] = 0.0f,
        .blendConstants[2] = 0.0f,
        .blendConstants[3] = 0.0f
    };
    const VkPipelineDepthStencilStateCreateInfo depth_stencil_state = (VkPipelineDepthStencilStateCreateInfo) {
        .sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable  = TRUE,
        .depthWriteEnable = TRUE,
        .depthCompareOp   = VK_COMPARE_OP_LESS_OR_EQUAL
    };

    VkGraphicsPipelineCreateInfo graphics_pipeline_info = (VkGraphicsPipelineCreateInfo) {
        .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .basePipelineHandle  = NULL,
        .basePipelineIndex   = -1,
        .stageCount          = 2,
        .pStages             = shader_stages,
        .pVertexInputState   = &vertex_input_state,
        .pInputAssemblyState = &input_assembly_state,
        .pViewportState      = &viewport_state,
        .pRasterizationState = &rasterization_state,
        .pMultisampleState   = &multisample_state,
        .pDepthStencilState  = &depth_stencil_state,
        .pColorBlendState    = &color_blend_state,
        .pDynamicState       = &dynamic_state,
        .layout              = pipeline_layout,
        .renderPass          = NULL,
        .pNext               = &rendering_create_info
    };

    VkPipeline graphics_pipeline = NULL;

    if(vkCreateGraphicsPipelines(device, NULL, 1, &graphics_pipeline_info, NULL, &graphics_pipeline) != VK_SUCCESS) {
        LOG_ERROR("failed to create graphics pipeline");
        goto fail;
    }

    return graphics_pipeline;

    fail: {
        return NULL;
    }
}

VkPipeline create_compute_pipeline(
    VkDevice         device,
    VkPipelineLayout pipeline_layout,
    VkShaderModule   module_compute
) {
    const VkComputePipelineCreateInfo compute_pipeline_info = {
        .sType              = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .layout             = pipeline_layout,
        .basePipelineHandle = NULL,
        .basePipelineIndex  = -1,
        .stage              = {
            .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage  = VK_SHADER_STAGE_COMPUTE_BIT,
            .pName  = GPU_SHADER_ENTRY_COMPUTE,
            .module = module_compute
        }
    };

    VkPipeline compute_pipeline = NULL;

    if(vkCreateComputePipelines(device, NULL, 1, &compute_pipeline_info, NULL, &compute_pipeline) != VK_SUCCESS) {
        LOG_ERROR("failed to create compute pipeline");
        goto fail;
    }

    return compute_pipeline;

    fail: {
        return NULL;
    }
}

b32 create_pipelines(
    VkDevice            device,
    VkFormat            surface_format,
    VkPipelineLayout    pipeline_layout,
    const PipelineInfo* pipeline_infos,
    u32                 pipeline_infos_count,
    VkPipeline*         pipelines
) {
    for(u32 i = 0; i != pipeline_infos_count; i++) {
        /* graphics pipeline */
        if(pipeline_infos[i].type == GPU_PIPELINE_TYPE_GRAPHICS) {
            if(
                pipeline_infos[i].vertex   == NULL || pipeline_infos[i].vertex_size   == 0 ||
                pipeline_infos[i].fragment == NULL || pipeline_infos[i].fragment_size == 0 ||
                pipeline_infos[i].compute  != NULL || pipeline_infos[i].compute_size  != 0
            ) {
                LOG_ERROR(
                    "graphics pipeline invalid spir-v pointers vertex: %p,%llu fragment: %p,%llu compute: %p,%llu", 
                    pipeline_infos[i].vertex  , pipeline_infos[i].vertex_size,
                    pipeline_infos[i].fragment, pipeline_infos[i].fragment_size,
                    pipeline_infos[i].compute , pipeline_infos[i].compute_size
                );
                goto fail;
            }

            /* create shader modules */
            const VkShaderModuleCreateInfo module_vertex_info = {
                .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                .pCode    = pipeline_infos[i].vertex,
                .codeSize = pipeline_infos[i].vertex_size
            };
            const VkShaderModuleCreateInfo module_fragment_info = {
                .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                .pCode    = pipeline_infos[i].fragment,
                .codeSize = pipeline_infos[i].fragment_size
            };

            VkShaderModule module_vertex   = NULL;
            VkShaderModule module_fragment = NULL;

            if(vkCreateShaderModule(device, &module_vertex_info, NULL, &module_vertex) != VK_SUCCESS) {
                LOG_ERROR("failed to create vertex shader module id: %u/%u", i, pipeline_infos_count);
                goto fail;
            }
            if(vkCreateShaderModule(device, &module_fragment_info, NULL, &module_fragment) != VK_SUCCESS) {
                LOG_ERROR("failed to create fragment shader module id: %u/%u", i, pipeline_infos_count);
                goto fail;
            }

            /* create pipeline */
            pipelines[i] = create_grpahics_pipeline(
                device,
                surface_format,
                pipeline_layout,
                module_vertex,
                module_fragment,
                pipeline_infos[i].color_formats,
                pipeline_infos[i].color_formats_count,
                pipeline_infos[i].depth_format
            );
            if(pipelines[i] == NULL) {
                LOG_ERROR("failed to create graphics pipeline id: %u/%u", i, pipeline_infos_count);
                goto fail;
            }

            vkDestroyShaderModule(device, module_vertex  , NULL);
            vkDestroyShaderModule(device, module_fragment, NULL);
        }
        /* compute pipeline */
        if(pipeline_infos[i].type == GPU_PIPELINE_TYPE_COMPUTE) {
            if(
                pipeline_infos[i].vertex   != NULL || pipeline_infos[i].vertex_size   != 0 ||
                pipeline_infos[i].fragment != NULL || pipeline_infos[i].fragment_size != 0 ||
                pipeline_infos[i].compute  == NULL || pipeline_infos[i].compute_size  == 0
            ) {
                LOG_ERROR(
                    "compute pipeline invalid spir-v pointers vertex: %p,%llu fragment: %p,%llu compute: %p,%llu", 
                    pipeline_infos[i].vertex  , pipeline_infos[i].vertex_size,
                    pipeline_infos[i].fragment, pipeline_infos[i].fragment_size,
                    pipeline_infos[i].compute , pipeline_infos[i].compute_size
                );
                goto fail;
            }

            /* create shader module */
            const VkShaderModuleCreateInfo module_compute_info = {
                .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                .pCode    = pipeline_infos[i].compute,
                .codeSize = pipeline_infos[i].compute_size
            };
            
            VkShaderModule module_compute = NULL;

            if(vkCreateShaderModule(device, &module_compute_info, NULL, &module_compute) != VK_SUCCESS) {
                LOG_ERROR("failed to create compute shader module id: %u/%u", i, pipeline_infos_count);
                goto fail;
            }

            /* create pipeline */
            pipelines[i] = create_compute_pipeline(
                device,
                pipeline_layout,
                module_compute
            );
            if(pipelines[i] == NULL) {
                LOG_ERROR("failed to create compute pipeline id: %u/%u", i, pipeline_infos_count);
                goto fail;
            }

            vkDestroyShaderModule(device, module_compute, NULL);
        }
    }

    return TRUE;

    fail: {
        return FALSE;
    }
}

b32 create_descriptors(
    VkDevice                  device,
    const DescriptorSetInfo*  descriptor_set_infos,
    VkDescriptorPool*         descriptor_pool,
    VkDescriptorSet*          descriptor_sets,
    VkDescriptorSetLayout*    descriptor_set_layouts,
    VkDescriptorType*         descriptor_types,
    VkPipelineLayout*         pipeline_layout
) {
    /* create descriptor pool */
    const VkDescriptorPoolCreateInfo descriptor_pool_info = {
        .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets       = GPU_DESCRIPTOR_SET_COUNT,
        .poolSizeCount = ARRAY_SIZE(descriptor_pool_sizes),
        .pPoolSizes    = descriptor_pool_sizes
    };

    if(vkCreateDescriptorPool(device, &descriptor_pool_info, NULL, descriptor_pool) != VK_SUCCESS) {
        LOG_ERROR("failed to create descriptor pool");
        goto fail;
    }

    /* fill descriptor types with invalid */
    for(u32 i = 0; i != GPU_DESCRIPTOR_SET_COUNT * GPU_MAX_BINDINGS_PER_DESCRIPTOR; i++) {
        descriptor_types[i] = VK_DESCRIPTOR_TYPE_MAX_ENUM;
    }

    /* create descriptor set layouts */
    for(u32 i = 0; i != GPU_DESCRIPTOR_SET_COUNT; i++) {

        /* generate bindings */
        VkDescriptorSetLayoutBinding bindings[GPU_MAX_BINDINGS_PER_DESCRIPTOR] = {0};
        const GpuDescriptorType*     bindings_types                            = descriptor_set_infos[i].bindings;
        u32                          bindings_count                            = descriptor_set_infos[i].bindings_count;
        
        if(bindings_count > GPU_MAX_BINDINGS_PER_DESCRIPTOR) {
            LOG_ERROR(
                "too many set id: %u/%u bindings: %u/%u", 
                i, GPU_DESCRIPTOR_SET_COUNT, bindings_count, GPU_MAX_BINDINGS_PER_DESCRIPTOR
            );
            goto fail;
        }

        /* if no descriptors on set create 1 "empty" */
        if(bindings_count == 0) {
            bindings_count = 1;
            bindings[0]    = (VkDescriptorSetLayoutBinding) {
                .binding         = 0,
                .stageFlags      = VK_SHADER_STAGE_ALL,
                .descriptorType  = GPU_EMPTY_DESCRIPTOR_TYPE,
                .descriptorCount = 1
            };
        } else {
            for(u32 j = 0; j != bindings_count; j++) {

                if(bindings_types[j] < GPU_DESCRIPTOR_TYPE_COUNT) {
                    /* fill binding */
                    const VkDescriptorType type = descriptor_type_conversion_table[bindings_types[j]];

                    bindings[j] = (VkDescriptorSetLayoutBinding) {
                        .binding         = j,
                        .stageFlags      = VK_SHADER_STAGE_ALL,
                        .descriptorType  = type,
                        .descriptorCount = 1
                    };

                    descriptor_types[j + i * GPU_MAX_BINDINGS_PER_DESCRIPTOR] = type;
                } 
                else {
                    LOG_ERROR(
                        "invalid descriptor type set id: %u/%u descriptor id: %u/%u",
                        i, GPU_DESCRIPTOR_SET_COUNT, j, bindings_count
                    );
                    goto fail;
                }
            }
        }
        
        /* create descriptor layout */
        const VkDescriptorSetLayoutCreateInfo set_layout_info = {
            .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pBindings    = bindings,
            .bindingCount = bindings_count
        };

        if(vkCreateDescriptorSetLayout(device, &set_layout_info, NULL, &descriptor_set_layouts[i]) != VK_SUCCESS) {
            LOG_ERROR("failed to create descriptor set layout id: %u/%u", i, GPU_DESCRIPTOR_SET_COUNT);
            goto fail;
        }
    }

    /* allocate descriptor sets */
    const VkDescriptorSetAllocateInfo descriptor_sets_info = {
        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool     = *descriptor_pool,
        .descriptorSetCount = GPU_DESCRIPTOR_SET_COUNT,
        .pSetLayouts        = descriptor_set_layouts 
    };

    if(vkAllocateDescriptorSets(device, &descriptor_sets_info, descriptor_sets) != VK_SUCCESS) {
        LOG_ERROR("failed to allocate descriptor sets");
        goto fail;
    }

    /* create pipeline layout */
    const VkPushConstantRange push_constant_range = {
        .offset     = 0,
        .size       = GPU_PUSH_CONSTANTS_SIZE,
        .stageFlags = VK_SHADER_STAGE_ALL
    };
    const VkPipelineLayoutCreateInfo pipeline_layout_info = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pPushConstantRanges    = &push_constant_range,
        .pushConstantRangeCount = 1,
        .pSetLayouts            = descriptor_set_layouts,
        .setLayoutCount         = GPU_DESCRIPTOR_SET_COUNT
    };

    if(vkCreatePipelineLayout(device, &pipeline_layout_info, NULL, pipeline_layout) != VK_SUCCESS) {
        LOG_ERROR("failed to create pipeline layout");
        goto fail;
    }

    return TRUE;

    fail: {
        return FALSE;
    }
}

/* */

b32 gpu_compile_shaders(
    CtxHandle          ctx, 
    const ShadersInfo* shaders_info
) {
    if(ctx == NULL || shaders_info == NULL) {
        LOG_ERROR("input params are NULL");
        goto fail;
    }

    if(shaders_info->pipeline_infos_count == 0) {
        LOG_ERROR("compiling zero shaders");
        goto fail;
    }

    GpuContext*          gpu_ctx        = (GpuContext*)ctx;
    const VulkanDevice*  vulkan_device  = &gpu_ctx->vulkan_device;
    VulkanShaders*       vulkan_shaders = &gpu_ctx->vulkan_shaders;

    /* commit resource memory */
    void*       commit_base = gpu_ctx->resources_base + GPU_VIRTUAL_SHADERS_BASE;
    const u64   commit_size = ALIGN(shaders_info->pipeline_infos_count * sizeof(VkPipeline), 0x1000);

    if(commit_size > GPU_VIRTUAL_SHADERS_LIMIT - GPU_VIRTUAL_SHADERS_BASE) {
        LOG_ERROR("exceed shaders virtual space");
        goto fail;
    }
    if(VirtualAlloc(commit_base, commit_size, MEM_COMMIT, PAGE_READWRITE) == NULL) {
        LOG_ERROR("failed to commit shaders memory");
    }

    vulkan_shaders->pipelines       = commit_base;
    vulkan_shaders->pipelines_count = shaders_info->pipeline_infos_count;

    /* create descriptors and pipeline layout */
    if(!create_descriptors(
        vulkan_device->device,
        shaders_info->descriptor_set_infos,
        &vulkan_shaders->descriptor_pool,
        vulkan_shaders->descriptor_sets,
        vulkan_shaders->descriptor_layouts,
        vulkan_shaders->descriptor_types,
        &vulkan_shaders->pipeline_layout
    )) {
        LOG_ERROR("failed to create descriptors");
        goto fail;
    }

    /* create pipelines */
    if(!create_pipelines(
        vulkan_device->device,
        vulkan_device->adapter->surface_format,
        vulkan_shaders->pipeline_layout,
        shaders_info->pipeline_infos,
        shaders_info->pipeline_infos_count,
        vulkan_shaders->pipelines
    )) {
        LOG_ERROR("failed to create pipelines");
        goto fail;
    }

    return TRUE;

    fail: {
        return FALSE;
    }
}

void gpu_release_shaders(
    CtxHandle ctx
) {
    if(ctx == NULL) {
        LOG_ERROR("input params are NULL");
        goto fail;
    }

    GpuContext*    gpu_ctx        = (GpuContext*)ctx;
    VulkanShaders* vulkan_shaders = &gpu_ctx->vulkan_shaders;
    const VkDevice device         = gpu_ctx->vulkan_device.device;

    /* pipelines */
    const VkPipeline* pipelines       = vulkan_shaders->pipelines;
    const u32         pipelines_count = vulkan_shaders->pipelines_count;
    for(u32 i = 0; i != pipelines_count; i++) {
        vkDestroyPipeline(device, pipelines[i], NULL);
    }

    /* pipeline layout */
    vkDestroyPipelineLayout(device, vulkan_shaders->pipeline_layout, NULL);

    /* descriptors */
    const VkDescriptorSetLayout* descriptor_set_layouts = vulkan_shaders->descriptor_layouts;

    vkDestroyDescriptorPool(device, vulkan_shaders->descriptor_pool, NULL);
    
    for(u32 i = 0; i != GPU_DESCRIPTOR_SET_COUNT; i++) {
        vkDestroyDescriptorSetLayout(device, descriptor_set_layouts[i], NULL);    
    }

    *vulkan_shaders = (VulkanShaders){0};

    /* FIX: may be decommit memory? */

    fail: {}
}

b32 gpu_write_bindings(
    CtxHandle          ctx, 
    const BindingInfo* binding_infos, 
    u32                binding_infos_count
) {
    GpuContext*            gpu_ctx        = (GpuContext*)ctx;
    const VulkanDevice*    vulkan_device = &gpu_ctx->vulkan_device;
    const VulkanResources* vulkan_resources = &gpu_ctx->vulkan_resources;
    VulkanShaders*         vulkan_shaders = &gpu_ctx->vulkan_shaders;

    /* */
    const VkDescriptorSet*  descriptor_sets        = vulkan_shaders->descriptor_sets;
    const VkDescriptorType* descriptor_types       = vulkan_shaders->descriptor_types;
    const GpuBuffer*        resource_buffers       = vulkan_resources->buffers;
    const GpuImage*         resource_images        = vulkan_resources->images;
    const u32               resource_buffers_count = vulkan_resources->buffers_count;
    const u32               resource_images_count  = vulkan_resources->images_count;

    /* fill descriptor write infos */
    VkWriteDescriptorSet   descriptor_writes[GPU_DESCRIPTOR_SET_COUNT * GPU_MAX_BINDINGS_PER_DESCRIPTOR] = {0};
    VkDescriptorBufferInfo buffer_infos     [GPU_DESCRIPTOR_SET_COUNT * GPU_MAX_BINDINGS_PER_DESCRIPTOR] = {0};
    VkDescriptorImageInfo  image_infos      [GPU_DESCRIPTOR_SET_COUNT * GPU_MAX_BINDINGS_PER_DESCRIPTOR] = {0};

    for(u32 i = 0; i != binding_infos_count; i++) {
        const u32 set_id      = binding_infos[i].set_id;
        const u32 binding_id  = binding_infos[i].binding_id;
        const u32 resource_id = binding_infos[i].resource_id;

        if(set_id > GPU_DESCRIPTOR_SET_COUNT) {
            LOG_ERROR("invalid set id: %u/%u", i, GPU_DESCRIPTOR_SET_COUNT);
            goto fail;
        }
        if(binding_id > GPU_MAX_BINDINGS_PER_DESCRIPTOR) {
            LOG_ERROR("invalid binding id: %u/%u", i, GPU_MAX_BINDINGS_PER_DESCRIPTOR);
            goto fail;
        }

        const VkDescriptorType binding_type = descriptor_types[binding_id + set_id * GPU_MAX_BINDINGS_PER_DESCRIPTOR];
        
        /* buffer */
        if(
            binding_type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
            binding_type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER 
        ) {
            if(resource_id >= resource_buffers_count) {
                LOG_ERROR("invalid buffer id: %u/%u", resource_id, resource_buffers_count);
                goto fail;
            }

            /* write infos */
            buffer_infos[i] = (VkDescriptorBufferInfo) {
                .buffer = resource_buffers[resource_id].buffer,
                .offset = 0,
                .range  = VK_WHOLE_SIZE
            };
            descriptor_writes[i] = (VkWriteDescriptorSet) {
                .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet          = descriptor_sets[set_id],
                .dstBinding      = binding_id,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType  = binding_type,
                .pBufferInfo     = &buffer_infos[i]
            };
        }
        /* image */
        else if(
            binding_type == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ||
            binding_type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
        ) {
            if(resource_id >= resource_images_count) {
                LOG_ERROR("invalid image id: %u/%u", resource_id, resource_images_count);
                goto fail;
            }

            /* image layout */
            VkImageLayout image_layout = VK_IMAGE_LAYOUT_UNDEFINED;

            if(binding_type == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE) {
                image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            }
            if(binding_type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE) {
                image_layout = VK_IMAGE_LAYOUT_GENERAL;
            }

            /* write infos */
            image_infos[i] = (VkDescriptorImageInfo) {
                .imageLayout = image_layout,
                .imageView   = resource_images[resource_id].view
            };
            descriptor_writes[i] = (VkWriteDescriptorSet) {
                .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet          = descriptor_sets[set_id],
                .dstBinding      = binding_id,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType  = binding_type,
                .pImageInfo      = &image_infos[i]
            };
        }
        /* sampler */
        else if(
            binding_type == VK_DESCRIPTOR_TYPE_SAMPLER
        ) {
            /* find sampler */
            VkSampler sampler = NULL;

            switch (resource_id) {
                case GPU_SAMPLER_LINEAR_REPEAT_ID:
                    sampler = vulkan_resources->sampler_linear_repeat;
                break;
                case GPU_SAMPLER_LINEAR_CLAMP_ID:
                    sampler = vulkan_resources->sampler_linear_clamp;
                break;
                case GPU_SAMPLER_NEAREST_REPEAT_ID:
                    sampler = vulkan_resources->sampler_nearest_repeat;
                break;
                case GPU_SAMPLER_NEAREST_CLAMP_ID:
                    sampler = vulkan_resources->sampler_nearest_clamp;
                break;
                default:
                    LOG_ERROR("invalid sampler id: %u", resource_id);
                goto fail;
            }

            /* write infos */
            image_infos[i] = (VkDescriptorImageInfo) {
                .sampler = sampler
            };
            descriptor_writes[i] = (VkWriteDescriptorSet) {
                .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet          = descriptor_sets[set_id],
                .dstBinding      = binding_id,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType  = binding_type,
                .pImageInfo      = &image_infos[i]
            };
        }
        else {
            LOG_ERROR("invalid binding write set id: %u binding id: %u", binding_infos[i].set_id, binding_infos[i].binding_id);
            goto fail;
        }
    }

    vkUpdateDescriptorSets(vulkan_device->device, binding_infos_count, descriptor_writes, 0, NULL);

    return TRUE;

    fail: {
        return FALSE;
    }
}
