#include "vk.h"

#define MAX_SWAPCHAIN_IMAGE_COUNT 8
#define MAX_DESCRIPTOR_SET_COUNT 16
#define MAX_BINDING_PER_DESCRIPTOR_COUNT 32

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
    RenderDraw_pfn draw_callback;
} RenderNodeInternal;

typedef struct {
    RenderBindingType type;
    u32 binding;
    u32 set;
    void* host_resource;
    void* device_resource;
    u64 host_offset; /* offset in vram allocation */
    u64 device_offset; /* offset in vram allocation */
    RenderMemoryWrite_pfn init_batch;
    RenderMemoryWrite_pfn frame_batch;
    u64 size;
} RenderBindingInternal;

typedef struct {
    VkDescriptorPool descriptor_pool;
    u32 descriptor_set_count;
    u32 render_binding_count;
    /* special layouts */
    VkDescriptorSetLayout empty_set_layout;
    VkPipelineLayout empty_pipeline_layout;
    VkPipelineLayout full_pipeline_layout;
    /* vram allocations */
    VkDeviceMemory host_memory_allocation;
    VkDeviceMemory device_memory_allocation;
    u64 host_memory_size;
    u64 device_memory_size;
    /* descriptor sets */
    VkDescriptorSet descriptor_sets[MAX_DESCRIPTOR_SET_COUNT];
    VkDescriptorSetLayout descriptor_layouts[MAX_DESCRIPTOR_SET_COUNT];
    RenderBindingInternal render_bindings[MAX_DESCRIPTOR_SET_COUNT * MAX_BINDING_PER_DESCRIPTOR_COUNT];
} RenderBindingContext;

typedef struct {
    RenderNodeInternal* render_nodes; /* allocated on heap */
    VkImageMemoryBarrier* image_barriers;
    u32 render_nodes_count;
} RenderPipelineContext;

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
    RenderBindingContext* binding_context;
    RenderPipelineContext* pipeline_context;
    /* callbacks */
    RenderStart_pfn start_callback;
    RenderUpdate_pfn update_callback;
} RenderContext;


/* remove shaders and compile directly to Pipeline node */
VkShaderModule createShaderModule(VkDevice device, const char* shader_path, ByteBuffer* read_buffer) {
    VkShaderModule shader_module = NULL;
    /* open file */
    FILE* file = fopen(shader_path, "rb");
    if(!file) return NULL;
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

/* modiffies pipeline nodes */
b32 createGraphicsPipeline(VkDevice device, VkFormat color_format, VkFormat depth_format, RenderNodeInternal* node) {
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

i32 createPipelineContext(const VulkanContext* vulkan_context, const RenderSettings* settings, const RenderContext* render_context, MsgCallback_pfn msg_callback, RenderPipelineContext* pipeline_context) {
    *pipeline_context = (RenderPipelineContext){0};

    if(!settings || settings->node_count == 0) {
        return MSG_CODE_SUCCESS;
    }

    /* VALIDATION */ {
        const u32 node_count = settings->node_count;
        for(u32 i = 0; i < node_count; i++) {
            const RenderNode* node = &settings->nodes[i];
            if(!node->draw_callback) {
                MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_RENDER_NODE_INVALID, "graphics render null draw callback");
            }
            /* validate types and shaders */
            if(node->type == RENDER_NODE_TYPE_GRAPHICS) {
                if(!node->vertex_shader || !node->fragment_shader || node->compute_shader) {
                    MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_RENDER_NODE_INVALID, "graphics render node invalid shaders");
                }
                continue;
            }
            if(node->type == RENDER_NODE_TYPE_COMPUTE) {
                if(node->vertex_shader || node->fragment_shader || !node->compute_shader) {
                    MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_RENDER_NODE_INVALID, "compute render node invalid shaders");
                }
                continue;
            }
            MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_RENDER_NODE_INVALID, "invalid render node type");
        }
    }
    
    /* NODES CREATION */ {
        /* byte buffer for reading shaders */
        ByteBuffer read_buffer = {
            .buffer = malloc(4096 * 4),
            .size = 4096 * 4
        };
        if(!read_buffer.buffer) {
            MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_BUFFER_MALLOC_FAIL, "failed to allocate shader read buffer");
        }
        /* allocate pipeline nodes buffer */
        if(!(pipeline_context->render_nodes = malloc(sizeof(RenderNodeInternal) * settings->node_count))) {
            MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_BUFFER_MALLOC_FAIL, "failed to allocate pipeline nodes buffer");
        }

        /* create internal render nodes */
        const u32 node_count = pipeline_context->render_nodes_count = settings->node_count;
        for(u32 i = 0; i < node_count; i++) {
            const RenderNode* node = &settings->nodes[i];
            RenderNodeInternal* internal_node = &pipeline_context->render_nodes[i];
            
            if(node->type == RENDER_NODE_TYPE_GRAPHICS) {
                *internal_node = (RenderNodeInternal) {
                    .type = RENDER_NODE_TYPE_GRAPHICS,
                    .pipeline_layout = render_context->binding_context->full_pipeline_layout ? render_context->binding_context->full_pipeline_layout : render_context->binding_context->empty_pipeline_layout,
                    .draw_callback = node->draw_callback
                };
                if(!(internal_node->vertex_shader = createShaderModule(vulkan_context->device, node->vertex_shader, &read_buffer))) {
                    MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_SHADER_MODULE_CREATE, "failed to create vertex shader module");
                }
                if(!(internal_node->fragment_shader = createShaderModule(vulkan_context->device, node->fragment_shader, &read_buffer))) {
                    MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_SHADER_MODULE_CREATE, "failed to create fragment shader module");
                }

                /* @(FIX): different render attachments */
                if(!createGraphicsPipeline(vulkan_context->device, render_context->swapchain_params.surface_format.format, render_context->swapchain_params.depth_format, internal_node)) {
                    MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_PIPELINE_CREATE, "failed to create graphics pipeline");
                }
            }
        }

        free(read_buffer.buffer);
        read_buffer.buffer = NULL;
    }

    return MSG_CODE_SUCCESS;
}

