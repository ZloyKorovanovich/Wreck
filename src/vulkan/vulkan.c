#include "vulkan.h"

/* settings for debug utils messenger callback */
const char* c_debug_layer_name = "VK_LAYER_KHRONOS_validation";
const VkDebugUtilsMessageSeverityFlagsEXT c_debug_message_severity = (
    VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | 
    VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT
);
const VkDebugUtilsMessageTypeFlagBitsEXT c_debug_message_type = (
    VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | 
    VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | 
    VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT
);
/* debug utils messenger callback */
VKAPI_ATTR VkBool32 VKAPI_CALL validationDebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT type, const VkDebugUtilsMessengerCallbackDataEXT* callback_data, void* user_data
) {
    printf("%s\n", callback_data->pMessage);
    return !(severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT);
}

/* necessary device extensions, as minimum of them as possible */
const u32 c_required_device_extension_names_count = 3;
const char* c_required_device_extension_names[3] = {
    VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_KHR_SPIRV_1_4_EXTENSION_NAME
};


/* used to get physical device info only once */
typedef struct {
    u32 score;
    u32 type;
    u32 render_family;
    u32 compute_family;
    u32 transfer_family;
    VkPhysicalDevice device;
    char device_name[VK_MAX_DRIVER_NAME_SIZE];
    u32 device_id;
} DeviceInfo;


/* function that determines if physical device is suitable or not */
b32 vulkanCheckPhysicalDevice(VkPhysicalDevice device, const char** required_extension_names, u32 required_extension_name_count, DeviceInfo* device_info) {
    #define PHYSICAL_DEVICE_MAX_QUEUE_FAMILIES 64
    #define PHYSICAL_DEVICE_MAX_EXTENSIONS 512
    #define QUEUE_FLAGS_MASK (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT)

    *device_info = (DeviceInfo) {
        .device = device
    };

    /* MATCHING EXTENSIONS */ {
        VkExtensionProperties extension_properties[PHYSICAL_DEVICE_MAX_EXTENSIONS] = {0};
        u32 extension_property_count = 0;

        /* getting extensions */
        vkEnumerateDeviceExtensionProperties(device, NULL, &extension_property_count, NULL);
        extension_property_count = MIN(extension_property_count, PHYSICAL_DEVICE_MAX_EXTENSIONS);
        vkEnumerateDeviceExtensionProperties(device, NULL, &extension_property_count, extension_properties);

        for(u32 i = 0; i < required_extension_name_count; i++) {
            for(u32 j = 0; j < extension_property_count; j++) {
                if(strcmp(required_extension_names[i], extension_properties[j].extensionName) == 0) {
                    goto _found_required_extension;
                }
            }
            //_not_found_required_extension
            return FALSE;
            _found_required_extension: {}
        }
    }

    /* QUEUE LAYOUT */ {
        VkQueueFamilyProperties family_properties[PHYSICAL_DEVICE_MAX_QUEUE_FAMILIES] = {0};
        u32 family_property_count = 0;

        /* getting families */
        vkGetPhysicalDeviceQueueFamilyProperties(device, &family_property_count, NULL);
        family_property_count = MIN(family_property_count, PHYSICAL_DEVICE_MAX_QUEUE_FAMILIES);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &family_property_count, family_properties);

        /* search for queue indices */
        device_info->render_family = device_info->compute_family = device_info->transfer_family = U32_MAX;
        for(u32 i = 0; i < family_property_count; i++) {
            if(device_info->render_family == U32_MAX && (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT) == (QUEUE_FLAGS_MASK & family_properties[i].queueFlags)) {
                device_info->render_family = i;
                continue;
            }
            if(device_info->compute_family == U32_MAX && (VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT) == (QUEUE_FLAGS_MASK & family_properties[i].queueFlags)) {
                device_info->compute_family = i;
                continue;
            }
            if(device_info->transfer_family == U32_MAX && (VK_QUEUE_TRANSFER_BIT) == (QUEUE_FLAGS_MASK & family_properties[i].queueFlags)) {
                device_info->transfer_family = i;
                continue;
            }
        }
        if(device_info->render_family == U32_MAX) {
            return FALSE;
        }
    }

    /* DEVICE PROPERTIES */ {
        VkPhysicalDeviceProperties device_properties = (VkPhysicalDeviceProperties){0};
        vkGetPhysicalDeviceProperties(device, &device_properties);
        if(device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            device_info->score += 1000;
            device_info->type = DEVICE_TYPE_DESCRETE;
        } else if(device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
            device_info->score += 500;
            device_info->type = DEVICE_TYPE_INTEGRATED;
        }

        strcpy(device_info->device_name, device_properties.deviceName);
        device_info->device_id = device_properties.deviceID;
    }

    return TRUE;
}


