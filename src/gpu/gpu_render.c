#include "gpu_internal.h"

b32 gpu_render_init(
    CtxHandle ctx
) {
    if(ctx == NULL) {
        LOG_ERROR("invalid input params");
        goto fail;
    }

    GpuContext*         gpu_ctx       = (GpuContext*)ctx;
    const VulkanDevice* vulkan_device = &gpu_ctx->vulkan_device;
    const VkDevice      device        = vulkan_device->device;
    VulkanRender*       vulkan_render = &gpu_ctx->vulkan_render;

    /* create command buffer */
    const VkCommandBufferAllocateInfo command_buffer_render_info = {
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool        = vulkan_device->command_pool_render,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };

    if(vkAllocateCommandBuffers(device, &command_buffer_render_info, &vulkan_render->command_buffer_render) != VK_SUCCESS) {
        LOG_ERROR("failed to create render command buffer");
        goto fail;
    }

    /* create fences and semaphores */
    const VkFenceCreateInfo fence_signaled_info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };
    const VkSemaphoreCreateInfo semaphore_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
    };

    VkSemaphore* semaphores_images_finished = vulkan_render->semaphores_images_finished;

    if(vkCreateFence(device, &fence_signaled_info, NULL, &vulkan_render->fence_frame) != VK_SUCCESS) {
        LOG_ERROR("failed to create frame fence");
        goto fail;
    }
    if(vkCreateSemaphore(device, &semaphore_info, NULL, &vulkan_render->semaphore_image_available) != VK_SUCCESS) {
        LOG_ERROR("failed to create image available semaphore");
        goto fail;
    }
    for(u32 i = 0; i != GPU_MAX_SWAPCHAIN_IMAGES; i++) {
        if(vkCreateSemaphore(device, &semaphore_info, NULL, &semaphores_images_finished[i]) != VK_SUCCESS) {
            LOG_ERROR("failed to create image finished semaphore id: %u/%u", i, GPU_MAX_SWAPCHAIN_IMAGES);
            goto fail;
        }
    }

    return TRUE;

    fail: {
        return FALSE;
    }
}

void gpu_render_terminate(
    CtxHandle ctx
) {
    if(ctx == NULL) {
        LOG_ERROR("invalid input params");
        goto fail;
    }

    GpuContext*         gpu_ctx       = (GpuContext*)ctx;
    const VulkanDevice* vulkan_device = &gpu_ctx->vulkan_device;
    const VkDevice      device        = vulkan_device->device;
    VulkanRender*       vulkan_render = &gpu_ctx->vulkan_render;

    /* wait on pending operations */
    vkDeviceWaitIdle(device);

    /* semaphores and fences */
    const VkSemaphore* semaphores_images_finished = vulkan_render->semaphores_images_finished;

    for(u32 i = 0; i != GPU_MAX_SWAPCHAIN_IMAGES; i++) {
        vkDestroySemaphore(device, semaphores_images_finished[i], NULL);
    }
    vkDestroySemaphore(device, vulkan_render->semaphore_image_available, NULL);
    vkDestroyFence(device, vulkan_render->fence_frame, NULL);

    /* command buffers */
    vkFreeCommandBuffers(device, vulkan_device->command_pool_render, 1, &vulkan_render->command_buffer_render);

    *vulkan_render = (VulkanRender){0};

    fail: {}
}

/* 0 = success
   1 = fail
   2 = window_closed */
