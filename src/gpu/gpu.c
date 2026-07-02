#include "gpu_internal.h"

LRESULT CALLBACK window_proc(
    HWND   hwnd, 
    UINT   msg, 
    WPARAM w_param, 
    LPARAM l_param
) {
    switch (msg) {
        case WM_CLOSE:
            PostQuitMessage(0);
        return 0;
        case WM_KEYDOWN:
            if (w_param == VK_ESCAPE) {
                PostQuitMessage(0);
            }
        return 0;
        default:
        break;
    }
    return DefWindowProc(hwnd, msg, w_param, l_param);
}

HWND create_window(
    u32         size_x, 
    u32         size_y, 
    const char* name
) {
    HWND        window_handle     = NULL;
    HMODULE     h_instance        = GetModuleHandle(NULL);
    WNDCLASSEXA window_class_info = (WNDCLASSEXA) {
        .cbSize        = sizeof(WNDCLASSEXA),
        .style         = CS_HREDRAW | CS_VREDRAW,
        .lpfnWndProc   = window_proc,
        .hInstance     = h_instance,
        .hCursor       = LoadCursor(NULL, IDC_ARROW),
        .lpszClassName = name
    };

    /* register class */
    if(!RegisterClassExA(&window_class_info)) {
        LOG_ERROR("failed to register win32 window class");
        goto fail;
    }

    /* create window */
    window_handle = CreateWindowExA(
        0,
        name,
        name,
        WS_OVERLAPPEDWINDOW,
        0, 
        0,
        size_x, 
        size_y,
        NULL,
        NULL,
        h_instance,
        NULL
    );
    if(window_handle == NULL) {
        LOG_ERROR("failed to create win32 window");
        goto fail;
    }

    const RAWINPUTDEVICE raw_input_devices[2] = {
        { 0x01, 0x02, RIDEV_INPUTSINK, window_handle }, /* mouse */
        { 0x01, 0x06, RIDEV_INPUTSINK, window_handle }, /* keyboard */
    };
    RegisterRawInputDevices(raw_input_devices, 2, sizeof(RAWINPUTDEVICE));

    ShowWindow(window_handle, SW_SHOW);
    return window_handle;

    fail: {
        return NULL;
    }
}

void destroy_window(
    HWND        window, 
    const char* name
) {
    DestroyWindow(window);
    UnregisterClassA(name, GetModuleHandle(NULL));
}

/*  */

VKAPI_ATTR VkBool32 VKAPI_CALL debug_messenger_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT      severity, 
    VkDebugUtilsMessageTypeFlagsEXT             type, 
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data, 
    void*                                       user_data
) {
    printf(":: vulkan :: %s\n", callback_data->pMessage);
    return (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) ? FALSE : TRUE;
}


VkSurfaceKHR create_surface(
    HWND       window, 
    VkInstance instance
) {
    VkSurfaceKHR                surface_handle = NULL;
    VkWin32SurfaceCreateInfoKHR surface_info   = {
        .sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
        .hwnd      = (HWND)window,
        .hinstance = GetModuleHandle(NULL)
    };
    if(vkCreateWin32SurfaceKHR(
        instance,
        &surface_info,
        NULL,
        &surface_handle
    ) != VK_SUCCESS) {
        LOG_ERROR("failed to create vulkan win32 surface");
        goto fail;
    }

    return surface_handle;

    fail: {
        return NULL;
    }
}

VkInstance create_instance(
    const char*               name, 
    VkDebugUtilsMessengerEXT* debug_messenger
) {
    const char* release_instance_ext[]  = {
        "VK_KHR_surface",
        "VK_KHR_win32_surface"
    };
    const char* debug_instance_ext[]    = {
        "VK_KHR_surface",
        "VK_KHR_win32_surface",
        "VK_EXT_debug_utils"
    };
    const char* debug_instance_layers[] = {
        "VK_LAYER_KHRONOS_validation"
    };

    VkApplicationInfo                   application_info;
    VkDebugUtilsMessengerCreateInfoEXT  debug_messenger_info;
    VkInstanceCreateInfo                instance_info;
    VkInstance                          instance                = NULL;
    PFN_vkCreateDebugUtilsMessengerEXT  create_debug_messenger  = NULL;

    application_info = (VkApplicationInfo) {
        .sType                   = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName        = name,
        .pEngineName             = name,
        .applicationVersion      = VK_MAKE_VERSION(1, 0, 0),
        .engineVersion           = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion              = VK_API_VERSION_1_3
    };

    if(debug_messenger == NULL) {
        instance_info = (VkInstanceCreateInfo) {
            .sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pNext                   = NULL,
            .pApplicationInfo        = &application_info,
            .enabledLayerCount       = 0,
            .ppEnabledLayerNames     = NULL,
            .enabledExtensionCount   = ARRAY_SIZE(release_instance_ext),
            .ppEnabledExtensionNames = release_instance_ext
        };
    } else {
        debug_messenger_info = (VkDebugUtilsMessengerCreateInfoEXT) {
            .sType                   = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            .messageSeverity         = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT,
            .messageType             = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
            .pfnUserCallback         = debug_messenger_callback
        };
        instance_info = (VkInstanceCreateInfo) {
            .sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pNext                   = &debug_messenger_info,
            .pApplicationInfo        = &application_info,
            .enabledLayerCount       = ARRAY_SIZE(debug_instance_layers),
            .ppEnabledLayerNames     = debug_instance_layers,
            .enabledExtensionCount   = ARRAY_SIZE(debug_instance_ext),
            .ppEnabledExtensionNames = debug_instance_ext
        };
    }

    if(vkCreateInstance(
        &instance_info,
        NULL,
        &instance
    ) != VK_SUCCESS) {
        LOG_ERROR("failed to create vulkan instance");
        goto fail;
    }

    if(debug_messenger != NULL) {
        create_debug_messenger = (void*)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
        if(create_debug_messenger == NULL) {
            LOG_ERROR("failed to load vkCreateDebugUtilsMessengerEXT proc");
            goto fail;
        }

        if(create_debug_messenger(
            instance,
            &debug_messenger_info,
            NULL,
            debug_messenger
        ) != VK_SUCCESS) {
            LOG_ERROR("failed to create debug messenger");
            goto fail;
        }
    }

    return instance;

    fail: {
        return NULL;
    }
}

