#ifndef _MAIN_INCLUDED
#define _MAIN_INCLUDED

#define _CRT_SECURE_NO_DEPRECATE 

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef char i8;
typedef short i16;
typedef int i32;
typedef long i32x;
typedef long long i64;

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned u32;
typedef unsigned long u32x;
typedef unsigned long long u64;

typedef float f32;
typedef double f64;

typedef unsigned b32;
#define TRUE 1
#define FALSE 0

#define U32_MAX 0xffffffff
#define U64_MAX 0xffffffffffffffff

#define MIN(a, b) (a < b ? a : b)
#define MAX(a, b) (a > b ? a : b)
#define CLAMP(a, b, t) ((t < a) ? a : (t > b) ? b : t)

#define ALIGN(ptr, alig) ((ptr + (alig - 1)) & ~(alig - 1))
#define ARRAY_COUNT(array) (sizeof(array) / sizeof(array[0]))

typedef struct {
    u8* buffer;
    u64 size;
} ByteBuffer;


#define MSG_IS_ERROR(msg) (msg < 0)
#define MSG_IS_WARNING(msg) (msg > 0)
#define MSG_IS_SUCCESS(msg) (msg == 0)

/* Errors and Warnings of the whole program (structure).
Codes should be consistent, so assigning value manually is mandatory.
That simplifies error handling as project code base changes. */
typedef enum {
    MSG_CODE_SUCCESS = 0, /* success code */
    MSG_CODE_ERROR = -2147483648, /* first error code (reserved) */
    MSG_CODE_WARNING = 1 /* first warning code (reserved) */
} MSG_CODES;
/* Use this as a error/warning message reciever */
typedef b32 (*MsgCallback_pfn) (i32 msg_code, const char* msg);

/* These stringfy look ugly, but thats the only way around :( */
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
/* There might be __func__ arg added in the future, but its const char[] so thats fucked up */
#define LOCATION_TRACE " *** " __FILE__ ":" TOSTRING(__LINE__)

#define MSG_CALLBACK(callback, code, message) if(callback(code, message LOCATION_TRACE)) return code
#define MSG_CALLBACK_NO_TRACE(callback, code, message) if(callback(code, message)) return code

/*======================================================================
    VK INTERFACE
  ======================================================================*/

