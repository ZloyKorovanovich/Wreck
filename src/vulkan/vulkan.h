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
b32 runVulkanLoop(VulkanHandle vulkan);

#define REQUIRED_DEVICE_VRAM_SIZE (1024llu * 1024llu * 1024llu * 2llu)
#define REQUIRED_HOST_VRAM_SIZE (1024llu * 1024llu * 1024llu * 4llu)
#define MAX_PHYSICAL_DEVICE_COUNT (4)

#endif /* _VULKAN_INCLUDED */