i32 gpu_render_frame_begin(
    CtxHandle ctx,
    u32*      screen_x,
    u32*      screen_y
) {
    GpuContext*          gpu_ctx          = (GpuContext*)ctx;
    const VulkanObjects* vulkan_objects   = &gpu_ctx->vulkan_objects;
    const VulkanDevice*  vulkan_device    = &gpu_ctx->vulkan_device;
    const VulkanShaders* vulkan_shaders   = &gpu_ctx->vulkan_shaders;
    VulkanResources*     vulkan_resources = &gpu_ctx->vulkan_resources;
    VulkanRender*        vulkan_render    = &gpu_ctx->vulkan_render;

    /* wait for frame fence */
    if(vkWaitForFences(vulkan_device->device, 1, &vulkan_render->fence_frame, VK_TRUE, U64_MAX) != VK_SUCCESS) {
        LOG_ERROR("failed to wait for frame fence");
        goto fail;
    }
    vkResetFences(vulkan_device->device, 1, &vulkan_render->fence_frame);

    reacquire: {}

    /* acquire next swapchain image */
    u32 swapchain_image_id = U32_MAX;

    const VkResult acquire_result = vkAcquireNextImageKHR(
        vulkan_device->device,
        vulkan_resources->swapchain,
        U64_MAX,
        vulkan_render->semaphore_image_available,
        NULL,
        &swapchain_image_id
    );
    /* check for resizing */
    if(acquire_result == VK_ERROR_OUT_OF_DATE_KHR) {
        /* resize swapchain */
        vkDeviceWaitIdle(vulkan_device->device);

        const i32 resize_result = create_swapchain(
            vulkan_device->device,
            vulkan_device->adapter->physical_device,
            vulkan_objects->surface,
            vulkan_device->adapter->surface_format,
            vulkan_device->adapter->surface_color_space,
            &vulkan_resources->swapchain,
            &vulkan_resources->swapchain_images_count,
            &vulkan_resources->swapchain_x,
            &vulkan_resources->swapchain_y,
            vulkan_resources->swapchain_images,
            vulkan_resources->swapchain_views
        );
        if(resize_result == 1) {
            LOG_ERROR("failed to resize window");
            goto fail;
        }
        if(resize_result == 2) {
            goto window_closed;
        }
        goto reacquire;
    }
    else if(acquire_result != VK_SUBOPTIMAL_KHR && acquire_result != VK_SUCCESS) {
        LOG_ERROR("failed to acquire swapchain image");
        goto fail;
    }

    /* load surface image info */
    vulkan_render->swapchain_image_id   = swapchain_image_id;
    vulkan_render->swapchain_image      = vulkan_resources->swapchain_images[swapchain_image_id];
    vulkan_render->swapchain_image_view = vulkan_resources->swapchain_views [swapchain_image_id];
    vulkan_render->sync_transfer_size   = 0;

    /* start command buffer recording */
    const VkCommandBufferBeginInfo command_buffer_render_begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
    };
    if(vkResetCommandBuffer(vulkan_render->command_buffer_render, 0) != VK_SUCCESS) {
        LOG_ERROR("failed to reset render command buffer");
        goto fail;
    }
    if(vkBeginCommandBuffer(vulkan_render->command_buffer_render, &command_buffer_render_begin_info) != VK_SUCCESS) {
        LOG_ERROR("failed to begin render command buffer");
        goto fail;
    }

    GpuImageState*  image_states        = vulkan_render->image_states;
    GpuBufferState* buffer_states       = vulkan_render->buffer_states;
    const u32       image_states_count  = vulkan_resources->images_count;
    const u32       buffer_states_count = vulkan_resources->buffers_count;
    for(u32 i = 0; i != image_states_count; i++) {
        image_states[i] = (GpuImageState) {
            .access = VK_ACCESS_NONE,
            .layout = VK_IMAGE_LAYOUT_UNDEFINED,
            .stage  = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT
        };
    }
    for(u32 i = 0; i != buffer_states_count; i++) {
        buffer_states[i] = (GpuBufferState) {
            .access = VK_ACCESS_NONE,
            .stage  = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT
        };
    }

    /* surface top barrier */
    const VkImageMemoryBarrier surface_memory_barrier = {
        .sType            = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask    = VK_ACCESS_NONE,
        .dstAccessMask    = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .oldLayout        = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout        = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .image            = vulkan_render->swapchain_image,
        .subresourceRange = (VkImageSubresourceRange) {
            .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel   = 0,
            .levelCount     = 1,
            .baseArrayLayer = 0,
            .layerCount     = 1
        }
    };
    vkCmdPipelineBarrier(
        vulkan_render->command_buffer_render, 
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        0,
        0,
        NULL,
        0,
        NULL,
        1,
        &surface_memory_barrier
    );

    vkCmdBindDescriptorSets(
        vulkan_render->command_buffer_render, 
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        vulkan_shaders->pipeline_layout,
        0,
        GPU_DESCRIPTOR_SET_COUNT,
        vulkan_shaders->descriptor_sets,
        0,
        NULL
    );
    vkCmdBindDescriptorSets(
        vulkan_render->command_buffer_render, 
        VK_PIPELINE_BIND_POINT_COMPUTE,
        vulkan_shaders->pipeline_layout,
        0,
        GPU_DESCRIPTOR_SET_COUNT,
        vulkan_shaders->descriptor_sets,
        0,
        NULL
    );

    /* screen info */
    *screen_x = vulkan_resources->swapchain_x;
    *screen_y = vulkan_resources->swapchain_y;

    return 0;

    fail: {
        return 1;
    }
    
    window_closed: {
        return 2;
    }
}

/* 0 = success
   1 = fail
   2 = window_closed */
