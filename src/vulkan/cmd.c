#include "vulkan.h"

/*======================================================================
    GRAPHICS
  ======================================================================*/

void *cmdWriteHostUniformBuffer(RenderCmd *cmd) {
    RenderContext *render_context = cmd->render_context;
    /* we always write to host uniform buffer, so lest check if it exists */
    return (render_context->buffers.host_uniform_buffer) ? (u8 *)render_context->buffers_host_vram_map + render_context->buffers.host_uniform_region.offset : NULL;
}

void cmdTransferUniformBuffer(RenderCmd *cmd, u64 size) {
    RenderContext *render_context = cmd->render_context;
    /* we always write to host uniform buffer, so lest check if it exists */
    if(render_context->buffers.device_uniform_buffer) {
        const VkBufferCopy buffer_copy = {
            .srcOffset = 0,
            .size = size
        };
        vkCmdCopyBuffer(cmd->command_buffer, render_context->buffers.host_uniform_buffer, render_context->buffers.device_uniform_buffer, 1, &buffer_copy);
        
        const VkBufferMemoryBarrier buffer_barrier = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
            .buffer = render_context->buffers.device_uniform_buffer,
            .offset = 0,
            .size = size,
            .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT
        };
        vkCmdPipelineBarrier(
            cmd->command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,
            0, NULL,
            1, &buffer_barrier,
            0, NULL
        );
    }
}

void cmdPushContsants(RenderCmd *cmd, const void *constants, u64 size) {
    RenderContext *render_context = cmd->render_context;
    vkCmdPushConstants(cmd->command_buffer, render_context->shader_programs.full_pipeline_layout, VK_SHADER_STAGE_ALL, 0, size, constants);
}

void cmdBeginRendering(RenderCmd *cmd, u32 color_count, u32 *color_ids, u32 depth_id) {
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

void cmdEndRendering(RenderCmd *cmd) {
    cmd->render_context->vulkan_context->cmd_end_rendering(cmd->command_buffer);
    cmd->color_attachment_count = 0;
    cmd->use_depth_atachment = FALSE;
}

void cmdDrawProcedural(RenderCmd *cmd, u32 program_id, u32 vertex_count, u32 instance_count) {
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

void cmdDrawMesh(RenderCmd *cmd, u32 program_id, u32 mesh_id, u32 instance_count) {
    const RenderContext *render_context = cmd->render_context;

    ShaderProgram *last_program = cmd->last_shader_program;
    ShaderProgram *new_program = &render_context->shader_programs.shader_programs[program_id];
    /* if pipeline changed, bind new pipeline */
    if(last_program != new_program) {
        vkCmdBindPipeline(cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, new_program->pipeline);
        cmd->last_shader_program = new_program;
    }

    RenderMesh *last_mesh = cmd->last_render_mesh;
    RenderMesh *new_mesh = &render_context->render_meshes.render_meshes[mesh_id];
    /* if mesh chenaged, bind new mesh */
    if(last_mesh != new_mesh) {
        vkCmdBindVertexBuffers(cmd->command_buffer, 0, 1, &new_mesh->vertex_buffer, &(VkDeviceSize){0});
        vkCmdBindIndexBuffer(cmd->command_buffer, new_mesh->index_buffer, 0, VK_INDEX_TYPE_UINT16);
        cmd->last_render_mesh = new_mesh;
    }
    /* draw call */
    vkCmdDrawIndexed(cmd->command_buffer, new_mesh->index_count, instance_count, 0, 0, 0);
}