/* DEVICE SELECTION */

const char* device_extensions[] = {
    "VK_KHR_swapchain",
    "VK_KHR_dynamic_rendering"
};

b32 check_graphics_adapter_memory(
    VkPhysicalDevice physical_device,
    u64*             device_size,
    u64*             host_size
) {
    const u64 required_device_size = VRAM_SIZE_DEVICE_BUFFERS + VRAM_SIZE_DEVICE_IMAGES + VRAM_SIZE_DEVICE_VULKAN; 
    const u64 required_host_size   = VRAM_SIZE_HOST_TRANSFER + VRAM_SIZE_HOST_VULKAN;
    VkPhysicalDeviceMemoryProperties memory_properties = (VkPhysicalDeviceMemoryProperties){0};
    
    *device_size = 0;
    *host_size   = 0;

    /* get device heaps & count */
    vkGetPhysicalDeviceMemoryProperties(physical_device, &memory_properties);
    const VkMemoryHeap* memory_heaps       = memory_properties.memoryHeaps;
    const u32           memory_heaps_count = memory_properties.memoryHeapCount;

    for(u32 i = 0; i != memory_heaps_count; i++) {
        *device_size = (memory_heaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) ? *device_size + memory_heaps[i].size : *device_size;
        *host_size   = (memory_heaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) ? *host_size                          : *host_size + memory_heaps[i].size;
    }

    /* if not enough space available */
    if(*device_size < required_device_size) {
        goto fail;
    }
    if(*device_size < required_host_size) {
        goto fail;
    }

    return TRUE;

    fail: {
        return FALSE;
    }
}

b32 check_graphics_adapter_queues(
    VkPhysicalDevice physical_device,
    u32*             render_queue,
    u32*             compute_queue,
    u32*             transfer_queue
) {
    const VkQueueFlags queue_flags_mask     = (VK_QUEUE_TRANSFER_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_GRAPHICS_BIT);
    const VkQueueFlags render_queue_flags   = (VK_QUEUE_TRANSFER_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_GRAPHICS_BIT);
    const VkQueueFlags compute_queue_flags  = (VK_QUEUE_TRANSFER_BIT | VK_QUEUE_COMPUTE_BIT);
    const VkQueueFlags transfer_queue_flags = (VK_QUEUE_TRANSFER_BIT);
    
    VkQueueFamilyProperties queue_properties[GPU_MAX_QUEUE_FAMILIES] = {0};
    u32                     queue_properties_count                   = 0;

    *render_queue   = U32_MAX;
    *compute_queue  = U32_MAX;
    *transfer_queue = U32_MAX;

    /* get queues count */
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_properties_count, NULL);
    if(queue_properties_count == 0) {
        goto fail;
    }
    if(queue_properties_count > GPU_MAX_QUEUE_FAMILIES) {
        LOG_ERROR("physical device has too many queues: %u/%u", queue_properties_count, GPU_MAX_QUEUE_FAMILIES);
        queue_properties_count = GPU_MAX_QUEUE_FAMILIES;
    }
    /* get queues */
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_properties_count, queue_properties);

    for(u32 i = 0; i != queue_properties_count; i++) {
        if(queue_properties[i].queueCount == 0) {
            continue;
        }

        if(*render_queue   == U32_MAX && (queue_properties[i].queueFlags & queue_flags_mask) == render_queue_flags) {
            *render_queue = i;
            continue;
        }
        if(*compute_queue  == U32_MAX && (queue_properties[i].queueFlags & queue_flags_mask) == compute_queue_flags) {
            *compute_queue = i;
            continue;
        }
        if(*transfer_queue == U32_MAX && (queue_properties[i].queueFlags & queue_flags_mask) == transfer_queue_flags) {
            *transfer_queue = i;
            continue;
        }
    }

    if(*render_queue == U32_MAX) {
        goto fail;
    }

    return TRUE;

    fail: {
        return FALSE;
    }
}