void destroyPipelineContext(const VulkanContext* vulkan_context, RenderPipelineContext* pipeline_context) {
    const u32 render_node_count = pipeline_context->render_nodes_count;
    /* destroy pipelines and shader modules */
    for(u32 i = 0; i < render_node_count; i++) {
        if(pipeline_context->render_nodes[i].type == RENDER_NODE_TYPE_GRAPHICS) {
            vkDestroyPipeline(vulkan_context->device, pipeline_context->render_nodes[i].pipeline, NULL);
            vkDestroyShaderModule(vulkan_context->device, pipeline_context->render_nodes[i].vertex_shader, NULL);
            vkDestroyShaderModule(vulkan_context->device, pipeline_context->render_nodes[i].fragment_shader, NULL);
        }
        if(pipeline_context->render_nodes[i].type == RENDER_NODE_TYPE_COMPUTE) {
            vkDestroyPipeline(vulkan_context->device, pipeline_context->render_nodes[i].pipeline, NULL);
            vkDestroyShaderModule(vulkan_context->device, pipeline_context->render_nodes[i].compute_shader, NULL);
        }
    }
    free(pipeline_context->render_nodes);
    *pipeline_context = (RenderPipelineContext){0};
}


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

i32 createBindingContext(const VulkanContext* vulkan_context, const RenderSettings* settings, MsgCallback_pfn msg_callback, RenderBindingContext* binding_context) {
    *binding_context = (RenderBindingContext){0};

    /* get enough space in pool for descriptors */
    VkDescriptorPoolCreateInfo descriptor_pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = MAX_DESCRIPTOR_SET_COUNT,
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
    if(vkCreateDescriptorPool(vulkan_context->device, &descriptor_pool_info, NULL, &binding_context->descriptor_pool) != VK_SUCCESS) {
        MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_DESCRIPTOR_POOL_CREATE, "failed to create render descriptor pool");
    }

    /* CREATE EMPTY LAYOUT */ {
        const VkDescriptorSetLayoutCreateInfo layout_create_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pBindings = NULL,
            .bindingCount = 0
        };
        const VkPipelineLayoutCreateInfo pipeline_layout_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .pSetLayouts = &binding_context->empty_set_layout,
            .setLayoutCount = 1
        };
        if(vkCreateDescriptorSetLayout(vulkan_context->device, &layout_create_info, NULL, &binding_context->empty_set_layout) != VK_SUCCESS) {
            MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_CREATE_DESCRIPTOR_SET_LAYOUT, "failed to create empty descriptor set layout");
        }
        if(vkCreatePipelineLayout(vulkan_context->device, &pipeline_layout_info, NULL, &binding_context->empty_pipeline_layout) != VK_SUCCESS) {
            MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_PIPELINE_LAYOUT_CREATE, "failed to create empty pipeline layout");
        };
    }
   
    if(!settings || settings->binding_count == 0) {
        return MSG_CODE_SUCCESS;
    }

    /* CREATE DESCRIPTOR SETS & LAYOUTS */ {
        VkDescriptorSetLayoutBinding layout_bindings[MAX_DESCRIPTOR_SET_COUNT][MAX_BINDING_PER_DESCRIPTOR_COUNT] = {0};
        VkDescriptorSetLayoutCreateInfo layout_infos[MAX_DESCRIPTOR_SET_COUNT] = {0};
        u32 binding_counts[MAX_DESCRIPTOR_SET_COUNT] = {0};

        /* validate bindings and fill layout bindings */
        const u32 binding_count = settings->binding_count;
        for(u32 i = 0; i < binding_count; i++) {
            const RenderBinding* render_binding = &settings->bindings[i];
            /* validate if it is unique */
            for(u32 j = 0; j < i; j++) {
                if(render_binding->binding == settings->bindings[j].binding && render_binding->set == settings->bindings[j].set) {
                    MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_RENDER_BINDING_INVALID, "render binding validation failed, two bindings exists with same location");
                }
            }
            /* validate type */
            if(
                render_binding->type != RENDER_BINDING_TYPE_UNIFORM_BUFFER &&
                render_binding->type != RENDER_BINDING_TYPE_STORAGE_BUFFER
            ) {
                MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_RENDER_BINDING_INVALID, "render binding validation failed, two bindings exists with same location");
            }

            /* configure binding descriptor for descriptor set */
            layout_bindings[render_binding->set][binding_counts[render_binding->set]] = (VkDescriptorSetLayoutBinding) {
                .binding = render_binding->binding,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_ALL
            };
            switch (render_binding->type) {
                case RENDER_BINDING_TYPE_UNIFORM_BUFFER:
                    layout_bindings[render_binding->set][binding_counts[render_binding->set]].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                    break;
                case RENDER_BINDING_TYPE_STORAGE_BUFFER:
                    layout_bindings[render_binding->set][binding_counts[render_binding->set]].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                default:
                    break;
            }
            binding_counts[render_binding->set]++;
        }

        /* get descriptor set count */
        for(u32 i = 0; i < MAX_DESCRIPTOR_SET_COUNT; i++) {
            if(binding_counts[i] != 0 && binding_context->descriptor_set_count != i) {
                MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_BAD_BINDING_LAYOUT, "some descriptor sets are missing and have no bindings");
            }
            binding_context->descriptor_set_count = (binding_counts[i] != 0) ? i + 1 : binding_context->descriptor_set_count;
        }

        const u32 descriptor_set_count = binding_context->descriptor_set_count;
        for(u32 i = 0; i < descriptor_set_count; i++) {
            /* create descriptor set layout */
            layout_infos[i] = (VkDescriptorSetLayoutCreateInfo) {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                .pBindings = layout_bindings[i],
                .bindingCount = binding_counts[i]
            };
            if(vkCreateDescriptorSetLayout(vulkan_context->device, &layout_infos[i], NULL, &binding_context->descriptor_layouts[i]) != VK_SUCCESS) {
                MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_CREATE_DESCRIPTOR_SET_LAYOUT, "failed to create vulkan descriptor set layout");
            }
        }

        /* create all descriptor sets */
        const VkDescriptorSetAllocateInfo descriptor_set_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = binding_context->descriptor_pool,
            .descriptorSetCount = binding_context->descriptor_set_count,
            .pSetLayouts = binding_context->descriptor_layouts
        };
        if(vkAllocateDescriptorSets(vulkan_context->device, &descriptor_set_info, binding_context->descriptor_sets) != VK_SUCCESS) {
            MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_CREATE_DESCRIPTOR_SET, "failed to create descriptor set");
        }

        VkPipelineLayoutCreateInfo pipeline_layout_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .pSetLayouts = binding_context->descriptor_layouts,
            .setLayoutCount = binding_context->descriptor_set_count
        };
        if(vkCreatePipelineLayout(vulkan_context->device, &pipeline_layout_info, NULL, &binding_context->full_pipeline_layout) != VK_SUCCESS) {
            MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_PIPELINE_LAYOUT_CREATE, "failed to create full pipeline layout");
        }
    }

    /* in this variables allocation information will be accumulated */
    VkMemoryRequirements device_memory_requirements = (VkMemoryRequirements){.memoryTypeBits = U32_MAX};
    VkMemoryRequirements host_memory_requirements = (VkMemoryRequirements){.memoryTypeBits = U32_MAX};

    /* CREATE BINDINGS */ {
        const u32 binding_count = binding_context->render_binding_count = settings->binding_count;
        for(u32 i = 0; i < binding_count; i++) {
            const RenderBinding* setting_binding = &settings->bindings[i];
            RenderBindingInternal* internal_binding = &binding_context->render_bindings[i];
            /* if uniform buffer */
            if(setting_binding->type == RENDER_BINDING_TYPE_UNIFORM_BUFFER) {
                *internal_binding = (RenderBindingInternal) {
                    .type = RENDER_BINDING_TYPE_UNIFORM_BUFFER,
                    .binding = setting_binding->binding,
                    .set = setting_binding->set,
                    .init_batch = setting_binding->initial_batch,
                    .frame_batch = setting_binding->frame_batch,
                    .size = setting_binding->size
                };
                if(vulkan_context->device_type == DEVICE_TYPE_DESCRETE) {
                    /* check for host side mutability */
                    if(setting_binding->initial_batch || setting_binding->frame_batch) {
                        /* create device buffer */
                        internal_binding->device_resource = createBuffer(vulkan_context->device, setting_binding->size, vulkan_context->render_family_id, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
                        if(!internal_binding->device_resource) {
                            MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_BUFFER_CREATE, "failed to create uniform device buffer");
                        }
                        /* create host buffer */
                        internal_binding->host_resource = createBuffer(vulkan_context->device, setting_binding->size, vulkan_context->render_family_id, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
                        if(!internal_binding->host_resource) { 
                            MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_BUFFER_CREATE, "failed to create uniform host buffer");
                        }
                    } else {
                        /* create only device buffer */
                        internal_binding->device_resource = createBuffer(vulkan_context->device, setting_binding->size, vulkan_context->render_family_id, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
                        if(!internal_binding->device_resource) {
                            MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_BUFFER_CREATE, "failed to create uniform device buffer");
                        }
                    }
                }
                if(vulkan_context->device_type == DEVICE_TYPE_INTEGRATED) {
                    /* create only device buffer */
                    internal_binding->device_resource = createBuffer(vulkan_context->device, setting_binding->size, vulkan_context->render_family_id, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
                    if(!internal_binding->device_resource) {
                        MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_BUFFER_CREATE, "failed to create uniform host buffer");
                    }
                }
            }
            /* if storage buffer */
            if(setting_binding->type == RENDER_BINDING_TYPE_STORAGE_BUFFER) {
                *internal_binding = (RenderBindingInternal) {
                    .type = RENDER_BINDING_TYPE_STORAGE_BUFFER,
                    .binding = setting_binding->binding,
                    .set = setting_binding->set,
                    .init_batch = setting_binding->initial_batch,
                    .frame_batch = setting_binding->frame_batch,
                    .size = setting_binding->size
                };
                if(vulkan_context->device_type == DEVICE_TYPE_DESCRETE) {
                    /* check for host side mutability */
                    if(setting_binding->initial_batch || setting_binding->frame_batch) {
                        /* create device buffer */
                        internal_binding->device_resource = createBuffer(vulkan_context->device, setting_binding->size, vulkan_context->render_family_id, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
                        if(!internal_binding->device_resource) {
                            MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_BUFFER_CREATE, "failed to create uniform device buffer");
                        }
                        /* create host buffer */
                        internal_binding->host_resource = createBuffer(vulkan_context->device, setting_binding->size, vulkan_context->render_family_id, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
                        if(!internal_binding->host_resource) { 
                            MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_BUFFER_CREATE, "failed to create uniform host buffer");
                        }
                    } else {
                        /* create only device buffer */
                        internal_binding->device_resource = createBuffer(vulkan_context->device, setting_binding->size, vulkan_context->render_family_id, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
                        if(!internal_binding->device_resource) {
                            MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_BUFFER_CREATE, "failed to create uniform device buffer");
                        }
                    }
                }
                if(vulkan_context->device_type == DEVICE_TYPE_INTEGRATED) {
                    /* create only device buffer */
                    internal_binding->device_resource = createBuffer(vulkan_context->device, setting_binding->size, vulkan_context->render_family_id, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
                    if(!internal_binding->device_resource) {
                        MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_BUFFER_CREATE, "failed to create uniform host buffer");
                    }
                }
            }


            /* get memory requirements for buffers */
            if(internal_binding->device_resource) {
                /* get memory requirements */
                VkMemoryRequirements requirements = (VkMemoryRequirements){0};
                /* different functions for different types */
                if(setting_binding->type == RENDER_BINDING_TYPE_UNIFORM_BUFFER || setting_binding->type == RENDER_BINDING_TYPE_STORAGE_BUFFER) {
                    vkGetBufferMemoryRequirements(vulkan_context->device, (VkBuffer)internal_binding->device_resource, &requirements);
                }
                /* set data to render binding */
                internal_binding->device_offset = ALIGN(device_memory_requirements.size, requirements.alignment);
                /* adjust allocation requirement */
                device_memory_requirements.size = internal_binding->device_offset + requirements.size;
                device_memory_requirements.alignment = MAX(device_memory_requirements.alignment, requirements.alignment);
                device_memory_requirements.memoryTypeBits &= device_memory_requirements.memoryTypeBits;
            }
            if(internal_binding->host_resource) {
                /* get memory requirements */
                VkMemoryRequirements requirements = (VkMemoryRequirements){0};
                /* different functions for different types */
                if(setting_binding->type == RENDER_BINDING_TYPE_UNIFORM_BUFFER || setting_binding->type == RENDER_BINDING_TYPE_STORAGE_BUFFER) {
                    vkGetBufferMemoryRequirements(vulkan_context->device, (VkBuffer)internal_binding->host_resource, &requirements);
                }
                /* set data to render binding */
                internal_binding->host_offset = ALIGN(host_memory_requirements.size, requirements.alignment);
                /* adjust allocation requirement */
                host_memory_requirements.size = internal_binding->device_offset + requirements.size;
                host_memory_requirements.alignment = MAX(host_memory_requirements.alignment, requirements.alignment);
                host_memory_requirements.memoryTypeBits &= host_memory_requirements.memoryTypeBits;
            }
        }
    }

    /* ALLOCATE ARENA */ {
        if(vulkan_context->device_type == DEVICE_TYPE_DESCRETE) {
            if(device_memory_requirements.size != 0) {
                binding_context->device_memory_allocation = vramArenaAllocate(&device_memory_requirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
            }
            if(host_memory_requirements.size != 0) {
                binding_context->host_memory_allocation = vramArenaAllocate(&host_memory_requirements, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            }
        }
        if(vulkan_context->device_type == DEVICE_TYPE_INTEGRATED) {
            /* device memory allocation usually is not used */
            if(device_memory_requirements.size != 0) {
                binding_context->device_memory_allocation = vramArenaAllocate(&device_memory_requirements, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, 0);
            }
            if(host_memory_requirements.size != 0) {
                binding_context->host_memory_allocation = vramArenaAllocate(&host_memory_requirements, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, 0);
            }
        }

        binding_context->device_memory_size = device_memory_requirements.size;
        binding_context->host_memory_size = host_memory_requirements.size;
    } 

    /* BIND MEMORY */ {
        const u32 render_binding_count = binding_context->render_binding_count;
        for(u32 i = 0; i < render_binding_count; i++) {
            const RenderBindingInternal* render_binding = &binding_context->render_bindings[i];
            /* bind buffer memory */
            if(render_binding->type == RENDER_BINDING_TYPE_UNIFORM_BUFFER || render_binding->type == RENDER_BINDING_TYPE_STORAGE_BUFFER) {
                if(render_binding->device_resource) {
                    if(vkBindBufferMemory(vulkan_context->device, (VkBuffer)render_binding->device_resource, binding_context->device_memory_allocation, render_binding->device_offset) != VK_SUCCESS) {
                        MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_BIND_RESOURCE_MEMORY, "failed to bind device buffer memory");
                    }
                }
                if(render_binding->host_resource) {
                    if(vkBindBufferMemory(vulkan_context->device, (VkBuffer)render_binding->host_resource, binding_context->host_memory_allocation, render_binding->host_offset) != VK_SUCCESS) {
                        MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_BIND_RESOURCE_MEMORY, "failed to bind host buffer memory");
                    }
                }
            }
        }
    }

    return MSG_CODE_SUCCESS;
}