/* FIX: refactor flushing */
i32 gpu_render_frame_end(
    CtxHandle ctx
) {
    GpuContext*          gpu_ctx          = (GpuContext*)ctx;
    const VulkanObjects* vulkan_objects   = &gpu_ctx->vulkan_objects;
    const VulkanDevice*  vulkan_device    = &gpu_ctx->vulkan_device;
    VulkanResources*     vulkan_resources = &gpu_ctx->vulkan_resources;
    VulkanRender*        vulkan_render    = &gpu_ctx->vulkan_render;

    const u32 swapchain_image_id = vulkan_render->swapchain_image_id;

    /* surface bottom barrier */
    const VkImageMemoryBarrier surface_memory_barrier = {
        .sType            = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask    = VK_ACCESS_NONE,
        .dstAccessMask    = VK_ACCESS_NONE,
        .oldLayout        = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .newLayout        = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .image            = vulkan_render->swapchain_image,
        .subresourceRange = (VkImageSubresourceRange) {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel   = 0,
            .levelCount     = 1,
            .baseArrayLayer = 0,
            .layerCount     = 1
        }
    };
    vkCmdPipelineBarrier(
        vulkan_render->command_buffer_render, 
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        0,
        0,
        NULL,
        0,
        NULL,
        1,
        &surface_memory_barrier
    );

    /* end command buffer recording */
    if(vkEndCommandBuffer(vulkan_render->command_buffer_render) != VK_SUCCESS) {
        LOG_ERROR("failed to end render command buffer");
        goto fail;
    }
    
    /* FIX: flush memory if perform transfer */
    /* in if condition and flush range are different memory */
    if(
        vulkan_device->video_memory_device_buffers.memory_map == NULL && 
        vulkan_render->sync_transfer_size                     != 0
    ) {
        const VkMappedMemoryRange flush_range = {
            .sType  = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
            .offset = (vulkan_resources->buffer_sync_transfer.allocation_offset) & 0xFFFFFFFFFFFFFF00,
            .size   = (vulkan_render->sync_transfer_size + 0xFF)                 & 0xFFFFFFFFFFFFFF00,
            .memory = vulkan_device->video_memory_host_transfer.device_memory
        };

        vkFlushMappedMemoryRanges(vulkan_device->device, 1, &flush_range);
    }

    /* submit and present frame */
    const VkPipelineStageFlags wait_stages              = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    const VkSubmitInfo         submit_render_queue_info = {
        .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount   = 1,
        .pCommandBuffers      = &vulkan_render->command_buffer_render,
        .waitSemaphoreCount   = 1,
        .pWaitSemaphores      = &vulkan_render->semaphore_image_available,
        .pWaitDstStageMask    = &wait_stages,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores    = &vulkan_render->semaphores_images_finished[swapchain_image_id]
    };
    const VkPresentInfoKHR present_swapchain_image_info = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .swapchainCount     = 1,
        .pSwapchains        = &vulkan_resources->swapchain,
        .pImageIndices      = &vulkan_render->swapchain_image_id,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores    = &vulkan_render->semaphores_images_finished[swapchain_image_id]
    };

    if(vkQueueSubmit(vulkan_device->queue_render, 1, &submit_render_queue_info, vulkan_render->fence_frame) != VK_SUCCESS) {
        LOG_ERROR("failed to submit frame to render queue");
        goto fail;
    }

    VkResult present_result = vkQueuePresentKHR(
        vulkan_device->queue_render, 
        &present_swapchain_image_info
    );
    if(present_result == VK_ERROR_OUT_OF_DATE_KHR || present_result == VK_SUBOPTIMAL_KHR) {
        /* resize swapchain */
        vkDeviceWaitIdle(vulkan_device->device);

        const i32 resize_result = create_swapchain(
            vulkan_device->device,
            vulkan_device->adapter->physical_device,
            vulkan_objects->surface,
            vulkan_device->adapter->surface_format,
            vulkan_device->adapter->surface_color_space,
            &vulkan_resources->swapchain,
            &vulkan_resources->swapchain_images_count,
            &vulkan_resources->swapchain_x,
            &vulkan_resources->swapchain_y,
            vulkan_resources->swapchain_images,
            vulkan_resources->swapchain_views
        );

        if(resize_result == 1) {
            LOG_ERROR("failed to resize window");
            goto fail;
        }
        if(resize_result == 2) {
            goto window_closed;
        }
    }
    else if(present_result != VK_SUCCESS) {
        LOG_ERROR("failed to present frame");
        goto fail;
    }
    
    return 0;

    fail: {
        return 1;
    }

    window_closed: {
        return 2;
    }
}

/* GRAPHICS */

