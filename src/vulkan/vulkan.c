#include "vulkan.h"

/* settings for debug utils messenger callback */
const String c_debug_layer_name = CONST_STRING("VK_LAYER_KHRONOS_validation");
const VkDebugUtilsMessageSeverityFlagsEXT c_debug_message_severity = (
    VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | 
    VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT
);
const VkDebugUtilsMessageTypeFlagBitsEXT c_debug_message_type = (
    VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | 
    VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | 
    VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT
);

const char *c_required_device_extension_names[] = {
    VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_KHR_SPIRV_1_4_EXTENSION_NAME
};

#define QUEUE_MASK_ALL (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT)

#define QUEUE_MASK_RENDER (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT)
#define QUEUE_MASK_COMPUTE (VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT)
#define QUEUE_MASK_TRANSFER (VK_QUEUE_TRANSFER_BIT)

typedef struct {
    VkPhysicalDevice device;
    VkPhysicalDeviceMemoryProperties memory_properties;
    /* queue layout */
    u32 render_queue_id;
    u32 compute_queue_id;
    u32 transfer_queue_id;
    /* device info */
    DeviceModel device_model;
    u32 device_id;
    char name[VK_MAX_PHYSICAL_DEVICE_NAME_SIZE];
} DeviceInfo;


b32 checkPhysicalDevice(VkPhysicalDevice device, MsgCallback_pfn msg_callback, DeviceInfo *info, Stack *stack) {
    /* be careful inside this function, it might return false because of failed allocation 
        also dont forget to pop stack before return, better use goto _success; goto _fail;*/
    pushStack(stack);

    *info = (DeviceInfo) {
        .device = device,
        .render_queue_id = U32_MAX,
        .compute_queue_id = U32_MAX,
        .transfer_queue_id = U32_MAX
    };

    /* CHECK EXTENSIONS */ {
        u32 available_extension_count = 0;
        vkEnumerateDeviceExtensionProperties(device, NULL, &available_extension_count, NULL);
        /* allocate available_extensions array */
        VkExtensionProperties *available_extensions = allocateStack(stack, available_extension_count * sizeof(VkExtensionProperties), 16);
        if(!available_extensions) {
            MSG_ERROR(msg_callback, &CONST_STRING("failed to allocate available_extensions array"));
            goto _fail;
        }
        vkEnumerateDeviceExtensionProperties(device, NULL, &available_extension_count, available_extensions);

        for(u32 i = 0; i < ARRAY_SIZE(c_required_device_extension_names); i++) {
            for(u32 j = 0; j < available_extension_count; j++) {
                if(cstringCmp(c_required_device_extension_names[i], available_extensions[j].extensionName) == 0) {
                    goto _found_required_extension;
                }
            }
            goto _fail;
            _found_required_extension: {}
        }
    }

    /* QUEUE LAYOUT */ {
        u32 queue_family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, NULL);
        /* allocate queue_families array */
        VkQueueFamilyProperties *queue_families = allocateStack(stack, queue_family_count * sizeof(VkQueueFamilyProperties), 16);
        if(!queue_families) {
            MSG_ERROR(msg_callback, &CONST_STRING("failed to allocate queue_families array"));
            goto _fail;
        }
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families);

        for(u32 i = 0; i < queue_family_count; i++) {
            if(info->render_queue_id == U32_MAX && (queue_families[i].queueFlags & QUEUE_MASK_ALL) == QUEUE_MASK_RENDER) {
                info->render_queue_id = i;
                continue;
            }
            if(info->compute_queue_id == U32_MAX && (queue_families[i].queueFlags & QUEUE_MASK_ALL) == QUEUE_MASK_COMPUTE) {
                info->compute_queue_id = i;
                continue;
            }
            if(info->transfer_queue_id == U32_MAX && (queue_families[i].queueFlags & QUEUE_MASK_ALL) == QUEUE_MASK_TRANSFER) {
                info->transfer_queue_id = i;
                continue;
            }
        }

        /* if render queue doesnt exist device is not supported */
        if(info->render_queue_id == U32_MAX) {
            goto _fail;
        }
    }

    /* DEVICE PROPERTIES */ {
        VkPhysicalDeviceProperties *device_properties = allocateStack(stack, sizeof(VkPhysicalDeviceProperties), 16);
        if(!device_properties) {
            MSG_ERROR(msg_callback, &CONST_STRING("failed to allocate device_properties"));
            goto _fail;
        }
        vkGetPhysicalDeviceProperties(device, device_properties);

        info->device_id = device_properties->deviceID;
        cstringCpy(info->name, device_properties->deviceName);

        if(device_properties->deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            info->device_model = DEVICE_MODEL_DESCRETE;
        }
        if(device_properties->deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
            info->device_model = DEVICE_MODEL_INTEGRATED;
        }

        /* memory properties */
        vkGetPhysicalDeviceMemoryProperties(device, &info->memory_properties);
    }

    /* _success: */ {
        popStack(stack);
        return TRUE;
    }
    _fail: {
        popStack(stack);
        return FALSE;
    }
}

