#define VULKAN_INTERNAL
#include "gpu.h"

/* FIX: remake window procedures */
#ifdef _WIN32
    /* window function that proceeds events */
    LRESULT CALLBACK windowProcedure(
        HWND hwnd, 
        UINT uMsg, 
        WPARAM wParam, 
        LPARAM lParam
    ) {
        switch (uMsg) {
            case WM_CLOSE:
                PostQuitMessage(0);
                return 0;
            case WM_KEYDOWN:
                if (wParam == VK_ESCAPE) {
                    PostQuitMessage(0);
                }
                return 0;
            default:
                return DefWindowProc(hwnd, uMsg, wParam, lParam);
        }
    }

    /* creates win32 windows handle */
    static GPUWindow createWindow(
        const char *name, 
        u32 x, 
        u32 y, 
        u32 flags,
        MsgCallback_pfn msg_callback
    ) {
        /* get hInstance for class and window creation */
        HMODULE module = GetModuleHandle(NULL);

        /* register window class */
        WNDCLASSEX window_class_info = {
            .cbSize = sizeof(WNDCLASSEX),
            .style = CS_HREDRAW | CS_VREDRAW,
            .lpfnWndProc = windowProcedure,
            .hInstance = module,
            .hCursor = LoadCursor(NULL, IDC_ARROW),
            .lpszClassName = name
        };
        if(!RegisterClassEx(&window_class_info)) {
            goto _register_class_fail;
        }

        /* create window */
        HWND window_handle = CreateWindowEx(
            0,
            name,
            name,
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT,
            800, 600,
            NULL,
            NULL,
            module,
            NULL
        );
        if(!window_handle) {
            goto _window_handle_fail;
        }

        ShowWindow(window_handle, 1);
        return window_handle;

        /* fails */
        _register_class_fail: {
            MSG_ERROR(msg_callback, &TRACED_STR("failed to register win32 window class"));
            return NULL;
        }
        _window_handle_fail: {
            MSG_ERROR(msg_callback, &TRACED_STR("failed to create win32 window handle"));
            UnregisterClass(name, module);
            return NULL;
        }
    }

    static void destroyWindow(
        GPUWindow window,
        MsgCallback_pfn msg_callback
    ) {
        /* check for null pointer to indicate if application has bugs in it */
        if(!DestroyWindow(window)) {
            MSG_WARNING(msg_callback, &TRACED_STR("failed to detsroy winapi window"));
        }
    }

    static VkSurfaceKHR createSurface(
        VkInstance instance,
        GPUWindow window,
        MsgCallback_pfn msg_callback
    ) {
        VkSurfaceKHR surface = NULL;
        VkWin32SurfaceCreateInfoKHR win32_surface_info = {
            .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
            .hwnd = (HWND)window,
            .hinstance = GetModuleHandle(NULL)
        };
        if(vkCreateWin32SurfaceKHR(instance, &win32_surface_info, NULL, &surface) != VK_SUCCESS) {
            MSG_ERROR(msg_callback, &TRACED_STR("failed to create win32 surface"));
            return NULL;
        }

        MSG_LOG(msg_callback, &CONST_STRING("created vulkan surface win32"));
        return surface;
    }
#endif

VKAPI_ATTR VkBool32 VKAPI_CALL validationDebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity, 
    VkDebugUtilsMessageTypeFlagsEXT type, 
    const VkDebugUtilsMessengerCallbackDataEXT *callback_data, 
    void *user_data
) {
    String string = (String) {
        .string = (char[512]){0},
        .capacity = 512
    };
    stringPattern(&CONST_STRING(":: vk message :: %c\n"), (const void *[]){callback_data->pMessage}, &string);
    printConsole(&string);
    return !(severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT);
}

