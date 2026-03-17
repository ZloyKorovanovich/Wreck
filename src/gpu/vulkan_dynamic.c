#define VULKAN_INTERNAL
#include "gpu.h"

const char *allocation_names[] = {
    "dynamic meshes allocation swap 0",
    "dynamic meshes allocation swap 1"
};

/* FIX: zeroed struct indicates empty slot */
b32 createMeshBuffers(
    /* inputs */
    GPUMemoryAllocator *gpu_memory_allocator,
    const GPUDevice *gpu_device,
    const GPUMeshInfo *mesh_infos,
    u32 mesh_count,
    const char *allocation_name,
    MsgCallback_pfn msg_callback,
    /* outputs */
    GPUMesh *meshes,
    GPUBufferLocation *mesh_locations,
    GPUMemory *dst_memory
) {
    VkDevice vk_device = gpu_device->vk_device;
    String log_str = STACK_STR(512);

    u64 dst_memory_size = 0;
    u32 dst_memory_type_bits = U32_MAX;

    /* zero initilaize all meshes */
    for(u32 i = 0; i < mesh_count; i++) {
        meshes[i] = (GPUMesh){0};
        mesh_locations[i * 2] = (GPUBufferLocation){0};
        mesh_locations[i * 2 + 1] = (GPUBufferLocation){0};
    }

    /* CREATE MESH BUFFERS */
    for(u32 i = 0; i < mesh_count; i++) {
        const char *mesh_name = mesh_infos[i].name ? mesh_infos[i].name : "";

        /* VALIDATION */ {
            if(!mesh_infos[i].vertices || !mesh_infos[i].indices) {
                stringPattern(
                    &TRACED_STR("mesh data is null name: \"%c\" id: %u32/%u32"),
                    (const void *[]){mesh_name, &i, &mesh_count},
                    &log_str
                );
                MSG_ERROR(msg_callback, &log_str);
                return FALSE;         
            }
            if(mesh_infos[i].vertex_count > U16_MAX) {
                stringPattern(
                    &TRACED_STR("too big mesh, too many vertices exceeing limit of U16_MAX name: \"%c\" id: %u32/%u32"),
                    (const void *[]){mesh_name, &i, &mesh_count},
                    &log_str
                );
                MSG_ERROR(msg_callback, &log_str);
                return FALSE;         
            }
            if(mesh_infos[i].index_count < mesh_infos[i].vertex_count) {
                stringPattern(
                    &TRACED_STR("index count is greated than vertex count, not all indices index to vertices name: \"%c\" id: %u32/%u32"),
                    (const void *[]){mesh_name, &i, &mesh_count},
                    &log_str
                );
                MSG_ERROR(msg_callback, &log_str);
                return FALSE;
            }
        }
        
        /* buffer infos */
        VkBufferCreateInfo vertex_buffer_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .size = mesh_infos[i].vertex_count * sizeof(GPUVertex)
        };
        VkBufferCreateInfo index_buffer_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .size = mesh_infos[i].index_count * sizeof(GPUIndex)
        };

        /* create buffers */
        if(vkCreateBuffer(vk_device, &vertex_buffer_info, NULL, &meshes[i].vertex_buffer) != VK_SUCCESS) {
            stringPattern(
                &TRACED_STR("failed to create vertex buffer name: \"%c\" id: %u32/%u32"),
                (const void *[]){mesh_name, &i, &mesh_count},
                &log_str
            );
            MSG_ERROR(msg_callback, &log_str);
            return FALSE;
        }
        if(vkCreateBuffer(vk_device, &index_buffer_info, NULL, &meshes[i].index_buffer) != VK_SUCCESS) {
            stringPattern(
                &TRACED_STR("failed to create vertex buffer name: \"%c\" id: %u32/%u32"),
                (const void *[]){mesh_name, &i, &mesh_count},
                &log_str
            );
            MSG_ERROR(msg_callback, &log_str);
            return FALSE;
        }

        VkMemoryRequirements vertex_memory_requirements = (VkMemoryRequirements){0};
        VkMemoryRequirements index_memory_requirements = (VkMemoryRequirements){0};
        vkGetBufferMemoryRequirements(vk_device, meshes[i].vertex_buffer, &vertex_memory_requirements);
        vkGetBufferMemoryRequirements(vk_device, meshes[i].index_buffer, &index_memory_requirements);

        ADJUST_GPU_BUFFER_LOCATION(vertex_memory_requirements, mesh_locations[i * 2], dst_memory_size, dst_memory_type_bits);
        ADJUST_GPU_BUFFER_LOCATION(index_memory_requirements, mesh_locations[i * 2 + 1], dst_memory_size, dst_memory_type_bits);
    }
    
    /* ALLOCATE MEMORY IF NEEDED */
    if(dst_memory->size < dst_memory_size) {
        if(dst_memory->memory) {
            if(!freeGPUMemory(gpu_memory_allocator, dst_memory)) {
                MSG_ERROR(msg_callback, &TRACED_STR("failed to free meshes dst allocation"));
                return FALSE;
            }

            if(!allocateGPUMemory(
                gpu_memory_allocator,
                allocation_name,
                GPU_MEMORY_USE_DEVICE,
                dst_memory_type_bits,
                dst_memory_size,
                dst_memory
            )) {
                MSG_ERROR(msg_callback, &TRACED_STR("failed to allocate gpu meshes memory"));
                return FALSE;
            }
        }
    }

    /* BIND MESHES */
    for(u32 i = 0; i < mesh_count; i++) {
        const char *mesh_name = mesh_infos[i].name ? mesh_infos[i].name : "";

        /* bind vertex buffer */
        if(!vkBindBufferMemory(
            vk_device,
            meshes[i].vertex_buffer,
            dst_memory->memory,
            mesh_locations[i * 2].offset
        )) {
            stringPattern(
                &TRACED_STR("failed to bind vertex buffer memory name: \"%c\" id: %u32/%u32"),
                (const void *[]){mesh_name, &i, &mesh_count},
                &log_str
            );
            MSG_ERROR(msg_callback, &log_str);
        }
        /* bind index buffer */
        if(!vkBindBufferMemory(
            vk_device,
            meshes[i].index_buffer,
            dst_memory->memory,
            mesh_locations[i * 2 + 1].offset
        )) {
            stringPattern(
                &TRACED_STR("failed to bind index buffer memory name: \"%c\" id: %u32/%u32"),
                (const void *[]){mesh_name, &i, &mesh_count},
                &log_str
            );
            MSG_ERROR(msg_callback, &log_str);
        }
    }

    return TRUE;
}

