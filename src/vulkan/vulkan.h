#ifndef _VULKAN_INCLUDED
#define _VULKAN_INCLUDED

#include <base.h>

typedef struct VulkanSegment VulkanSegment;

typedef enum {
    VULKAN_IN_FLAGS_NONE = 0x0,
    VULKAN_IN_FLAG_DEBUG = 0x1,
    VULKAN_IN_FLAG_RESIZE = 0x2
} VulkanFlags;

/* not in use yet */
typedef enum {
    VULKAN_SEGMENT_STATES_NONE               = 0x0,
    VULKAN_SEGMENT_STATE_OBJECTS_CREATED     = 0x1,
    VULKAN_SEGMENT_STATE_DEVICE_CREATED      = 0x2,
    VULKAN_SEGMENT_STATE_MEMORY_CREATED      = 0x4,
    VULKAN_SEGMENT_STATE_SCREEN_CREATED      = 0x8,
    VULKAN_SEGMENT_STATE_DESCRIPTORS_CREATED = 0x10,
    VULKAN_SEGMENT_STATE_PIPELINES_CREATED   = 0x20,
    VULKAN_SEGMENT_STATE_BUFFERS_CREATED     = 0x40
} VulkanSegmentStates;

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
    VulkanFlags flags;
    u32 x;
    u32 y;
} CreateVulkanIn;

typedef struct {
    VulkanOutFlags flags;
    VulkanMemoryModel memory_model;
    VulkanDeviceModel device_model;
} CreateVulkanOut;

VulkanSegment *createVulkan(const CreateVulkanIn *input, CreateVulkanOut *output);
b32 destroyVulkan(VulkanSegment *vulkan);


#define SHADER_VERTEX_ENTRY "vertexMain"
#define SHADER_FRAGMENT_ENTRY "fragmentMain"
#define SHADER_COMPUTE_ENTRY "computeEntry"

#define MAX_STORAGE_BUFFER_COUNT (16)
#define MAX_PIPELINE_COUNT (1024)

typedef struct {
    f32 position[4];
    f32 normal[4];
    f32 texcoord[4];
} Vertex;

typedef struct {
    Vertex *vertices;
    u16 *indices;
    u32 vertex_count;
    u32 index_count;
} VulkanMeshInfo;

typedef enum {
    VULKAN_SHADER_FLAGS_NONE = 0x0,
    VULKAN_SHADER_FLAG_USE_VERTEX_POSITION = 0x1,
    VULKAN_SHADER_FLAG_USE_VERTEX_NORMAL = 0x2,
    VULKAN_SHADER_FLAG_USE_VERTEX_TEXCOORD = 0x4,
    VULKAN_SHADER_FLAG_TRANSPARENT = 0x8
} VulkanShaderFlags;

typedef enum {
    VULKAN_BUFFER_FLAGS_NONE = 0x0,
    VULKAN_BUFFER_FLAG_ASYNC_TRANSFER = 0x1
} VulkanBufferFlags;

typedef struct {
    const char *name;
    const void *vertex_spv;
    const void *fragment_spv;
    u32 vertex_spv_size;
    u32 fragment_spv_size;
    VulkanShaderFlags flags;
} VulkanGraphicsPipelineInfo;

typedef struct {
    const char *name;
    const void *compute_spv;
    u32 compute_spv_size;
    VulkanShaderFlags flags;
} VulkanComputePipelineInfo;

typedef struct {
    const VulkanGraphicsPipelineInfo *graphics_shaders;
    const VulkanComputePipelineInfo *compute_shaders; 
    u32 graphics_shader_count;
    u32 compute_shader_count;
} CreateVulkanDynamicIn;

typedef struct {
    VulkanBufferFlags flags;
    u64 size;
    void *data;
} VulkanBufferInfo;

typedef struct {
    u32 storage_buffer_count;
    const VulkanBufferInfo *uniform_buffer_info;
    const VulkanBufferInfo *storage_buffer_infos;
} CreateVulkanBuffersIn;

typedef struct {
    void *uniform_buffer_address;
    void **storage_buffer_addresses;
} CreateVulkanBuffersOut;

typedef struct VulkanRenderCmd VulkanRenderCmd;
typedef b32 (*VulkanLoop_pfn) (VulkanRenderCmd *cmd);

b32 createVulkanDynamic(VulkanSegment *vulkan, const CreateVulkanDynamicIn *input);
b32 destroyVulkanDynamic(VulkanSegment *vulkan);
b32 createVulkanBuffers(VulkanSegment *vulkan, CreateVulkanBuffersIn *input, CreateVulkanBuffersOut *output);
b32 runVulkanLoop(VulkanSegment *vulkan, VulkanLoop_pfn loop_callback, MsgCallback_pfn msg_callback);

#define REQUIRED_DEVICE_VRAM_SIZE (1024llu * 1024llu * 1024llu * 2llu)
#define REQUIRED_HOST_VRAM_SIZE (1024llu * 1024llu * 1024llu * 4llu)
#define MAX_PHYSICAL_DEVICE_COUNT (4)


#define IMAGE_SWAPCHAIN_ID (0xfffffffe)
#define IMAGE_SCREEN_COLOR_ID (0xfffffffd)
#define IMAGE_SCREEN_DEPTH_ID (0xfffffffc)
#define MAX_COLOR_TARGET_COUNT (8)

