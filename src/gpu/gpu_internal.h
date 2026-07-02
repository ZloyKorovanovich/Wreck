#ifndef _GPU_INTERNAL_INCLUDED
#define _GPU_INTERNAL_INCLUDED

#include "gpu.h"

#define VK_USE_PLATFORM_WIN32_KHR
#include <windows.h>
#include <vulkan/vulkan.h>

#define GPU_VIRTUAL_RESOURCES_BASE         (0x0000000000000000)
#define GPU_VIRTUAL_SHADERS_BASE           (0x0000000000010000)
#define GPU_VIRTUAL_SHADERS_LIMIT          (0x000000000001F000)
#define GPU_VIRTUAL_RESOURCES_LIMIT        (0x0000000000020000)

#define GPU_MAX_GRAPHICS_ADAPTERS          (8)
#define GPU_MAX_NAME_LENGTH                (32)
#define GPU_MAX_DEVICE_EXTENSIONS          (256)
#define GPU_MAX_DEVICE_SURFACE_FORMATS     (256)
#define GPU_MAX_DEVICE_PRESENT_MODES       (256)
#define GPU_MAX_QUEUE_FAMILIES             (32)
#define GPU_MAX_SWAPCHAIN_IMAGES           (32)

#define GPU_OPTIMAL_SWAPCHAIN_IMAGES       (2)
#define GPU_EMPTY_DESCRIPTOR_TYPE          (VK_DESCRIPTOR_TYPE_SAMPLER)

typedef struct {
    u16                  vendor_id;
    u16                  device_id;
    VkPhysicalDeviceType device_type;
    VkPhysicalDevice     physical_device;
    u32                  render_queue_id;
    u32                  compute_queue_id;
    u32                  transfer_queue_id;
    VkFormat             surface_format;
    VkColorSpaceKHR      surface_color_space;
    VkPresentModeKHR     surface_present_mode;
    u64                  heap_device_size;
    u64                  heap_host_size;
} GraphicsAdapter;

typedef struct {
    VkBufferUsageFlags usage;
    u64                allocation_offset;
    u64                allocation_size;
    u64                used_size;
    VkBuffer           buffer;
} GpuBuffer;

typedef struct {
    VkImageUsageFlags  usage;
    VkFormat           format;
    u64                allocation_offset;
    u64                allocation_size;
    u32                size_x;
    u32                size_y;
    VkImage            image;
    VkImageView        view;
    VkImageAspectFlags aspect;
} GpuImage;

typedef struct {
    VkDeviceMemory memory;
    u64            offset;
    u64            limit;
} GpuMemorySection;

typedef struct {
    VkAccessFlags        access;
    VkImageLayout        layout;
    VkPipelineStageFlags stage;
} GpuImageState;

typedef struct {
    VkAccessFlags        access;
    VkPipelineStageFlags stage;
} GpuBufferState;

typedef struct {
    VkDeviceMemory        device_memory;
    void*                 memory_map;
    u32                   type_id;
    VkMemoryPropertyFlags type_flags;
} GpuVideoMemoryAllocation;

/* CONTEXT STRUCTS */

typedef struct {
    HWND                     window;
    VkInstance               instance;
    VkSurfaceKHR             surface;
    VkDebugUtilsMessengerEXT debug_messenger;
    u32                      available_devices_count;
    GraphicsAdapter          available_devices[GPU_MAX_GRAPHICS_ADAPTERS];
    char                     window_name[GPU_MAX_NAME_LENGTH];
} VulkanObjects;

typedef struct {
    VkDevice                   device;
    const GraphicsAdapter*     adapter;
    VkQueue                    queue_render;
    VkQueue                    queue_compute;
    VkQueue                    queue_transfer;
    VkCommandPool              command_pool_render;
    VkCommandPool              command_pool_compute;
    VkCommandPool              command_pool_transfer;
    PFN_vkCmdBeginRenderingKHR cmd_begin_rendering_khr;
    PFN_vkCmdEndRenderingKHR   cmd_end_rendering_khr;

    GpuVideoMemoryAllocation   video_memory_device_buffers;
    GpuVideoMemoryAllocation   video_memory_device_images;
    GpuVideoMemoryAllocation   video_memory_host_transfer;
} VulkanDevice;

typedef struct {
    VkSwapchainKHR swapchain;
    VkImage        swapchain_images[GPU_MAX_SWAPCHAIN_IMAGES];
    VkImageView    swapchain_views [GPU_MAX_SWAPCHAIN_IMAGES];
    u32            swapchain_images_count;
    u32            swapchain_x;
    u32            swapchain_y;

    VkSampler      sampler_linear_repeat;
    VkSampler      sampler_linear_clamp;
    VkSampler      sampler_nearest_repeat;
    VkSampler      sampler_nearest_clamp;    

    GpuBuffer      buffer_sync_transfer;

    GpuBuffer      buffers[GPU_MAX_STATIC_BUFFERS];
    GpuImage       images [GPU_MAX_STATIC_IMAGES ];
    u32            buffers_count;
    u32            images_count;
} VulkanResources;

typedef struct {
    VkDescriptorPool      descriptor_pool;
    VkDescriptorSet       descriptor_sets   [GPU_DESCRIPTOR_SET_COUNT];
    VkDescriptorSetLayout descriptor_layouts[GPU_DESCRIPTOR_SET_COUNT];
    /* descriptor_types[binding_id + set_id * GPU_MAX_BINDINGS_PER_DESCRIPTOR] */
    VkDescriptorType      descriptor_types  [GPU_DESCRIPTOR_SET_COUNT * GPU_MAX_BINDINGS_PER_DESCRIPTOR];

    VkPipelineLayout      pipeline_layout;
    VkPipeline*           pipelines;
    u32                   pipelines_count;
} VulkanShaders;

typedef struct {
    VkCommandBuffer command_buffer_render;
    VkFence         fence_frame;
    VkSemaphore     semaphore_image_available;
    VkSemaphore     semaphores_images_finished[GPU_MAX_SWAPCHAIN_IMAGES];

    u32             swapchain_image_id;
    VkImage         swapchain_image;
    VkImageView     swapchain_image_view;

    GpuImageState   image_states [GPU_MAX_STATIC_IMAGES ];
    GpuBufferState  buffer_states[GPU_MAX_STATIC_BUFFERS];

    u64             sync_transfer_size;
} VulkanRender;

typedef struct {
    void*           resources_base;
    void*           resources_limit;

    VulkanObjects   vulkan_objects;
    VulkanDevice    vulkan_device;
    VulkanResources vulkan_resources;
    VulkanShaders   vulkan_shaders;
    VulkanRender    vulkan_render;
} GpuContext;


i32 create_swapchain(
    VkDevice            device,
    VkPhysicalDevice    physical_device,
    VkSurfaceKHR        surface,
    VkFormat            surface_format,
    VkColorSpaceKHR     surface_color_space,
    VkSwapchainKHR*     swapchain,
    u32*                swapchain_images_count,
    u32*                swapchain_x,
    u32*                swapchain_y,
    VkImage*            swapchain_images,
    VkImageView*        swapchain_image_views
);

#endif