b32 check_graphics_adapter_extensions(
    VkPhysicalDevice physical_device
) {
    VkExtensionProperties extension_properties[GPU_MAX_DEVICE_EXTENSIONS] = {0};
    u32                   extension_properties_count                      = 0;

    /* get extension count */
    vkEnumerateDeviceExtensionProperties(physical_device, NULL, &extension_properties_count, NULL);
    if(extension_properties_count < ARRAY_SIZE(device_extensions)) {
        goto fail;
    }
    if(extension_properties_count > GPU_MAX_DEVICE_EXTENSIONS) {
        LOG_ERROR("physical device has too many extensions: %u/%u", extension_properties_count, GPU_MAX_DEVICE_EXTENSIONS);
        extension_properties_count = GPU_MAX_DEVICE_EXTENSIONS;
    }
    /* get extensions */
    vkEnumerateDeviceExtensionProperties(physical_device, NULL, &extension_properties_count, extension_properties);

    for(u32 i = 0; i != ARRAY_SIZE(device_extensions); i++) {
        for(u32 j = 0; j != extension_properties_count; j++) {
            if(strcmp(device_extensions[i], extension_properties[j].extensionName) == 0) {
                goto found_extension;
            }
        }
        goto fail;
        found_extension: {};
    }

    return TRUE;

    fail: {
        return FALSE;
    }
}

b32 check_graphics_adapter_surface_formats(
    VkPhysicalDevice physical_device,
    VkSurfaceKHR     surface,
    VkFormat*        format,
    VkColorSpaceKHR* color_space
) {
    VkSurfaceFormatKHR surface_formats[GPU_MAX_DEVICE_SURFACE_FORMATS] = {0};
    u32                surface_formats_count;
    *format      = VK_FORMAT_UNDEFINED;
    *color_space = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

    /* get formats count */
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &surface_formats_count, NULL);
    if(surface_formats_count == 0) {
        goto fail;
    }
    if(surface_formats_count > GPU_MAX_DEVICE_SURFACE_FORMATS) {
        LOG_ERROR("physical device has too many surface formats: %u/%u", surface_formats_count, GPU_MAX_DEVICE_SURFACE_FORMATS);
        surface_formats_count = GPU_MAX_DEVICE_SURFACE_FORMATS;
    }
    /* get formats */
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &surface_formats_count, surface_formats);
    
    *format      = surface_formats[0].format;
    *color_space = surface_formats[0].colorSpace;
    for(u32 i = 0; i != surface_formats_count; i++) {
        if(
            surface_formats[i].format     == VK_FORMAT_R8G8B8A8_UNORM && 
            surface_formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
        ) {
            *format                       =  VK_FORMAT_R8G8B8A8_UNORM;
            *color_space                  =  VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        }
    }

    return TRUE;

    fail: {
        return FALSE;
    }
}

b32 check_graphics_adapter_present_modes(
    VkPhysicalDevice  physical_device,
    VkSurfaceKHR      surface,
    VkPresentModeKHR* present_mode
) {
    VkPresentModeKHR present_modes[GPU_MAX_DEVICE_PRESENT_MODES] = {0};
    u32              present_modes_count                         = 0;
    *present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;

    /* get present modes count */
    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_modes_count, NULL);
    if(present_modes_count == 0) {
        goto fail;
    }
    if(present_modes_count > GPU_MAX_DEVICE_PRESENT_MODES) {
        LOG_ERROR("physical device has too many present modes: %u/%u", present_modes_count, GPU_MAX_DEVICE_PRESENT_MODES);
        present_modes_count = GPU_MAX_DEVICE_PRESENT_MODES;
    }
    /* get present modes */
    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_modes_count, present_modes);

    *present_mode = VK_PRESENT_MODE_FIFO_KHR;
    for(u32 i = 0; i != present_modes_count; i++) {
        if(present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
            *present_mode   =  VK_PRESENT_MODE_MAILBOX_KHR;
        }
    }

    return TRUE;

    fail: {
        return FALSE;
    }
}

b32 check_graphics_adapter(
    VkInstance       instance,
    VkSurfaceKHR     surface,
    VkPhysicalDevice device,
    GraphicsAdapter* adapter
) {
    VkPhysicalDeviceProperties device_properties = (VkPhysicalDeviceProperties){0};
    
    *adapter = (GraphicsAdapter){0};

    vkGetPhysicalDeviceProperties(device, &device_properties);
    adapter->vendor_id       = (u16)device_properties.vendorID;
    adapter->device_id       = (u16)device_properties.deviceID;
    adapter->device_type     = (u16)device_properties.deviceType;
    adapter->physical_device = device;

    if(!check_graphics_adapter_extensions(device)) {
        goto fail;
    }

    if(!check_graphics_adapter_queues(
        device, 
        &adapter->render_queue_id, 
        &adapter->compute_queue_id, 
        &adapter->transfer_queue_id
    )) {
        goto fail;
    }

    if(!check_graphics_adapter_memory(
        device,
        &adapter->heap_device_size,
        &adapter->heap_host_size
    )) {
        goto fail;
    }

    if(!check_graphics_adapter_surface_formats(
        device,
        surface,
        &adapter->surface_format,
        &adapter->surface_color_space
    )) {
        goto fail;
    }

    if(!check_graphics_adapter_present_modes(
        device,
        surface,
        &adapter->surface_present_mode
    )) {
        goto fail;
    }

    return TRUE;

    fail: {
        return FALSE;
    }
}