/* return pointer to best of 2 */
const DeviceInfo *compareDevices(const DeviceInfo *a, const DeviceInfo *b) {
    if(b->device_model == DEVICE_MODEL_DESCRETE && a->device_model != DEVICE_MODEL_DESCRETE) return b;
    /* check queues */
    if(b->compute_queue_id != U32_MAX && a->compute_queue_id == U32_MAX) return b;
    if(b->transfer_queue_id != U32_MAX && a->transfer_queue_id == U32_MAX) return b;
    /* default is a */
    return a;
}


b32 allocateVram(VulkanContext *vulkan_context, const VramInfo *info, Vram *vram) {
    if(info->size == 0 || info->memory_type_bits == 0) return FALSE;

    const VkMemoryHeap *memory_heaps = vulkan_context->memory_properties.memoryHeaps;
    const VkMemoryType *memory_types = vulkan_context->memory_properties.memoryTypes;
    const u32 memory_type_count = vulkan_context->memory_properties.memoryTypeCount;

    u32 memory_id = U32_MAX;
    u32 heap_id = U32_MAX;
    /* check memory types for requirements */
    for(u32 i = 0; i < memory_type_count; i++) {
        const u32 flags = memory_types[i].propertyFlags;
        /* not sutiable conditions */
        if((flags & info->mandatory_flags) != info->mandatory_flags) continue;
        if(flags & info->restricted_flags) continue;
        if(!((info->memory_type_bits >> i) & 0x1)) continue;
        if((memory_heaps[memory_types[i].heapIndex].size < info->size)) continue;

        memory_id = i;
        heap_id = memory_types[i].heapIndex;
        goto _found_memory_id;
    }
    return FALSE;
    
    _found_memory_id: {};

    /* allocate memory */
    VkMemoryAllocateInfo allocate_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .memoryTypeIndex = memory_id,
        .allocationSize = info->size
    };
    VkDeviceMemory device_memory = NULL;
    if(vkAllocateMemory(vulkan_context->device, &allocate_info, NULL, &device_memory) != VK_SUCCESS) {
        return FALSE;
    }

    *vram = (Vram) {
        .memory = device_memory,
        .size = info->size,
        .heap_id = heap_id,
        .memory_id = memory_id
    };
    return TRUE;
}

void freeVram(VulkanContext *vulkan_context, Vram *vram) {
    vkFreeMemory(vulkan_context->device, vram->memory, NULL);
    vulkan_context->memory_properties.memoryHeaps[vram->heap_id].size += vram->size;
    *vram = (Vram){0};
}


/* debug utils messenger callback */
VKAPI_ATTR VkBool32 VKAPI_CALL validationDebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT type, const VkDebugUtilsMessengerCallbackDataEXT *callback_data, void *user_data
) {
    String string = (String) {
        .string = (char[512]){0},
        .capacity = 512
    };

    stringPattern(&CONST_STRING(":: vk message :: %c\n"), (const void *[]){callback_data->pMessage}, &string);
    printConsole(&string);
    return !(severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT);
}