/* FIX: add transition barrier transfer to render if needed */
b32 transferMeshes(
    /* inputs */
    GPUMemoryAllocator *gpu_memory_allocator,
    const GPUDevice *gpu_device,
    const GPUMemory *dst_memory,
    const GPUMeshInfo *mesh_infos,
    const GPUBufferLocation *mesh_locations,
    const GPUMesh *meshes,
    u32 mesh_count,
    MsgCallback_pfn msg_callback,
    VkCommandBuffer transfer_command_buffer,
    VkFence transfer_fence,
    /* outputs */
    GPUMemory *src_memory,
    VkBuffer *src_buffer,
    u64 *src_buffer_size,
    void **mesh_memory_address
) {
    VkDevice vk_device = gpu_device->vk_device;

    /* if src buffer is not needed */
    if(dst_memory->flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
        /* if src buffer not needed, no allocations needed anymore */
        if(!*mesh_memory_address) {
            if(vkMapMemory(
                vk_device, 
                dst_memory->memory, 
                0, dst_memory->size, 
                0, mesh_memory_address
            ) != VK_SUCCESS) {
                MSG_ERROR(msg_callback, &TRACED_STR("failed to map dst mesh memory"));
                return FALSE;
            }
        }

        for(u32 i = 0; i < mesh_count; i++) {
            copyMemory((u8 *)*mesh_memory_address + mesh_locations[i * 2].offset, mesh_infos[i].vertices, mesh_locations[i * 2].size);
            copyMemory((u8 *)*mesh_memory_address + mesh_locations[i * 2 + 1].offset, mesh_infos[i].indices, mesh_locations[i * 2 + 1].size);
        }

        /* apply changes to memory if its not done automatically */
        if(!(dst_memory->flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
            if(vkFlushMappedMemoryRanges(
                vk_device, 
                1, 
                (VkMappedMemoryRange[1]){
                    (VkMappedMemoryRange) {
                        .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
                        .memory = dst_memory->memory,
                        .offset = 0,
                        .size = dst_memory->size
                    }
                }
            ) != VK_SUCCESS) {
                MSG_ERROR(msg_callback, &TRACED_STR("failed to flush meshes dst memory"));
                return FALSE;
            }
        }
    }
    /* if src buffer is needed */
    else {
        /* if not enough size in src buffer for transfering to dst memory, recreate */
        if(*src_buffer_size < dst_memory->size) {
            if(*src_buffer) {
                vkDestroyBuffer(vk_device, *src_buffer, NULL);
            }
            
            VkBufferCreateInfo buffer_info = {
                .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                .size = dst_memory->size
            };
            if(vkCreateBuffer(vk_device, &buffer_info, NULL, src_buffer) != VK_SUCCESS) {
                MSG_ERROR(msg_callback, &TRACED_STR("failed to create src mesh buffer"));
                return FALSE;
            }
            
            /* get buffer requirements */
            VkMemoryRequirements buffer_requirements = (VkMemoryRequirements){0};
            vkGetBufferMemoryRequirements(vk_device, *src_buffer, &buffer_requirements);

            /* allocate new memory if needed */
            if(src_memory->size < buffer_requirements.size) {
                /* unmap and free old src memory */
                if(src_memory->memory) {
                    if(*mesh_memory_address) {
                        vkUnmapMemory(vk_device, src_memory->memory);
                        *mesh_memory_address = NULL;
                    }
                    if(!freeGPUMemory(gpu_memory_allocator, src_memory)) {
                        MSG_ERROR(msg_callback, &TRACED_STR("failed to free src mesh memory"));
                        return FALSE;
                    }
                }

                /* allocate new src memory */
                if(!allocateGPUMemory(
                    gpu_memory_allocator, 
                    "meshes src buffer",
                    GPU_MEMORY_USE_HOST_TO_DEVICE, 
                    buffer_requirements.memoryTypeBits, 
                    buffer_requirements.size, 
                    src_memory
                )) {
                    MSG_ERROR(msg_callback, &TRACED_STR("failed to allocate src mesh memory"));
                    return FALSE;
                }

                if(vkMapMemory(
                    vk_device,
                    src_memory->memory,
                    0, src_memory->size,
                    0, mesh_memory_address
                ) != VK_SUCCESS) {
                    MSG_ERROR(msg_callback, &TRACED_STR("failed to map src mesh memory"));
                    return FALSE;
                }
            }

            if(vkBindBufferMemory(vk_device, *src_buffer, src_memory->memory, 0) != VK_SUCCESS) {
                MSG_ERROR(msg_callback, &TRACED_STR("failed to bind src mesh memory to buffer"));
                return FALSE;
            }
        }

        /* copy to src memory */
        for(u32 i = 0; i < mesh_count; i++) {
            copyMemory((u8 *)*mesh_memory_address + mesh_locations[i * 2].offset, mesh_infos[i].vertices, mesh_locations[i * 2].size);
            copyMemory((u8 *)*mesh_memory_address + mesh_locations[i * 2 + 1].offset, mesh_infos[i].indices, mesh_locations[i * 2 + 1].size);
        }

        /* apply changes to memory if its not done automatically
            its not the same code, we use different memory */
        if(!(src_memory->flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
            if(vkFlushMappedMemoryRanges(
                vk_device, 
                1, 
                (VkMappedMemoryRange[1]){
                    (VkMappedMemoryRange) {
                        .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
                        .memory = src_memory->memory,
                        .offset = 0,
                        .size = src_memory->size
                    }
                }
            ) != VK_SUCCESS) {
                MSG_ERROR(msg_callback, &TRACED_STR("failed to flush meshes src memory"));
                return FALSE;
            }
        }
        
        /* start command buffer recording */
        VkCommandBufferBeginInfo begin_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
        };
        if(vkBeginCommandBuffer(transfer_command_buffer, &begin_info) != VK_SUCCESS) {
            MSG_ERROR(msg_callback, &TRACED_STR("failed to begin mesh transfer command buffer"));
            return FALSE;
        }

        b32 use_transfer_queue = gpu_device->vk_transfer_queue_id != U32_MAX;

        /* record buffer copies */
        for(u32 i = 0; i < mesh_count; i++) {
            VkBufferCopy vertex_copy = {
                .srcOffset = mesh_locations[i * 2].offset,
                .dstOffset = 0,
                .size = mesh_locations[i * 2].size
            };
            VkBufferCopy index_copy = {
                .srcOffset = mesh_locations[i * 2 + 1].offset,
                .dstOffset = 0,
                .size = mesh_locations[i * 2 + 1].size
            };
            vkCmdCopyBuffer(transfer_command_buffer, *src_buffer, meshes[i].vertex_buffer, 1, &vertex_copy);
            vkCmdCopyBuffer(transfer_command_buffer, *src_buffer, meshes[i].index_buffer, 1, &index_copy);

            /* if using transfer we need to transit buffers to render queue */
            if(use_transfer_queue) {
                VkBufferMemoryBarrier buffer_barriers[] = {
                    (VkBufferMemoryBarrier) {
                        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                        .srcAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT,
                        .dstAccessMask       = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
                        .srcQueueFamilyIndex = gpu_device->vk_transfer_queue_id,
                        .dstQueueFamilyIndex = gpu_device->vk_render_queue_id,
                        .buffer = meshes[i].vertex_buffer
                    },
                    (VkBufferMemoryBarrier) {
                        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                        .dstAccessMask = VK_ACCESS_INDEX_READ_BIT,
                        .srcQueueFamilyIndex = gpu_device->vk_transfer_queue_id,
                        .dstQueueFamilyIndex = gpu_device->vk_render_queue_id,
                        .buffer = meshes[i].index_buffer
                    }
                };

                /* barrier from transfer to the end of command buffer */
                vkCmdPipelineBarrier(
                    transfer_command_buffer, 
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                    0,
                    0, NULL,
                    2, buffer_barriers,
                    0, NULL
                );
            }
        }
        
        /* end command buffer recording */
        if(vkEndCommandBuffer(transfer_command_buffer) != VK_SUCCESS) {
            MSG_ERROR(msg_callback, &TRACED_STR("failed to end mesh transfer command buffer"));
            return FALSE;
        }

        /* submit */
        VkSubmitInfo submit_info = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .commandBufferCount = 1,
            .pCommandBuffers = &transfer_command_buffer
        };
        if(vkQueueSubmit(
            use_transfer_queue ? gpu_device->vk_transfer_queue : gpu_device->vk_render_queue, 
            1, &submit_info, 
            transfer_fence
        ) != VK_SUCCESS) {
            MSG_ERROR(msg_callback, &TRACED_STR("failed to submit mesh transfer operations"));
        }
    }

    return TRUE;
}