u32 enumerate_available_devices(
    VkInstance       instance,
    VkSurfaceKHR     surface,
    GraphicsAdapter* available_devices
) {
    VkPhysicalDevice physical_devices[GPU_MAX_GRAPHICS_ADAPTERS] = {0};
    u32              physical_devices_count                      = 0;
    u32              available_devices_count                     = 0;

    /* get devices count */
    vkEnumeratePhysicalDevices(instance, &physical_devices_count, NULL);
    if(physical_devices_count == 0) {
        LOG_ERROR("no physical devices found");
        goto fail;
    }
    if(physical_devices_count > GPU_MAX_GRAPHICS_ADAPTERS) {
        LOG_ERROR("too many physical devices found");
        physical_devices_count = GPU_MAX_GRAPHICS_ADAPTERS;
    }
    /* get devices */
    vkEnumeratePhysicalDevices(instance, &physical_devices_count, physical_devices);

    /* get adapter infos and check suitability */
    for(u32 i = 0; i != physical_devices_count; i++) {
        if(check_graphics_adapter(
            instance,
            surface,
            physical_devices[i],
            &available_devices[available_devices_count]
        )) {
            available_devices_count++;
        }
    }

    return available_devices_count;

    fail: {
        return 0;
    }
}

/* */

const VkMemoryPropertyFlags memory_flags_device_only[] = {
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
    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
};

const VkMemoryPropertyFlags memory_flags_device_shared[] = {
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
    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
};

const VkMemoryPropertyFlags memory_flags_host[] = {
    VK_MEMORY_PROPERTY_HOST_CACHED_BIT  ,

    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
    VK_MEMORY_PROPERTY_HOST_CACHED_BIT  ,

    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,

    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
    VK_MEMORY_PROPERTY_HOST_CACHED_BIT  |
    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
};

u32 find_memory_type(
    VkPhysicalDeviceMemoryProperties* device_memory_properties,
    VkPhysicalDeviceType              device_type,
    VkPhysicalDevice                  device,
    u64                               size,
    u32                               type_bits,
    b32                               is_device_local
) {
    /* select list of prioritized memory types */
    const VkMemoryPropertyFlags* memory_flags_list        = NULL;
    u32                          memory_flags_list_length = 0;

    if(device_type == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        memory_flags_list        = is_device_local ?            memory_flags_device_only    :            memory_flags_host;
        memory_flags_list_length = is_device_local ? ARRAY_SIZE(memory_flags_device_only)   : ARRAY_SIZE(memory_flags_host);
    } else {
        memory_flags_list        = is_device_local ?            memory_flags_device_shared  :            memory_flags_host;
        memory_flags_list_length = is_device_local ? ARRAY_SIZE(memory_flags_device_shared) : ARRAY_SIZE(memory_flags_host);
    }

    /* find memory type */
    const u32           memory_types_count = device_memory_properties->memoryTypeCount;
    const VkMemoryType* memory_types       = device_memory_properties->memoryTypes;
    VkMemoryHeap*       memory_heaps       = device_memory_properties->memoryHeaps;

    u32 selected_type_id = U32_MAX;
    u32 selected_heap_id = U32_MAX;

    for(u32 i = 0; i != memory_flags_list_length; i++) {
        for(u32 j = 0; j != memory_types_count; j++) {
            if(
                memory_heaps[memory_types[j].heapIndex].size >= size &&
                memory_types[j].propertyFlags == memory_flags_list[i]
            ) {
                /* perfect type */
                if(type_bits & (0x1 << j)) {
                    selected_type_id = j;
                    selected_heap_id = memory_types[j].heapIndex;
                    goto found_memory_type;
                }
                /* worse type (might be not perfect for certtain resource)*/
                if(selected_type_id == U32_MAX) {
                    selected_type_id = j;
                    selected_heap_id = memory_types[j].heapIndex;
                }
            }
        }
    }

    /* not found */
    return U32_MAX;

    found_memory_type: {
        memory_heaps[selected_heap_id].size = memory_heaps[selected_heap_id].size - size;
        return selected_type_id;
    }
}