void gpu_render_begin_drawing(
    CtxHandle          ctx,
    const DrawingInfo* drawing_info
) {
    GpuContext*            gpu_ctx          = (GpuContext*)ctx;
    const VulkanResources* vulkan_resources = &gpu_ctx->vulkan_resources;
    const VulkanDevice*    vulkan_device    = &gpu_ctx->vulkan_device;
    VulkanRender*          vulkan_render    = &gpu_ctx->vulkan_render;

    /* resources */
    const GpuBuffer* gpu_buffers       = vulkan_resources->buffers;
    const GpuImage*  gpu_images        = vulkan_resources->images;
    const u32        gpu_buffers_count = vulkan_resources->buffers_count;
    const u32        gpu_images_count  = vulkan_resources->images_count;

    GpuImageState*  image_states  = vulkan_render->image_states;
    GpuBufferState* buffer_states = vulkan_render->buffer_states;
    
    /* read images & read buffers barriers */ {
    const u32* read_images_ids    = drawing_info->images_read;
    const u32* read_buffers_ids   = drawing_info->buffers_read;
    const u32  read_images_count  = drawing_info->images_read_count;
    const u32  read_buffers_count = drawing_info->buffers_read_count;

    for(u32 i = 0; i != read_images_count; i++) {
        const u32 read_image_id = read_images_ids[i];

        if(read_image_id >= gpu_images_count) {
            LOG_ERROR("invalid read image id: %u/%u", read_image_id, gpu_images_count);
            goto fail;
        }

        /* gnerated read image barrier */
        const VkAccessFlags        src_access = image_states[read_image_id].access;
        const VkImageLayout        src_layout = image_states[read_image_id].layout;
        const VkPipelineStageFlags src_stage  = image_states[read_image_id].stage;
            
        const VkAccessFlags        dst_access = VK_ACCESS_SHADER_READ_BIT;
        const VkImageLayout        dst_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        const VkPipelineStageFlags dst_stage  = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
        
        const GpuImage*          image  = &gpu_images[read_image_id];

        const VkImageMemoryBarrier read_image_barrier = {
            .sType            = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .image            = image->image,
            .srcAccessMask    = src_access,
            .oldLayout        = src_layout,
            .dstAccessMask    = dst_access,
            .newLayout        = dst_layout,
            .subresourceRange = (VkImageSubresourceRange) {
                .aspectMask     = image->aspect,
                .baseArrayLayer = 0,
                .layerCount     = 1,
                .baseMipLevel   = 0,
                .levelCount     = 1
            }
        };

        vkCmdPipelineBarrier(
            vulkan_render->command_buffer_render, 
            src_stage,
            dst_stage,
            0,
            0,
            NULL,
            0,
            NULL,
            1,
            &read_image_barrier
        );

        image_states[read_image_id] = (GpuImageState) {
            .access = dst_access,
            .layout = dst_layout,
            .stage  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
        };
    }

    for(u32 i = 0; i != read_buffers_count; i++) {
        const u32 read_buffer_id = read_buffers_ids[i];

        if(read_buffer_id >= gpu_buffers_count) {
            LOG_ERROR("invalid read buffer id: %u/%u", read_buffer_id, gpu_buffers_count);
            goto fail;
        }

        /* generate read buffer barrier */
        const VkAccessFlags        src_access = buffer_states[read_buffer_id].access;
        const VkPipelineStageFlags src_stage  = buffer_states[read_buffer_id].stage;

        const VkAccessFlags        dst_access = VK_ACCESS_SHADER_READ_BIT;
        const VkPipelineStageFlags dst_stage  = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;

        const VkBufferMemoryBarrier read_buffer_barrier = {
            .sType         = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
            .srcAccessMask = src_access,
            .dstAccessMask = dst_access,
            .buffer        = gpu_buffers[read_buffer_id].buffer,
            .size          = gpu_buffers[read_buffer_id].used_size,
            .offset        = 0
        };

        vkCmdPipelineBarrier(
            vulkan_render->command_buffer_render, 
            src_stage,
            dst_stage,
            0,
            0,
            NULL,
            1,
            &read_buffer_barrier,
            0,
            NULL
        );

        buffer_states[read_buffer_id] = (GpuBufferState) {
            .access = dst_access,
            .stage  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
        };
    }
    }

    /* generate attachments */
    VkRenderingAttachmentInfo rendering_color_attachments[GPU_MAX_COLOR_ATTACHMENTS] = {0};
    VkRenderingAttachmentInfo rendering_depth_attachment                             = (VkRenderingAttachmentInfo){0};

    const u32* color_attachments_ids   = drawing_info->attachments_color;
    const u32  depth_attachment_id     = drawing_info->attachment_depth;
    const u32  color_attachments_count = drawing_info->attachments_color_count; 

    if(color_attachments_count > GPU_MAX_COLOR_ATTACHMENTS) {
        LOG_ERROR("too many color attachments: %u/%u", color_attachments_count, GPU_MAX_COLOR_ATTACHMENTS);
        goto fail;
    }

    /* color attachment */
    for(u32 i = 0; i != color_attachments_count; i++) {

        const u32 color_attachment_id = color_attachments_ids[i];
        /* swapchain image target */
        if(color_attachment_id == GPU_IMAGE_SURFACE_ID) {
            rendering_color_attachments[i] = (VkRenderingAttachmentInfo) {
                .sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .imageView   = vulkan_render->swapchain_image_view,
                .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp     = VK_ATTACHMENT_STORE_OP_STORE,
                .clearValue  = (VkClearValue) {
                    .color = (VkClearColorValue){.float32 = {0.0, 0.0, 0.0, 0.0}}
                }
            };
        }
        /* resource image target */
        else if(color_attachment_id < gpu_images_count) {
            /* transit image */
            const VkAccessFlags        src_access = image_states[color_attachment_id].access;
            const VkImageLayout        src_layout = image_states[color_attachment_id].layout;
            const VkPipelineStageFlags src_stage  = image_states[color_attachment_id].stage;
            
            const VkAccessFlags        dst_access = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            const VkImageLayout        dst_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            const VkPipelineStageFlags dst_stage  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;            

            const VkImageMemoryBarrier color_attachment_barrier = {
                .sType            = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .image            = gpu_images[color_attachment_id].image,
                .srcAccessMask    = src_access,
                .oldLayout        = src_layout,
                .dstAccessMask    = dst_access,
                .newLayout        = dst_layout,
                .subresourceRange = (VkImageSubresourceRange) {
                    .aspectMask     = gpu_images[color_attachment_id].aspect,
                    .baseArrayLayer = 0,
                    .layerCount     = 1,
                    .baseMipLevel   = 0,
                    .levelCount     = 1
                }
            };

            vkCmdPipelineBarrier(
                vulkan_render->command_buffer_render, 
                src_stage,
                dst_stage,
                0,
                0,
                NULL,
                0,
                NULL,
                1,
                &color_attachment_barrier
            );

            image_states[color_attachment_id] = (GpuImageState) {
                .access = dst_access,
                .layout = dst_layout,
                .stage  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
            };

            /* fill render attachment */
            rendering_color_attachments[i] = (VkRenderingAttachmentInfo) {
                .sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .imageView   = gpu_images[color_attachment_id].view,
                .imageLayout = dst_layout,
                .loadOp      = drawing_info->do_not_clear ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp     = VK_ATTACHMENT_STORE_OP_STORE,
                .clearValue  = (VkClearValue) {
                    .color = (VkClearColorValue){.float32 = {0.0, 0.0, 0.0, 0.0}}
                }
            };
        }
        /* invalid */
        else {
            LOG_ERROR("invalid color attachment image id: %u/%u", color_attachment_id, gpu_images_count);
            goto fail;
        }
    }

    /* depth attachemnt */
    if(depth_attachment_id != U32_MAX) {
        /* resource image */
        if(depth_attachment_id < gpu_images_count) {
            /* transit image */
            const VkAccessFlags        src_access = image_states[depth_attachment_id].access;
            const VkImageLayout        src_layout = image_states[depth_attachment_id].layout;
            const VkPipelineStageFlags src_stage  = image_states[depth_attachment_id].stage;

            const VkAccessFlags        dst_access = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            const VkImageLayout        dst_layout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
            const VkPipelineStageFlags dst_stage  = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;

            const VkImageMemoryBarrier depth_attachment_barrier = {
                .sType            = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .image            = gpu_images[depth_attachment_id].image,
                .srcAccessMask    = src_access,
                .oldLayout        = src_layout,
                .dstAccessMask    = dst_access,
                .newLayout        = dst_layout,
                .subresourceRange = (VkImageSubresourceRange) {
                    .aspectMask     = gpu_images[depth_attachment_id].aspect,
                    .baseArrayLayer = 0,
                    .layerCount     = 1,
                    .baseMipLevel   = 0,
                    .levelCount     = 1
                }
            };

            vkCmdPipelineBarrier(
                vulkan_render->command_buffer_render, 
                src_stage,
                dst_stage,
                0,
                0,
                NULL,
                0,
                NULL,
                1,
                &depth_attachment_barrier
            );

            image_states[depth_attachment_id] = (GpuImageState) {
                .access = dst_access,
                .layout = dst_layout,
                .stage  = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT
            };

            /* fill render depth attachment */
            rendering_depth_attachment = (VkRenderingAttachmentInfo) {
                .sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .imageView   = gpu_images[depth_attachment_id].view,
                .imageLayout = dst_layout,
                .loadOp      = drawing_info->do_not_clear ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp     = VK_ATTACHMENT_STORE_OP_STORE,
                .clearValue  = (VkClearValue) {
                    .depthStencil = (VkClearDepthStencilValue) {.depth = drawing_info->max_depth}
                }
            };
        }
        /* invalid */
        else {
            LOG_ERROR("invalid depth attachment image id: %u/%u", depth_attachment_id, gpu_images_count);
            goto fail;
        }
    }

    /* begin rendering */
    const VkRenderingInfoKHR rendering_info = {
        .sType                = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .colorAttachmentCount = color_attachments_count,
        .pColorAttachments    = rendering_color_attachments,
        .pDepthAttachment     = (depth_attachment_id != U32_MAX) ? &rendering_depth_attachment : NULL,
        .layerCount           = 1,
        .renderArea           = (VkRect2D) {
            .offset = {drawing_info->offset_x, drawing_info->offset_y},
            .extent = {drawing_info->size_x  , drawing_info->size_y  }
        }
    };
    vulkan_device->cmd_begin_rendering_khr(vulkan_render->command_buffer_render, &rendering_info);

    /* set dynamic states */
    const VkViewport viewport = {
        .x        = (f32)drawing_info->offset_x,
        .y        = (f32)drawing_info->offset_y,
        .width    = (f32)drawing_info->size_x,
        .height   = (f32)drawing_info->size_y,
        .minDepth = drawing_info->min_depth,
        .maxDepth = drawing_info->max_depth
    };
    const VkRect2D scissor = {
        .offset = {drawing_info->offset_x, drawing_info->offset_y},
        .extent = {drawing_info->size_x  , drawing_info->size_y  }
    };

    vkCmdSetViewport(vulkan_render->command_buffer_render, 0, 1, &viewport);
    vkCmdSetScissor(vulkan_render->command_buffer_render, 0, 1, &scissor);

    fail: {}
}