static b32 createVkInstance(
    GPUFlags flags,
    const GPUWindow window,
    const char *name,
    MsgCallback_pfn msg_callback,
    VkInstance *vk_instance, 
    VkDebugUtilsMessengerEXT *debug_messenger
) {
    b32 is_debug = flags & GPU_FLAG_DEBUG;

    /* extensions required for vulkan are platform specific */
    #ifdef _WIN32
        const u32 required_extension_count = is_debug ? 3 : 2;
        const char *required_extensions[] = {
            "VK_KHR_surface",
            "VK_KHR_win32_surface",
            "VK_EXT_debug_utils"
        };
    #endif
    /* layers are same on all platforms for now, they either exist or they dont */
    const u32 required_layer_count = is_debug ? 1 : 0;
    const char *required_layers[] = {
        "VK_LAYER_KHRONOS_validation"
    };

    /* vulkan version is defined here */
    const VkApplicationInfo application_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pEngineName = name,
        .pApplicationName = name,
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_2,
    };
    /* used only if debug */
    const VkDebugUtilsMessengerCreateInfoEXT debug_messenger_info = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = validationDebugCallback
    };
    /* vulkan instance create info */
    const VkInstanceCreateInfo instance_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &application_info,
        .enabledExtensionCount = required_extension_count,
        .enabledLayerCount = required_layer_count,
        .ppEnabledExtensionNames = required_extensions,
        .ppEnabledLayerNames = required_layers,
        .pNext = is_debug ? &debug_messenger_info : NULL
    };
    /* create vulkan instance */
    if(vkCreateInstance(&instance_info, NULL, vk_instance) != VK_SUCCESS) {
        MSG_ERROR(msg_callback, &TRACED_STR("failed to create vulkan instance"));
        return FALSE;
    }
    MSG_LOG(msg_callback, &CONST_STRING("created vulkan instance"))

    if(is_debug) {
        /* load create proc */
        PFN_vkCreateDebugUtilsMessengerEXT create_messenger_pfn = (void *)vkGetInstanceProcAddr(*vk_instance, "vkCreateDebugUtilsMessengerEXT");
        if(!create_messenger_pfn) {
            MSG_ERROR(msg_callback, &TRACED_STR("failed to create debug messenger"));
            return FALSE;
        }
        /* create debug messenger */
        if(create_messenger_pfn(*vk_instance, &debug_messenger_info, NULL, debug_messenger) != VK_SUCCESS) {
            MSG_ERROR(msg_callback, &TRACED_STR("failed to create vulkan debug messenger"));
            return FALSE;
        }
        MSG_LOG(msg_callback, &CONST_STRING("created vulkan debug messenger"));
    }

    return TRUE;
}

typedef struct {
    VkPhysicalDevice physical_device;
    GPUType gpu_type;
    /* queues */
    u32 render_queue_id;
    u32 compute_queue_id;
    u32 transfer_queue_id;
    /* memory */
    u64 host_heap_size;
    u64 device_heap_size;
    /* available formats */
    VkPresentModeKHR present_mode;
    VkColorSpaceKHR color_space_surface;
    VkFormat format_surface_color;
    VkFormat format_rgba_color;
    VkFormat format_depth;
    /* identification */
    u32 device_id;
    char name[256];
} VulkanDeviceInfo;