b32 allocate_video_memory(
    VkDevice                  device,
    VkPhysicalDevice          physical_device,
    VkPhysicalDeviceType      physical_device_type,
    GpuVideoMemoryAllocation* video_memory_device_buffers,
    GpuVideoMemoryAllocation* video_memory_device_images,
    GpuVideoMemoryAllocation* video_memory_host_transfer
) {
    /* get memory properties */
    VkPhysicalDeviceMemoryProperties memory_properties = (VkPhysicalDeviceMemoryProperties){0};
    vkGetPhysicalDeviceMemoryProperties(physical_device, &memory_properties);

    u32                   type_device_buffers  = U32_MAX;
    u32                   type_device_images   = U32_MAX;
    u32                   type_host_transfer   = U32_MAX;
    VkMemoryPropertyFlags flags_device_buffers = 0;
    VkMemoryPropertyFlags flags_device_images  = 0;
    VkMemoryPropertyFlags flags_host_transfer  = 0;

    /* find memory types */ {

    /* create dummy protoypes to get memory type bits */
    const VkBufferCreateInfo dummy_buffer_info = (VkBufferCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        .size  = 2 * 1024 * 1024
    };
    const VkImageCreateInfo dummy_image_info = (VkImageCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType   = VK_IMAGE_TYPE_2D,
        .tiling      = VK_IMAGE_TILING_OPTIMAL,
        .samples     = VK_SAMPLE_COUNT_1_BIT,
        .format      = VK_FORMAT_R32G32B32A32_SFLOAT,
        .usage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .mipLevels   = 1,
        .arrayLayers = 1,
        .extent      = (VkExtent3D) {
            .width  = 1024,
            .height = 1024,
            .depth  = 1
        }  
    };

    VkBuffer dummy_buffer = NULL;
    VkImage  dummy_image  = NULL;
    
    if(vkCreateBuffer(device, &dummy_buffer_info, NULL, &dummy_buffer) != VK_SUCCESS) {
        LOG_ERROR("failed to create dummy buffer");
        goto fail;
    }
    if(vkCreateImage(device, &dummy_image_info, NULL, &dummy_image) != VK_SUCCESS) {
        LOG_ERROR("failed to create dummy image");
        goto fail;
    }

    /* get prototypes memory type bits */
    VkMemoryRequirements dummy_buffer_requirements = (VkMemoryRequirements){0};
    VkMemoryRequirements dummy_image_requirements  = (VkMemoryRequirements){0};

    vkGetBufferMemoryRequirements(device, dummy_buffer, &dummy_buffer_requirements);
    vkGetImageMemoryRequirements(device, dummy_image, &dummy_image_requirements);

    /* get rid of prototypes*/
    vkDestroyBuffer(device, dummy_buffer, NULL);
    vkDestroyImage(device, dummy_image, NULL);
    
    
    const VkMemoryType* memory_types = memory_properties.memoryTypes;

    if(VRAM_SIZE_DEVICE_BUFFERS != 0) {
        type_device_buffers = find_memory_type(
            &memory_properties, 
            physical_device_type, 
            physical_device, 
            VRAM_SIZE_DEVICE_BUFFERS, 
            dummy_buffer_requirements.memoryTypeBits, 
            TRUE
        );
        if(type_device_buffers == U32_MAX) {
            LOG_ERROR("failed to find device buffers memory type");
            goto fail;
        }

        flags_device_buffers = memory_types[type_device_buffers].propertyFlags;
    }
    if(VRAM_SIZE_DEVICE_IMAGES != 0) {
        type_device_images = find_memory_type(
            &memory_properties, 
            physical_device_type, 
            physical_device, 
            VRAM_SIZE_DEVICE_IMAGES, 
            dummy_image_requirements.memoryTypeBits, 
            TRUE
        );
        if(type_device_images == U32_MAX) {
            LOG_ERROR("failed to find device images memory type");
            goto fail;
        }

        flags_device_images = memory_types[type_device_images].propertyFlags;
    }
    if(VRAM_SIZE_HOST_TRANSFER != 0) {
        type_host_transfer = find_memory_type(
            &memory_properties,
            physical_device_type,
            physical_device,
            VRAM_SIZE_HOST_TRANSFER,
            dummy_buffer_requirements.memoryTypeBits,
            FALSE
        );
        if(type_host_transfer == U32_MAX) {
            LOG_ERROR("failed to find host transfer type");
            goto fail;
        }

        flags_host_transfer = memory_types[type_host_transfer].propertyFlags;
    }
    }

    VkDeviceMemory memory_device_buffers = NULL;
    VkDeviceMemory memory_device_images  = NULL;
    VkDeviceMemory memory_host_transfer  = NULL;
    void*          map_device_buffers    = NULL;
    void*          map_device_images     = NULL;
    void*          map_host_transfer     = NULL;

    /* allocate memory */ {
    const VkMemoryAllocateInfo alloc_device_buffers_info = {
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .memoryTypeIndex = type_device_buffers,
        .allocationSize  = VRAM_SIZE_DEVICE_BUFFERS
    };
    const VkMemoryAllocateInfo alloc_device_images_info = {
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .memoryTypeIndex = type_device_images,
        .allocationSize  = VRAM_SIZE_DEVICE_IMAGES
    };
    const VkMemoryAllocateInfo alloc_host_transfer_info = {
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .memoryTypeIndex = type_host_transfer,
        .allocationSize  = VRAM_SIZE_HOST_TRANSFER
    };

    if(type_device_buffers != U32_MAX) {
        if(vkAllocateMemory(device, &alloc_device_buffers_info, NULL, &memory_device_buffers) != VK_SUCCESS) {
            LOG_ERROR("failed to allocate device buffers memory");
            goto fail;
        }
        
        if(flags_device_buffers & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
            if(vkMapMemory(device, memory_device_buffers, 0, VRAM_SIZE_DEVICE_BUFFERS, 0, &map_device_buffers) != VK_SUCCESS) {
                LOG_ERROR("failed to map device buffers memory");
                goto fail;
            }
        }
    }
    if(type_device_images != U32_MAX) {
        if(vkAllocateMemory(device, &alloc_device_images_info, NULL, &memory_device_images) != VK_SUCCESS) {
            LOG_ERROR("failed to allocate device images memory");
            goto fail;
        }

        if(flags_device_images & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
            if(vkMapMemory(device, memory_device_images, 0, VRAM_SIZE_DEVICE_IMAGES, 0, &map_device_images) != VK_SUCCESS) {
                LOG_ERROR("failed to map device images memory");
                goto fail;
            }
        }
    }
    if(type_host_transfer != U32_MAX) {
        if(vkAllocateMemory(device, &alloc_host_transfer_info, NULL, &memory_host_transfer) != VK_SUCCESS) {
            LOG_ERROR("failed to allocate host transfer memory");
            goto fail;
        }

        if(vkMapMemory(device, memory_host_transfer, 0, VRAM_SIZE_HOST_TRANSFER, 0, &map_host_transfer) != VK_SUCCESS) {
            LOG_ERROR("failed to map host transfer memory");
            goto fail;
        }
    }
    }

    /* fill allocation structs */
    *video_memory_device_buffers = (GpuVideoMemoryAllocation) {
        .device_memory = memory_device_buffers,
        .memory_map    = map_device_buffers,
        .type_id       = type_device_buffers,
        .type_flags    = flags_device_buffers
    };
    *video_memory_device_images = (GpuVideoMemoryAllocation) {
        .device_memory = memory_device_images,
        .memory_map    = map_device_images,
        .type_id       = type_device_images,
        .type_flags    = flags_device_images
    };
    *video_memory_host_transfer = (GpuVideoMemoryAllocation) {
        .device_memory = memory_host_transfer,
        .memory_map    = map_host_transfer,
        .type_id       = type_host_transfer,
        .type_flags    = flags_host_transfer
    };

    return TRUE;

    fail: {
        return FALSE;
    }
}