void destroyDynamicResources(
    GPUMemoryAllocator *allocator,
    GPUDynamicResources *resources,
    MsgCallback_pfn msg_callback
) {
    VkDevice vk_device = allocator->device;
    /* if meshes exist destroy them */
    if(resources->mesh_count != 0) {
        const u32 mesh_count = resources->mesh_count;
        GPUMesh *meshes = resources->meshes;
        /* detsroy meshes */
        for(u32 i = 0; i != mesh_count; i++) {
            if(meshes[i].vertex_buffer) {
                vkDestroyBuffer(vk_device, meshes[i].vertex_buffer, NULL);
            }
            if(meshes[i].index_buffer) {
                vkDestroyBuffer(vk_device, meshes[i].index_buffer, NULL);
            }
            meshes[i] = (GPUMesh){0};
        }
    }

    if(resources->meshes_memory.memory) {
        if(!freeGPUMemory(allocator, &resources->meshes_memory)) {
            MSG_WARNING(msg_callback, &TRACED_STR("failed to free meshes gpu memory"))
        }
    }

    *resources = (GPUDynamicResources){0};
}

/* valid usage: use it only in 1 thread, no 2 functions of this type should run in parallel */
b32 swapGPUDynamicResources(
    GPU *gpu, 
    const SwapGPUDynamicResourcesIn *input
) {
    VkDevice              vk_device         = gpu->device.vk_device;
    MsgCallback_pfn       msg_callback      = gpu->msg_callback;
    /* new resources for swap */
    GPUDynamicResources   old_resources     = gpu->dynamic_resources;
    GPUDynamicResources   new_resources     = (GPUDynamicResources){0};
    VkFence               meshes_fence      = NULL;

    /* detsruction */
    if(!input) {
        goto _sync_copy; /* new resources is empty */
    }

    /* TRANSFER OBJECTS */ {
        VkFenceCreateInfo fence_info = {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT
        };
        if(vkCreateFence(vk_device, &fence_info, NULL, &meshes_fence) != VK_SUCCESS) {
            MSG_ERROR(msg_callback, &TRACED_STR("failed to create mesh transfer fence"));
            return FALSE;
        }
    }

    if(input->mesh_count != 0) {
        /* create new meshes */
        new_resources.mesh_count = input->mesh_count;
        if(!createMeshBuffers(
            &gpu->memory_allocator,
            &gpu->device,
            input->meshes,
            input->mesh_count,
            allocation_names[new_resources.swap_id],
            msg_callback,
            new_resources.meshes,
            new_resources.mesh_locations,
            &new_resources.meshes_memory
        )) {
            MSG_ERROR(msg_callback, &TRACED_STR("failed to create mesh buffers"));
            return FALSE;
        }
    }

    /* wait untill all gpu operations are done */
    if(vkWaitForFences(vk_device, 1, &meshes_fence, TRUE, U64_MAX) != VK_SUCCESS) {
        MSG_ERROR(msg_callback, &TRACED_STR("failed to wait on transfer fences"));
        return FALSE;
    }

    /* sync with render thread, because it uses these meshes */
    _sync_copy: {
        #ifdef _WIN32
            WaitForSingleObject(gpu->control.dynamic_resources_mutex, INFINITE);
            gpu->dynamic_resources = new_resources;
            ReleaseMutex(gpu->control.dynamic_resources_mutex);
        #endif
    }

    destroyDynamicResources(&gpu->memory_allocator, &old_resources, msg_callback);

    return TRUE;
}