typedef enum {
    MSG_CODE_ERROR_VK_CREATE_CONTEXT = -2147483647,
    MSG_CODE_ERROR_VK_GLFW_INIT = -2147483646,
    MSG_CODE_ERROR_VK_WINDOW_CREATE = -2147483645,
    MSG_CODE_ERROR_VK_INSATNCE_EXTENSION_MISMATCH = -2147483644,
    MSG_CODE_ERROR_VK_LAYERS_MISMATCH = -2147483643,
    MSG_CODE_ERROR_VK_INSTANCE_CREATE = -2147483642,
    MSG_CODE_ERROR_VK_SURFACE_CREATE = -2147483641,
    MSG_CODE_ERROR_VK_CONTEXT_CREATE_BUFFER_MALLOC = -2147483640,
    MSG_CODE_ERROR_VK_INFO_INVALID = -2147483639,
    MSG_CODE_ERROR_VK_LOAD_PROC = -2147483638,
    MSG_CODE_ERROR_VK_DEBUG_MESSENGER_CREATE = -2147483637,
    MSG_CODE_ERROR_VK_FAILED_TO_FIND_GPU = -2147483636,
    MSG_CODE_ERROR_VK_CREATE_DEVICE = -2147483635,
    MSG_CODE_ERROR_VK_SURFACE_STATS_NOT_SUITABLE = -2147483634,
    MSG_CODE_ERROR_VK_SWAPCHAIN_CREATE = -2147483633,
    MSG_CODE_ERROR_VK_CREATE_RENDER_CONTEXT = -2147483632,
    MSG_CODE_ERROR_VK_RENDER_RUN = -2147483631,
    MSG_CODE_ERROR_VK_SWAPCHAIN_TOO_MANY_IMAGES = -2147483630,
    MSG_CODE_ERROR_VK_IMAGE_VIEW_CREATE = -2147483629,
    MSG_CODE_ERROR_VK_RENDER_LOOP_FAIL = -2147483628,
    MSG_CODE_ERROR_VK_IMAGE_CREATE = -2147483627,
    MSG_CODE_ERROR_VK_IMAGE_BIND_MEMORY = -2147483626,
    MSG_CODE_ERROR_VK_DESCRIPTOR_POOL_CREATE = -2147483625,
    MSG_CODE_ERROR_VK_CREATE_DESCRIPTOR_SET_LAYOUT = -2147483622,
    MSG_CODE_ERROR_VK_ALLOCATE_DESCRIPTOR_SETS = -2147483621,
    MSG_CODE_ERROR_VK_ALLOCATE_VRAM = -2147483621,
    MSG_CODE_ERROR_VK_PIPELINE_LAYOUT_CREATE = -2147483620,
    MSG_CODE_ERROR_VK_SEMAPHORE_CREATE = -2147483619,
    MSG_CODE_ERROR_VK_FENCE_CREATE = -2147483618,
    MSG_CODE_ERROR_VK_COMMAND_BUFFER_ALLOCATE = -2147483617,
    MSG_CODE_ERROR_VK_COMMAND_POOL_CREATE = -2147483616,
    MSG_CODE_ERROR_VK_READ_BUFFER_REALLOC_FAIL = -2147483614,
    MSG_CODE_ERROR_VK_READ_FILE_TO_BUFFER = -2147483613,
    MSG_CODE_ERROR_VK_BUFFER_MALLOC_FAIL = -2147483612,
    MSG_CODE_ERROR_VK_SHADER_MODULE_CREATE = -2147483611,
    MSG_CODE_ERROR_VK_PIPELINE_CREATE = -2147483609,
    MSG_CODE_ERROR_VK_SWAPCHAIN_RECREATE = -2147483608,
    MSG_CODE_ERROR_VK_RESIZE_FAIL = -2147483607,
    MSG_CODE_ERROR_VK_BUFFER_CREATE = -2147483606,
    MSG_CODE_ERROR_VK_CREATE_RESOURCE = -2147483603,
    MSG_CODE_ERROR_VK_BIND_RESOURCE_MEMORY = -2147483602,
    MSG_CODE_ERROR_VK_INIT_VRAM_ARENA = -2147483601,
    MSG_CODE_ERROR_VK_MAP_MEMORY = -2147483600,
    MSG_CODE_ERROR_VK_NO_SURFACE_FORMATS_AVAILABLE = -2147483599,
    MSG_CODE_ERROR_VK_NO_PRESENT_MODES_AVAILABLE = -2147483598,
    MSG_CODE_ERROR_VK_NO_DEPTH_MODES_AVAILABLE = -2147483597,
    MSG_CODE_ERROR_VK_RESOURCE_INFO_INVALID = -2147483596,
    MSG_CODE_ERROR_VK_RENDER_PIPELINE_INVALID = -2147483595,
    MSG_CODE_ERROR_VK_OPEN_FILE = -2147483594
} MSG_CODES_VK;


typedef struct VulkanContext VulkanContext;
typedef struct VulkanCmdContext VulkanCmdContext;
typedef struct RenderContext RenderContext;

/* Used by vulkan info struct, affects vulkan instance and glfw creation.
If you want to use render doc, make sure DEBUG flag is not set!  */
typedef enum {
    VULKAN_FLAG_DEBUG = 0x1, /* enables vulkan validation layers, if not present a error will appear */
    VULKAN_FLAG_WIN_RESIZE = 0x2 /* makes resizable window useful in windowed mode */
} VulkanFlags;

/* Used by vulkanRun function, provides necessary information for initialization
and runtime of this function.   */
typedef struct {
    const char* name;
    u32 x; /* width of window in pixels, not used if fullscreen*/
    u32 y; /* heigh of window in pixels, not used if fullscreen*/
    VulkanFlags flags; /* VulkanFlags value, determines intialization flags */
    u32 version; /* informs vulkan about your app version */
} VulkanContextInfo;

/* Runs vulkan function, initalizes glfw, vulkan and all rendering involved,
then executes render code.  */
i32 createVulkanContext(const VulkanContextInfo* info, MsgCallback_pfn msg_callback, VulkanContext** vulkan_context);
i32 destroyVulkanContext(MsgCallback_pfn msg_callback, VulkanContext* context);

typedef enum {
    RENDER_RESOURCE_TYPE_NONE = 0,
    RENDER_RESOURCE_TYPE_UNIFORM_BUFFER = 1,
    RENDER_RESOURCE_TYPE_STORAGE_BUFFER = 2
} RenderResourceType;