/* */

b32 create_vulkan_objects(
    u32            window_x,
    u32            window_y,
    const char*    window_name,
    b32            enable_debug, 
    VulkanObjects* vulkan_objects
) {
    /* any fail is considered crtitical and expects program to terminate ASAP */
    strcpy_s(vulkan_objects->window_name, GPU_MAX_NAME_LENGTH, window_name);

    /* window */
    vulkan_objects->window = create_window(
        window_x, 
        window_y, 
        vulkan_objects->window_name
    );
    if(vulkan_objects->window == NULL) {
        LOG_ERROR("failed to create win32 window");
        goto fail;
    }

    /* vulkan instance */
    vulkan_objects->instance = create_instance(
        vulkan_objects->window_name,
        enable_debug ? &vulkan_objects->debug_messenger : NULL
    );
    if(vulkan_objects->instance == NULL) {
        LOG_ERROR("failed to create vulkan instance");
        goto fail;
    }

    /* surface */
    vulkan_objects->surface = create_surface(
        vulkan_objects->window, 
        vulkan_objects->instance
    );
    if(vulkan_objects->surface == NULL) {
        LOG_ERROR("failed to create vulkan surface");
        goto fail;
    }

    /* enumerate devices */
    vulkan_objects->available_devices_count = enumerate_available_devices(
        vulkan_objects->instance,
        vulkan_objects->surface,
        vulkan_objects->available_devices
    );
    if(vulkan_objects->available_devices_count == 0) {
        LOG_ERROR("no available graphics adapters found");
        goto fail;
    }

    return TRUE;

    fail: {
        return FALSE;
    }
}