VramAllocation* allocateVram(VulkanContext* vulkan_context, const VramAllocationInfo* info) {
    if(vulkan_context->vram_allocation_count >= MAX_VRAM_ALLOCATIONS || info->size == FALSE || info->memory_type_flags == 0) return NULL;

    /* find free slot for record */
    VramAllocation* allocation_slot = NULL;
    for(u32 i = 0; i < MAX_VRAM_ALLOCATIONS; i++) {
        if(vulkan_context->vram_allocations[i].size == 0) {
            allocation_slot = &vulkan_context->vram_allocations[i];
            break;
        }
    }

    u32 memory_id = U32_MAX;
    u32 heap_id = U32_MAX;
    VkPhysicalDeviceMemoryProperties* memory_properties = &vulkan_context->memory_properties;
    const u32 memory_type_count = memory_properties->memoryTypeCount;
    for(u32 i = 0; i < memory_type_count; i++) {
        VkMemoryType memory_type = memory_properties->memoryTypes[i];
        if(
            ((memory_type.propertyFlags & info->positive_flags) == info->positive_flags) && 
            !(memory_type.propertyFlags & info->negative_flags) &&
            (memory_type.propertyFlags & info->memory_type_flags) &&
            memory_properties->memoryHeaps[memory_type.heapIndex].size > info->size
        ) {
            memory_id = i;
            heap_id = memory_type.heapIndex;
            memory_properties->memoryHeaps[memory_type.heapIndex].size -= info->size;
            goto _found_memory; 
        }
    }
    //_not_found_memory_id:
    return NULL;

    _found_memory: {
        /* allocation call */
        VkMemoryAllocateInfo alloc_info = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .memoryTypeIndex = memory_id,
            .allocationSize = info->size
        };
        VkDeviceMemory memory = NULL;
        if(vkAllocateMemory(vulkan_context->device, &alloc_info, NULL, &memory) != VK_SUCCESS) {
            return FALSE;
        }
        *allocation_slot = (VramAllocation) {
            .size = info->size,
            .heap_id = heap_id,
            .type_id = memory_id,
            .memory = memory
        };
        vulkan_context->vram_allocation_count++;
        return allocation_slot;
    }
}

void freeVram(VulkanContext* vulkan_context, VramAllocation* allocation) {
    if(allocation->size == 0) return;
    vkFreeMemory(vulkan_context->device, allocation->memory, NULL);
    vulkan_context->memory_properties.memoryHeaps[allocation->heap_id].size += allocation->size;
    *allocation = (VramAllocation){0};
    vulkan_context->vram_allocation_count--;
}