void destroyBindingContext(const VulkanContext* vulkan_context, RenderBindingContext* binding_context) {
    /* destroy resources */
    for(u32 i = 0; i < binding_context->render_binding_count; i++) {
        RenderBindingInternal* render_binding = &binding_context->render_bindings[i];
        if(render_binding->type == RENDER_BINDING_TYPE_UNIFORM_BUFFER || render_binding->type == RENDER_BINDING_TYPE_STORAGE_BUFFER) {
            if(render_binding->device_resource) {
                vkDestroyBuffer(vulkan_context->device, (VkBuffer)render_binding->device_resource, NULL);
            }
            if(render_binding->host_resource) {
                vkDestroyBuffer(vulkan_context->device, (VkBuffer)render_binding->host_resource, NULL);
            }
        }
    }
    /* descriptor sets */
    for(u32 i = 0; i < binding_context->descriptor_set_count; i++) {
        vkDestroyDescriptorSetLayout(vulkan_context->device, binding_context->descriptor_layouts[i], NULL);
    }
    /* destroy full layout */
    if(binding_context->full_pipeline_layout) {
        vkDestroyPipelineLayout(vulkan_context->device, binding_context->full_pipeline_layout, NULL);
    }
    /* destroy empty layouts */
    vkDestroyPipelineLayout(vulkan_context->device, binding_context->empty_pipeline_layout, NULL);
    vkDestroyDescriptorSetLayout(vulkan_context->device, binding_context->empty_set_layout, NULL);
    /* destroy descriptor pool */
    vkDestroyDescriptorPool(vulkan_context->device, binding_context->descriptor_pool, NULL);

    /* invalidate context */
    *binding_context = (RenderBindingContext){0};
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
        if(format_count == 0) {
            return FALSE;
        }
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count, surface_formats);

        /* find best surface format */
        swapchain_params->surface_format = surface_formats[0];
        for(u32 i = 0; i < format_count; i++) {
            if(surface_formats[i].format == VK_FORMAT_B8G8R8A8_SRGB && surface_formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                swapchain_params->surface_format = (VkSurfaceFormatKHR) {
                    VK_FORMAT_B8G8R8A8_SRGB,
                    VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
                };
                break;
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
i32 renderCreateSwapchain(const VulkanContext* vulkan_context, MsgCallback_pfn msg_callback, RenderContext* render_context) {

    /* COLOR */ {
        /* create swapchain based on selected params*/
        const VkSwapchainCreateInfoKHR swapchain_info = {
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
i32 renderOnWindowResize(const VulkanContext* vulkan_context, MsgCallback_pfn msg_callback, RenderContext* render_context) {
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
    do {
        glfwPollEvents();
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vulkan_context->physical_device, vulkan_context->surface, &surface_capabilities);
    } while (surface_capabilities.currentExtent.width == 0 && surface_capabilities.currentExtent.height == 0);

    /* get glfw size to clamp if surface resolution is invalid */
    i32 width, height;
    glfwGetFramebufferSize(vulkan_context->window, &width, &height);

    render_context->swapchain_params.extent = (VkExtent2D) {
        MIN((u32)width, surface_capabilities.currentExtent.width),
        MIN((u32)height, surface_capabilities.currentExtent.height)
    };
    if(MSG_IS_ERROR(renderCreateSwapchain(vulkan_context, msg_callback, render_context))) {
        MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_SWAPCHAIN_RECREATE, "failed to recreate swapchain");
    }

    return MSG_CODE_SUCCESS;
}


i32 renderLoop(const VulkanContext* vulkan_context, MsgCallback_pfn msg_callback, RenderContext* render_context) {
    #define COMMAND_BUFFER_RENDER_ID 0
    #define COMMAND_BUFFER_TRANSFER_ID 1
    #define COMMAND_BUFFER_COUNT 2

    #define FENCE_FRAME_ID 0
    #define FENCE_TRANSFER_ID 1
    #define MAX_FENCE_COUNT 2

    #define FENCE_COUNT_INTEGRATED 1
    #define FENCE_COUNT_DESCRETE 2

    typedef struct {
        VkWriteDescriptorSet descriptor_set_writes[MAX_DESCRIPTOR_SET_COUNT * MAX_BINDING_PER_DESCRIPTOR_COUNT];
        VkDescriptorBufferInfo descriptor_buffer_infos[MAX_DESCRIPTOR_SET_COUNT * MAX_BINDING_PER_DESCRIPTOR_COUNT];
        u32 descriptor_set_write_count;
    } RenderLoopInitBuffer;

    typedef struct {
        VkSemaphore image_submit_semaphores[MAX_SWAPCHAIN_IMAGE_COUNT];
        VkSemaphore image_available_semaphore;
        VkFence fences[MAX_FENCE_COUNT];
        u32 fence_count;
        VkImageMemoryBarrier image_top_barrier;
        VkImageMemoryBarrier image_bottom_barrier;
    } SyncObjects;

    
    const RenderBindingContext* binding_context = render_context->binding_context;
    SyncObjects sync_objects = (SyncObjects){0};
    VkCommandBuffer command_buffers[COMMAND_BUFFER_COUNT] = {0};
    void* memory_map = NULL;
    b32 should_resize = FALSE;

    /* COMMAND BUFFERS */ {
        VkCommandBufferAllocateInfo cmbuffers_info = (VkCommandBufferAllocateInfo) {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandBufferCount = 2,
            .commandPool = render_context->command_pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY
        };
        if(vkAllocateCommandBuffers(vulkan_context->device, &cmbuffers_info, command_buffers) != VK_SUCCESS) {
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

        /* fence for gpu cpu sync */
        if(vulkan_context->device_type == DEVICE_TYPE_DESCRETE) {
            for(u32 i = 0; i < FENCE_COUNT_DESCRETE; i++) {
                if(vkCreateFence(vulkan_context->device, &fence_info, NULL, &sync_objects.fences[i]) != VK_SUCCESS) {
                    MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_FENCE_CREATE, "failed to create fence");
                }
            }
            sync_objects.fence_count = FENCE_COUNT_DESCRETE;
        }
        if(vulkan_context->device_type == DEVICE_TYPE_INTEGRATED) {
            for(u32 i = 0; i < FENCE_COUNT_INTEGRATED; i++) {
                if(vkCreateFence(vulkan_context->device, &fence_info, NULL, &sync_objects.fences[i]) != VK_SUCCESS) {
                    MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_FENCE_CREATE, "failed to create fence");
                }
            }
            sync_objects.fence_count = FENCE_COUNT_INTEGRATED;
        }
    }

    RenderLoopInitBuffer* init_buffer = malloc(sizeof(RenderLoopInitBuffer));
    if(!init_buffer) {
        MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_BUFFER_MALLOC_FAIL, "failed to allocate init buffer in render loop");
    }
    *init_buffer = (RenderLoopInitBuffer){0};

    /* WRITE_DESCRIPTORS */ {
        const RenderBindingInternal* bindings = render_context->binding_context->render_bindings;
        const u32 binding_count = render_context->binding_context->render_binding_count;
        for(u32 i = 0; i < binding_count; i++) {
            const RenderBindingInternal* binding = &bindings[i];
            VkWriteDescriptorSet* descriptor_set_write = &init_buffer->descriptor_set_writes[init_buffer->descriptor_set_write_count];

            *descriptor_set_write = (VkWriteDescriptorSet) {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstBinding = binding->binding,
                .dstSet = render_context->binding_context->descriptor_sets[binding->set],
                .descriptorCount = 1
            };
            /* if uniform buffer */
            if(binding->type == RENDER_BINDING_TYPE_UNIFORM_BUFFER) {
                init_buffer->descriptor_buffer_infos[i] = (VkDescriptorBufferInfo) {
                    .buffer = (VkBuffer)binding->device_resource,
                    .offset = 0,
                    .range = binding->size
                };
                descriptor_set_write->descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                descriptor_set_write->pBufferInfo = &init_buffer->descriptor_buffer_infos[i];
            }
            /* if storage buffer */
            if(binding->type == RENDER_BINDING_TYPE_STORAGE_BUFFER) {
                init_buffer->descriptor_buffer_infos[i] = (VkDescriptorBufferInfo) {
                    .buffer = (VkBuffer)binding->device_resource,
                    .offset = 0,
                    .range = binding->size
                };
                descriptor_set_write->descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                descriptor_set_write->pBufferInfo = &init_buffer->descriptor_buffer_infos[i];
            }
            
            init_buffer->descriptor_set_write_count++;
        }

        vkUpdateDescriptorSets(vulkan_context->device, render_context->binding_context->render_binding_count, init_buffer->descriptor_set_writes, 0, NULL);
    }

    free(init_buffer);

    if(render_context->start_callback) {
        const RenderUpdateContext update_context = {
            .screen_x = render_context->swapchain_params.extent.width,
            .screen_y = render_context->swapchain_params.extent.height
        };
        render_context->start_callback(&update_context);
    }

    /* INITIAL BATCH */ {
        const RenderBindingInternal* bindings = render_context->binding_context->render_bindings;
        const u32 binding_count = render_context->binding_context->render_binding_count;

        if(binding_count != 0) {
            if(vulkan_context->device_type == DEVICE_TYPE_DESCRETE) {
                if(vkMapMemory(vulkan_context->device, render_context->binding_context->host_memory_allocation, 0, render_context->binding_context->host_memory_size, 0, &memory_map) != VK_SUCCESS) {
                    MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_MAP_MEMORY, "failed to map host memory");
                }

                const VkCommandBufferBeginInfo command_buffer_begin_info = {
                    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
                };
                vkResetCommandBuffer(command_buffers[COMMAND_BUFFER_TRANSFER_ID], 0);
                vkBeginCommandBuffer(command_buffers[COMMAND_BUFFER_TRANSFER_ID], &command_buffer_begin_info);

                for(u32 i = 0; i < binding_count; i++) {
                    const RenderBindingInternal* binding = &bindings[i];
                    if(!binding->init_batch) {
                        continue;
                    }
                    binding->init_batch((u8*)memory_map + binding->host_offset);
                    if(binding->type == RENDER_BINDING_TYPE_UNIFORM_BUFFER || binding->type == RENDER_BINDING_TYPE_STORAGE_BUFFER) {
                        const VkBufferCopy buffer_copy = {
                            .srcOffset = 0,
                            .dstOffset = 0,
                            .size = binding->size
                        };
                        vkCmdCopyBuffer(command_buffers[COMMAND_BUFFER_TRANSFER_ID], (VkBuffer)binding->host_resource, (VkBuffer)binding->device_resource, 1, &buffer_copy);
                    }
                }
                vkEndCommandBuffer(command_buffers[COMMAND_BUFFER_TRANSFER_ID]);

                /* sunmit work for render queue */
                VkSubmitInfo submit_info = {
                    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                    .pWaitDstStageMask = (const VkPipelineStageFlags[]){VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT},
                    .commandBufferCount = 1,
                    .pCommandBuffers = &command_buffers[COMMAND_BUFFER_TRANSFER_ID],
                };
                vkResetFences(vulkan_context->device, 1, &sync_objects.fences[FENCE_TRANSFER_ID]);
                vkQueueSubmit(vulkan_context->render_queue, 1, &submit_info, sync_objects.fences[FENCE_TRANSFER_ID]);
            }
            if(vulkan_context->device_type == DEVICE_TYPE_INTEGRATED) {
                if(vkMapMemory(vulkan_context->device, render_context->binding_context->device_memory_allocation, 0, render_context->binding_context->device_memory_size, 0, &memory_map) != VK_SUCCESS) {
                    MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_MAP_MEMORY, "failed to map host memory");
                }

                for(u32 i = 0; i < binding_count; i++) {
                    const RenderBindingInternal* binding = &bindings[i];
                    if(binding->frame_batch) {
                        binding->frame_batch((u8*)memory_map + binding->device_offset);
                    }
                }
            }
        }
    }

    while(!glfwWindowShouldClose(vulkan_context->window)) {
        glfwPollEvents();

        u32 render_image_id = U32_MAX;

        /* update */
        if(render_context->update_callback) {
            const RenderUpdateContext update_context = {
                .screen_x = render_context->swapchain_params.extent.width,
                .screen_y = render_context->swapchain_params.extent.height
            };
            render_context->update_callback(&update_context);
        }

        /* FRAME BATCH */ {
            if(vulkan_context->device_type == DEVICE_TYPE_DESCRETE && vkGetFenceStatus(vulkan_context->device, sync_objects.fences[FENCE_TRANSFER_ID]) == VK_NOT_READY) goto _skip_frame_batch;

            const RenderBindingInternal* bindings = binding_context->render_bindings;
            const u32 binding_count = binding_context->render_binding_count;

            if(binding_count != 0) {
                /* descrete strategy */
                if(vulkan_context->device_type == DEVICE_TYPE_DESCRETE) {
                    const VkCommandBufferBeginInfo command_buffer_begin_info = {
                        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
                    };
                    vkResetCommandBuffer(command_buffers[COMMAND_BUFFER_TRANSFER_ID], 0);
                    vkBeginCommandBuffer(command_buffers[COMMAND_BUFFER_TRANSFER_ID], &command_buffer_begin_info);

                    for(u32 i = 0; i < binding_count; i++) {
                        const RenderBindingInternal* binding = &bindings[i];
                        if(!binding->frame_batch) {
                            continue;
                        }
                        binding->frame_batch((u8*)memory_map + binding->host_offset);
                        if(binding->type == RENDER_BINDING_TYPE_UNIFORM_BUFFER || binding->type == RENDER_BINDING_TYPE_STORAGE_BUFFER) {
                            const VkBufferCopy buffer_copy = {
                                .srcOffset = 0,
                                .dstOffset = 0,
                                .size = binding->size
                            };
                            vkCmdCopyBuffer(command_buffers[COMMAND_BUFFER_TRANSFER_ID], (VkBuffer)binding->host_resource, (VkBuffer)binding->device_resource, 1, &buffer_copy);
                        }
                    }
                    vkEndCommandBuffer(command_buffers[COMMAND_BUFFER_TRANSFER_ID]);

                    /* sunmit work for render queue */
                    VkSubmitInfo submit_info = {
                        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                        .pWaitDstStageMask = (const VkPipelineStageFlags[]){VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT},
                        .commandBufferCount = 1,
                        .pCommandBuffers = &command_buffers[COMMAND_BUFFER_TRANSFER_ID],
                    };
                    vkResetFences(vulkan_context->device, 1, &sync_objects.fences[FENCE_TRANSFER_ID]);
                    vkQueueSubmit(vulkan_context->render_queue, 1, &submit_info, sync_objects.fences[FENCE_TRANSFER_ID]);
                }
                if(vulkan_context->device_type == DEVICE_TYPE_INTEGRATED) {
                    for(u32 i = 0; i < binding_count; i++) {
                        const RenderBindingInternal* binding = &bindings[i];
                        if(binding->frame_batch) {
                            binding->frame_batch((u8*)memory_map + binding->device_offset);
                        }
                    }
                }
            }

            _skip_frame_batch: {}
        }

        _render_start: {}

        if(should_resize) {
            if(MSG_IS_ERROR(renderOnWindowResize(vulkan_context, msg_callback, render_context))) {
                MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_RESIZE_FAIL, "error occured during window resize in the beginning of loop");
            }
            should_resize = FALSE;
        }

        /* AQUIRE */ {
            /* wait untill previous frame finishes */
            vkWaitForFences(vulkan_context->device, sync_objects.fence_count, sync_objects.fences, VK_TRUE, U64_MAX);
            VkResult image_acquire_result = vkAcquireNextImageKHR(vulkan_context->device, render_context->swapchain, U64_MAX, sync_objects.image_available_semaphore, NULL, &render_image_id);

            /* check if frambuffer should resize */
            if(image_acquire_result == VK_ERROR_OUT_OF_DATE_KHR || image_acquire_result == VK_SUBOPTIMAL_KHR) {

                should_resize = TRUE;
                const VkSemaphoreSignalInfo unsignal_info = {
                    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO,
                    .semaphore = sync_objects.image_available_semaphore,
                    .value = 0
                };
                vkSignalSemaphore(vulkan_context->device, &unsignal_info);
                goto _render_start;
            }
            vkResetFences(vulkan_context->device, 1, &sync_objects.fences[FENCE_FRAME_ID]);
        }

        /* begin command buffer */
        const VkCommandBufferBeginInfo command_buffer_begin_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
        };
        vkResetCommandBuffer(command_buffers[COMMAND_BUFFER_RENDER_ID], 0);
        vkBeginCommandBuffer(command_buffers[COMMAND_BUFFER_RENDER_ID], &command_buffer_begin_info);

        /* NODE RENDERING */ {
            /* transit image to be used as attachment */
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
                .image = render_context->swapchain_images[render_image_id]
            };
            vkCmdPipelineBarrier(
                command_buffers[COMMAND_BUFFER_RENDER_ID], VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 
                0, 0, NULL, 0, NULL, 1, &image_top_barrier
            );

            if(binding_context->full_pipeline_layout) {
                vkCmdBindDescriptorSets(command_buffers[COMMAND_BUFFER_RENDER_ID], VK_PIPELINE_BIND_POINT_GRAPHICS, binding_context->full_pipeline_layout, 0, binding_context->descriptor_set_count, binding_context->descriptor_sets, 0, NULL);
            }

            /* screen attachments */
            const VkRenderingAttachmentInfoKHR screen_color_attachment = {
                .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
                .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .imageView = render_context->swapchain_image_views[render_image_id]
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
            vulkan_context->cmd_begin_rendering_khr(command_buffers[COMMAND_BUFFER_RENDER_ID], &rendering_info);

            /* set dynamic state */
            VkViewport viewport = {
                .width = render_context->swapchain_params.extent.width,
                .height = render_context->swapchain_params.extent.height,
                .minDepth = 0.0,
                .maxDepth = 1.0,
                .x = 0,
                .y = 0
            };
            vkCmdSetViewport(command_buffers[COMMAND_BUFFER_RENDER_ID], 0, 1, &viewport);
            vkCmdSetScissor(command_buffers[COMMAND_BUFFER_RENDER_ID], 0, 1, &rendering_info.renderArea);

            /* draw actual nodes */
            const u32 render_node_count = render_context->pipeline_context->render_nodes_count;
            const RenderNodeInternal* render_nodes = render_context->pipeline_context->render_nodes;
            for(u32 i = 0; i < render_node_count; i++) {
                const RenderNodeInternal* render_node = &render_nodes[i];
                if(render_node->type == RENDER_NODE_TYPE_GRAPHICS) {
                    RenderDrawInfo* draw_infos = NULL;
                    u32 draw_count = 0;
                    render_node->draw_callback(&draw_infos, &draw_count);

                    vkCmdBindPipeline(command_buffers[COMMAND_BUFFER_RENDER_ID], VK_PIPELINE_BIND_POINT_GRAPHICS, render_node->pipeline);
                    for(u32 j = 0; j < draw_count; j++) {
                        vkCmdDraw(command_buffers[COMMAND_BUFFER_RENDER_ID], draw_infos[j].vertex_count, draw_infos[j].instance_count, 0, 0);
                    }
                }
            }

            vulkan_context->cmd_end_rendering_khr(command_buffers[COMMAND_BUFFER_RENDER_ID]);
            /* transition to present */
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
                .image = render_context->swapchain_images[render_image_id]
            };
            vkCmdPipelineBarrier(
                command_buffers[COMMAND_BUFFER_RENDER_ID], VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 
                0, 0, NULL, 0, NULL, 1, &image_bottom_barrier
            );
        }

        vkEndCommandBuffer(command_buffers[COMMAND_BUFFER_RENDER_ID]);

        /* SUBMIT */ {
            /* sunmit work for render queue */
            VkSubmitInfo submit_info = {
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                .waitSemaphoreCount = 1,
                .pWaitSemaphores = &sync_objects.image_available_semaphore,
                .pWaitDstStageMask = (const VkPipelineStageFlags[]){VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT},
                .signalSemaphoreCount = 1,
                .pSignalSemaphores = &sync_objects.image_submit_semaphores[render_image_id],
                .commandBufferCount = 1,
                .pCommandBuffers = &command_buffers[COMMAND_BUFFER_RENDER_ID]
            };
            vkQueueSubmit(vulkan_context->render_queue, 1, &submit_info, sync_objects.fences[FENCE_FRAME_ID]);
            /* present image with render queue */
            VkPresentInfoKHR present_info = (VkPresentInfoKHR) {
                .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                .waitSemaphoreCount = 1,
                .pWaitSemaphores = &sync_objects.image_submit_semaphores[render_image_id],
                .swapchainCount = 1,
                .pSwapchains = &render_context->swapchain,
                .pImageIndices = &render_image_id,
                .pResults = NULL
            };
            VkResult present_result = vkQueuePresentKHR(vulkan_context->render_queue, &present_info);
            if(present_result == VK_ERROR_OUT_OF_DATE_KHR || present_result == VK_SUBOPTIMAL_KHR) {
                should_resize = TRUE;
            }
        }
    }

    vkDeviceWaitIdle(vulkan_context->device);
    if(memory_map) {
        if(vulkan_context->device_type == DEVICE_TYPE_DESCRETE) {
            vkUnmapMemory(vulkan_context->device, render_context->binding_context->host_memory_allocation);   
        }
        if(vulkan_context->device_type == DEVICE_TYPE_INTEGRATED) {
            vkUnmapMemory(vulkan_context->device, render_context->binding_context->device_memory_allocation);   
        }
    }

    /* SYNC OBJECTS DESTROY */ {
        for(u32 i = 0; i < MAX_SWAPCHAIN_IMAGE_COUNT; i++) {
            vkDestroySemaphore(vulkan_context->device, sync_objects.image_submit_semaphores[i], NULL);
        }
        vkDestroySemaphore(vulkan_context->device, sync_objects.image_available_semaphore, NULL);
        for(u32 i = 0; i < sync_objects.fence_count; i++) {
            vkDestroyFence(vulkan_context->device, sync_objects.fences[i], NULL);
        }
        sync_objects = (SyncObjects){0};
    }
    
    return MSG_CODE_SUCCESS;
}

