#include "vulkan.h"

/*======================================================================
    GRAPHICS
  ======================================================================*/

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