typedef enum {
    RENDER_RESOURCE_HOST_IMMUTABLE = 0,
    RENDER_RESOURCE_HOST_MUTABLE = 1
} RenderResourceHostMutability;

typedef struct {
    u32 binding;
    u32 set;
} RenderBinding;

/* resources are all accessed via binding and set */
typedef struct {
    RenderResourceType type;
    RenderResourceHostMutability mutability;
    u32 binding; 
    u32 set;
    u64 size;
} RenderResourceInfo;

typedef enum {
    RENDER_PIPELINE_TYPE_NONE = 0,
    RENDER_PIPELINE_TYPE_GRAPHICS = 1,
    RENDER_PIPELINE_TYPE_COMPUTE = 2
} RenderPipelineType;

typedef enum {
    RENDER_RESOURCE_ACCESS_TYPE_READ = 0,
    RENDER_RESOURCE_ACCESS_TYPE_WRITE = 1,
    RENDER_RESOURCE_ACCESS_TYPE_READ_WRITE = 2
} RenderResourceAccessType;

typedef struct {
    u32 binding;
    u32 set;
    RenderResourceAccessType type;
} RenderResourceAccess;

typedef struct {
    RenderPipelineType type;

    u32 resource_access_count;
    const RenderResourceAccess* resources_access;

    const char* name; /* used for debug only */
    const char* vertex_shader;
    const char* fragment_shader;
    const char* compute_shader;
} RenderPipelineInfo;

typedef struct {
    u32 x;
    u32 y;
} RenderWindowContext;


typedef void (*RenderStart_pfn)(VulkanCmdContext* cmd);
typedef void (*RenderUpdate_pfn)(const RenderWindowContext* window_context, VulkanCmdContext* cmd);

typedef struct {
    RenderStart_pfn start_callback;
    RenderUpdate_pfn update_callback;
    const RenderResourceInfo* resource_infos;
    const RenderPipelineInfo* pipeline_infos; /* indices of programms are preserved, so you can always access it via index you chouse */
    u32 resource_count;
    u32 pipeline_count;
} RenderContextInfo;

/* vulkan context is not constant because memmory data will change during vram allocations */
i32 createRenderContext(VulkanContext* vulkan_context, const RenderContextInfo* render_info, MsgCallback_pfn msg_callback, RenderContext** render_context);
i32 destroyRenderContext(VulkanContext* vulkan_context, MsgCallback_pfn msg_callback, RenderContext* render_context);
i32 renderLoop(const VulkanContext* vulkan_context, MsgCallback_pfn msg_callback, RenderContext* render_context);

void cmdCompute(VulkanCmdContext* cmd, u32 pipeline_id, u32 groups_x, u32 groups_y, u32 groups_z);
void cmdDraw(VulkanCmdContext* cmd, u32 pipeline_id, u32 vertex_count, u32 instance_count);
void* cmdBeginWriteResource(VulkanCmdContext* cmd, const RenderBinding* binding);
void cmdEndWriteResource(VulkanCmdContext* cmd);

/*======================================================================
    STD
  ======================================================================*/

static inline void strcat_u32(char* dst, u32 src) {
    for(;*dst;dst++);
    if(!src) {
        *dst++ = '0';
    } else {
        u32 reverse = 0;
        u32 digits = 0;
        while(src){
            reverse = reverse * 10 + src % 10;
            src = src / 10;
            digits++;
        }
        for(;digits; digits--) {
            *dst++ = reverse % 10 + 48;
            reverse /= 10;
        }
    }
    *dst = '\0';
}

static inline void strcat_u64(char* dst, u64 src) {
    for(;*dst;dst++);
    if(!src) {
        *dst++ = '0';
    } else {
        u64 reverse = 0;
        u32 digits = 0;
        while(src){
            reverse = reverse * 10 + src % 10;
            src = src / 10;
            digits++;
        }
        for(;digits; digits--) {
            *dst++ = reverse % 10 + 48;
            reverse /= 10;
        }
    }
    *dst = '\0';
}

/*======================================================================
    ENGINE INFO
  ======================================================================*/

#define MAKE_VERSION(major, minor, patch) ((((u32)(major)) << 22U) | (((u32)(minor)) << 12U) | ((u32)(patch)))

#define ENGINE_NAME "Wreck"
#define ENGINE_VERSION (VK_MAKE_VERSION(0, 1, 0))

#endif