/* creates render objects */
i32 renderCreateContext(const VulkanContext* vulkan_context, const RenderSettings* settings, MsgCallback_pfn msg_callback, RenderContext* render_context) {
    /* select swapchain params */
    if(!getSwapchainParams(vulkan_context->surface, vulkan_context->physical_device, vulkan_context->window, &render_context->swapchain_params)) {
        MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_SURFACE_STATS_NOT_SUITABLE, "vulkan surface stats not suitable");
    }

    /* copy callback pointers */
    render_context->start_callback = settings->start_callback;
    render_context->update_callback = settings->update_callback;

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

        if(!(render_context->targets_allocation = vramArenaAllocate(&depth_prototype_memory, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT))) {
            MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_ALLOCATE_VRAM, "failed to allocate memory for depth image");
        }
        /* dont forget to delete prototypes */
        vkDestroyImage(vulkan_context->device, depth_prototype, NULL);
    }

    /* create swapchain and depth texture */
    if(MSG_IS_ERROR(renderCreateSwapchain(vulkan_context, msg_callback, render_context))) {
        MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_SWAPCHAIN_CREATE, "failed to create vulkan swapchain");
    }

    if(MSG_IS_ERROR(createBindingContext(vulkan_context, settings, msg_callback, render_context->binding_context))) {
        MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_RENDER_BINDING_CONTEXT_CREATE, "failed to create render binding context");
    }

    if(MSG_IS_ERROR(createPipelineContext(vulkan_context, settings, render_context, msg_callback, render_context->pipeline_context))) {
        MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_RENDER_EXECUTE_CONTEXT_CREATE, "failed to create render execute context");
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

    vkDestroyCommandPool(vulkan_context->device, render_context->command_pool, NULL);

    destroyPipelineContext(vulkan_context, render_context->pipeline_context);
    destroyBindingContext(vulkan_context, render_context->binding_context);

    for(u32 i = 0; i < render_context->swapchain_image_count; i++) {
        vkDestroyImageView(vulkan_context->device, render_context->swapchain_image_views[i], NULL);
    }
    vkDestroyImageView(vulkan_context->device, render_context->depth_view, NULL);
    vkDestroyImage(vulkan_context->device, render_context->depth_image, NULL);

    vkDestroySwapchainKHR(vulkan_context->device, render_context->swapchain, NULL);
    *render_context = (RenderContext){0};
}

/* render main func */
i32 renderRun(const VulkanContext* vulkan_context, const RenderSettings* settings, MsgCallback_pfn msg_callback) {
    static RenderPipelineContext s_pipeline_context = (RenderPipelineContext){0};
    static RenderBindingContext s_binding_context = (RenderBindingContext){0};
    static RenderContext s_render_context = (RenderContext){.binding_context = &s_binding_context, .pipeline_context = &s_pipeline_context};
    if(MSG_IS_ERROR(renderCreateContext(vulkan_context, settings, msg_callback, &s_render_context))) {
        MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_CREATE_RENDER_CONTEXT, "failed to create render context");
    }

    if(MSG_IS_ERROR(renderLoop(vulkan_context, msg_callback, &s_render_context))) {
        MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_RENDER_LOOP_FAIL, "vulkan render loop error");
    }

    renderDestroyContext(vulkan_context, &s_render_context);
    return MSG_CODE_SUCCESS;
}