void gpu_render_end_drawing(
    CtxHandle ctx
) {
    GpuContext*         gpu_ctx          = (GpuContext*)ctx;
    const VulkanDevice* vulkan_device    = &gpu_ctx->vulkan_device;
    const VulkanRender* vulkan_render    = &gpu_ctx->vulkan_render;
    
    vulkan_device->cmd_end_rendering_khr(vulkan_render->command_buffer_render);
}

void gpu_render_bind_graphics_pipeline(
    CtxHandle ctx, 
    u32       pipeline_id
) {
    GpuContext*          gpu_ctx        = (GpuContext*)ctx;
    const VulkanShaders* vulkan_shaders = &gpu_ctx->vulkan_shaders;
    const VulkanRender*  vulkan_render  = &gpu_ctx->vulkan_render;

    if(pipeline_id >= vulkan_shaders->pipelines_count) {
        LOG_ERROR("invalid graphics pipeline id: %u/%u", pipeline_id, vulkan_shaders->pipelines_count);
        goto fail;
    }

    vkCmdBindPipeline(
        vulkan_render->command_buffer_render, 
        VK_PIPELINE_BIND_POINT_GRAPHICS, 
        vulkan_shaders->pipelines[pipeline_id]
    );

    fail: {}
}

void gpu_render_push_constants(
    CtxHandle   ctx, 
    const void* constants, 
    u64         size
) {
    GpuContext*          gpu_ctx        = (GpuContext*)ctx;
    const VulkanShaders* vulkan_shaders = &gpu_ctx->vulkan_shaders;
    const VulkanRender*  vulkan_render  = &gpu_ctx->vulkan_render;

    vkCmdPushConstants(
        vulkan_render->command_buffer_render,
        vulkan_shaders->pipeline_layout,
        VK_SHADER_STAGE_ALL,
        0,
        size,
        constants
    );
}