static b32 getPhysicalDeviceInfo(
    VkPhysicalDevice physical_device,
    VkSurfaceKHR surface,
    GPUFlags flags,
    u32 extension_count,
    const char **extensions,
    VulkanDeviceInfo *device_info
) {
    typedef union {
        VkQueueFamilyProperties queue_family_properties[MAX_PHYSICAL_DEVICE_QUEUE_COUNT];
        VkSurfaceFormatKHR surface_formats[MAX_PHYSICAL_DEVICE_SURFACE_FORMAT_COUNT];
        VkExtensionProperties extension_properties[MAX_PHYSICAL_DEVICE_EXTENSION_COUNT];
        VkPresentModeKHR present_modes[MAX_PHYSICAL_DEVICE_PRESENT_MODE_COUNT];
        VkPhysicalDeviceMemoryProperties memory_properties;
    } StackOverlapData;
    StackOverlapData overlap_data = (StackOverlapData){0};

    *device_info = (VulkanDeviceInfo){
        .physical_device = physical_device, 
        .render_queue_id = U32_MAX,
        .compute_queue_id = U32_MAX,
        .transfer_queue_id = U32_MAX
    };

    /* PROPERTIES */ {
        VkPhysicalDeviceProperties device_properties = (VkPhysicalDeviceProperties){0};
        vkGetPhysicalDeviceProperties(physical_device, &device_properties);
        /* check device type, working only with descrete or integrated devices */
        if(
            device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU || 
            device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU
        ) {
            device_info->gpu_type = (device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) ? GPU_TYPE_DESCRETE : GPU_TYPE_INTEGRATED;
        } else {
            return FALSE;
        }
        /* copy device identificators */
        cstringCpy(device_info->name, device_properties.deviceName);
        device_info->device_id = device_properties.deviceID;
    }

    /* QUEUE LAYOUT */ {
        u32 queue_families_count = 0;
        overlap_data = (StackOverlapData){0};
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_families_count, NULL);
        /* check if no queues */
        if(queue_families_count == 0) {
            return FALSE;
        }
        queue_families_count = MIN(queue_families_count, MAX_PHYSICAL_DEVICE_QUEUE_COUNT);
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_families_count, overlap_data.queue_family_properties);

        #define QUEUE_MASK (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT)
        #define QUEUE_GRAPHICS (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT)
        #define QUEUE_COMPUTE (VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT)
        #define QUEUE_TRANSFER (VK_QUEUE_TRANSFER_BIT)

        const VkQueueFamilyProperties *queue_family_properties = overlap_data.queue_family_properties;
        for(u32 i = 0; i < queue_families_count; i++) {
            /* if something bad is there */
            if(queue_family_properties[i].queueCount == 0) {
                continue;
            }
            const VkQueueFlags queue_flags = queue_family_properties[i].queueFlags;

            /* render queue */
            if(device_info->render_queue_id == U32_MAX && (queue_flags & QUEUE_MASK) == QUEUE_GRAPHICS) {
                device_info->render_queue_id = i;
                continue;
            }

            if(flags & GPU_FLAG_ASYNC_COMPUTE) {
                /* compute queue */
                if(device_info->compute_queue_id == U32_MAX && (queue_flags & QUEUE_MASK) == QUEUE_COMPUTE) {
                    device_info->compute_queue_id = i;
                    continue;
                }
            }

            if(flags & GPU_FLAG_ASYNC_TRANSFER) {
                /* transfer queue */
                if(device_info->transfer_queue_id == U32_MAX && (queue_flags & QUEUE_MASK) == QUEUE_TRANSFER) {
                    device_info->transfer_queue_id = i;
                    continue;
                }
            }
        }

        /* check if render queue (necessary) is active */
        if(device_info->render_queue_id == U32_MAX) {
            return FALSE;
        }
    }

    /* EXTENSIONS */ {
        if(extension_count == 0) {
            goto _extensions_end;
        }

        /* get device extensions */
        u32 device_extension_count = 0;
        overlap_data = (StackOverlapData){0};
        vkEnumerateDeviceExtensionProperties(physical_device, NULL, &device_extension_count, NULL);
        if(device_extension_count == 0) {
            return FALSE;
        }
        device_extension_count = MIN(device_extension_count, MAX_PHYSICAL_DEVICE_EXTENSION_COUNT);
        vkEnumerateDeviceExtensionProperties(physical_device, NULL, &device_extension_count, overlap_data.extension_properties);

        /* match extensions */
        u32 matched_extensions = 0;
        const VkExtensionProperties *device_extensions = overlap_data.extension_properties;
        for(u32 i = 0; i < device_extension_count; i++) {
            for(u32 j = 0; j < extension_count; j++) {
                if(cstringCmp(device_extensions[i].extensionName, extensions[j])) {
                    if(++matched_extensions == extension_count) {
                        goto _extensions_end;
                    }
                }
            }
        }
        return FALSE;

        _extensions_end: {};
    }

    /* MEMORY */ {
        overlap_data = (StackOverlapData){0};
        vkGetPhysicalDeviceMemoryProperties(physical_device, &overlap_data.memory_properties);

        /* Im not sure if we should group it only like heaps like that */
        const u32 memory_heap_count = overlap_data.memory_properties.memoryHeapCount;
        const VkMemoryHeap *memory_heaps = overlap_data.memory_properties.memoryHeaps;
        for(u32 i = 0; i < memory_heap_count; i++) {
            if(memory_heaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
                device_info->device_heap_size += memory_heaps[i].size;
            } else {
                device_info->host_heap_size += memory_heaps[i].size;
            }
        }

        if(device_info->device_heap_size < MIN_GPU_LOCAL_MEMORY || device_info->host_heap_size < MIN_GPU_SHARED_MEMORY) {
            return FALSE;
        }
    }

    /* CUSTOM FORMATS */ {
        const VkFormat rgba_formats[] = {
            VK_FORMAT_R32G32B32A32_SFLOAT,
            VK_FORMAT_R16G16B16A16_SFLOAT,
            VK_FORMAT_R8G8B8A8_UNORM
        };
        const VkFormat depth_formats[] = {
            VK_FORMAT_D32_SFLOAT,
            VK_FORMAT_D16_UNORM
        };

        #define RGBA_FORMAT_FLAGS (VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT)
        #define DEPTH_FORMAT_FLAGS (VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)

        VkFormatProperties properties = (VkFormatProperties){0};

        /* find rgba format */
        for(u32 i = 0; i < ARRAY_SIZE(rgba_formats); i++) {
            vkGetPhysicalDeviceFormatProperties(physical_device, rgba_formats[i], &properties);
            if((properties.optimalTilingFeatures & RGBA_FORMAT_FLAGS) == RGBA_FORMAT_FLAGS) {
                device_info->format_rgba_color = rgba_formats[i];
                goto _found_rgba_format;
            }
        }
        return FALSE;
        _found_rgba_format: {};

        /* find depth format */
        for(u32 i = 0; i < ARRAY_SIZE(depth_formats); i++) {
            vkGetPhysicalDeviceFormatProperties(physical_device, depth_formats[i], &properties);
            if((properties.optimalTilingFeatures & DEPTH_FORMAT_FLAGS) == DEPTH_FORMAT_FLAGS) {
                device_info->format_depth = depth_formats[i];
                goto _found_depth_format;
            }
        }
        return FALSE;
        _found_depth_format: {};
    }

    /* SURFACE FORMAT */ {
        u32 surface_format_count = 0;
        overlap_data = (StackOverlapData){0};
        vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &surface_format_count, NULL);
        if(surface_format_count == 0) {
            return FALSE;
        }
        surface_format_count = MIN(surface_format_count, MAX_PHYSICAL_DEVICE_SURFACE_FORMAT_COUNT);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &surface_format_count, overlap_data.surface_formats);

        /* search for color space, default index [0] */
        const VkSurfaceFormatKHR *surface_formats = overlap_data.surface_formats;
        device_info->format_surface_color = surface_formats[0].format;
        device_info->color_space_surface = surface_formats[0].colorSpace;
        /* search for better one */
        for(u32 i = 0; i < surface_format_count; i++) {
            if(surface_formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR && surface_formats[i].format == VK_FORMAT_R8G8B8A8_UNORM) {
                device_info->format_surface_color = VK_FORMAT_R8G8B8A8_UNORM;
                device_info->color_space_surface = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
                break;
            }
        }
    }

    /* PRESENT MODE */ {
        u32 present_mode_count = 0;
        overlap_data = (StackOverlapData){0};
        vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, NULL);
        if(present_mode_count == 0) {
            return FALSE;
        }
        present_mode_count = MIN(present_mode_count, MAX_PHYSICAL_DEVICE_PRESENT_MODE_COUNT);
        vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, overlap_data.present_modes);

        const VkPresentModeKHR *present_modes = overlap_data.present_modes;
        device_info->present_mode = VK_PRESENT_MODE_FIFO_KHR;
        for(u32 i = 0; i < present_mode_count; i++) {
            if(present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
                device_info->present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
                break;
            }
        }
    }

    return TRUE;
}

