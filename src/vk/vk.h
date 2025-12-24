#ifndef _VK_INCLUDED
#define _VK_INCLUDED

#include "../main.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

typedef struct {
    VkDevice device;
    /* extension functions */
    PFN_vkCmdBeginRenderingKHR cmd_begin_rendering_khr;
    PFN_vkCmdEndRenderingKHR cmd_end_rendering_khr;
    /* if queue is not okay its NULL */
    VkQueue render_queue;
    VkQueue compute_queue;
    VkQueue transfer_queue;
    /* if queue family index is not okay its 0xffffffff*/
    u32 render_family_id;
    u32 compute_family_id;
    u32 transfer_family_id;
    /* you rarely need these */
    VkSurfaceKHR surface;
    VkPhysicalDevice physical_device;
    VkInstance instance;
    GLFWwindow* window;
    /* local use */
    VkDebugUtilsMessengerEXT debug_messenger;
    PFN_vkCreateDebugUtilsMessengerEXT ext_create_debug_utils_messenger;
    PFN_vkDestroyDebugUtilsMessengerEXT ext_destroy_debug_utils_messenger;
} VulkanContext;

i32 renderRun(const VulkanContext* vulkan_context, const RenderSettings* settings, msg_callback_pfn msg_callback);

#endif