#ifdef INCLUDE_VULKAN_INTERNAL
    
    #ifdef _WIN32
        #define VK_USE_PLATFORM_WIN32_KHR
    #endif
    #include <vulkan/vulkan.h>

    /* on all platforms we cover window is a handle, see [WINDOW SYSTEMS] */
    typedef void *Window;

    typedef struct {
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
                
        VkCommandPool render_command_pool;
        VkCommandPool transfer_command_pool;
        VkCommandPool compute_command_pool;

        u32 render_queue_id;
        u32 transfer_queue_id;
        u32 compute_queue_id;

        VkColorSpaceKHR screen_color_space;
        VkFormat screen_color_format;
        VkFormat color_format;
        VkFormat depth_format;
        VkPresentModeKHR present_mode;

        PFN_vkCmdBeginRenderingKHR begin_rendering_khr;
        PFN_vkCmdEndRenderingKHR end_rendering_khr;
    } VulkanDevice;

    /* DYNAMIC */

    #define MAX_SWAPCHAIN_IMAGE_COUNT (16)
    #define MAX_RES_X (5120)
    #define MAX_RES_Y (2880)
    #define SCREEN_MAX_PIXEL_COUNT (5120 * 2880)
    
    typedef enum {
        DESCRIPTOR_SET_GENERAL_ID   = 0,
        DESCRIPTOR_SET_BUFFERS_ID   = 1,
        DESCRIPTOR_SET_COUNT
    } DescriptorSetIds;
    
    typedef struct {
        u64 size;
        u32 memory_bits;
        u32 mandatory_flags;
        u32 restricted_flags;
        u32 positive_flags;
        u32 negative_flags;
    } VramAllocationRequest;

    typedef struct {
        VkDeviceMemory memory;
        u64 size;
        u32 heap_id;
        u32 type_id;
    } VramAllocation;
    
    typedef struct {
        u64 offset;
        u64 size;
    } VramRegion;

    typedef struct {
        VkPhysicalDeviceMemoryProperties memory_properties;
        VkDevice device;
        u32 vram_allocation_count;
    } VulkanMemory;

    typedef struct {
        VkSwapchainKHR swapchain;
        VramAllocation vram_allocation;
        u32 swapchain_image_count;
        u32 screen_size_x;
        u32 screen_size_y;
        VkImage swapchain_images[MAX_SWAPCHAIN_IMAGE_COUNT];
        VkImageView swapchain_views[MAX_SWAPCHAIN_IMAGE_COUNT];
        VkImage depth_image;
        VkImage color_image;
        VkImageView depth_view;
        VkImageView color_view;
    } VulkanScreen;

    typedef struct {
        VkDescriptorPool pool;
        VkDescriptorSetLayout layouts[DESCRIPTOR_SET_COUNT];
        VkDescriptorSet sets[DESCRIPTOR_SET_COUNT];
    } VulkanDescriptors;

    typedef struct {
        VkPipeline *graphics_pipelines;
        VkPipeline *compute_pipelines;
        u32 graphics_pipeline_count;
        u32 compute_pipeline_count;
        VkPipelineLayout pipeline_layout;
    } VulkanPipelines;

    typedef struct {
        VkBuffer vertex_buffer;
        VkBuffer index_buffer;
        u32 vertex_count;
        u32 index_count;
    } DrawMesh;

    typedef struct {
        u32 storage_buffer_count;
        VkBuffer uniform_buffer;
        VkBuffer *storage_buffers;
        
        VramRegion uniform_src_region;
        VramRegion uniform_dst_region;
        VramRegion *storage_src_regions;
        VramRegion *storage_dst_regions;

        VkBuffer src_buffer;

        VramAllocation src_vram;
        VramAllocation dst_vram;
    } VulkanBuffers;

    struct VulkanSegment {
        VulkanSegmentStates states;
        VulkanFlags flags;

        VulkanObjects vulkan_objects;
        VulkanDevice vulkan_device;
        VulkanMemory vulkan_memory;
        VulkanScreen vulkan_screen;
        VulkanDescriptors vulkan_descriptors;
        VulkanPipelines vulkan_pipelines;
        VulkanBuffers vulkan_buffers;

        void *resource_address;
        void *resource_pipelines_base;
        void *resource_pipelines_limit;
        void *resource_buffers_base;
        void *resource_buffers_limit;

        void *segment_end;
    };

    #define VULKAN_STRUCT_SIZE (ALIGN(sizeof(VulkanSegment), MEMORY_PAGE_SIZE))

    #define MAX_PIPELINE_DEPENDENCY_COUNT (2048)
    #define MAX_PIPELINES_SIZE (MAX_PIPELINE_COUNT * sizeof(VkPipeline))
    #define MAX_BUFFERS_SIZE (MAX_STORAGE_BUFFER_COUNT * (sizeof(VkBuffer) + sizeof(VramRegion) * 2))
    #define MIN_VULKAN_SEGMENT_SIZE (ALIGN((VULKAN_STRUCT_SIZE + MAX_PIPELINES_SIZE + MAX_BUFFERS_SIZE), ALLOCATION_GRANULARITY))

    /*  Segment layout representation:
    +----------------+---------------+---------------+---------------+--------------------+----------------+-------------------------------------------------------+
    | VULKAN OBJECTS | VULKAN DEVICE | VULKAN MEMORY | VULKAN SCREEN | VULKAN DESCRIPTORS | VULKAN SHADERS |                                                       |                                                       |
    +----------------+---------------+---------------+---------------+--------------------+----------------+-------------------------------------------------------+
                                                                                                resource address                                                    */
                                                                                                                                                                                   
    struct VulkanRenderCmd {
        VkCommandBuffer command_buffer;
        const VulkanScreen *vulkan_screen;

        VkRect2D screen_render_area;
        PFN_vkCmdBeginRendering begin_rendering;
        PFN_vkCmdEndRendering end_rendering;
    };

#endif /* #ifdef INCLUDE_VULKAN_INTERNAL */

#endif /* #ifndef _VULKAN_INCLUDED */
