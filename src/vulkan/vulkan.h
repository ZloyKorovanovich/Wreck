#ifndef VULKAN_INCLUDED
#define VULKAN_INCLUDED

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include "../main.h"

typedef enum {
    DEVICE_MODEL_NONE = 0,
    DEVICE_MODEL_DESCRETE = 1, /* device local is not visible to host, use staging buffers */
    DEVICE_MODEL_INTEGRATED = 2 /* device local is host visible, do not use staging buffers */
} DeviceModel;

typedef struct {
    u64 offset;
    u64 size;
} VramRegion;

typedef struct {
    u64 size;
    u64 aligment;
    u32 memory_type_bits;
    u32 mandatory_flags;
    u32 restricted_flags;
} VramInfo;

typedef struct {
    VkDeviceMemory memory;
    u64 size;
    u32 heap_id;
    u32 memory_id;
} Vram;

/* context of vulkan */
struct VulkanContext {
    /* vulkan and glfw objects */
    GLFWwindow *window;
    VkInstance instance;
    VkSurfaceKHR surface;
    VkPhysicalDevice physical_device;
    VkDevice device;
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
    /* memory*/
    DeviceModel device_model;
    VkPhysicalDeviceMemoryProperties memory_properties;
    /* dynamic rendering ext */
    PFN_vkCmdBeginRenderingKHR cmd_begin_rendering;
    PFN_vkCmdEndRenderingKHR cmd_end_rendering;
    /* used only if debug flag on */
    PFN_vkCreateDebugUtilsMessengerEXT create_debug_messenger;
    PFN_vkDestroyDebugUtilsMessengerEXT destroy_debug_messenger;
    VkDebugUtilsMessengerEXT debug_messenger;
};

b32 allocateVram(VulkanContext *vulkan_context, const VramInfo *info, Vram *vram);
void freeVram(VulkanContext *vulkan_context, Vram *vram);

#define MAX_SWAPCHAIN_IMAGES (16)

typedef struct {
    VkColorSpaceKHR color_space;
    VkFormat color_format;
    VkFormat depth_format;
    VkPresentModeKHR present_mode;
    VkExtent2D extent;
} RenderSettings;

typedef struct {
    char file[256];
    VkBuffer vertex_buffer;
    u32 vertex_count;
} Mesh;

typedef struct {
    u32 swapchain_image_count;
    VkSwapchainKHR swapchain;
    VkImage *swapchain_images;
    VkImageView *swapchain_image_views;
    VkImage depth_image;
    VkImageView depth_image_view;
    VramRegion depth_vram_region;
} ScreenImages;

/* TEXTURE */
/* MESH */
/* DYNAMIC MESH */
/* STORAGE BUFFER */

/* context of render queue */
struct RenderContext {
    VulkanContext *vulkan_context;
    Arena resource_arena;
    /* screen */
    RenderSettings render_settings;
    ScreenImages screen_images;
    /* callback for logs and errors */
    MsgCallback_pfn msg_callback;
    /* user control */
    RenderUpdate_pfn update_callback;
    /* resources */
    VkDescriptorPool descriptor_pool;
    /* memory */
    Vram images_vram;
    /* commands */
    VkCommandPool command_pool;
};

struct RenderCmd {
    RenderContext *render_context;
    VkCommandBuffer command_buffer;
};

#endif
