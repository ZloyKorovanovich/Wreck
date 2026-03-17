#ifndef _GPU_INCLUDED
#define _GPU_INCLUDED

#include <base.h>

/* extenral */
typedef enum {
    GPU_FLAGS_NONE = 0x0,
    GPU_FLAG_DEBUG = 0x1,
    GPU_FLAG_RESIZE = 0x2,
    GPU_FLAG_ASYNC_TRANSFER = 0x4,
    GPU_FLAG_ASYNC_COMPUTE = 0x8,
    GPU_FLAG_PREFER_0_DESCRETE_1_COMPUTE = 0x10
} GPUFlags;

typedef enum {
    GPU_TYPE_NONE = 0,
    GPU_TYPE_DESCRETE = 1,
    GPU_TYPE_INTEGRATED = 2
} GPUType;

#define MIN_GPU_LOCAL_MEMORY (256 * 1024 * 1024)
#define MAX_GPU_LOCAL_MEMORY (2 * 1024 * 1024 * 1024)
#define MIN_GPU_SHARED_MEMORY (4 * 1024 * 1024)
#define MAX_GPU_SCAN_COUNT (8)

#define GPU_VIRTUAL_ADDRESS_SPACE_SIZE (1024 * 1024 * 64)

typedef struct GPU GPU;

typedef struct {
    u32 id;
    GPUType type;
    char name[256];
    u64 host_heap_size;
    u64 device_heap_size;
} GPUInfo;

typedef struct {
    GPUFlags flags;
    u32 window_x;
    u32 window_y;
    const char *window_name;
    /* optional */
    GPUInfo gpu_info;
    MsgCallback_pfn msg_callback;
} MountGPUIn;

typedef struct {
    GPUFlags flags;
    GPUInfo gpu_info;
} MountGPUOut;

GPU *mountGPU(const MountGPUIn *input, MountGPUOut *output, Allocate_pfn struct_alloc, Free_pfn struct_free);
void dismountGPU(GPU *gpu);

/* DYNAMIC RESOURCES */

#define SHADER_ENTRY_VERTEX "vertexMain"
#define SHADER_ENTRY_FRAGMENT "fragmentMain"
#define SHADER_ENTRY_COMPUTE "computeMain"

typedef enum {
    GPU_PROGRAM_TYPE_NONE = 0,
    GPU_PROGRAM_TYPE_GRAPHICS = 1,
    GPU_PROGRAM_TYPE_COMPUTE = 2
} GPUProgramType;

typedef enum {
    GPU_PROGRAM_FLAGS_NONE = 0x0,
    GPU_PROGRAM_FLAG_VERTEX_POSITION = 0x1,
    GPU_PROGRAM_FLAG_VERTEX_NORMAL = 0x2,
    GPU_PROGRAM_FLAG_VERTEX_TEXCOORD = 0x4
} GPUProgramFlags;

typedef enum {
    GPU_FORMAT_NONE = 0,
    GPU_FORMAT_RGBA_COLOR = 1,
    GPU_RENDER_SURFACE_COLOR = 2,
    GPU_FORMAT_DEPTH = 3
} GPUFormat;

typedef struct {
    f32 position[4];
    f32 normal[4];
    f32 texcoord[4];
    u32 bone_indices[4];
    f32 bone_weights[4];
} GPUVertex;

typedef u16 GPUIndex;

typedef struct {
    const char *name;
    GPUVertex *vertices;
    GPUIndex *indices;
    u32 vertex_count;
    u32 index_count;
} GPUMeshInfo;

typedef struct {
    u64 size;
    void *init_data;
} GPUBufferInfo;

typedef struct {
    GPUProgramType type;
    GPUProgramFlags flags;
    const char *name;
    union {
        /* compute program */
        struct {
            const void* compute_spv;
            u32 compute_size;
        };
        /* graphics program */
        struct {
            const void* vertex_spv;
            const void* fragment_spv;
            u32 vertex_size;
            u32 fragment_size;

            u32 color_format_count;
            GPUFormat depth_format;
            const GPUFormat *color_formats;
        };
    };
} GPUProgramInfo;

/* simpler filling of graphics program info based on files module data */
#define GPU_GRAPHICS_PROGRAM(program_name, gpu_flags, shaders_address)  \
(GPUProgramInfo) {                                                      \
    .type = GPU_PROGRAM_TYPE_GRAPHICS,                                  \
    .name = program_name##_V_NAME "+" program_name##_F_NAME,            \
    .flags = gpu_flags,                                                 \
    .vertex_spv = shaders_address + program_name##_V_BEGIN,             \
    .fragment_spv = shaders_address + program_name##_F_BEGIN,           \
    .vertex_size = program_name##_V_SIZE,                               \
    .fragment_size = program_name##_F_SIZE                              \
}