i32 createVulkanContext(const VulkanContextInfo* info, MsgCallback_pfn msg_callback, VulkanContext** vulkan_context) {
    /* ContextCreateBuffer params to control heap allocation size*/
    #define MAX_REQUIRED_EXT_COUNT 128
    #define MAX_INSTANCE_EXT_COUNT 512
    #define MAX_INSATNCE_LAYER_COUNT 512
    #define MAX_PHYSICAL_DEVICE_COUNT 8

    /* Layout of ContextCreateBuffer, it is used as a heap buffer for extension arrays and other */
    typedef struct{
        const char* required_extension_names[MAX_REQUIRED_EXT_COUNT];
        u32 required_extension_name_count;
        VkExtensionProperties instance_extensions[MAX_INSTANCE_EXT_COUNT];
        u32 instance_extension_count;
        VkLayerProperties insatnce_layers[MAX_INSATNCE_LAYER_COUNT];
        u32 instance_layer_count;
        VkPhysicalDevice physical_devices[MAX_PHYSICAL_DEVICE_COUNT];
        u32 physical_device_count;
        DeviceInfo suitable_device_infos[MAX_PHYSICAL_DEVICE_COUNT];
        u32 suitable_device_count;
    } ContextCreateBuffer;

    char log_message[512];
    ContextCreateBuffer* context_create_buffer;

    VulkanContext* context = malloc(sizeof(VulkanContext));
    if(!vulkan_context) {
        MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_CONTEXT_CREATE_BUFFER_MALLOC, "failed to allocate vulkan context");
    }
    *context = (VulkanContext){0};


    /* ALLOCATE CONTEXT BUFFER */ {
        context_create_buffer = (ContextCreateBuffer*)malloc(sizeof(ContextCreateBuffer)); /* ContextCreateBuffer is freed in the end of this function */
        if(!context_create_buffer) {
            MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_CONTEXT_CREATE_BUFFER_MALLOC, "failed to allocate context buffer");
        }
        *context_create_buffer = (ContextCreateBuffer){0};
    }

    
    /* GLFW */ {
        if(!glfwInit()) {
            MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_GLFW_INIT, "failed to init glfw!");
        }
        /* log glfw initialization */
        MSG_CALLBACK_NO_TRACE(msg_callback, MSG_CODE_SUCCESS, "initialized glfw");

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); /* detach from opengl context, so that vulkan can capture window */
        glfwWindowHint(GLFW_RESIZABLE, VULKAN_FLAG_WIN_RESIZE & info->flags ? GLFW_TRUE : GLFW_FALSE);
        if(!(context->window = glfwCreateWindow((i32)info->x, (i32)info->y, info->name, NULL, NULL))) {
            MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_WINDOW_CREATE, "failed to create window!");
        }

        /* LOG */ {
            /* make create window log */
            strcpy(log_message, "created glfw window name: \"");
            strcat(log_message, info->name);
            strcat(log_message, "\" x: ");
            strcat_u32(log_message, info->x);
            strcat(log_message, " y: ");
            strcat_u32(log_message, info->y);
            MSG_CALLBACK_NO_TRACE(msg_callback, MSG_CODE_SUCCESS, log_message);
        }
    }

    /* EXTENSIONS & LAYERS MATCHING */ {
        /* first we need to get and copy glfw extensions */
        u32 glfw_extension_count = 0;
        const char** glfw_extension_names = glfwGetRequiredInstanceExtensions(&glfw_extension_count);
        glfw_extension_count = MIN(glfw_extension_count, MAX_REQUIRED_EXT_COUNT - 2);
        for(u32 i = 0; i < glfw_extension_count; i++) {
            context_create_buffer->required_extension_names[i] = glfw_extension_names[i];
        }
        /* then add vk extensions that we need we use VK_EXT_DEBUG_UTILS_EXTENSION_NAME only if debug flag is on, but we will store pointer anyway */
        context_create_buffer->required_extension_names[glfw_extension_count] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
        context_create_buffer->required_extension_name_count = (VULKAN_FLAG_DEBUG & info->flags) ? glfw_extension_count + 1 : glfw_extension_count;

        /* now get instance extensions that are present */
        vkEnumerateInstanceExtensionProperties(NULL, &context_create_buffer->instance_extension_count, NULL);
        context_create_buffer->instance_extension_count = MIN(context_create_buffer->instance_extension_count, MAX_INSTANCE_EXT_COUNT);
        vkEnumerateInstanceExtensionProperties(NULL, &context_create_buffer->instance_extension_count, context_create_buffer->instance_extensions);

        /* find all of required extensions in present extensions */
        const u32 required_ext_count = context_create_buffer->required_extension_name_count;
        const u32 instance_ext_count = context_create_buffer->instance_extension_count;
        for(u32 i = 0; i < required_ext_count; i++) {
            for(u32 j = 0; j < instance_ext_count; j++) {
                if(strcmp(context_create_buffer->required_extension_names[i], context_create_buffer->instance_extensions[j].extensionName) == 0) {
                    goto _found_instance_ext;
                }
            }
            //_not_found_instance_ext:
            MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_INSATNCE_EXTENSION_MISMATCH, "failed to match required extension to instance extensions");
            _found_instance_ext: {}
        }

        /* enable vulkan debug layers if necessary */
        if(info->flags & VULKAN_FLAG_DEBUG) {
            vkEnumerateInstanceLayerProperties(&context_create_buffer->instance_layer_count, NULL);
            context_create_buffer->instance_layer_count = MIN(context_create_buffer->instance_layer_count, MAX_INSATNCE_LAYER_COUNT);
            vkEnumerateInstanceLayerProperties(&context_create_buffer->instance_layer_count, context_create_buffer->insatnce_layers);
            
            /* check if debug layer is present */
            const u32 instance_layer_count = context_create_buffer->instance_layer_count;
            for(u32 i = 0; i < instance_layer_count; i++) {
                if(strcmp(context_create_buffer->insatnce_layers[i].layerName, c_debug_layer_name) == 0) {
                    goto _found_layer;
                }
            }
        //_not_found_layer:
            strcpy(log_message, "failed to match required layer:");
            strcat(log_message, c_debug_layer_name);
            strcat(log_message, LOCATION_TRACE);
            MSG_CALLBACK_NO_TRACE(msg_callback, MSG_CODE_ERROR_VK_LAYERS_MISMATCH, log_message);
        _found_layer: {}
        }
    }

    /* INSTANCE & DEBUG MESSENGER & SURFACE CREATION */ {
        const VkApplicationInfo app_info = {
            .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pEngineName = ENGINE_NAME,
            .engineVersion = ENGINE_VERSION,
            .pApplicationName = info->name,
            .applicationVersion = info->version,
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
            .ppEnabledExtensionNames = context_create_buffer->required_extension_names,
            .enabledExtensionCount = context_create_buffer->required_extension_name_count,
            .ppEnabledLayerNames = (info->flags & VULKAN_FLAG_DEBUG) ? &c_debug_layer_name : NULL, /* be careful here*/
            .enabledLayerCount = (info->flags & VULKAN_FLAG_DEBUG) ? 1 : 0, /* be careful here*/
            .pNext = (info->flags & VULKAN_FLAG_DEBUG) ? &debug_utils_info : NULL /* be careful here*/
        };

        if(vkCreateInstance(&instance_info, NULL, &context->instance) != VK_SUCCESS) {
            MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_INSTANCE_CREATE, "failed to create vulkan instance");
        }

        /* LOG */ {
            /* log extensions */
            strcpy(log_message, "vulkan instance extensions: {");
            for(u32 i = 0; i < instance_info.enabledExtensionCount; i++) {
                strcat(log_message, "\"");
                strcat(log_message, instance_info.ppEnabledExtensionNames[i]);
                strcat(log_message, "\"");
            }
            strcat(log_message,"}");
            MSG_CALLBACK_NO_TRACE(msg_callback, MSG_CODE_SUCCESS, log_message);
            /* log layers */
            strcpy(log_message, "vulkan instance layers: {");
            for(u32 i = 0; i < instance_info.enabledLayerCount; i++) {
                strcat(log_message, "\"");
                strcat(log_message, instance_info.ppEnabledLayerNames[i]);
                strcat(log_message, "\"");
            }
            strcat(log_message, "}");
            MSG_CALLBACK_NO_TRACE(msg_callback, MSG_CODE_SUCCESS, log_message);
        }

        /* create debug messenger if debug flag is set */
        if(info->flags & VULKAN_FLAG_DEBUG) {
            /* load ext functions for debug utils messenger */
            if(!(context->ext_create_debug_utils_messenger = (void*)vkGetInstanceProcAddr(context->instance, "vkCreateDebugUtilsMessengerEXT"))) {
                MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_LOAD_PROC, "failed to load vkCreateDebugUtilsMessengerEXT");
            }
            if(!(context->ext_destroy_debug_utils_messenger = (void*)vkGetInstanceProcAddr(context->instance, "vkDestroyDebugUtilsMessengerEXT"))) {
                MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_LOAD_PROC, "failed to load vkDestroyDebugUtilsMessengerEXT");
            }

            /* actual creation of debug utils messenger */
            if(context->ext_create_debug_utils_messenger(context->instance, &debug_utils_info, NULL, &context->debug_messenger) != VK_SUCCESS) {
                MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_DEBUG_MESSENGER_CREATE, "failed to load vulkan debug messenger");
            }
            /* log debug utils */
            MSG_CALLBACK_NO_TRACE(msg_callback, MSG_CODE_SUCCESS, "created vulkan debug utils messenger");
        }

        /* create surface ASAP */
        if(glfwCreateWindowSurface(context->instance, context->window, NULL, &context->surface) != VK_SUCCESS) {
            MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_SURFACE_CREATE, "failed to create vulkan surface");
        }
    }

    /* DEVICE SELECTION & CREATION */ {
        /* get device options */
        vkEnumeratePhysicalDevices(context->instance, &context_create_buffer->physical_device_count, NULL);
        context_create_buffer->physical_device_count = MIN(context_create_buffer->physical_device_count, MAX_PHYSICAL_DEVICE_COUNT);
        vkEnumeratePhysicalDevices(context->instance, &context_create_buffer->physical_device_count, context_create_buffer->physical_devices);

        /* layout queues and check if its suitable */
        const u32 device_count = context_create_buffer->physical_device_count;
        for(u32 i = 0; i < device_count; i++) {
            DeviceInfo* device_info = &context_create_buffer->suitable_device_infos[context_create_buffer->suitable_device_count];
            if(vulkanCheckPhysicalDevice(context_create_buffer->physical_devices[i], c_required_device_extension_names, c_required_device_extension_names_count, device_info)) {
                context_create_buffer->suitable_device_count++;
            }
        }

        /* if gpu suitable not found */
        if(context_create_buffer->suitable_device_count == 0) {
            MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_FAILED_TO_FIND_GPU, "failed to find suitable gpu");
        }
        /* select the best option */
        const DeviceInfo* best_device_info = context_create_buffer->suitable_device_infos;
        for(u32 i = 0; i < context_create_buffer->suitable_device_count; i++) {
            if(best_device_info->score < context_create_buffer->suitable_device_infos[i].score) {
                best_device_info = &context_create_buffer->suitable_device_infos[i];
            }
        }

        /* LOG DEVICE */ {
            strcpy(log_message, "selected gpu name: ");
            strcat(log_message, best_device_info->device_name);
            strcat(log_message, " device id: ");
            strcat_u32(log_message, best_device_info->device_id);
            strcat(log_message, " device type: ");
            strcat_u32(log_message, best_device_info->type);
            strcat(log_message, " queues {");
            strcat(log_message, "render: ");
            strcat_u32(log_message, best_device_info->render_family);
            strcat(log_message, " compute: ");
            if(best_device_info->compute_family != U32_MAX) {
                strcat_u32(log_message, best_device_info->compute_family);
            } else {
                strcat(log_message, "NO");
            }
            strcat(log_message, " transfer: ");
            if(best_device_info->transfer_family != U32_MAX) {
                strcat_u32(log_message, best_device_info->transfer_family);
            } else {
                strcat(log_message, "NO");
            }
            strcat(log_message, "}");
            MSG_CALLBACK_NO_TRACE(msg_callback, MSG_CODE_SUCCESS, log_message);
        }

        /* now we need to create VkDevice */
        u32 queue_info_count = 0;
        VkDeviceQueueCreateInfo queue_infos[3] = {0};
        f32 queue_priorities[3] = {1.0f, 1.0f, 1.0f};

        /* render queue should always exist or device check will fail */
        queue_infos[queue_info_count] = (VkDeviceQueueCreateInfo) {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = best_device_info->render_family,
            .queueCount = 1,
            .pQueuePriorities = queue_priorities + queue_info_count
        };
        queue_info_count++;
        /* add transfer queue if exists */
        if(best_device_info->compute_family != U32_MAX) {
            queue_infos[queue_info_count] = (VkDeviceQueueCreateInfo) {
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .queueFamilyIndex = best_device_info->compute_family,
                .queueCount = 1,
                .pQueuePriorities = queue_priorities + queue_info_count
            };
            queue_info_count++;
        }
        if(best_device_info->transfer_family != U32_MAX) {
            queue_infos[queue_info_count] = (VkDeviceQueueCreateInfo) {
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .queueFamilyIndex = best_device_info->transfer_family,
                .queueCount = 1,
                .pQueuePriorities = queue_priorities + queue_info_count
            };
            queue_info_count++;
        }

        VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamic_rendering_feature = (VkPhysicalDeviceDynamicRenderingFeaturesKHR) {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR,
            .dynamicRendering = VK_TRUE,
            .pNext = NULL
        };
        VkDeviceCreateInfo device_info = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .ppEnabledExtensionNames = c_required_device_extension_names,
            .enabledExtensionCount = c_required_device_extension_names_count,
            .pQueueCreateInfos = queue_infos,
            .queueCreateInfoCount = queue_info_count,
            .pNext = &dynamic_rendering_feature
        };
        /* copy physical device params */
        context->physical_device = best_device_info->device;
        context->device_type = best_device_info->type;
        vkGetPhysicalDeviceMemoryProperties(context->physical_device, &context->memory_properties);
        /* create device */
        if(vkCreateDevice(context->physical_device, &device_info, NULL, &context->device) != VK_SUCCESS) {
            MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_CREATE_DEVICE, "failed to create vulkan device");
        }

        /* get queues if they exists, render queue always exist or something is horribly bad */
        context->render_family_id = best_device_info->render_family;
        vkGetDeviceQueue(context->device, best_device_info->render_family, 0, &context->render_queue);
        if((context->compute_family_id = best_device_info->compute_family) != U32_MAX) {
            vkGetDeviceQueue(context->device, best_device_info->compute_family, 0, &context->compute_queue);
        }
        if((context->transfer_family_id = best_device_info->transfer_family) != U32_MAX) {
            vkGetDeviceQueue(context->device, best_device_info->transfer_family, 0, &context->transfer_queue);
        }

    }

    /* LOAD EXTENSIONS */ {
        if(!(context->cmd_begin_rendering_khr = (void*)vkGetDeviceProcAddr(context->device, "vkCmdBeginRenderingKHR"))) {
            MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_LOAD_PROC, "failed to load vkCmdBeginRenderingKHR procedure");
        }
        if(!(context->cmd_end_rendering_khr = (void*)vkGetDeviceProcAddr(context->device, "vkCmdEndRenderingKHR"))) {
            MSG_CALLBACK(msg_callback, MSG_CODE_ERROR_VK_LOAD_PROC, "failed to load vkCmdEndRenderingKHR procedure");
        }
    }

    free(context_create_buffer); /* end of ContextCreateBuffer lifetime, its not needed anymore */
    *vulkan_context = context;

    MSG_CALLBACK_NO_TRACE(msg_callback, MSG_CODE_SUCCESS, "created vulkan context");
    return MSG_CODE_SUCCESS;
}