/* return true if swap, left is prioritized */
static b32 compareDevices(
    const VulkanDeviceInfo *left,
    const VulkanDeviceInfo *right,
    u32 device_id,
    const char *device_name
) {
    /* IDENTIFIED DEVICE */ {
        if(device_id == left->device_id) {
            return FALSE;
        }
        if(device_id == right->device_id) {
            return TRUE;
        }

        if(device_name) {
            if(cstringCmp(device_name, left->name)) {
                return FALSE;
            }
            if(cstringCmp(device_name, right->name)) {
                return TRUE;
            }
        }
    }
    
    if(left->gpu_type == GPU_TYPE_DESCRETE && right->gpu_type == GPU_TYPE_INTEGRATED) {
        return FALSE;
    }
    if(left->gpu_type == GPU_TYPE_INTEGRATED && right->gpu_type == GPU_TYPE_DESCRETE) {
        return TRUE;
    }

    return FALSE;
}

static b32 createDevice(
    GPUFlags flags,
    VkInstance vk_instance,
    VkSurfaceKHR vk_surface,
    MsgCallback_pfn msg_callback,
    const GPUInfo *in_gpu_info,
    GPUInfo *out_gpu_info,
    GPUDevice *gpu_device
) {
    u32 physical_device_count = 0;
    VkPhysicalDevice physical_devices[MAX_GPU_SCAN_COUNT] = {0};

    vkEnumeratePhysicalDevices(vk_instance, &physical_device_count, NULL);
    /* check if any devices found */
    if(physical_device_count == 0) {
        MSG_ERROR(msg_callback, &TRACED_STR("no vulkan physical devices available"));
        return FALSE;
    }
    /* clamp number if someone is too crazy */
    physical_device_count = MIN(physical_device_count, MAX_GPU_SCAN_COUNT);
    vkEnumeratePhysicalDevices(vk_instance, &physical_device_count, physical_devices);

    u32 suitable_device_count = 0;
    VulkanDeviceInfo suitable_device_infos[MAX_GPU_SCAN_COUNT] = {0};
    const char *required_extensions[] = {
        VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    /* fill info about available devices */
    for(u32 i = 0; i < physical_device_count; i++) {
        /* check if device is suitable, if yes then fill the struct in array and move to next element */
        if(getPhysicalDeviceInfo(
            physical_devices[i],
            vk_surface,
            flags,
            ARRAY_SIZE(required_extensions),
            required_extensions,
            &suitable_device_infos[suitable_device_count]
        )) {
            suitable_device_count++;
        }
    }
    if(suitable_device_count == 0) {
        MSG_ERROR(msg_callback, &TRACED_STR("no suitable vulkan physical device found"));
        return FALSE;
    }

    /* specific device info */
    u32 device_id = U32_MAX;
    const char *device_name = NULL;
    if(in_gpu_info) {
        device_id = in_gpu_info->id;
        device_name = in_gpu_info->name;
    }
    /* simple insert sort, based on comparasion of devices. 
        if device data is provided and such device is found, it will be prioritized */
    for(u32 i = 1; i < suitable_device_count; i++) {
        for (u32 j = i - 1; j != U32_MAX; j--) {
            if(compareDevices(
                &suitable_device_infos[j],
                &suitable_device_infos[i],
                device_id,
                device_name
            )) {
                VulkanDeviceInfo temp_info = suitable_device_infos[j];
                suitable_device_infos[j] = suitable_device_infos[i];
                suitable_device_infos[i] = temp_info;
            }
        }
    }

    /* try creating devices */
    for(u32 i = 0; i < suitable_device_count; i++) {
        /* vulkan bs to initalize queues on device */
        u32 queue_count = 0;
        f32 queue_priority = 1.0f;
        VkDeviceQueueCreateInfo queue_infos[3] = {0};
        /* zero struct */
        *gpu_device = (GPUDevice){
            .vk_physical_device = suitable_device_infos[i].physical_device,

            .vk_render_queue_id = suitable_device_infos[i].render_queue_id,
            .vk_compute_queue_id = suitable_device_infos[i].compute_queue_id,
            .vk_transfer_queue_id = suitable_device_infos[i].transfer_queue_id,
            
            .present_mode = suitable_device_infos[i].present_mode,
            .color_space_surface = suitable_device_infos[i].color_space_surface,
            .format_surface_color = suitable_device_infos[i].format_surface_color,
            .format_rgba_color = suitable_device_infos[i].format_rgba_color,
            .format_depth = suitable_device_infos[i].format_depth
        };

        /* always generate render queue info */
        queue_infos[queue_count++] = (VkDeviceQueueCreateInfo) {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .pQueuePriorities = &queue_priority,
            .queueFamilyIndex = suitable_device_infos[i].render_queue_id,
            .queueCount = 1
        };
        /* if compute queue avaliable and used */
        if(suitable_device_infos[i].compute_queue_id != U32_MAX) {
            queue_infos[queue_count++] = (VkDeviceQueueCreateInfo) {
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .pQueuePriorities = &queue_priority,
                .queueFamilyIndex = suitable_device_infos[i].compute_queue_id,
                .queueCount = 1
            };
        }
        /* if transfer queue available and used */
        if(suitable_device_infos[i].transfer_queue_id != U32_MAX) {
            queue_infos[queue_count++] = (VkDeviceQueueCreateInfo) {
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .pQueuePriorities = &queue_priority,
                .queueFamilyIndex = suitable_device_infos[i].transfer_queue_id,
                .queueCount = 1
            };
        }

        /* enable dynamic rendering extension through feature struct */
        const VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamic_rendering_features = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR,
            .dynamicRendering = TRUE,
            .pNext = NULL
        };
        /* device create info points to features structs chain through pNext */
        const VkDeviceCreateInfo device_info = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .enabledExtensionCount = ARRAY_SIZE(required_extensions),
            .ppEnabledExtensionNames = required_extensions,
            .queueCreateInfoCount = queue_count,
            .pQueueCreateInfos = queue_infos,
            .pNext = &dynamic_rendering_features
        };

        /* create device */
        if(vkCreateDevice(suitable_device_infos[i].physical_device, &device_info, NULL, &gpu_device->vk_device) != VK_SUCCESS) {
            MSG_ERROR(msg_callback, &TRACED_STR("could not create device"));
            goto _device_create_fail;
        }

        /* get queue handles and create command pools  */
        /* RENDER QUEUE ID */ {
            vkGetDeviceQueue(gpu_device->vk_device, gpu_device->vk_render_queue_id, 0, &gpu_device->vk_render_queue);

            const VkCommandPoolCreateInfo command_pool_info = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                .queueFamilyIndex = gpu_device->vk_render_queue_id
            };
            if(vkCreateCommandPool(gpu_device->vk_device, &command_pool_info, NULL, &gpu_device->vk_render_command_pool) != VK_SUCCESS) {
                MSG_ERROR(msg_callback, &TRACED_STR("could not create render command pool"));
                goto _device_create_fail;
            }
        }
        if(gpu_device->vk_compute_queue_id != U32_MAX) {
            vkGetDeviceQueue(gpu_device->vk_device, gpu_device->vk_compute_queue_id, 0, &gpu_device->vk_compute_queue);
            
            const VkCommandPoolCreateInfo command_pool_info = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                .queueFamilyIndex = gpu_device->vk_compute_queue_id
            };
            if(vkCreateCommandPool(gpu_device->vk_device, &command_pool_info, NULL, &gpu_device->vk_compute_command_pool) != VK_SUCCESS) {
                MSG_ERROR(msg_callback, &TRACED_STR("could not create compute command pool"));
                goto _device_create_fail;
            }
        }
        if(gpu_device->vk_transfer_queue_id != U32_MAX) {
            vkGetDeviceQueue(gpu_device->vk_device, gpu_device->vk_transfer_queue_id, 0, &gpu_device->vk_transfer_queue);
            
            const VkCommandPoolCreateInfo command_pool_info = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                .queueFamilyIndex = gpu_device->vk_transfer_queue_id
            };
            if(vkCreateCommandPool(gpu_device->vk_device, &command_pool_info, NULL, &gpu_device->vk_transfer_command_pool) != VK_SUCCESS) {
                MSG_ERROR(msg_callback, &TRACED_STR("could not create transfer command pool"));
                goto _device_create_fail;
            }
        }

        if(out_gpu_info) {
            *out_gpu_info = (GPUInfo) {
                .id = suitable_device_infos[i].device_id,
                .type = suitable_device_infos[i].gpu_type,
                .host_heap_size = suitable_device_infos[i].host_heap_size,
                .device_heap_size = suitable_device_infos[i].device_heap_size
            };
            cstringCpy(out_gpu_info->name, suitable_device_infos[i].name);
        }

        /* success */
        MSG_LOG(msg_callback, &CONST_STRING("created vulkan device"));
        return TRUE;

        _device_create_fail: {
            if(gpu_device->vk_render_command_pool) {
                vkDestroyCommandPool(gpu_device->vk_device, gpu_device->vk_render_command_pool, NULL);
            }
            if(gpu_device->vk_compute_command_pool) {
                vkDestroyCommandPool(gpu_device->vk_device, gpu_device->vk_compute_command_pool, NULL);
            }
            if(gpu_device->vk_transfer_command_pool) {
                vkDestroyCommandPool(gpu_device->vk_device, gpu_device->vk_transfer_command_pool, NULL);
            }
            if(gpu_device->vk_device) {
                vkDestroyDevice(gpu_device->vk_device, NULL);
            }
        }
    }

    MSG_ERROR(msg_callback, &TRACED_STR("failed to create vulkan device"));
    return FALSE;
}