void gpu_render_draw(
    CtxHandle ctx, 
    i32       instance_count, 
    i32       vertex_count
) {
    GpuContext*         gpu_ctx       = (GpuContext*)ctx;
    const VulkanRender* vulkan_render = &gpu_ctx->vulkan_render;

    vkCmdDraw(
        vulkan_render->command_buffer_render,
        vertex_count,
        instance_count,
        0,
        0
    );
}

/* FIX: refactor */
void gpu_render_write_buffer(
    CtxHandle   ctx, 
    u32         buffer_id, 
    const void* data, 
    u64         offset, 
    u64         size
) {
    GpuContext*            gpu_ctx          = (GpuContext*)ctx;
    const VulkanDevice*    vulkan_device    = &gpu_ctx->vulkan_device;
    const VulkanResources* vulkan_resources = &gpu_ctx->vulkan_resources;
    VulkanRender*          vulkan_render    = &gpu_ctx->vulkan_render;

    /* validation */
    const GpuBuffer* buffers               = vulkan_resources->buffers;
    const u32        buffer_count          = vulkan_resources->buffers_count;

    if(buffer_id >= buffer_count) {
        LOG_ERROR("invalid buffer id: %u/%u", buffer_id, buffer_count);
        goto fail;
    }
    if(vulkan_render->sync_transfer_size + size > GPU_SYNC_TRANSFER_SIZE) {
        LOG_ERROR("exceed sync transfer limit: %llu/%llu", vulkan_render->sync_transfer_size + size, (u64)GPU_SYNC_TRANSFER_SIZE);
        goto fail;
    }
    if(vulkan_render->buffer_states[buffer_id].access != VK_ACCESS_NONE) {
        LOG_ERROR("invalid buffer access id: %u/%u", buffer_id, buffer_count);
        goto fail;
    }

    const GpuBuffer* gpu_buffer      = &buffers[buffer_id];
    const GpuBuffer* transfer_buffer = &vulkan_resources->buffer_sync_transfer;

    if(offset + size > gpu_buffer->used_size) {
        LOG_ERROR(
            "trying to write size bigger than buffer: (%llu+%llu)/%llu", 
            offset, size, gpu_buffer->used_size
        );
        goto fail;
    }

    /* direct copy */
    if(vulkan_device->video_memory_device_buffers.memory_map != NULL) {
        /* copy and flush */
        memcpy(
            (u8*)vulkan_device->video_memory_device_buffers.memory_map + gpu_buffer->allocation_offset + offset, 
            data, 
            size
        );

        const VkMappedMemoryRange flush_range = {
            .sType  = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
            .memory = vulkan_device->video_memory_device_buffers.device_memory,
            .offset = (gpu_buffer->allocation_offset     ) & 0xFFFFFFFFFFFFFF00,
            .size   = (gpu_buffer->allocation_size + 0xFF) & 0xFFFFFFFFFFFFFF00
        };

        vkFlushMappedMemoryRanges(vulkan_device->device, 1, &flush_range);
        vulkan_render->sync_transfer_size += size;
    }
    /* host-device transfer */
    else {
        /* copy data */
        memcpy(
            (u8*)vulkan_device->video_memory_host_transfer.memory_map + transfer_buffer->allocation_offset + vulkan_render->sync_transfer_size, 
            data,
            size
        );

        /* transfer */
        const VkBufferCopy buffer_copy = {
            .srcOffset = vulkan_render->sync_transfer_size,
            .dstOffset = offset,
            .size      = size
        };

        vkCmdCopyBuffer(vulkan_render->command_buffer_render, transfer_buffer->buffer, gpu_buffer->buffer, 1, &buffer_copy);
        vulkan_render->buffer_states[buffer_id] = (GpuBufferState) {
            .access = VK_ACCESS_TRANSFER_WRITE_BIT,
            .stage  = VK_PIPELINE_STAGE_TRANSFER_BIT
        };

        vulkan_render->sync_transfer_size += size;
    }

    fail: {}
}

