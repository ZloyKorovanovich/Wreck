#ifndef _MAIN_INCLUDED
#define _MAIN_INCLUDED

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

#define MIN(a, b) (a < b ? a : b)
#define MAX(a, b) (a > b ? a : b)
#define CLAMP(a, b, t) ((t < a) ? a : (t > b) ? b : t)


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
typedef b32 (*msg_callback_pfn) (i32 msg_code, const char* msg);

/* These stringfy look ugly, but thats the only way around :( */
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
/* There might be __func__ arg added in the future, but its const char[] so thats fucked up */
#define LOCATION_TRACE " *** " __FILE__ ":" TOSTRING(__LINE__)

#define MSG_CALLBACK(callback, code, message) if(callback(code, message LOCATION_TRACE)) return code

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
    MSG_CODE_ERROR_VK_DESCRIPTOR_TOO_MANY_BINDINGS = -2147483624,
    MSG_CODE_ERROR_VK_INVALID_BINDING_TYPE = -2147483623,
    MSG_CODE_ERROR_VK_CREATE_DESCRIPTOR_SET_LAYOUT = -2147483622,
    MSG_CODE_ERROR_VK_CREATE_DESCRIPTOR_SET = -2147483621,
    MSG_CODE_ERROR_VK_ALLOCATE_VRAM = -2147483621,
    MSG_CODE_ERROR_VK_PIPELINE_LAYOUT_CREATE = -2147483620,
    MSG_CODE_ERROR_VK_SEMAPHORE_CREATE = -2147483619,
    MSG_CODE_ERROR_VK_FENCE_CREATE = -2147483618,
    MSG_CODE_ERROR_VK_COMMAND_BUFFER_ALLOCATE = -2147483617,
    MSG_CODE_ERROR_VK_COMMAND_POOL_CREATE = -2147483616,
    MSG_CODE_ERROR_VK_RENDER_NODE_SHADERS_MISSING = -2147483615,
    MSG_CODE_ERROR_VK_READ_BUFFER_REALLOC_FAIL = -2147483614,
    MSG_CODE_ERROR_VK_READ_FILE_TO_BUFFER = -2147483613,
    MSG_CODE_ERROR_VK_BUFFER_MALLOC_FAIL = -2147483612,
    MSG_CODE_ERROR_VK_CREATE_SHADER_MODULE = -2147483611
} MSG_CODES_VK;

/* Used by vulkan info struct, affects vulkan instance and glfw creation.
If you want to use render doc, make sure DEBUG flag is not set!  */
typedef enum {
    VULKAN_FLAG_DEBUG = 0x1, /* enables vulkan validation layers, if not present a error will appear */
    VULKAN_FLAG_WIN_RESIZE = 0x2 /* makes resizable window useful in windowed mode */
} VULKAN_FLAGS;

/* indentifies type of binding in shader */
typedef enum {
    RENDER_BINDING_TYPE_NONE = 0,
    RENDER_BINDING_TYPE_UNIFORM_BUFFER = 1,
    RENDER_BIDNING_TYPE_STORAGE_BUFFER = 2
} RENDER_BINDING_TYPES;

typedef struct {
    u32 binding;
    u32 descriptor_set;
    u32 type;
} RenderBinding;

typedef enum {
    RENDER_NODE_TYPE_NONE = 0,
    RENDER_NODE_TYPE_GRAPHICS = 1
} RENDER_NODE_TYPES;

typedef struct {
    u32 node_type;
    const char* vertex_shader;
    const char* fragment_shader;
    const char* compute_shader;
} RenderNode;

/* settings of render in vulkan */
typedef struct {
    const RenderBinding* bindings;
    u32 binding_count;
    const RenderNode* nodes;
    u32 node_count;
} RenderSettings;

/* Used by vulkanRun function, provides necessary information for initialization
and runtime of this function.   */
typedef struct {
    msg_callback_pfn msg_callback; /* error and warning handling function */
    const char* name;
    u32 x; /* width of window in pixels, not used if fullscreen*/
    u32 y; /* heigh of window in pixels, not used if fullscreen*/
    u32 flags; /* VULKAN_FLAGS value, determines intialization flags */
    u32 version; /* informs vulkan about your app version */
    /* settings */
    const RenderSettings* render_settings; /* leave as null if not used */
} VulkanInfo;

/* Runs vulkan function, initalizes glfw, vulkan and all rendering involved,
then executes render code.  */
i32 vulkanRun(const VulkanInfo* info);

/*======================================================================
    ENGINE INFO
  ======================================================================*/

#define MAKE_VERSION(major, minor, patch) ((((u32)(major)) << 22U) | (((u32)(minor)) << 12U) | ((u32)(patch)))

#define ENGINE_NAME "Wreck"
#define ENGINE_VERSION (VK_MAKE_VERSION(0, 1, 0))

#endif
