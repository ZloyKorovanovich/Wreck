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
    /* dynamic rendering ext */
    PFN_vkCmdBeginRenderingKHR cmd_begin_rendering;
    PFN_vkCmdEndRenderingKHR cmd_end_rendering;
    /* used only if debug flag on */
    PFN_vkCreateDebugUtilsMessengerEXT create_debug_messenger;
    PFN_vkDestroyDebugUtilsMessengerEXT destroy_debug_messenger;
    VkDebugUtilsMessengerEXT debug_messenger;
};


typedef struct {
    u64 size;
    u64 offset;
} VramResource;

typedef struct {
    u64 memory_size;
    u32 memory_type_bits;
    u32 allocations_count; /* for debug */
} VramBlock;

typedef struct {
    VkColorSpaceKHR color_space;
    VkFormat color_format;
    VkFormat depth_format;
    VkPresentModeKHR present_mode;
} RenderSettings;

typedef struct {
    VkExtent2D extent;
    VkViewport viewport;
} ScreenSettings;

/* context of render queue */
struct RenderContext {
    VulkanContext *vulkan_context;
    /* allocators */
    Arena resource_arena;
    /* settings */
    RenderSettings render_settings;
    ScreenSettings screen_settings;
    /* screen objects */
    VkSwapchainKHR swapchain;
    VkImage *swapchain_images;
    VkImageView *swapchain_image_views;
    u32 swapchain_image_count;

    VkImage depth_image;
    VkImageView depth_image_view;
    VramResource depth_vram;
    /* callback for logs and errors */
    MsgCallback_pfn msg_callback;
    /* user control */
    RenderUpdate_pfn update_callback;
    /* memory */
    VramBlock images_block;
    VkDeviceMemory images_memory;
};

#endif