b32 create_vulkan_device(
    u16                  vendor_id,
    u16                  device_id,
    const VulkanObjects* vulkan_objects,
    VulkanDevice*        vulkan_device
) {
    /* find adapter based on pci id or pick first one */
    const u32              available_adapters_count = vulkan_objects->available_devices_count;
    const GraphicsAdapter* available_adapters       = vulkan_objects->available_devices;
    const GraphicsAdapter* adapter                  = NULL;

    if(vendor_id == U16_MAX || device_id == U16_MAX) {
        adapter                = &available_adapters[0];
        vulkan_device->adapter = &available_adapters[0];
    } else {
        for(u32 i = 0; i != available_adapters_count; i++) {
            if(available_adapters[i].vendor_id == vendor_id && available_adapters[i].device_id == device_id) {
                adapter                = &available_adapters[i];
                vulkan_device->adapter = &available_adapters[i];
                goto found_adapter;
            }
        }

        LOG_ERROR("failed to find specified device vendor_id: %04x device_id: %04x", vendor_id, device_id);
        goto fail;
    }

    found_adapter: {};

    /* 1 queue per family fuck any other layout */
    VkDeviceQueueCreateInfo queues_infos[3] = {0};
    f32                     queues_priority = 1.0f;
    u32                     queues_count    = 0;
    
    if(adapter->render_queue_id != U32_MAX) {
        queues_infos[queues_count++] = (VkDeviceQueueCreateInfo) {
            .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueCount       = 1,
            .queueFamilyIndex = adapter->render_queue_id,
            .pQueuePriorities = &queues_priority
        };
    }
    if(adapter->compute_queue_id != U32_MAX) {
        queues_infos[queues_count++] = (VkDeviceQueueCreateInfo) {
            .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueCount       = 1,
            .queueFamilyIndex = adapter->compute_queue_id,
            .pQueuePriorities = &queues_priority
        };
    }
    if(adapter->transfer_queue_id != U32_MAX) {
        queues_infos[queues_count++] = (VkDeviceQueueCreateInfo) {
            .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueCount       = 1,
            .queueFamilyIndex = adapter->transfer_queue_id,
            .pQueuePriorities = &queues_priority
        };
    }

    /* create device */
    const VkPhysicalDeviceDynamicRenderingFeatures dynamic_rendering_feature = {
        .sType            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
        .dynamicRendering = TRUE
    };
    const VkDeviceCreateInfo device_info = {
        .sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .enabledExtensionCount   = ARRAY_SIZE(device_extensions),
        .ppEnabledExtensionNames = device_extensions,
        .queueCreateInfoCount    = queues_count,
        .pQueueCreateInfos       = queues_infos,
        .pNext                   = &dynamic_rendering_feature
    };

    if(vkCreateDevice(adapter->physical_device, &device_info, NULL, &vulkan_device->device) != VK_SUCCESS) {
        LOG_ERROR("failed to create vulkan device");
        goto fail;
    }

    /* create queues & create command pools */
    const VkCommandPoolCreateInfo render_command_pool_info = {
        .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = adapter->render_queue_id,
        .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
    };
    const VkCommandPoolCreateInfo compute_command_pool_info = {
        .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = adapter->compute_queue_id,
        .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
    };
    const VkCommandPoolCreateInfo transfer_command_pool_info = {
        .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = adapter->transfer_queue_id,
        .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
    };

    if(adapter->render_queue_id != U32_MAX) {
        vkGetDeviceQueue(vulkan_device->device, adapter->render_queue_id, 0, &vulkan_device->queue_render);
        if(vkCreateCommandPool(vulkan_device->device, &render_command_pool_info, NULL, &vulkan_device->command_pool_render) != VK_SUCCESS) {
            LOG_ERROR("failed to create render command pool");
            goto fail;
        }
    }
    if(adapter->compute_queue_id != U32_MAX) {
        vkGetDeviceQueue(vulkan_device->device, adapter->compute_queue_id, 0, &vulkan_device->queue_compute);
        if(vkCreateCommandPool(vulkan_device->device, &compute_command_pool_info, NULL, &vulkan_device->command_pool_compute) != VK_SUCCESS) {
            LOG_ERROR("failed to create compute command pool");
            goto fail;
        }
    }
    if(adapter->transfer_queue_id != U32_MAX) {
        vkGetDeviceQueue(vulkan_device->device, adapter->transfer_queue_id, 0, &vulkan_device->queue_transfer);
        if(vkCreateCommandPool(vulkan_device->device, &transfer_command_pool_info, NULL, &vulkan_device->command_pool_transfer) != VK_SUCCESS) {
            LOG_ERROR("failed to create transfer command pool");
            goto fail;
        }
    }

    /* load device extensions procedures */
    vulkan_device->cmd_begin_rendering_khr = (void*)vkGetDeviceProcAddr(vulkan_device->device, "vkCmdBeginRenderingKHR");
    vulkan_device->cmd_end_rendering_khr   = (void*)vkGetDeviceProcAddr(vulkan_device->device, "vkCmdEndRenderingKHR");

    if(
        vulkan_device->cmd_begin_rendering_khr == NULL ||
        vulkan_device->cmd_end_rendering_khr   == NULL
    ) {
        LOG_ERROR("failed to load device extension procedures");
        goto fail;
    }

    /* allocate memory */
    if(!allocate_video_memory(
        vulkan_device->device,
        vulkan_device->adapter->physical_device,
        vulkan_device->adapter->device_type,
        &vulkan_device->video_memory_device_buffers,
        &vulkan_device->video_memory_device_images,
        &vulkan_device->video_memory_host_transfer
    )) {
        LOG_ERROR("failed to allocate device memory");
        goto fail;
    }

    return TRUE;

    fail: {
        return FALSE;
    }
}

