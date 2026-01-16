#ifndef VULKAN_INCLUDED
#define VULKAN_INCLUDED

#include "../main.h"
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

typedef enum {
    DEVICE_MODEL_NONE = 0,
    DEVICE_MODEL_DESCRETE = 1, /* device local is not visible to host, use staging buffers */
    DEVICE_MODEL_INTEGRATED = 2 /* device local is host visible, do not use staging buffers */
} DeviceModel;

struct VulkanContext {
    /* vulkan and glfw objects */
    GLFWwindow* window;
    VkInstance instance;
    VkSurfaceKHR surface;
    VkPhysicalDevice physical_device;
    VkDevice device;
    DeviceModel device_model;
    /* callback for logs and errors */
    MsgCallback_pfn msg_callback;
    /* queue objects */
    VkQueue render_queue;
    VkQueue compute_queue;
    VkQueue transfer_queue;
    /* queue family indices */
    u32 render_queue_id;
    u32 compute_queue_id;
    u32 transfer_queue_id;
    /* dynamic rendering ext */
    PFN_vkCmdBeginRenderingKHR cmd_begin_rendering;
    PFN_vkCmdEndRenderingKHR cmd_end_rendering;
    /* used only if debug flag on */
    PFN_vkCreateDebugUtilsMessengerEXT create_debug_messenger;
    PFN_vkDestroyDebugUtilsMessengerEXT destroy_debug_messenger;
    VkDebugUtilsMessengerEXT debug_messenger;
};

#endif