static void destroyDevice(
    GPUDevice *gpu_device
) {
    if(gpu_device->vk_render_command_pool) {
        vkDestroyCommandPool(gpu_device->vk_device, gpu_device->vk_render_command_pool, NULL);
    }
    if(gpu_device->vk_compute_command_pool) {
        vkDestroyCommandPool(gpu_device->vk_device, gpu_device->vk_compute_command_pool, NULL);
    }
    if(gpu_device->vk_transfer_command_pool) {
        vkDestroyCommandPool(gpu_device->vk_device, gpu_device->vk_transfer_command_pool, NULL);
    }
    if(gpu_device->vk_device) {
        vkDestroyDevice(gpu_device->vk_device, NULL);
    }
}

b32 allocateGPUMemory(
    GPUMemoryAllocator *allocator,
    const char *name,
    GPUMemoryUse use,
    u32 memory_type_bits,
    u64 size,
    GPUMemory *memory
) {
    /* spec: 
        https://docs.vulkan.org/refpages/latest/refpages/source/VkMemoryPropertyFlagBits.html
        https://docs.vulkan.org/refpages/latest/refpages/source/VkPhysicalDeviceMemoryProperties.html   */

    /* VALIDATION */ {
        if(size == 0) {
            return FALSE;
        }
        if(use != GPU_MEMORY_USE_HOST_TO_DEVICE && use != GPU_MEMORY_USE_DEVICE) {
            return FALSE;
        }
        if(name == NULL) {
            return FALSE;
        }
        if(allocator->hash_name_count == MAX_GPU_MEMORY_ALLOCATIONS) {
            return FALSE;
        }

        const u32 hash_name_count = allocator->hash_name_count;
        const char **hash_names = allocator->hash_names;
        for(u32 i = 0; i < hash_name_count; i++) {
            if(name == hash_names[i]) {
                return FALSE;
            }
        }
    }

    *memory = (GPUMemory){0};

    /* types are sorted in good to worse order */
    const VkMemoryPropertyFlags host_to_device_types[] = {
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_CACHED_BIT  |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,

        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_CACHED_BIT  ,

        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,

        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_CACHED_BIT  |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,

        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_CACHED_BIT  ,

        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
    };
    const VkMemoryPropertyFlags device_only_types[] = {
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT ,

        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_CACHED_BIT  ,

        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,

        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_CACHED_BIT  |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,

        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_CACHED_BIT,

        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,

        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_CACHED_BIT  |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
    };

    const u32 memory_type_count = allocator->memory_properties.memoryTypeCount;
    const VkMemoryType *memory_types = allocator->memory_properties.memoryTypes;
    const VkMemoryHeap *memory_heaps = allocator->memory_properties.memoryHeaps;

    u32 memory_type_id = U32_MAX;
    u32 memory_heap_id = U32_MAX;
    VkMemoryPropertyFlags memory_type_flags = 0;

    /* select the right possible types arrays to search right flags */
    u32 possible_memory_type_count = 0;
    const VkMemoryPropertyFlags *possible_memory_types = NULL;
    if(use == GPU_MEMORY_USE_HOST_TO_DEVICE) {
        possible_memory_type_count = ARRAY_SIZE(host_to_device_types);
        possible_memory_types = host_to_device_types;
    }
    if(use == GPU_MEMORY_USE_DEVICE) {
        possible_memory_type_count = ARRAY_SIZE(device_only_types);
        possible_memory_types = device_only_types;
    }

    /* FIND MEMORY TYPE */ {
        /* go through all memory types we can accept */
        for(u32 i = 0; i < possible_memory_type_count; i++) {
            /* go through all possible memory types */
            for(u32 j = 0; j < memory_type_count; j++) {
                if(
                    memory_types[j].propertyFlags == possible_memory_types[i] &&
                    memory_heaps[memory_types[j].heapIndex].size - (1024 * 1024 * 4) > size
                ) {
                    b32 is_optimal = (memory_type_bits & (1 << j)) ? TRUE : FALSE;

                    /* check if no memory type found yet, set this one */
                    if(memory_type_id == U32_MAX) {
                        memory_type_id = j;
                        memory_heap_id = memory_types[j].heapIndex;
                        memory_type_flags = possible_memory_types[i];

                        /* if it is optimal we found the right type */
                        if(is_optimal) {
                            goto _found_memory_type;
                        }
                    }
                    /* if found memory type that is actually suitable for specific resource type,
                        indicated by memory bits */
                    else if(is_optimal) {
                        memory_type_id = j;
                        memory_heap_id = memory_types[j].heapIndex;
                        memory_type_flags = possible_memory_types[i];

                        /* stop search, everything else is worse */
                        goto _found_memory_type;
                    }

                    goto _next_memory_type;
                }
            }

            _next_memory_type: {};
        }

        /* not found suitable memory type */
        if(memory_type_id == U32_MAX) {
            return FALSE;
        }
        /* successfully found memory type index */
        _found_memory_type: {};
    }

    /* allocate memory */
    const VkMemoryAllocateInfo allocate_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .memoryTypeIndex = memory_type_id,
        .allocationSize = size
    };
    VkDeviceMemory device_memory = NULL;
    if(vkAllocateMemory(allocator->device, &allocate_info, NULL, &device_memory) != VK_SUCCESS) {
        return FALSE;
    }
    
    /* set values */
    allocator->memory_properties.memoryHeaps[memory_heap_id].size -= size;
    allocator->hash_names[allocator->hash_name_count++] = name;
    *memory = (GPUMemory) {
        .hash_name = name,
        .memory = device_memory,
        .heap_id = memory_heap_id,
        .flags = memory_type_flags,
        .size = size
    };

    return TRUE;
}

