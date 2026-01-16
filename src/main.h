#ifndef MAIN_INCLUDED
#define MAIN_INCLUDED

#include <base.h>

/*======================================================================
    MSG
  ======================================================================*/

typedef enum {
    MSG_CODE_SUCCESS = 0,
    MSG_CODE_ERROR = -1,
    MSG_CODE_WARNING = 1
} MSG_TYPE;

typedef void (*MsgCallback_pfn)(i32 type, const String* msg);

#define MSG_LOG(callback, msg) if(callback) {callback(MSG_CODE_SUCCESS, msg);}
#define MSG_ERROR(callback, msg) if(callback) {callback(MSG_CODE_ERROR, msg);}
#define MSG_WARNING(callback, msg) if(callback) {callback(MSG_CODE_WARNING, msg);}

/* These stringfy look ugly, but thats the only way around :( */
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
#define LOCATION_TRACE " *** " __FILE__ ":" TOSTRING(__LINE__)

#define TRACED_STR(str) CONST_STRING(str LOCATION_TRACE)

/*======================================================================
    VULKAN
  ======================================================================*/

typedef struct VulkanContext VulkanContext;
typedef struct RenderContext RenderContext;

typedef enum {
    VULKAN_FLAG_RESIZABLE = 0x1,
    VULKAN_FLAG_DEBUG = 0x2
} VulkanFlags;

typedef struct {
    String name;
    VulkanFlags flags;
    u32 resolution_x;
    u32 resolution_y;
    MsgCallback_pfn msg_callback;
} VulkanContextInfo;

VulkanContext* createVulkanContext(Allocate_pfn context_allocate, const VulkanContextInfo* info);
void destroyVulkanContext(VulkanContext* context);

typedef struct {
    VulkanContext* vulkan_context;
    MsgCallback_pfn msg_callback;
} RenderContextInfo;

RenderContext* createRenderContext(Allocate_pfn context_allocate, const RenderContextInfo* info);
void destroyRenderContext(RenderContext* context);


#endif