i32 destroyVulkanContext(MsgCallback_pfn msg_callback, VulkanContext* context) {
    char msg_log[256] = {0};

    u64 vram_arena_size = 0;
    const u32 vram_allocation_count = context->vram_allocation_count;
    for(u32 i = 0; i < MAX_VRAM_ALLOCATIONS && vram_allocation_count != 0; i++) {
        if(context->vram_allocations[i].size != 0) {
            vram_arena_size += context->vram_allocations[i].size;
            vkFreeMemory(context->device, context->vram_allocations[i].memory, NULL);
        }
    }

    /* LOG */ {
        strcpy(msg_log, "free vram arena of size: ");
        strcat_u64(msg_log, vram_arena_size);
        MSG_CALLBACK_NO_TRACE(msg_callback, MSG_CODE_SUCCESS, msg_log);
    }

    vkDestroyDevice(context->device, NULL);
    vkDestroySurfaceKHR(context->instance, context->surface, NULL);
    if(context->debug_messenger) {
        context->ext_destroy_debug_utils_messenger(context->instance, context->debug_messenger, NULL);
    }
    vkDestroyInstance(context->instance, NULL);
    glfwDestroyWindow(context->window);
    glfwTerminate();

    free(context);
    MSG_CALLBACK_NO_TRACE(msg_callback, MSG_CODE_SUCCESS, "destroyed vulkan context");

    return MSG_CODE_SUCCESS;
}