b32 freeGPUMemory(
    GPUMemoryAllocator *allocator,
    GPUMemory *memory
) {
    const u32 hash_count = allocator->hash_name_count;
    const char **hashes = allocator->hash_names;
    const char *hash_key = memory->hash_name;
    for(u32 i = 0; i < hash_count; i++) {
        if(hashes[i] == hash_key) {
            hashes[i] = hashes[hash_count - 1];
            hashes[hash_count - 1] = NULL;
            vkFreeMemory(allocator->device, memory->memory, NULL);
            allocator->hash_name_count -= 1;
            allocator->memory_properties.memoryHeaps[memory->heap_id].size += memory->size;
            *memory = (GPUMemory){0};
            return TRUE;
        }
    }

    return FALSE;
}

GPU *mountGPU(
    const MountGPUIn *input, 
    MountGPUOut *output,
    Allocate_pfn struct_alloc,
    Free_pfn struct_free
) {
    if(!input || !output) {
        return NULL;
    }
    MsgCallback_pfn msg_callback = input->msg_callback;
    *output = (MountGPUOut){0};

    /* allocate GPU (ctx) struct in space provided from outside */
    GPU *gpu = struct_alloc(sizeof(GPU), 16);
    if(!gpu) {
        MSG_ERROR(msg_callback, &TRACED_STR("failed to allocate GPU struct"));
        return NULL;
    }
    /* intialize struct */
    *gpu = (GPU) {
        .flags = input->flags,
        .msg_callback = msg_callback,
        .alloc = struct_alloc,
        .free = struct_free
    };

    /* create platform specific window */
    gpu->window = createWindow(
        input->window_name,
        input->window_x,
        input->window_y,
        input->flags,
        msg_callback
    );
    if(!gpu->window) {
        MSG_ERROR(msg_callback, &TRACED_STR("failed to create GPU window"));
        return NULL;
    }

    /* create vulkan instance and debug messenger if debug flag is set */
    if(!createVkInstance(
        input->flags,
        gpu->window,
        input->window_name,
        msg_callback,
        &gpu->vk_instance,
        &gpu->vk_debug_messenger
    )) {
        MSG_ERROR(msg_callback, &TRACED_STR("failed to create vulkan instance"));
        return NULL;
    }

    /* create surface prior to device, so that we can detect necessarry formats */
    gpu->vk_surface = createSurface(
        gpu->vk_instance,
        gpu->window,
        msg_callback
    );
    if(!gpu->vk_surface) {
        MSG_ERROR(msg_callback, &TRACED_STR("failed to create vulkan surface"));
        return NULL;
    }

    /* create device, choose vk physical device and create interface */
    if(!createDevice(
        input->flags,
        gpu->vk_instance,
        gpu->vk_surface,
        msg_callback,
        &input->gpu_info,
        &output->gpu_info,
        &gpu->device
    )) {
        MSG_ERROR(msg_callback, &TRACED_STR("failed to create vulkan device"));
        return NULL;
    }

    /* allocate virtual space for resources */
    #ifdef _WIN32
        void *virtual_address_space = VirtualAlloc(NULL, GPU_VIRTUAL_ADDRESS_SPACE_SIZE, MEM_RESERVE, PAGE_READWRITE);
        if(!virtual_address_space) {
            MSG_ERROR(msg_callback, &TRACED_STR("failed to allocate gpu virtual address space"));
            return NULL;
        }
        gpu->resource_allocator = (GPUResourceAllocator) {
            .base_address = virtual_address_space,
            .virtual_size = GPU_VIRTUAL_ADDRESS_SPACE_SIZE
        };
    #endif

    /* create gpu memory allocator */
    gpu->memory_allocator = (GPUMemoryAllocator) {
        .device = gpu->device.vk_device
    };
    vkGetPhysicalDeviceMemoryProperties(gpu->device.vk_physical_device, &gpu->memory_allocator.memory_properties);

    /* SET FLAGS */ {
        GPUFlags flags = input->flags;
        flags &= ~(GPU_FLAG_ASYNC_TRANSFER | GPU_FLAG_ASYNC_COMPUTE);
        flags |= (gpu->device.vk_compute_queue_id != U32_MAX) ? GPU_FLAG_ASYNC_COMPUTE : 0;
        flags |= (gpu->device.vk_transfer_queue_id != U32_MAX) ? GPU_FLAG_ASYNC_TRANSFER : 0;
        output->flags = gpu->flags = flags; 
    }

    return gpu;
}

