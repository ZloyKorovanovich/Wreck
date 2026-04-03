#ifndef _VULKAN_INCLUDED
#define _VULKAN_INCLUDED

#include "render.h"

#ifdef _WIN32
    #define VK_USE_PLATFORM_WIN32_KHR
    typedef HWND WindowHandle;
#endif
#include <vulkan/vulkan.h>

#define MAX_SCAN_GRAPHICS_DEVICE_COUNT  (8)
#define MAX_SCAN_QUEUE_FAMILIES_COUNT   (32)
#define MAX_SCAN_DEVICE_EXTENSION_COUNT (256)
#define MAX_SCAN_SURFACE_PRESENT_MODES  (256)
#define MAX_SCAN_SURFACE_FORMATS        (256)
#define MAX_SWAPCHAIN_IMAGES            (32)

#define MAX_DESCRIPTOR_SET_COUNT        (3)
#define MAX_BINDINGS_PER_DESCRIPTOR     (16)

typedef struct {
    VkDeviceMemory device_memory;
    u64            size;
    u32            memory_type;
    u32            memory_heap;
} VramAllocation;

typedef enum {
    VRAM_TYPE_NONE        = 0,
    VRAM_TYPE_DEVICE      = 1,
    VRAM_TYPE_DEVICE_HOST = 2,
    VRAM_TYPE_HOST        = 3
} VramType;

typedef struct {
    VkPhysicalDevice physical_device;
    /* device identifiers */
    char                 device_name[VK_MAX_PHYSICAL_DEVICE_NAME_SIZE];
    u32                  device_id;
    VkPhysicalDeviceType device_type;
    /* queue families */
    u32                  render_family_id;
    u32                  compute_family_id;
    u32                  transfer_family_id;
    /* memory */
    u64                  device_heaps_size;
    u64                  host_heaps_size;
    u32                  device_heap_count;
    u32                  host_heap_count;
    /* surface */
    VkPresentModeKHR     surface_present_mode;
    VkColorSpaceKHR      surface_color_space;
    VkFormat             surface_color_format;
} AvailableDevice;

typedef struct {
    VkPipelineLayout pipeline_layout;
    VkPipeline*      pipelines;
    u32              pipeline_count;
} VulkanShaders;

typedef struct {
    VkDescriptorPool      descriptor_pool;
    VkDescriptorSetLayout set_layouts   [MAX_DESCRIPTOR_SET_COUNT];
    VkDescriptorSet       sets          [MAX_DESCRIPTOR_SET_COUNT];
} VulkanBindings;

typedef struct {
    u64                      ctx_hash;
    const char*              app_name;
    WindowHandle             window;
    VkInstance               instance;
    VkSurfaceKHR             surface;
    VkDevice                 device;
    VkQueue                  render_queue;
    VkQueue                  compute_queue;
    VkQueue                  transfer_queue;
    VkCommandPool            render_command_pool;
    VkCommandPool            compute_command_pool;
    VkCommandPool            transfer_command_pool;
    VkDebugUtilsMessengerEXT debug_messenger;
    /* screen data */
    VkSwapchainKHR           swapchain;
    VkImage                  swapchain_images[MAX_SWAPCHAIN_IMAGES];
    VkImageView              swapchain_views [MAX_SWAPCHAIN_IMAGES];
    u32                      swapchain_image_count;
    /* memory */
    Mutex                    memory_mutex;
    VkMemoryType             memory_types[VK_MAX_MEMORY_TYPES];
    VkMemoryHeap             memory_heaps[VK_MAX_MEMORY_HEAPS];
    u32                      memory_type_count;
    /* resources */
    VulkanShaders            shaders;
    VulkanBindings           bindings;
    /* backup */
    u32                      current_device_id;
    u32                      available_device_count;
    AvailableDevice          available_devices[MAX_SCAN_GRAPHICS_DEVICE_COUNT];
} VulkanCtx;

b32 allocateVram(VulkanCtx* vk_ctx, VramAllocation* allocation, u64 size, u32 type_bits, VramType type);
void freeVram(VulkanCtx* vk_ctx, VramAllocation* allocation);

#ifdef DEBUG_LOG
    #define allocateVram(ctx, allocation, size, bits, type)   ({                                                                                         \
        b32 __res = (allocateVram)(ctx, allocation, size, bits, type);                                                                                   \
        printf(":* allocate vram ctx: %p memory: %p size: %llu bits: %u type: %u " DEBUG_TRACE "\r\n", ctx, allocation->device_memory, size, bits, type);\
        _res;                                                                                                                                            \
    })
    #define freeVram(ctx, allocation)                         ({                                                                                                              \
        printf(":* free vram ctx: %p memory: %p size: %llu heap_id: %u"            DEBUG_TRACE "\r\n", ctx, allocation->device_memory, allocation->size, allocation->heap_id);\
        (freeVram)(ctx, allocation);                                                                                                                                          \
    })
#else
    #define allocateVram(ctx, allocation, size, bits, type) (allocateVram)(ctx, allocation, size, bits, type)
    #define freeVram(ctx, allocation)                       (freeVram)(ctx, allocation)
#endif


#endif