void destroy_vulkan_device(
    const VulkanObjects* vulkan_objects,
    VulkanDevice*        vulkan_device
) {
    const VkDevice device = vulkan_device->device;

    vkDeviceWaitIdle(device);

    const VkDeviceMemory memory_device_buffers = vulkan_device->video_memory_device_buffers.device_memory;
    const VkDeviceMemory memory_device_images  = vulkan_device->video_memory_device_images.device_memory;
    const VkDeviceMemory memory_host_transfer  = vulkan_device->video_memory_host_transfer.device_memory;
    const void*          map_device_buffers    = vulkan_device->video_memory_device_buffers.memory_map;
    const void*          map_device_images     = vulkan_device->video_memory_device_images.memory_map;
    const void*          map_host_transfer     = vulkan_device->video_memory_host_transfer.memory_map;

    if(map_device_buffers != NULL) {
        vkUnmapMemory(device, memory_device_buffers);
    }
    if(map_device_images != NULL) {
        vkUnmapMemory(device, memory_device_images);
    }
    if(map_host_transfer != NULL) {
        vkUnmapMemory(device, memory_host_transfer);
    }

    if(memory_device_buffers != NULL) {
        vkFreeMemory(device, memory_device_buffers, NULL);
    }
    if(memory_device_images != NULL) {
        vkFreeMemory(device, memory_device_images, NULL);
    }
    if(memory_host_transfer != NULL) {
        vkFreeMemory(device, memory_host_transfer, NULL);
    }

    if(vulkan_device->command_pool_render != NULL) {
        vkDestroyCommandPool(device, vulkan_device->command_pool_render, NULL);
    }
    if(vulkan_device->command_pool_compute != NULL) {
        vkDestroyCommandPool(device, vulkan_device->command_pool_compute, NULL);
    }
    if(vulkan_device->command_pool_transfer != NULL) {
        vkDestroyCommandPool(device, vulkan_device->command_pool_transfer, NULL);
    }

    vkDestroyDevice(device, NULL);

    *vulkan_device = (VulkanDevice){0};
}

void destroy_vulkan_objects(
    VulkanObjects* vulkan_objects
) {
    vkDestroySurfaceKHR(vulkan_objects->instance, vulkan_objects->surface, NULL);
    destroy_window(vulkan_objects->window, vulkan_objects->window_name);
    
    if(vulkan_objects->debug_messenger != NULL) {
        PFN_vkDestroyDebugUtilsMessengerEXT destroy_debug_utils_messenger_ext = (void*)vkGetInstanceProcAddr(vulkan_objects->instance, "vkDestroyDebugUtilsMessengerEXT");
        if(destroy_debug_utils_messenger_ext == NULL) {
            LOG_ERROR("failed to load vkDestroyDebugUtilsMessengerEXT");
        }
        destroy_debug_utils_messenger_ext(vulkan_objects->instance, vulkan_objects->debug_messenger, NULL);
    }

    vkDestroyInstance(vulkan_objects->instance, NULL);
    *vulkan_objects = (VulkanObjects){0};
}

/* GLOBAL INTERFACE */

CtxHandle gpu_start(
    const GpuInfo* gpu_info
) {
    /* any fail is considered crtitical and expects program to terminate ASAP */
    if(gpu_info == NULL) {
        LOG_ERROR("gpu_info is NULL");
        goto fail;
    }

    /* allocate virtual memory */
    /* [context (aligned to 4KB)] + [4KB] + [GPU_VIRTUAL_RESOURCES_BASE : GPU_VIRTUAL_RESOURCES_LIMIT] */
    const u64 ctx_size        = ALIGN(sizeof(GpuContext), 0x1000);
    const u64 allocation_size = ctx_size + 0x1000 + GPU_VIRTUAL_RESOURCES_LIMIT;
    GpuContext* context = VirtualAlloc(NULL, allocation_size, MEM_RESERVE, PAGE_READWRITE);
    if(context == NULL) {
        LOG_ERROR("failed to allocate gpu virtual space");
        goto fail;
    }
    if(VirtualAlloc(context, ctx_size, MEM_COMMIT, PAGE_READWRITE) == NULL) {
        LOG_ERROR("failed to commit context memory");
        goto fail;
    }
    context->resources_base       = (u8*)context + ctx_size + 0x1000 + GPU_VIRTUAL_RESOURCES_BASE;
    context->resources_limit      = (u8*)context + ctx_size + 0x1000 + GPU_VIRTUAL_RESOURCES_LIMIT;

    /* create context */
    if(!create_vulkan_objects(
        gpu_info->frame_buffer_x,
        gpu_info->frame_buffer_y,
        gpu_info->window_name,
        gpu_info->vulkan_debug_enabled,
        &context->vulkan_objects
    )) {
        LOG_ERROR("failed to create vulkan objects");
        goto fail;
    }

    if(!create_vulkan_device(
        gpu_info->pci_vendor_id,
        gpu_info->pci_device_id,
        &context->vulkan_objects,
        &context->vulkan_device
    )) {
        LOG_ERROR("failed to create vulkan device");
        goto fail;
    }

    return context;

    fail: {
        return NULL;
    }
}

void gpu_stop(
    CtxHandle ctx
) {
    if(ctx == NULL) {
        LOG_ERROR("gpu context is NULL");
        goto fail;
    }

    GpuContext* context = (GpuContext*)ctx;

    destroy_vulkan_device(&context->vulkan_objects, &context->vulkan_device);
    destroy_vulkan_objects(&context->vulkan_objects);

    if(!VirtualFree(context, 0, MEM_RELEASE)) {
        LOG_ERROR("failed to free virtual context memory");
        goto fail;
    }

    fail: {};
}
