#ifndef _VK_INCLUDED
#define _VK_INCLUDED

#include "../main.h"

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

typedef enum {
    DEVICE_TYPE_NONE = 0,
    DEVICE_TYPE_DESCRETE = 1,
    DEVICE_TYPE_INTEGRATED = 2
} DeviceType;

typedef struct {
    VkDevice device;
    DeviceType device_type;
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

i32 renderRun(const VulkanContext* vulkan_context, const RenderSettings* settings, MsgCallback_pfn msg_callback);


/* creates vram arena */
i32 vramArenaInit(const VulkanContext* vulkan_context);
/* destroys vram arena */
void vramArenaTemrinate(void);
/* allocate on vram arena */
VkDeviceMemory vramArenaAllocate(const VkMemoryRequirements* requirements, u32 positive_flags, u32 negative_flags);

#endif