void dismountGPU(
    GPU *gpu
) {
    String log_str = STACK_STR(256);
    Free_pfn free = gpu->free;
    MsgCallback_pfn msg_callback = gpu->msg_callback;

    const u32 gpu_allocation_count = gpu->memory_allocator.hash_name_count;
    const char **gpu_allocation_names = gpu->memory_allocator.hash_names;
    for(u32 i = 0; i < gpu_allocation_count; i++) {
        stringPattern(
            &TRACED_STR("gpu memory not released name: \"%c\""),
            (const void *[]){gpu_allocation_names[i]},
            &log_str
        );
        MSG_WARNING(msg_callback, &log_str);
    }

    #ifdef _WIN32
        if(!VirtualFree(gpu->resource_allocator.base_address, 0, MEM_RELEASE)) {
            MSG_WARNING(msg_callback, &TRACED_STR("virtual free fail"));
        }
    #endif

    if(gpu->device.vk_device) {
        destroyDevice(&gpu->device);
        MSG_LOG(msg_callback, &CONST_STRING("destroyed vulkan device"));
    }
    if(gpu->vk_surface) {
        vkDestroySurfaceKHR(gpu->vk_instance, gpu->vk_surface, NULL);
        MSG_LOG(msg_callback, &CONST_STRING("destroyed vulkan surface"));
    }
    if(gpu->flags & GPU_FLAG_DEBUG) {
        PFN_vkDestroyDebugUtilsMessengerEXT destroy_messenger_pfn = (void *)vkGetInstanceProcAddr(gpu->vk_instance, "vkDestroyDebugUtilsMessengerEXT");
        if(!destroy_messenger_pfn) {
            MSG_ERROR(msg_callback, &TRACED_STR("failed to load vkDestroyDebugUtilsMessengerEXT extension procedure"));
        } else {
            destroy_messenger_pfn(gpu->vk_instance, gpu->vk_debug_messenger, NULL);
            MSG_LOG(msg_callback, &CONST_STRING("destroyed vulkan debug messenger"));
        }
    }
    if(gpu->vk_instance) {
        vkDestroyInstance(gpu->vk_instance, NULL);
        MSG_LOG(msg_callback, &CONST_STRING("destroyed vulkan instance"));
    }
    if(gpu->window) {
        destroyWindow(gpu->window, gpu->msg_callback);
    }

    /* zero out struct */
    *gpu = (GPU){0};
    free(gpu);
}