/* COMPUTE */

void gpu_render_compute_barrier(
    CtxHandle          ctx, 
    const ComputeInfo* compute_info
) {
    GpuContext*            gpu_ctx          = (GpuContext*)ctx;
    const VulkanResources* vulkan_resources = &gpu_ctx->vulkan_resources;
    VulkanRender*          vulkan_render    = &gpu_ctx->vulkan_render;

    /* resources */
    const GpuImage*  gpu_images        = vulkan_resources->images;
    const GpuBuffer* gpu_buffers       = vulkan_resources->buffers;
    const u32        gpu_images_count  = vulkan_resources->images_count;
    const u32        gpu_buffers_count = vulkan_resources->buffers_count;

    GpuImageState*  image_states  = vulkan_render->image_states;
    GpuBufferState* buffer_states = vulkan_render->buffer_states;

    /* read write */ {
    const u32* read_write_images_ids    = compute_info->images_read_write;
    const u32* read_write_buffers_ids   = compute_info->buffers_read_write;
    const u32  read_write_images_count  = compute_info->images_read_write_count;
    const u32  read_write_buffers_count = compute_info->buffers_read_write_count;

    /* read write images */
    for(u32 i = 0; i != read_write_images_count; i++) {
        const u32 image_id = read_write_images_ids[i];

        if(image_id >= gpu_images_count) {
            LOG_ERROR("invalid read write image id: %u/%u", image_id, gpu_images_count);
            goto fail;
        }

        const VkImageAspectFlags aspect = (gpu_images[image_id].usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

        /* image read write barrier */
        const VkAccessFlags        src_access = image_states[image_id].access;
        const VkImageLayout        src_layout = image_states[image_id].layout;
        const VkPipelineStageFlags src_stage  = image_states[image_id].stage;

        const VkAccessFlags        dst_access = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        const VkImageLayout        dst_layout = VK_IMAGE_LAYOUT_GENERAL;
        const VkPipelineStageFlags dst_stage  = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

        const VkImageMemoryBarrier read_write_image_barrier = {
            .sType            = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .image            = gpu_images[image_id].image,
            .srcAccessMask    = src_access,
            .oldLayout        = src_layout,
            .dstAccessMask    = dst_access,
            .newLayout        = dst_layout,
            .subresourceRange = (VkImageSubresourceRange) {
                .aspectMask     = aspect,
                .baseArrayLayer = 0,
                .layerCount     = 1,
                .baseMipLevel   = 0,
                .levelCount     = 1
            }
        };

        vkCmdPipelineBarrier(
            vulkan_render->command_buffer_render,
            src_stage,
            dst_stage,
            0,
            0,
            NULL,
            0,
            NULL,
            1,
            &read_write_image_barrier
        );

        image_states[image_id] = (GpuImageState) {
            .access = dst_access,
            .layout = dst_layout,
            .stage  = dst_stage
        };
    }

    /* read write buffers */
    for(u32 i = 0; i != read_write_buffers_count; i++) {
        const u32 buffer_id = read_write_buffers_ids[i];
        
        if(buffer_id >= gpu_buffers_count) {
            LOG_ERROR("invalid read write buffer id: %u/%u", buffer_id, gpu_buffers_count);
            goto fail;
        }

        /* image read write barrier */
        const VkAccessFlags        src_access = buffer_states[buffer_id].access;
        const VkPipelineStageFlags src_stage  = buffer_states[buffer_id].stage;

        const VkAccessFlags        dst_access = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        const VkPipelineStageFlags dst_stage  = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

        const VkBufferMemoryBarrier read_write_buffer_barrier = {
            .sType         = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
            .buffer        = gpu_buffers[buffer_id].buffer,
            .size          = gpu_buffers[buffer_id].used_size,
            .offset        = 0,
            .srcAccessMask = src_access,
            .dstAccessMask = dst_access
        };

        vkCmdPipelineBarrier(
            vulkan_render->command_buffer_render,
            src_stage,
            dst_stage,
            0,
            0,
            NULL,
            1,
            &read_write_buffer_barrier,
            0,
            NULL
        );

        buffer_states[buffer_id] = (GpuBufferState) {
            .access = dst_access,
            .stage  = dst_stage
        };
    }
    }

    /* read only */ {
    const u32* read_only_images_ids    = compute_info->images_read_only;
    const u32* read_only_buffers_ids   = compute_info->buffers_read_only;
    const u32  read_only_images_count  = compute_info->images_read_only_count;
    const u32  read_only_buffers_count = compute_info->buffers_read_only_count;

    /* read write images */
    for(u32 i = 0; i != read_only_images_count; i++) {
        const u32 image_id = read_only_images_ids[i];

        if(image_id >= gpu_images_count) {
            LOG_ERROR("invalid read write image id: %u/%u", image_id, gpu_images_count);
            goto fail;
        }

        const VkImageAspectFlags aspect = (gpu_images[image_id].usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

        /* image read write barrier */
        const VkAccessFlags        src_access = image_states[image_id].access;
        const VkImageLayout        src_layout = image_states[image_id].layout;
        const VkPipelineStageFlags src_stage  = image_states[image_id].stage;

        const VkAccessFlags        dst_access = VK_ACCESS_SHADER_READ_BIT;
        const VkImageLayout        dst_layout = VK_IMAGE_LAYOUT_GENERAL;
        const VkPipelineStageFlags dst_stage  = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

        const VkImageMemoryBarrier read_write_image_barrier = {
            .sType            = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .image            = gpu_images[image_id].image,
            .srcAccessMask    = src_access,
            .oldLayout        = src_layout,
            .dstAccessMask    = dst_access,
            .newLayout        = dst_layout,
            .subresourceRange = (VkImageSubresourceRange) {
                .aspectMask     = aspect,
                .baseArrayLayer = 0,
                .layerCount     = 1,
                .baseMipLevel   = 0,
                .levelCount     = 1
            }
        };

        vkCmdPipelineBarrier(
            vulkan_render->command_buffer_render,
            src_stage,
            dst_stage,
            0,
            0,
            NULL,
            0,
            NULL,
            1,
            &read_write_image_barrier
        );

        image_states[image_id] = (GpuImageState) {
            .access = dst_access,
            .layout = dst_layout,
            .stage  = dst_stage
        };
    }

    /* read write buffers */
    for(u32 i = 0; i != read_only_buffers_count; i++) {
        const u32 buffer_id = read_only_buffers_ids[i];
        
        if(buffer_id >= gpu_buffers_count) {
            LOG_ERROR("invalid read write buffer id: %u/%u", buffer_id, gpu_buffers_count);
            goto fail;
        }

        /* image read write barrier */
        const VkAccessFlags        src_access = buffer_states[buffer_id].access;
        const VkPipelineStageFlags src_stage  = buffer_states[buffer_id].stage;

        const VkAccessFlags        dst_access = VK_ACCESS_SHADER_READ_BIT;
        const VkPipelineStageFlags dst_stage  = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

        const VkBufferMemoryBarrier read_write_buffer_barrier = {
            .sType         = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
            .buffer        = gpu_buffers[buffer_id].buffer,
            .size          = gpu_buffers[buffer_id].used_size,
            .offset        = 0,
            .srcAccessMask = src_access,
            .dstAccessMask = dst_access
        };

        vkCmdPipelineBarrier(
            vulkan_render->command_buffer_render,
            src_stage,
            dst_stage,
            0,
            0,
            NULL,
            1,
            &read_write_buffer_barrier,
            0,
            NULL
        );

        buffer_states[buffer_id] = (GpuBufferState) {
            .access = dst_access,
            .stage  = dst_stage
        };
    }
    }

    fail: {}
}

void gpu_render_bind_compute_pipeline(
    CtxHandle ctx, 
    u32       pipeline_id
) {
    GpuContext*          gpu_ctx        = (GpuContext*)ctx;
    const VulkanShaders* vulkan_shaders = &gpu_ctx->vulkan_shaders;
    const VulkanRender*  vulkan_render  = &gpu_ctx->vulkan_render;

    if(pipeline_id >= vulkan_shaders->pipelines_count) {
        LOG_ERROR("invalid compute pipeline id: %u/%u", pipeline_id, vulkan_shaders->pipelines_count);
        goto fail;
    }

    vkCmdBindPipeline(
        vulkan_render->command_buffer_render, 
        VK_PIPELINE_BIND_POINT_COMPUTE, 
        vulkan_shaders->pipelines[pipeline_id]
    );

    fail: {}
}

void gpu_render_dispatch(
    CtxHandle ctx, 
    u32       groups_x, 
    u32       groups_y, 
    u32       groups_z
) {
    GpuContext*          gpu_ctx        = (GpuContext*)ctx;
    const VulkanRender*  vulkan_render  = &gpu_ctx->vulkan_render;

    vkCmdDispatch(vulkan_render->command_buffer_render, groups_x, groups_y, groups_z);
}
