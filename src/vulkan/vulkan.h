#include "../main.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define MAX_VRAM_ALLOCATIONS 512

typedef enum {
    DEVICE_TYPE_NONE = 0,
    DEVICE_TYPE_DESCRETE = 1,
    DEVICE_TYPE_INTEGRATED = 2
} DeviceType;

typedef struct {
    u64 size;
    u32 positive_flags;
    u32 negative_flags;
    u32 memory_type_flags;
} VramAllocationInfo;

typedef struct {
    VkDeviceMemory memory;
    u64 size;
    u32 heap_id;
    u32 type_id;
} VramAllocation;

struct VulkanContext {
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
    /* memory properties */
    VkPhysicalDeviceMemoryProperties memory_properties;
    VramAllocation vram_allocations[MAX_VRAM_ALLOCATIONS];
    u32 vram_allocation_count;
    /* local use */
    VkDebugUtilsMessengerEXT debug_messenger;
    PFN_vkCreateDebugUtilsMessengerEXT ext_create_debug_utils_messenger;
    PFN_vkDestroyDebugUtilsMessengerEXT ext_destroy_debug_utils_messenger;
};

VramAllocation* allocateVram(VulkanContext* vulkan_context, const VramAllocationInfo* info);
void freeVram(VulkanContext* vulkan_context, VramAllocation* allocation);


//
//
//

#define MAX_SWAPCHAIN_IMAGE_COUNT 8

#define MAX_SHADER_MODULE_COUNT 2
#define SHADER_GRAPHICS_VERTEX_ID 0
#define SHADER_GRAPHICS_FRAGMENT_ID 1
#define SHADER_COMPUTE_ID 0

#define SHADER_ENTRY_VERTEX "vertexMain"
#define SHADER_ENTRY_FRAGMENT "fragmentMain"
#define SHADER_ENTRY_COMPUTE "computeMain"

#define MAX_DESCRIPTOR_SETS 16
#define MAX_BINDINGS_PER_SET 16

typedef struct {
    VkExtent2D extent;
    VkPresentModeKHR present_mode;
    VkFormat color_format;
    VkFormat depth_format;
    u32 min_image_count;
    u32 max_image_count;
    VkSurfaceTransformFlagsKHR transform;
    VkColorSpaceKHR color_space;
} SurfaceData;

typedef struct {
    VkSwapchainKHR swapchain;
    VkImage swapchain_images[MAX_SWAPCHAIN_IMAGE_COUNT];
    VkImageView swapchain_image_views[MAX_SWAPCHAIN_IMAGE_COUNT];
    VkImage depth_image;
    VkImageView depth_image_view;
    VramAllocation* vram;
    u32 swapchain_image_count;
} Screen;

typedef struct {
    VkSemaphore image_available_semaphore[MAX_SWAPCHAIN_IMAGE_COUNT];
    VkSemaphore image_finished_semaphore;
    VkFence frame_fence;
} RenderLoopObjects;

typedef struct {
    RenderPipelineType type;
    VkShaderModule shaders[MAX_SHADER_MODULE_COUNT];
    VkPipeline pipeline;
} RenderPipeline;

typedef struct {
    RenderResourceType type;
    void* device_resource;
    void* host_resource; /* optionaly used for stransfer in descrete devices */
    u64 device_offset;
    u64 host_offset;
    u64 size;
} RenderResource;


struct VulkanCmdContext {
    const VulkanContext* vulkan_context;
    const RenderContext* render_context;
    VkCommandBuffer command_buffer;
    RenderPipelineType last_type;
    u32 render_image_id;
    u32 bound_pipeline_id;
};

struct RenderContext{
    UpdateCallback_pfn update_callback;
    RenderCallback_pfn render_callback;
    /* SCREEN */
    Screen screen;
    SurfaceData surface_data;

    /* RESOURCES */
    VkDescriptorPool descriptor_pool;
    VkDescriptorSetLayout descriptor_set_layouts[MAX_DESCRIPTOR_SETS];
    VkDescriptorSet descriptor_sets[MAX_DESCRIPTOR_SETS];
    RenderResource resources[MAX_DESCRIPTOR_SETS][MAX_BINDINGS_PER_SET]; /* descriptors * bindings not the oposite */
    RenderResource* resources_plain[MAX_DESCRIPTOR_SETS * MAX_BINDINGS_PER_SET];
    u32 resource_count;
    u32 descriptor_set_count;
    /* resource storage */
    VramAllocation* resource_host_allocation;
    VramAllocation* resource_device_allocation;
    /* pipeline layouts */
    VkPipelineLayout full_pipeline_layout;
    VkPipelineLayout empty_pipeline_layout;
    VkDescriptorSetLayout empty_set_layout;

    /* PIPELINES */
    RenderPipeline* render_pipelines;
    u32 render_pipeline_count;

    /* EXECUTION */
    VkCommandPool command_pool;
    
};