VulkanContext *createVulkanContext(Allocate_pfn context_allocate, const VulkanContextInfo *info) {
    String msg_string = (String) {
        .string = (char[256]){0},
        .capacity = 256
    };

    /* VALIDATE INFO */ {
        /* if info it self is valid data */
        if(!info) {
            return NULL;
        }
        /* name should have valid pointer */
        if(!info->name.string) {
            MSG_ERROR(info->msg_callback, &TRACED_STR("VulkanContextInfo param \"name\" invalid"));
            return NULL;
        }
        /* resolution should not be 0 0 */
        if(info->resolution_x == 0 || info->resolution_y == 0) {
            /* %u is 64 bit value so need some transformation */
            const u64 res_x = info->resolution_x;
            const u64 res_y = info->resolution_y;
            /* combine data into string */
            stringPattern(
                &TRACED_STR("VulkanContextInfo param resolution invalid x: %u y: %u "), 
                (const void *[]){&res_x, &res_y}, &msg_string
            );
            /* call error message */
            MSG_ERROR(info->msg_callback, &msg_string);
            return NULL;
        }
        /* check allocator pfn */
        if(!context_allocate) {
            MSG_ERROR(info->msg_callback, &TRACED_STR("context_allocate invalid pfn"));
            return NULL;
        }
    }

    /* allocate context with allocator function */
    VulkanContext *context = context_allocate(sizeof(VulkanContext), 8);
    if(!context) {
        MSG_ERROR(info->msg_callback, &TRACED_STR("failed to allocate context"));
        return NULL;
    }
    /* zero initialize */
    *context = (VulkanContext){.msg_callback = info->msg_callback};

    Stack init_stack = (Stack){0};
    if(!createStack(&init_stack, 1024 * 1024 * 32, 1024 * 64)) {
        MSG_ERROR(context->msg_callback, &TRACED_STR("failed to create init_stack"));
        return NULL; 
    }

    /* GLFW */ {
        if(!glfwInit()) {
            MSG_ERROR(context->msg_callback, &TRACED_STR("glfwInit failed"));
            return NULL;
        }
        /* detach from opengl context, so that vulkan can capture window */
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        /* change resizability */
        glfwWindowHint(GLFW_RESIZABLE, (info->flags & VULKAN_FLAG_RESIZABLE) ? GLFW_TRUE : GLFW_FALSE);
        if(!(context->window = glfwCreateWindow((i32)info->resolution_x, (i32)info->resolution_y, info->name.string, NULL, NULL))) {
            MSG_ERROR(context->msg_callback, &TRACED_STR("failed to create glfw window"));
            return NULL;
        }

        /* LOG */ {
            const u64 res_x = info->resolution_x;
            const u64 res_y = info->resolution_y;
            stringPattern(&CONST_STRING("created glfw window { name: \"%s\" x: %u y: %u }"), (const void *[]){&info->name, &res_x, &res_y}, &msg_string);
            MSG_LOG(context->msg_callback, &msg_string);
        }
    }

    /* start stack frame */
    pushStack(&init_stack);
    /* global instance info */
    u32 required_extension_count = 0;
    const char **required_extensions = NULL;

    /* EXTENSIONS & LAYERS MATCHING */ {
        /* get glfw extensions */
        u32 glfw_extension_count = 0;
        const char **glfw_extension_names = glfwGetRequiredInstanceExtensions(&glfw_extension_count);

        /* calculate number of extensions */
        required_extension_count = (VULKAN_FLAG_DEBUG & info->flags) ? glfw_extension_count + 1 : glfw_extension_count;
        if(required_extension_count == 0) {
            goto _match_instance_layers;
        }

        /* allocate required extension array */
        required_extensions = allocateStack(&init_stack, required_extension_count * sizeof(const char*), 16);
        if(!required_extensions) {
            MSG_ERROR(context->msg_callback, &TRACED_STR("failed to allocate required_extensions array"));
            return NULL;
        }

        /* write extensions */
        for(u32 i = 0; i < glfw_extension_count; i++) {
            required_extensions[i] = glfw_extension_names[i];
        }
        /* if debug layer are on we use one more extension*/
        if(VULKAN_FLAG_DEBUG & info->flags) {
            required_extensions[glfw_extension_count] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
        }
       
        
        /* get instance extensions that are present */
        u32 available_extension_count = 0;
        vkEnumerateInstanceExtensionProperties(NULL, &available_extension_count, NULL);
        /* allocate available exetnsions array */
        VkExtensionProperties *available_extensions = allocateStack(&init_stack, available_extension_count * sizeof(VkExtensionProperties), 16);
        if(!available_extensions) {
            MSG_ERROR(context->msg_callback, &TRACED_STR("failed to allocate available_extensions array"));
            return NULL;
        }
        vkEnumerateInstanceExtensionProperties(NULL, &available_extension_count, available_extensions);


        /* find if required extensions are available */
        for(u32 i = 0; i < required_extension_count; i++) {

            for(u32 j = 0; j < available_extension_count; j++) {
                if(cstringCmp(required_extensions[i], available_extensions[j].extensionName)) {
                   goto _matched_extension;
                }
            }
            /* if not in avaliable list */
            stringPattern(&TRACED_STR("required instance extension unavailbale name: %c"), (const void *[]){&required_extensions[i]}, &msg_string);
            MSG_ERROR(context->msg_callback, &msg_string);
            return NULL;
            /* if avaliable continue to next extension */
            _matched_extension: {};
        }

        _match_instance_layers: {};

        /* check for debug layer if debug flag is set */
        if(info->flags & VULKAN_FLAG_DEBUG) {
            /* get avaliable layers array */
            u32 vulkan_layer_count = 0;
            vkEnumerateInstanceLayerProperties(&vulkan_layer_count, NULL);
            if(vulkan_layer_count == 0) {
                MSG_ERROR(context->msg_callback, &CONST_STRING("required vulkan debug layer but no layers are present on device"));
                return NULL;
            }

            /* allocate available layers array */
            VkLayerProperties *avaliable_layers = allocateStack(&init_stack, vulkan_layer_count * sizeof(VkLayerProperties), 16);
            if(!avaliable_layers) {
                MSG_ERROR(context->msg_callback, &TRACED_STR("failed to allocate avaliable_layers array"));
                return NULL;
            }
            vkEnumerateInstanceLayerProperties(&vulkan_layer_count, avaliable_layers);

            /* check if debug layer is present */
            for(u32 i = 0; i < vulkan_layer_count; i++) {
                if(cstringCmp(c_debug_layer_name.string, avaliable_layers[i].layerName)) {
                    goto _found_debug_layer;
                }
            }
            /* if debug layer not found */
            stringPattern(&TRACED_STR("debug layer unavaliable: %s"), (const void *[]){&c_debug_layer_name}, &msg_string);
            MSG_ERROR(context->msg_callback, &msg_string);
            return NULL;
            /* if found debug layer all good */
            _found_debug_layer: {};
        }
    }

    /* INSTANCE & DEBUG MESSENGER & SURFACE */ {
        const VkApplicationInfo app_info = {
            .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pEngineName = "Wreck",
            .engineVersion = VK_MAKE_VERSION(1, 0, 0),
            .pApplicationName = info->name.string,
            .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
            .apiVersion = VK_API_VERSION_1_3
        };

        /* used only if debug flag is enabled */
        const VkDebugUtilsMessengerCreateInfoEXT debug_utils_info = {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            .pfnUserCallback = &validationDebugCallback,
            .messageSeverity = c_debug_message_severity,
            .messageType = c_debug_message_type
        };

        const VkInstanceCreateInfo instance_info = {
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .flags = 0,
            .pApplicationInfo = &app_info,
            .ppEnabledExtensionNames = required_extensions,
            .enabledExtensionCount = required_extension_count,
            .ppEnabledLayerNames = (info->flags & VULKAN_FLAG_DEBUG) ? (const char**)&c_debug_layer_name.string : NULL, /* be careful here*/
            .enabledLayerCount = (info->flags & VULKAN_FLAG_DEBUG) ? 1 : 0, /* be careful here*/
            .pNext = (info->flags & VULKAN_FLAG_DEBUG) ? &debug_utils_info : NULL /* be careful here*/
        };

        /* create instance */
        if(vkCreateInstance(&instance_info, NULL, &context->instance) != VK_SUCCESS) {
            MSG_ERROR(context->msg_callback, &TRACED_STR("failed to create vulkan instance"));
            return NULL;
        }

        /* LOG INSTANCE */ {
            /* extensions */
            stringZero(&msg_string);
            stringAddCstring(&msg_string, "vulkan instance extensions { ");
            for(u32 i = 0; i < instance_info.enabledExtensionCount; i++) {
                stringAddChar(&msg_string, '\"');
                stringAddCstring(&msg_string, instance_info.ppEnabledExtensionNames[i]);
                stringAddChar(&msg_string, '\"');
                stringAddChar(&msg_string, ' ');
            }
            stringAddCstring(&msg_string, "}");
            MSG_LOG(context->msg_callback, &msg_string);

            /* layers */
            stringZero(&msg_string);
            stringAddCstring(&msg_string, "vulkan instance layers { ");
            for(u32 i = 0; i < instance_info.enabledLayerCount; i++) {
                stringAddChar(&msg_string, '\"');
                stringAddCstring(&msg_string, instance_info.ppEnabledLayerNames[i]);
                stringAddChar(&msg_string, '\"');
                stringAddChar(&msg_string, ' ');
            }
            stringAddCstring(&msg_string, "}");
            MSG_LOG(context->msg_callback, &msg_string);
        }

        /* create debug messenger if debug flag is set */
        if(info->flags & VULKAN_FLAG_DEBUG) {
            /* load ext functions for debug utils messenger */
            if(!(context->create_debug_messenger = (void *)vkGetInstanceProcAddr(context->instance, "vkCreateDebugUtilsMessengerEXT"))) {
                MSG_ERROR(context->msg_callback, &TRACED_STR("failed to load vkCreateDebugUtilsMessengerEXT"));
                return NULL;
            }
            if(!(context->destroy_debug_messenger = (void *)vkGetInstanceProcAddr(context->instance, "vkDestroyDebugUtilsMessengerEXT"))) {
                MSG_ERROR(context->msg_callback, &TRACED_STR("failed to load vkDestroyDebugUtilsMessengerEXT"));
                return NULL;
            }

            /* actual creation of debug utils messenger */
            if(context->create_debug_messenger(context->instance, &debug_utils_info, NULL, &context->debug_messenger) != VK_SUCCESS) {
                MSG_ERROR(context->msg_callback, &TRACED_STR("failed to create vulkan debug messenger"));
                return NULL;
            }
            /* log debug utils */
            MSG_LOG(context->msg_callback, &CONST_STRING("created debug messenger, vulkan validation is on"));
        }

        /* create surface */
        if(glfwCreateWindowSurface(context->instance, context->window, NULL, &context->surface) != VK_SUCCESS) {
            MSG_ERROR(context->msg_callback, &TRACED_STR("failed to create vulkan surface"));
            return NULL;
        }
    }
    
    clearStack(&init_stack);

    const DeviceInfo *best_device_info = NULL;

    /* DEVICE SELECTION */ {
        /* request physical device count */
        u32 physical_device_count = 0;
        vkEnumeratePhysicalDevices(context->instance, &physical_device_count, NULL);
        if(physical_device_count == 0) {
            MSG_ERROR(context->msg_callback, &TRACED_STR("failed to find any rendering device"));
            return NULL;
        }
        /* allocate physical devices array */
        VkPhysicalDevice *physical_devices = allocateStack(&init_stack, physical_device_count * sizeof(VkPhysicalDevice), 16);
        if(!physical_devices) {
            MSG_ERROR(context->msg_callback, &TRACED_STR("failed to allocate physical_devices array"));
            return NULL;
        }
        /* request physical devices */
        vkEnumeratePhysicalDevices(context->instance, &physical_device_count, physical_devices);

        /* allocate device infos array */
        DeviceInfo *device_infos = allocateStack(&init_stack, physical_device_count * sizeof(DeviceInfo), 16);
        if(!device_infos) {
            MSG_ERROR(context->msg_callback, &TRACED_STR("failed to allocate device_infos array"));
            return NULL;
        }
        /* get device_infos and count suitable devices */
        u32 device_infos_count = 0;
        for(u32 i = 0; i < physical_device_count; i++) {
            /* if device suitable add device info to the array */
            if(checkPhysicalDevice(physical_devices[i], context->msg_callback, &device_infos[device_infos_count], &init_stack)) {
                device_infos_count++;
            }
        }
        /* if no suitable device exists */
        if(device_infos_count == 0) {
            MSG_ERROR(context->msg_callback, &TRACED_STR("avaliable rendering devices are not suitable"));
            return NULL;
        }

        /* find best device by comparasion */
        best_device_info = &device_infos[0];
        for(u32 i = 1; i < device_infos_count; i++) {
            best_device_info = compareDevices(best_device_info, &device_infos[i]);
        }

        /* LOG BEST DEVICE INFO */ {
            const u64 device_id = best_device_info->device_id;
            const u64 device_model = best_device_info->device_model;
            const String yes = CONST_STRING("YES");
            const String no = CONST_STRING("NO");
            stringPattern(
                &CONST_STRING("selected device { name: \"%c\" id: %u model: %u render: %s compute: %s transfer: %s }"),
                (const void *[]) {
                    best_device_info->name, &device_id, &device_model, 
                    (best_device_info->render_queue_id == U32_MAX) ? &no : &yes,
                    (best_device_info->compute_queue_id == U32_MAX) ? &no : &yes,
                    (best_device_info->transfer_queue_id == U32_MAX) ? &no : &yes
                },
                &msg_string
            );
            MSG_LOG(context->msg_callback, &msg_string);
        }
    }

    /* DEVICE CREATION */ {
        /* layout queue create infos*/
        u32 device_queue_count = 1; /* render queue always should exist */
        f32 queue_priority = 1.0; /* vulkan bullshit */
        VkDeviceQueueCreateInfo device_queue_infos[3] = {
            (VkDeviceQueueCreateInfo) {
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .queueFamilyIndex = best_device_info->render_queue_id,
                .pQueuePriorities = &queue_priority,
                .queueCount = 1
            },
            (VkDeviceQueueCreateInfo){0},
            (VkDeviceQueueCreateInfo){0}
        };
        /* compute is queue 1 if exists */
        if(best_device_info->compute_queue_id != U32_MAX) {
            device_queue_infos[device_queue_count++] = (VkDeviceQueueCreateInfo) {
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .queueFamilyIndex = best_device_info->compute_queue_id,
                .pQueuePriorities = &queue_priority,
                .queueCount = 1
            };
        }
        /* transfer is queue 1 if exists and compute doesn't exist, 2 if compute exists and transfer exists */
        if(best_device_info->transfer_queue_id != U32_MAX) {
            device_queue_infos[device_queue_count++] = (VkDeviceQueueCreateInfo) {
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .queueFamilyIndex = best_device_info->transfer_queue_id,
                .pQueuePriorities = &queue_priority,
                .queueCount = 1
            };
        }

        /* dynamic rendering ext */
        VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamic_rendering_info = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR,
            .dynamicRendering = TRUE
        };
        /* create device */
        VkDeviceCreateInfo device_info = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .ppEnabledExtensionNames = c_required_device_extension_names,
            .enabledExtensionCount = ARRAY_SIZE(c_required_device_extension_names),
            .pQueueCreateInfos = device_queue_infos,
            .queueCreateInfoCount = device_queue_count,
            .pNext = &dynamic_rendering_info
        };
        if(vkCreateDevice(best_device_info->device, &device_info, NULL, &context->device) != VK_SUCCESS) {
            MSG_ERROR(context->msg_callback, &TRACED_STR("failed to create vulkan device"));
            return NULL;
        }

        /* set queue indices and physical device  */
        context->physical_device = best_device_info->device;
        context->device_model = best_device_info->device_model;
        context->memory_properties = best_device_info->memory_properties;
        context->render_queue_id = best_device_info->render_queue_id;
        context->compute_queue_id = best_device_info->compute_queue_id;
        context->transfer_queue_id = best_device_info->transfer_queue_id;
        /* get queue objects  */
        vkGetDeviceQueue(context->device, best_device_info->render_queue_id, 0, &context->render_queue);
        if(best_device_info->compute_queue_id != U32_MAX) {
            vkGetDeviceQueue(context->device, best_device_info->compute_queue_id, 0, &context->compute_queue);
        }
        if(best_device_info->transfer_queue_id != U32_MAX) {
            vkGetDeviceQueue(context->device, best_device_info->transfer_queue_id, 0, &context->transfer_queue);
        }
    }

    /* LOAD EXTENSIONS */ {
        if(!(context->cmd_begin_rendering = (void *)vkGetDeviceProcAddr(context->device, "vkCmdBeginRenderingKHR"))) {
            MSG_ERROR(context->msg_callback, &TRACED_STR("failed to load vkCmdBeginRenderingKHR"));
            return NULL;
        }
        if(!(context->cmd_end_rendering = (void *)vkGetDeviceProcAddr(context->device, "vkCmdEndRenderingKHR"))) {
            MSG_ERROR(context->msg_callback, &TRACED_STR("failed to load vkCmdEndRenderingKHR"));
            return NULL;
        }
    }

    freeStack(&init_stack);

    MSG_LOG(context->msg_callback, &CONST_STRING("created vulkan context"));

    return context;
}

void destroyVulkanContext(VulkanContext *context) {
    /* INSTANCE */ {
        vkDestroyDevice(context->device, NULL);
        vkDestroySurfaceKHR(context->instance, context->surface, NULL);
        if(context->debug_messenger) {
            context->destroy_debug_messenger(context->instance, context->debug_messenger, NULL);
        }
        vkDestroyInstance(context->instance, NULL);
    }

    /* GLFW */ {
        glfwDestroyWindow(context->window);
        glfwTerminate();
    }

    MSG_LOG(context->msg_callback, &CONST_STRING("destroyed vulkan context"));
}
