#define INCLUDE_VULKAN_INTERNAL
#include "vulkan.h"

void
cmdBeginRendering(
    VulkanRenderCmd *render_cmd, 
    u32 color_target_count, 
    const u32 *color_target_ids, 
    u32 depth_target_id
) {
    VkRenderingAttachmentInfo color_attachments[MAX_COLOR_TARGET_COUNT] = {0};
    VkRenderingAttachmentInfo depth_attachment = (VkRenderingAttachmentInfo){0};
    VkRenderingInfo rendering_info = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,
        .renderArea = (VkRect2D){.offset = {0, 0}, .extent = {U32_MAX, U32_MAX}},
        .colorAttachmentCount = color_target_count,
        .pColorAttachments = color_attachments,
        .pDepthAttachment = (depth_target_id != U32_MAX) ? &depth_attachment : NULL
    };

    for(u32 i = 0; i < color_target_count; i++) {
        if(color_target_ids[i] == IMAGE_SCREEN_COLOR_ID) {
            color_attachments[i] = (VkRenderingAttachmentInfo) {
                .clearValue = (VkClearValue){0},
                .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .imageView = render_cmd->screen_color_image_view
            };
            rendering_info.renderArea.offset.x = MAX(rendering_info.renderArea.offset.x, render_cmd->screen_render_area.offset.x);
            rendering_info.renderArea.offset.y = MAX(rendering_info.renderArea.offset.y, render_cmd->screen_render_area.offset.y);
            rendering_info.renderArea.extent.width = MIN(rendering_info.renderArea.extent.width, render_cmd->screen_render_area.extent.width);
            rendering_info.renderArea.extent.height = MIN(rendering_info.renderArea.extent.height, render_cmd->screen_render_area.extent.height);
            continue;
        }
    }

    render_cmd->begin_rendering(render_cmd->command_buffer, &rendering_info);
}

void
cmdEndRendering(
    VulkanRenderCmd *render_cmd
) {
    render_cmd->end_rendering(render_cmd->command_buffer);
}
