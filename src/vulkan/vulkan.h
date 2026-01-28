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
#define SHADER_ENTRY_VERTEX "vertexMain"
#define SHADER_ENTRY_FRAGMENT "fragmentMain"
#define SHADER_ENTRY_COMPUTE "computeMain"

typedef struct {
    VkColorSpaceKHR color_space;
    VkFormat color_format;
    VkFormat depth_format;
    VkPresentModeKHR present_mode;
    VkExtent2D extent;
} RenderSettings;

typedef struct {
    u32 swapchain_image_count;
    VkSwapchainKHR swapchain;
    VkImage *swapchain_images;
    VkImageView *swapchain_image_views;
    VkImage depth_image;
    VkImageView depth_image_view;
    VramRegion depth_vram_region;
} ScreenImages;

/* render prorgams */
typedef enum {
    SHADER_PROGRAM_TYPE_NONE = 0,
    SHADER_PROGRAM_TYPE_GRAPHICS = 1,
    SHADER_PROGRAM_TYPE_COMPUTE = 2
} ShaderProgramType;

typedef struct {
    ShaderProgramType type;
    VkPipeline pipeline;
    u32 resources_begin;
    u32 resources_end;
    /* different part */
    union {
        struct {
            VkShaderModule vertex_shader;
            VkShaderModule fragment_shader;
        };
        struct {
            VkShaderModule compute_shader;
        };
    };
} ShaderProgram;

typedef struct {
    /* all pointer allocations are made on reasouce_arena */
    VkPipelineLayout pipeline_layout;
    ShaderProgram *shader_programs;
    u32 *resources_usage;
    u32 program_count;
} Programs;


typedef struct {
    f32 position[4];
    f32 normal[4];
    f32 uv[4];
} Vertex;

typedef struct {
    Vertex *vertices;
    u16 *indices;
    u32 vertex_count;
    u32 index_count;
} RawMesh;

typedef struct {
    VkBuffer vertex_buffer;
    VkBuffer index_buffer;
    u32 vertex_count;
    u32 index_count;
} RenderMesh;

typedef struct {
    /* all pointer allocations are made on reasouce_arena */
    RenderMesh *render_meshes;
    u32 meshes_count;
} Meshes;

typedef struct {
    VkBuffer global_device_buffer;
    VkBuffer global_host_buffer;
    VramRegion global_host_region;
} Uniforms;

/* context of render queue */
struct RenderContext {
    VulkanContext *vulkan_context;
    Arena resource_arena;
    /* screen */
    RenderSettings render_settings;
    ScreenImages screen_images;
    /* shader shader_programs */
    Programs shader_programs;
    Meshes render_meshes;
    Uniforms uniforms;
    /* callback for logs and errors */
    MsgCallback_pfn msg_callback;
    /* memory */
    Vram images_device_vram;
    Vram mesh_device_vram;
    Vram uniform_device_vram;
    
    Vram uniform_host_vram;
    void *uniform_host_vram_map;
    /* commands */
    VkCommandPool command_pool;
};

#define MAX_COLOR_ATTACHMENTS (8)

struct RenderCmd {
    RenderContext *render_context;
    VkCommandBuffer command_buffer;
    ShaderProgram *last_shader_program;
    RenderMesh *last_render_mesh;

    VkRenderingAttachmentInfoKHR color_attachments[MAX_COLOR_ATTACHMENTS];
    VkRenderingAttachmentInfoKHR depth_attachment;
    u32 color_attachment_count;
    b32 use_depth_atachment;

    VkImageView screen_color_view;
    VkImageView screen_depth_view;
};

#endif
