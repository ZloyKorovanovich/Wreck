#ifndef _VULKAN_INCLUDED
#define _VULKAN_INCLUDED

#include <base.h>

typedef void *VulkanHandle;

typedef enum {
    VULKAN_IN_FLAGS_NONE = 0x0,
    VULKAN_IN_FLAG_DEBUG = 0x1,
    VULKAN_IN_FLAG_RESIZE = 0x2,
    VULKAN_IN_PROTECT_MEMORY = 0x4
} VulkanInFlags;

typedef enum {
    VULKAN_MEMORY_MODEL_NONE = 0,
    VULKAN_MEMORY_MODEL_HOST_DEVICE = 1,
    VULKAN_MEMORY_MODEL_FUSED_DEVICE = 2,
    VULKAN_MEMORY_MODEL_FUSED_HOST = 3
} VulkanMemoryModel;

typedef enum {
    VULKAN_DEVICE_MODEL_NONE = 0,
    VULKAN_DEVICE_MODEL_DESCRETE = 1,
    VULKAN_DEVICE_MODEL_INTEGRATED = 2
} VulkanDeviceModel;

typedef enum {
    VULKAN_OUT_FLAGS_NONE = 0x0,
    VULKAN_OUT_FLAG_ASYNC_TRANSFER = 0x1,
    VULKAN_OUT_FLAG_ASYNC_COMPUTE = 0x2
} VulkanOutFlags;

typedef struct {
    const Segment *segment;
    const char* name;
    MsgCallback_pfn msg_callback;
    VulkanInFlags flags;
    u32 x;
    u32 y;
} CreateVulkanIn;

typedef struct {
    VulkanOutFlags flags;
    VulkanMemoryModel memory_model;
    VulkanDeviceModel device_model;
} CreateVulkanOut;

VulkanHandle createVulkan(const CreateVulkanIn *input, CreateVulkanOut *output);
b32 destroyVulkan(VulkanHandle vulkan);

b32 createVulkanDynamic(VulkanHandle vulkan);
b32 destroyVulkanDynamic(VulkanHandle vulkan);
b32 runVulkanLoop(VulkanHandle vulkan);

#define REQUIRED_DEVICE_VRAM_SIZE (1024llu * 1024llu * 1024llu * 2llu)
#define REQUIRED_HOST_VRAM_SIZE (1024llu * 1024llu * 1024llu * 4llu)
#define MAX_PHYSICAL_DEVICE_COUNT (4)


#define INCLUDE_VULKAN_INTERNAL
#ifdef INCLUDE_VULKAN_INTERNAL
    
    #ifdef _WIN32
        #define VK_USE_PLATFORM_WIN32_KHR
    #endif
    #include <vulkan/vulkan.h>

    /* on all platforms we cover window is a handle, see [WINDOW SYSTEMS] */
    typedef void *Window;

    typedef struct {
        VulkanInFlags flags;
        MsgCallback_pfn msg_callback;
        Window window;
        VkInstance instance;
        VkSurfaceKHR surface;
        VkDebugUtilsMessengerEXT debug_messenger;
        /* debug extension */
        PFN_vkCreateDebugUtilsMessengerEXT create_debug_messenger;
        PFN_vkDestroyDebugUtilsMessengerEXT destroy_debug_messenger;
    } VulkanObjects;

    typedef struct {
        VkPhysicalDevice physical_device;
        VulkanDeviceModel device_model;
        VulkanMemoryModel memory_model;

        VkDevice device;
        VkQueue render_queue;
        VkQueue transfer_queue;
        VkQueue compute_queue;

        u32 render_queue_id;
        u32 transfer_queue_id;
        u32 compute_queue_id;

        VkColorSpaceKHR screen_color_space;
        VkFormat screen_color_format;
        VkFormat color_format;
        VkFormat depth_format;
        VkPresentModeKHR present_mode;

    } VulkanDevice;

    /* DYNAMIC */

    #define MAX_SWAPCHAIN_IMAGE_COUNT (16)
    #define MAX_RES_X (5120)
    #define MAX_RES_Y (2880)
    #define SCREEN_MAX_PIXEL_COUNT (5120 * 2880)

    typedef struct {
        VkDeviceMemory memory;
        u64 size;
        u32 heap_id;
        u32 type_id;
    } VramAllocation;

    typedef struct {
        VkPhysicalDeviceMemoryProperties memory_properties;
        u32 vram_allocation_count;
    } VulkanMemory;

    
    typedef struct {
        VkSwapchainKHR swapchain;
        VramAllocation vram_allocation;
        u32 swapchain_image_count;
        u16 screen_size_x;
        u16 screen_size_y;
        VkImage swapchain_images[MAX_SWAPCHAIN_IMAGE_COUNT];
        VkImageView swapchain_views[MAX_SWAPCHAIN_IMAGE_COUNT];
        VkImage depth_image;
        VkImage color_image;
        VkImageView depth_view;
        VkImageView color_view;
    } VulkanScreen;

    typedef struct {
        /* segment begin, static begin */
        void *segment_begin;
        void *vulkan_objects_begin;
        void *vulkan_objects_end_device_begin;
        void *vulkan_device_end;

        void *static_end_dynamic_begin;
        /* static end, dynamic begin */
        void *vulkan_textures_begin;
        void *vulkan_textures_end;
        /* dynamic end, segment end */
        void *segment_end;
    } VulkanSegment;

    #define STATIC_PART_SIZE (ALIGN(sizeof(VulkanSegment) + sizeof(VulkanObjects) + sizeof(VulkanDevice), MEMORY_PAGE_SIZE))
    #define MIN_VULKAN_SEGMENT_SIZE (ALIGN(STATIC_PART_SIZE, ALLOCATION_GRANULARITY))


    /*  Segment layout representation:
    +----------------+---------------+-----------------+---------------+---------------+--------+
    | VULKAN OBJECTS | VULKAN DEVICE |                 | VULKAN MEMORY | VULKAN SCREEN |        |
    +----------------+---------------+-----------------+---------------+---------------+--------+
                                            static end, dynamic begin 
                                                (page size aligned)                                 
                                                
    Once static part is created and written it may transition to read-only protection mode, if flag is set.
    The protection is removed after destruction of vulkan.                                                  */

    


#endif /* #ifdef INCLUDE_VULKAN_INTERNAL */

#endif /* #ifndef _VULKAN_INCLUDED */