typedef struct {
    const GPUBufferInfo *uniform_buffer;
    const GPUBufferInfo *storage_buffers;
    const GPUProgramInfo *programs;
    u32 mutable_storage_buffer_count;
    u32 storage_buffer_count;
    u32 program_count;
} CreateGPUStaticResourcesIn;

typedef struct {
    void *host_uniform_buffer;
    void **host_mutable_storage_buffers;
} CreateGPUStaticResourcesOut;

typedef struct {
    /* put mesh info to 0 if you want a skip slot */
    GPUMeshInfo *meshes;
    u32 mesh_count;
} SwapGPUDynamicResourcesIn;

b32 createGPUStaticResources(GPU *gpu, const CreateGPUStaticResourcesIn *input, CreateGPUStaticResourcesOut *output);
void destroyGPUStaticResources(GPU *gpu);

/* render system does not swap indices of meshes, its your problem to sort mesh infos in right order */
b32 swapGPUDynamicResources(GPU *gpu, const SwapGPUDynamicResourcesIn *input);

/* can be swapped with some OPENGL_INTERNAL or D3D_INTERNAL, whatever */
#ifdef VULKAN_INTERNAL
    #ifdef _WIN32
        #define VK_USE_PLATFORM_WIN32_KHR
        #include <windows.h>
    #endif
    #include <vulkan/vulkan.h>

    /* GPUWindow is a pointer type, it is used to represent any window type, 
        in case of windows 64 it represents HWND handle. */
    #ifdef _WIN32
        typedef HWND GPUWindow; 
    #endif

    #define MAX_PHYSICAL_DEVICE_QUEUE_COUNT (32)
    #define MAX_PHYSICAL_DEVICE_SURFACE_FORMAT_COUNT (256)
    #define MAX_PHYSICAL_DEVICE_EXTENSION_COUNT (256)
    #define MAX_PHYSICAL_DEVICE_PRESENT_MODE_COUNT (256)

    #define MAX_COLOR_TARGET_COUNT (4)
    #define MAX_GPU_MEMORY_ALLOCATIONS (256)
    #define MAX_DESCRIPTOR_BINDINGS (16)
    #define PUSH_CONSTANT_RANGE_SIZE (64)

    #define ADJUST_GPU_BUFFER_LOCATION(mem_req, loc, alloc_size, bits)  \
    alloc_size = ALIGN(alloc_size, mem_req.alignment);                  \
    loc = (GPUBufferLocation) {                                         \
        .offset = alloc_size,                                           \
        .size = mem_req.size                                            \
    };                                                                  \
    alloc_size += mem_req.size;                                         \
    bits &= mem_req.memoryTypeBits;

    typedef enum {
        DESCRIPTOR_SET_BASE_ID = 0,
        DESCRIPTOR_SET_STORAGE_BUFFERS_ID = 1,
        DESCRIPTOR_SET_IMAGES_ID = 2,
        DESCRIPTOR_SET_COUNT
    } GPUDescriptorSets;

    typedef enum {
        GPU_MEMORY_USE_NONE = 0,
        GPU_MEMORY_USE_HOST_TO_DEVICE = 1,
        GPU_MEMORY_USE_DEVICE = 2
    } GPUMemoryUse;

    typedef struct {
        VkDevice vk_device;
        VkPhysicalDevice vk_physical_device;
        /* queues */
        VkQueue vk_render_queue;
        VkQueue vk_transfer_queue;
        VkQueue vk_compute_queue;
        VkCommandPool vk_render_command_pool;
        VkCommandPool vk_transfer_command_pool;
        VkCommandPool vk_compute_command_pool;
        u32 vk_render_queue_id;
        u32 vk_transfer_queue_id;
        u32 vk_compute_queue_id;
        /* formats */
        VkPresentModeKHR present_mode;
        VkColorSpaceKHR color_space_surface;
        VkFormat format_depth;
        VkFormat format_surface_color;
        VkFormat format_rgba_color;
        /* extension procs */
        PFN_vkCmdBeginRenderingKHR begin_rendering;
        PFN_vkCmdEndRenderingKHR end_rendering;
    } GPUDevice;

    typedef struct {
        const char* hash_name; /* should point to real constant value (persistently) */
        VkDeviceMemory memory;
        u64 size;
        u32 heap_id;
        VkMemoryPropertyFlags flags;
    } GPUMemory;

    typedef struct {
        void *base_address;
        u64 virtual_size;
        u64 commited_size;
        u64 static_size;
        u64 dynamic_size;
    } GPUResourceAllocator;

    typedef struct {
        VkDevice device;
        VkPhysicalDeviceMemoryProperties memory_properties;
        u32 hash_name_count;
        const char *hash_names[MAX_GPU_MEMORY_ALLOCATIONS];
    } GPUMemoryAllocator;

    typedef struct {
        u64 offset;
        u64 size;
    } GPUBufferLocation;

    typedef struct {
        VkDescriptorPool descriptor_pool;
        VkDescriptorSetLayout descriptor_layouts[DESCRIPTOR_SET_COUNT];
        VkDescriptorSet descriptor_sets[DESCRIPTOR_SET_COUNT];
        VkImage screen_color_image;
        VkImage screen_depth_image;
        VkImageView screen_color_view;
        VkImageView screen_depth_view;

        VkPipelineLayout pipeline_layout;
        VkPipeline *pipelines;
        VkBuffer *storage_buffers;
        VkBuffer uniform_buffer;
        u32 mutable_storage_buffer_count;
        u32 storage_buffer_count;
        u32 pipeline_count;

        GPUBufferLocation *storage_buffer_locations;
        GPUBufferLocation uniform_buffer_location;
        GPUBufferLocation screen_color_location;
        GPUBufferLocation screen_depth_location;
        GPUMemory mutable_memory;
        GPUMemory immutable_memory;
        GPUMemory screen_memory;
        void *mutable_memory_map;
    } GPUStaticResources;

    typedef struct {
        VkBuffer vertex_buffer;
        VkBuffer index_buffer;
        u32 vertex_count;
        u32 index_count;
    } GPUMesh;

    typedef struct {
        /* jumps on swap operation */
        void *resource_address;
        GPUMesh *meshes;
        GPUBufferLocation *mesh_locations; /* array double size of mesh_count vertex [i * 2] index [i * 2 + 1] */
        u32 mesh_count;
        u32 swap_id;
        GPUMemory meshes_memory;
    } GPUDynamicResources;

    typedef struct {
        VkCommandBuffer transfer_command_buffer;
        VkCommandBuffer render_command_buffer;
        #ifdef _WIN32
            HANDLE dynamic_resources_mutex;
        #endif
    } GPUControlCenter;

    struct GPU {
        GPUFlags flags;
        GPUWindow window;
        VkInstance vk_instance;
        VkDebugUtilsMessengerEXT vk_debug_messenger;
        VkSurfaceKHR vk_surface;

        /* substructs */
        GPUDevice device;
        GPUControlCenter control;
        GPUResourceAllocator resource_allocator;
        GPUMemoryAllocator memory_allocator;
        GPUStaticResources static_resources;
        GPUDynamicResources dynamic_resources;
        /* pfns */
        MsgCallback_pfn msg_callback;
        Allocate_pfn alloc;
        Free_pfn free;
    };

    b32 allocateGPUMemory(GPUMemoryAllocator *allocator, const char *name, GPUMemoryUse use, u32 memory_type_bits, u64 size, GPUMemory *memory);
    b32 freeGPUMemory(GPUMemoryAllocator *allocator, GPUMemory *memory);

    #ifdef _WIN32
        static inline void *allocateStaticResources(
            GPUResourceAllocator *allocator, 
            u64 size,
            u64 alignment
        ) {
            alignment = (alignment == 0) ? 16 : alignment;
            if(allocator->dynamic_size != 0) {
                return NULL;
            }
            
            u64 alloc_begin = ALIGN(allocator->static_size, alignment);
            u64 alloc_end = alloc_begin + size;
            if(alloc_end > allocator->virtual_size) {
                return NULL;
            }

            /* commit more memory if needed */
            if(alloc_end > allocator->commited_size) {
                u64 commit_size = ALIGN((alloc_end - allocator->commited_size), MEMORY_PAGE_SIZE);
                /* commit */
                if(!VirtualAlloc(
                    (u8 *)allocator->base_address + allocator->commited_size,
                    commit_size,
                    MEM_COMMIT,
                    PAGE_READWRITE
                )) {
                    return NULL;
                }
                /* adjust offset */
                allocator->commited_size += commit_size;
            }
            
            allocator->static_size = alloc_end;
            return (void *)((u8 *)allocator->base_address + alloc_begin);
        };
    #endif /* _WIN32 */

#endif /* VULKAN_INTERNAL */
#endif
