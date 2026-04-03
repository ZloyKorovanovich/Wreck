#include "vulkan.h"

const u32   c_vulkan_debug_layer_count = 1;
const char* c_vulkan_debug_layers[]    = {
    "VK_LAYER_KHRONOS_validation"
};

const u32   c_device_extension_count   = 2;
const char* c_device_extensions[]      = {
    "VK_KHR_swapchain",
    "VK_KHR_dynamic_rendering"
};

/* win32 window procedures */
#ifdef _WIN32
    /* standart window win32 proc */
    LRESULT CALLBACK windowProcedure(
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
                return DefWindowProc(hwnd, msg, w_param, l_param);
        }
    }

    /* name   = string name of the wndow 
       x      = resolution x
       y      = resolution y                    
       return = vaild hwnd handle (success)/ NULL (fail) */
    WindowHandle createWindow(
        const char* name,
        u32         x, 
        u32         y
    ) {
        HMODULE    module            = GetModuleHandle(NULL);
        HWND       window_handle     = NULL;
        WNDCLASSEXA window_class_info = (WNDCLASSEXA) {
            .cbSize = sizeof(WNDCLASSEXA),
            .style = CS_HREDRAW | CS_VREDRAW,
            .lpfnWndProc = windowProcedure,
            .hInstance = module,
            .hCursor = LoadCursor(NULL, IDC_ARROW),
            .lpszClassName = name
        };

        if(x == 0 || y == 0 || name == NULL) {
            LOG_ERROR("invalid parameters for win32 window");
            goto fail;
        }

        if(!RegisterClassExA(&window_class_info)) {
            LOG_ERROR("failed to register win32 window class");
            goto fail;
        }
        window_handle = CreateWindowEx(
            0,
            name,
            name,
            WS_OVERLAPPEDWINDOW,
            0, 0,
            x, y,
            NULL,
            NULL,
            module,
            NULL
        );
        if(window_handle == NULL) {
            LOG_ERROR("failed to create win32 window");
            goto fail;
        }

        ShowWindow(window_handle, SW_SHOW);

        LOG_MESSAGE("created win32 window");
        return window_handle;

        fail: {
            if(name != NULL && module != NULL) {
                UnregisterClass(name, module);
            }
            if(window_handle != NULL) {
                DestroyWindow(window_handle);
            }
            return NULL;
        }
    }

    /* handle = valid handle to win32 window
       name   = valid string name of the window (also name of win32 class) */
    void destroyWindow(
        WindowHandle handle, 
        const char*  name
    ) {
        DestroyWindow(handle);
        UnregisterClass(name, GetModuleHandle(NULL));
        LOG_MESSAGE("destroyed win32 window");
    }

    /* creates win32 surface */
    VkSurfaceKHR createSurface(
        WindowHandle window,
        VkInstance   instance
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

        LOG_MESSAGE("created vulkan win32 surface");
        return surface_handle;

        fail: {
            if(surface_handle != NULL) {
                vkDestroySurfaceKHR(instance, surface_handle, NULL);
            }
            return NULL;
        }
    }

    b32 processWindow(
        WindowHandle window
    ) {
        MSG msg = (MSG){0};

        while(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if(msg.message == WM_QUIT) {
                return FALSE;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        return TRUE;
    }

    /* extensions required by win32 */
    const u32   c_vk_required_release_ext_count = 2;
    const u32   c_vk_required_debug_ext_count   = 3;
    const char* c_vk_required_extensions[]      = {
        "VK_KHR_surface",
        "VK_KHR_win32_surface",
        "VK_EXT_debug_utils"
    };
#endif


/* vulkan debug callback, prints messages recieved by debug utils messenger */
VKAPI_ATTR VkBool32 VKAPI_CALL debugMessengerCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT      severity, 
    VkDebugUtilsMessageTypeFlagsEXT             type, 
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data, 
    void*                                       user_data
) {
    printf(":: vk message :: %s", callback_data->pMessage);
    return (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) ? FALSE : TRUE;
}

/* name            = vulkan application name 
   version         = vulkan application version
   instance        = valid pointer to VkInstance
   debug_messenger = valid pointer to VkDebugUitlsMessengerEXT handle or a NULL pointer if no debug 
   return          = TRUE (success) / FALSE (fail)                                                  */
static b32 createVulkanInstance(
    const char*               name,
    u32                       version,
    VkInstance*               instance,
    VkDebugUtilsMessengerEXT* debug_messenger
) {
    /* this function is not supposed to validate anything, 
        all validation (if done) done on vulkan side       */
    VkApplicationInfo                   application_info;
    VkDebugUtilsMessengerCreateInfoEXT  debug_messenger_info;
    VkInstanceCreateInfo                instance_info;

    /* dynamically loaded ext procs */
    PFN_vkCreateDebugUtilsMessengerEXT  create_debug_messenger  = NULL;
    PFN_vkDestroyDebugUtilsMessengerEXT destroy_debug_messenger = NULL;

    /* fill create infos:
        - instance extensions are platform specific (depend on windowing system);
        - instance extensions have varitation in count 1 for using debug messenger (debug) and 1 with using it (release);
        - instance has only 1 possible layer for debug, its set only if vulkan debug enabled (debug_messenger != NULL); */
    application_info     = (VkApplicationInfo)                  {
        .sType                   = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName        = name,
        .applicationVersion      = version,
        .pEngineName             = name,
        .engineVersion           = version,
        .apiVersion              = VK_API_VERSION_1_3
    };
    debug_messenger_info = (VkDebugUtilsMessengerCreateInfoEXT) {
        .sType                   = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity         = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT,
        .messageType             = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback         = debugMessengerCallback
    };
    instance_info        = (VkInstanceCreateInfo)               {
        .sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext                   = (debug_messenger == NULL) ? NULL : &debug_messenger_info,
        .pApplicationInfo        = &application_info,
        .enabledLayerCount       = (debug_messenger == NULL) ? 0    : c_vulkan_debug_layer_count,
        .ppEnabledLayerNames     = (debug_messenger == NULL) ? NULL : c_vulkan_debug_layers,
        .enabledExtensionCount   = (debug_messenger == NULL) ? c_vk_required_release_ext_count : c_vk_required_debug_ext_count,
        .ppEnabledExtensionNames = c_vk_required_extensions
    };

    /* create instance */
    if(vkCreateInstance(
        &instance_info,
        NULL,
        instance
    ) != VK_SUCCESS) {
        LOG_ERROR("failed to create vulkan instance");
        goto fail;
    } else {
        LOG_MESSAGE("created vulkan instance");
    }

    /* create debug messenger (optional) */
    if(debug_messenger) {
        /* load create proc */
        create_debug_messenger = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
            *instance, 
            "vkCreateDebugUtilsMessengerEXT"
        );
        if(create_debug_messenger == NULL) {
            LOG_ERROR("failed to load vulkan create debug messenger proc");
            goto fail;
        }

        /* create messenger */
        if(create_debug_messenger(
            *instance,
            &debug_messenger_info,
            NULL,
            debug_messenger
        )) {
            LOG_ERROR("failed to create vulkan debug messenger");
        } else {
            LOG_MESSAGE("created vulkan debug messenger");
        }
    }

    return TRUE;

    fail: {
        /* destroy messenger if not NULL */
        if(debug_messenger != NULL && *debug_messenger != NULL) {
            /* load destroy proc */
            destroy_debug_messenger = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
                *instance,
                "vkDestroyDebugUtilsMessengerEXT"
            );
            if(destroy_debug_messenger != NULL) {
                destroy_debug_messenger(*instance, *debug_messenger, NULL);
            } else {
                LOG_ERROR("failed to load vulkan destroy debug messenger proc");
            }
        }
        /* detsroy instance if not NULL */
        if(instance != NULL && *instance != NULL) {
            vkDestroyInstance(*instance, NULL);
        }

        return FALSE;
    }
}

/* physical device  = vaild VkPhysicalDevice handle
   available_device = valid pointer to AvailableDevice struct which is supposed to be written
   surface          = valid VkSurface handle
   return           = TRUE (device meets criteria) / FALSE (device doesnt meet criteria)      */
static b32 getAvailableDevice(
    VkPhysicalDevice physical_device,
    VkSurfaceKHR     surface,
    AvailableDevice* available_device
) {
    #define QUEUE_MASK     (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT)
    #define RENDER_QUEUE   (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT)
    #define COMPUTE_QUEUE  (VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT)
    #define TRANSFER_QUEUE (VK_QUEUE_TRANSFER_BIT)

    /* local type for saving stack space */
    typedef union {
        VkPhysicalDeviceProperties       device_properties;
        VkPhysicalDeviceMemoryProperties memory_properties;
        VkQueueFamilyProperties          queue_families       [MAX_SCAN_QUEUE_FAMILIES_COUNT];
        VkExtensionProperties            extensions_properties[MAX_SCAN_DEVICE_EXTENSION_COUNT];
        VkPresentModeKHR                 present_modes        [MAX_SCAN_SURFACE_PRESENT_MODES];
        VkSurfaceFormatKHR               surface_formats      [MAX_SCAN_SURFACE_PRESENT_MODES];
    } TempStackData;

    TempStackData                  temp_stack_data       = (TempStackData){0};
    const VkMemoryHeap*            memory_heaps_begin    = NULL;
    const VkMemoryHeap*            memory_heaps_end      = NULL;
    const VkMemoryHeap*            memory_heap_i         = NULL;
    const VkQueueFamilyProperties* queues_begin          = NULL;
    const VkQueueFamilyProperties* queues_end            = NULL;
    const VkQueueFamilyProperties* queue_i               = NULL;
    const VkExtensionProperties*   extensions_begin      = NULL;
    const VkExtensionProperties*   extensions_end        = NULL;
    const VkExtensionProperties*   extension_i           = NULL;
    const char**                   extension_names_begin = NULL;
    const char**                   extension_names_end   = NULL;
    const char**                   extension_name_i      = NULL;
    const VkPresentModeKHR*        present_modes_begin   = NULL;
    const VkPresentModeKHR*        present_modes_end     = NULL;
    const VkPresentModeKHR*        present_mode_i        = NULL;
    const VkSurfaceFormatKHR*      surface_formats_begin = NULL;
    const VkSurfaceFormatKHR*      surface_formats_end   = NULL;
    const VkSurfaceFormatKHR*      surface_format_i      = NULL;
    u32                            queue_family_count    = 0;
    u32                            extension_count       = 0;
    u32                            present_mode_count    = 0;
    u32                            surface_format_count  = 0;
    u32                            i                     = 0;

    *available_device = (AvailableDevice){
        .physical_device    = physical_device,
        .render_family_id   = U32_MAX,
        .compute_family_id  = U32_MAX,
        .transfer_family_id = U32_MAX
    };

    /* get device info */
    vkGetPhysicalDeviceProperties(physical_device, &temp_stack_data.device_properties);
    available_device->device_id   = temp_stack_data.device_properties.deviceID;
    available_device->device_type = temp_stack_data.device_properties.deviceType;
    strcpy(available_device->device_name, temp_stack_data.device_properties.deviceName, VK_MAX_PHYSICAL_DEVICE_NAME_SIZE);
    LOG_MESSAGE("scanning vulkan physical device name: \"%s\" id: %u", available_device->device_name, available_device->device_id);
    
    /* calculate total device memory */
    vkGetPhysicalDeviceMemoryProperties(physical_device, &temp_stack_data.memory_properties);
    memory_heaps_begin = temp_stack_data.memory_properties.memoryHeaps;
    memory_heaps_end   = temp_stack_data.memory_properties.memoryHeaps + temp_stack_data.memory_properties.memoryHeapCount;
    memory_heap_i      = memory_heaps_begin;
    for(; memory_heap_i != memory_heaps_end; memory_heap_i++) {
        if(memory_heap_i->flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            available_device->device_heaps_size += memory_heap_i->size;
        } else {
            available_device->host_heaps_size   += memory_heap_i->size;
        }
    }

    /* query device queues */
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, NULL);
    if(queue_family_count == 0) {
        LOG_MESSAGE_TRACE("vulkan physical device has too no queue families found");
        goto fail;
    }
    if(queue_family_count > MAX_SCAN_QUEUE_FAMILIES_COUNT) {
        LOG_WARNING(
            "vulkan physical device has too many queues: %u, scanning paritally: %u", 
            queue_family_count, MAX_SCAN_QUEUE_FAMILIES_COUNT
        );
        queue_family_count = MAX_SCAN_QUEUE_FAMILIES_COUNT;
    }
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, temp_stack_data.queue_families);
    /* scan queue families */
    queues_begin = temp_stack_data.queue_families;
    queues_end   = temp_stack_data.queue_families + queue_family_count;
    queue_i      = queues_begin;
    for(i = 0; queue_i != queues_end; queue_i++, i++) {
        if(
            available_device->render_family_id == U32_MAX      &&
            (queue_i->queueFlags & QUEUE_MASK) == RENDER_QUEUE &&
            queue_i->queueCount != 0
        ) {
            LOG_MESSAGE("vulkan physical device found render queue id: %u", i);
            available_device->render_family_id = i;
        }
        if(
            available_device->compute_family_id == U32_MAX      &&
            (queue_i->queueFlags & QUEUE_MASK) == COMPUTE_QUEUE &&
            queue_i->queueCount != 0
        ) {
            LOG_MESSAGE("vulkan physical device found compute queue id: %u", i);
            available_device->compute_family_id = i;
        }
        if(
            available_device->transfer_family_id == U32_MAX      &&
            (queue_i->queueFlags & QUEUE_MASK) == TRANSFER_QUEUE &&
            queue_i->queueCount != 0
        ) {
            LOG_MESSAGE("vulkan physical device found transfer queue id: %u", i);
            available_device->transfer_family_id = i;
        }
    }

    if(available_device->render_family_id == U32_MAX) {
        LOG_MESSAGE_TRACE("vulkan physical device has no render queue");
        goto fail;
    }

    /* query etxensions */
    vkEnumerateDeviceExtensionProperties(physical_device, NULL, &extension_count, NULL);
    if(extension_count == 0 && c_device_extension_count != 0) {
        LOG_MESSAGE_TRACE("vulkan physical device has no extensions, but required: %u", c_device_extension_count);
        goto fail;
    }
    if(extension_count > MAX_SCAN_DEVICE_EXTENSION_COUNT) {
        LOG_WARNING(
            "vulkan physical device has too many extensions: %u, scanning partially: %u", 
            extension_count, MAX_SCAN_DEVICE_EXTENSION_COUNT
        );
        extension_count = MAX_SCAN_DEVICE_EXTENSION_COUNT;
    }
    vkEnumerateDeviceExtensionProperties(physical_device, NULL, &extension_count, temp_stack_data.extensions_properties);
    /* scan extensions */
    extension_names_begin = c_device_extensions;
    extension_names_end   = c_device_extensions + c_device_extension_count;
    extension_name_i      = extension_names_begin;
    for(; extension_name_i != extension_names_end; extension_name_i++) {
        extensions_begin  = temp_stack_data.extensions_properties;
        extensions_end    = temp_stack_data.extensions_properties + extension_count;
        extension_i       = extensions_begin;
        for(; extension_i != extensions_end; extension_i++) {
            if(strcmp(extension_i->extensionName, *extension_name_i) == 0) {
                goto matched_ext;    
            }
        }

        LOG_MESSAGE_TRACE("vulkan physical device unmatched extension: \"%s\"", *extension_name_i);
        goto fail;

        matched_ext: {
            continue;
        }
    }

    /* query present modes */
    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, NULL);
    if(present_mode_count == 0) {
        LOG_MESSAGE_TRACE("vulkan physical device has no present modes");
        goto fail;
    }
    if(present_mode_count > MAX_SCAN_SURFACE_PRESENT_MODES) {
        LOG_WARNING(
            "vulkan physical device has too many present modes: %u, scanning partially: %u", 
            present_mode_count, MAX_SCAN_SURFACE_PRESENT_MODES
        );
        present_mode_count = MAX_SCAN_SURFACE_PRESENT_MODES;
    }
    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, temp_stack_data.present_modes);
    /* scan present modes */
    available_device->surface_present_mode = VK_PRESENT_MODE_FIFO_KHR;
    present_modes_begin                    = temp_stack_data.present_modes;
    present_modes_end                      = temp_stack_data.present_modes + present_mode_count;
    present_mode_i                         = present_modes_begin;
    for(; present_mode_i != present_modes_end; present_mode_i++) {
        if(*present_mode_i == VK_PRESENT_MODE_MAILBOX_KHR) {
            available_device->surface_present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
            break;
        }
    }

    /* query surface formats */
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &surface_format_count, NULL);
    if(surface_format_count == 0) {
        LOG_MESSAGE_TRACE("vulkan physical device has no surface format");
        goto fail;
    }
    if(surface_format_count > MAX_SCAN_SURFACE_FORMATS) {
        LOG_WARNING(
            "vulkan physical device has too many surface formats: %u, scanning partially: %u", 
            surface_format_count, MAX_SCAN_SURFACE_FORMATS
        );
        surface_format_count = MAX_SCAN_SURFACE_FORMATS;
    }
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &surface_format_count, temp_stack_data.surface_formats);
    /* scan surface formats */
    available_device->surface_color_space  = temp_stack_data.surface_formats[0].colorSpace;
    available_device->surface_color_format = temp_stack_data.surface_formats[0].format;
    surface_formats_begin                  = temp_stack_data.surface_formats;
    surface_formats_end                    = temp_stack_data.surface_formats + surface_format_count;
    surface_format_i                       = surface_formats_begin;
    for(; surface_format_i != surface_formats_end; surface_format_i++) {
        if(
            surface_format_i->colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR &&
            surface_format_i->format     == VK_FORMAT_R8G8B8A8_UNORM
        ) {
            available_device->surface_color_space = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
            available_device->surface_color_format  = VK_FORMAT_R8G8B8A8_UNORM;
            break;
        }
    }

    LOG_MESSAGE("vulkan physical device accepted name: \"%s\" id: %u", available_device->device_name, available_device->device_id);
    return TRUE;

    fail: {
        LOG_MESSAGE("vulkan physical device rejected name: \"%s\" id: %u", available_device->device_name, available_device->device_id);
        return FALSE;
    }
}

/* device_l = valid pointer to AvailableDevice struct (priotitized option)
   device_r = valid pointer to AvailableDevice struct
   return   = TRUE if right is better than left, FALSE if left is better than right */
static b32 compareAvailableDevices(
    const AvailableDevice* device_l,
    const AvailableDevice* device_r
) {
    if(
        device_l->device_type == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU &&
        device_r->device_type == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU 
    ) {
        return TRUE;
    }
    if(
        device_l->device_type == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU   &&
        device_r->device_type == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU
    ) {
        return FALSE;
    }

    return FALSE;
}

/* instance                   = valid VkInstance handle 
   surface                    = valid VkSurfaceKHR handle
   available_devices          = allocated array of AvailableDevice structs of size no less than max_available_device_count
   max_available_device_count = maximum count of available devices that user is ready to get written in array, non zero value
   return                     = 0 (fail of any kind, no devices etc.) / written available device count                        */
static u32 queryAvailableDevices(
    VkInstance       instance,
    VkSurfaceKHR     surface,
    AvailableDevice* available_devices,
    u32              max_available_device_count
) {
    AvailableDevice   current_available_device                      = (AvailableDevice){0};
    VkPhysicalDevice  devices_begin[MAX_SCAN_GRAPHICS_DEVICE_COUNT] = {0};
    VkPhysicalDevice* devices_end                                   = NULL;
    VkPhysicalDevice* device_i                                      = NULL;
    AvailableDevice*  available_devices_end                         = NULL;
    AvailableDevice*  available_device_i                            = NULL;
    AvailableDevice*  available_device_insert                       = NULL;
    u32               device_count                                  = 0;
    u32               available_device_count                        = 0;

    /* get physical devices from vulkan */
    vkEnumeratePhysicalDevices(instance, &device_count, NULL);
    if(device_count == 0) {
        return 0;
    }
    if(device_count > MAX_SCAN_GRAPHICS_DEVICE_COUNT) {
        LOG_WARNING(
            "too many graphics devices attached: %u, scanning partially: %u",
            device_count, MAX_SCAN_GRAPHICS_DEVICE_COUNT
        );
        device_count = MAX_SCAN_GRAPHICS_DEVICE_COUNT;
    }
    vkEnumeratePhysicalDevices(instance, &device_count, devices_begin);

    /* iterate through devices and find suitable */
    devices_end = devices_begin + device_count;
    device_i    = devices_begin;
    for(; device_i != devices_end; device_i++) {
        /* if device is suitable, put it into array by sorting */
        if(getAvailableDevice(*device_i, surface, &current_available_device)) {
            /* iterate through existing suitable devices */
            available_devices_end   = available_devices + available_device_count;
            available_device_insert = available_devices;
            for(; available_device_insert != available_devices_end; available_device_insert++) {
                /* if found insert slot */
                if(compareAvailableDevices(
                    available_device_i, 
                    &current_available_device
                )) {
                    break;
                }
            }
            /* if reached limit and device is worse than everything else */
            if(
                available_devices_end == available_device_insert && 
                available_device_count == max_available_device_count
            ) {
                continue;
            }
            /* shift devices from insert slot 1 to the right; then write current device */
            available_device_i = available_device_insert;
            for(; available_device_i != available_devices_end; available_device_i++) {
                *(available_device_i + 1) = *(available_device_i);
            }
            *available_device_insert = current_available_device; 
            available_device_count++;
        }
    }

    return available_device_count;
}

/* instance              = valid VkInstance handle
   available_device      = valid pointer to AvailableDevice struct, that represents device we want to create
   device                = valid pointer to memory where VkDevice handle will be stored
   render_queue          = valid pointer to memory where VkQueue handle of render queue will be stored
   compute_queue         = valid pointer to memory where VkQueue handle of compute queue will be stored
   transfer_queue        = valid pointer to memory where VkQueue handle of transfer queue will be stored     
   render_command_pool   = valid pointer to memory where VkCommandPool handle of render queue will be stored
   compute_command_pool  = valid pointer to memory where VkCommandPool handle of compute queue will be stored
   transfer_command_pool = valid pointer to memory where VkCommandPool handle of transfer queue will be stored */
static b32 createVulkanDevice(
    VkInstance             instance,
    const AvailableDevice* available_device,
    VkDevice*              device,
    VkQueue*               render_queue,
    VkQueue*               compute_queue,
    VkQueue*               transfer_queue,
    VkCommandPool*         render_command_pool,
    VkCommandPool*         compute_command_pool,
    VkCommandPool*         transfer_command_pool
) {
    VkPhysicalDeviceDynamicRenderingFeatures dynamic_rendering_features = (VkPhysicalDeviceDynamicRenderingFeatures){0};
    VkPhysicalDeviceFeatures                 enabled_features           = (VkPhysicalDeviceFeatures){0};
    VkDeviceCreateInfo                       device_info                = (VkDeviceCreateInfo){0};
    VkDeviceQueueCreateInfo                  queue_infos[3]             = {0};
    VkCommandPoolCreateInfo                  pool_info                  = (VkCommandPoolCreateInfo){0};
    f32                                      queue_priority             = 1.0;
    u32                                      queue_count                = 0;

    /* setup queues infos */
    if(available_device->render_family_id != U32_MAX) {
        queue_infos[queue_count++] = (VkDeviceQueueCreateInfo) {
            .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = available_device->render_family_id,
            .queueCount       = 1,
            .pQueuePriorities = &queue_priority
        };
    }
    if(available_device->compute_family_id != U32_MAX) {
        queue_infos[queue_count++] = (VkDeviceQueueCreateInfo) {
            .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = available_device->compute_family_id,
            .queueCount       = 1,
            .pQueuePriorities = &queue_priority
        };
    }
    if(available_device->transfer_family_id != U32_MAX) {
        queue_infos[queue_count++] = (VkDeviceQueueCreateInfo) {
            .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = available_device->transfer_family_id,
            .queueCount       = 1,
            .pQueuePriorities = &queue_priority
        };
    }

    /* setup device info and features */
    dynamic_rendering_features   = (VkPhysicalDeviceDynamicRenderingFeatures) {
        .sType            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
        .dynamicRendering = TRUE
    };
    device_info = (VkDeviceCreateInfo) {
        .sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pEnabledFeatures        = &enabled_features,
        .pQueueCreateInfos       = queue_infos,
        .queueCreateInfoCount    = queue_count,
        .ppEnabledExtensionNames = c_device_extensions,
        .enabledExtensionCount   = c_device_extension_count,
        .pNext                   = &dynamic_rendering_features
    };
    /* create device */
    if(vkCreateDevice(
        available_device->physical_device,
        &device_info,
        NULL,
        device
    ) != VK_SUCCESS) {
        LOG_ERROR("failed to crate vulkan device");
        goto fail;
    }

    /* get queue handles */
    if(available_device->render_family_id != U32_MAX) {
        pool_info = (VkCommandPoolCreateInfo) {
            .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .queueFamilyIndex = available_device->render_family_id,
            .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
        };
        vkGetDeviceQueue(*device, available_device->render_family_id, 0, render_queue);
        if(vkCreateCommandPool(
            *device, 
            &pool_info, 
            NULL, 
            render_command_pool
        ) != VK_SUCCESS) {
            LOG_ERROR("failed to create vulkan render command pool");
            goto fail;
        }

    }
    if(available_device->compute_family_id != U32_MAX) {
        pool_info = (VkCommandPoolCreateInfo) {
            .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .queueFamilyIndex = available_device->compute_family_id,
            .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
        };
        vkGetDeviceQueue(*device, available_device->compute_family_id, 0, compute_queue);
        if(vkCreateCommandPool(
            *device,
            &pool_info,
            NULL,
            compute_command_pool
        ) != VK_SUCCESS) {
            LOG_ERROR("failed to create vulkan compute command pool");
            goto fail;
        } 
    }
    if(available_device->transfer_family_id != U32_MAX) {
        pool_info = (VkCommandPoolCreateInfo) {
            .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .queueFamilyIndex = available_device->transfer_family_id,
            .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
        };
        vkGetDeviceQueue(*device, available_device->transfer_family_id, 0, transfer_queue);
        if(vkCreateCommandPool(
            *device,
            &pool_info,
            NULL,
            transfer_command_pool
        ) != VK_SUCCESS) {
            LOG_ERROR("failed to create vulkan transfer command pool");
            goto fail;
        } 
    }

    LOG_MESSAGE(
        "created vulkan device name: \"%s\" id: %u render queue id: %u compute queue id: %u transfer queue id %u",
        available_device->device_name, available_device->device_id, 
        available_device->render_family_id, available_device->compute_family_id, available_device->transfer_family_id
    );
    return TRUE;

    fail: {
        if(*render_command_pool != NULL) {
            vkDestroyCommandPool(*device, *render_command_pool, NULL);
        }
        if(*compute_command_pool != NULL) {
            vkDestroyCommandPool(*device, *compute_command_pool, NULL);
        }
        if(*transfer_command_pool != NULL) {
            vkDestroyCommandPool(*device, *transfer_command_pool, NULL);
        }
        if(*device != NULL) {
            vkDestroyDevice(*device, NULL);
        }
        *device                = NULL;
        *render_queue          = NULL;
        *compute_queue         = NULL;
        *transfer_queue        = NULL;
        *render_command_pool   = NULL;
        *compute_command_pool  = NULL;
        *transfer_command_pool = NULL;
        return FALSE;
    }
}

/* available_device = valid pointer to AvailableDevice struct
   window           = valid WindowHandle handle that is displaying swapchain
   device           = valid VkDevice handle
   surface          = valid VkSurfaceKHR handle used by window and device
   swapchain        = valid pointer to VkSwapchainKHR handle memory, NULL if creating and old swapchain if recreating
   images           = valid pointer to array of VkImage handles */

static b32 createVulkanSwapchain(
    const AvailableDevice* available_device,
    WindowHandle           window,
    VkDevice               device,
    VkSurfaceKHR           surface,
    VkSwapchainKHR*        swapchain,
    VkImage*               images,
    VkImageView*           views,
    u32*                   image_count,
    u32                    max_image_count,
    u32                    optimal_image_count
) {
    VkSurfaceCapabilitiesKHR surface_capabilities = (VkSurfaceCapabilitiesKHR){0};
    VkSwapchainCreateInfoKHR swapchain_info       = (VkSwapchainCreateInfoKHR){0};
    VkImageViewCreateInfo    view_info            = (VkImageViewCreateInfo){0};
    VkImageView*             views_end            = NULL;
    VkImageView*             view_i               = NULL;
    VkImage*                 image_i              = NULL;
    u32                      min_image_count      = 0;
    u32                      id                   = 0;
    VkSwapchainKHR           old_swapchain        = *swapchain;

    while(1) {
        if(!processWindow(window)) {
            return TRUE;
        }
        /* if request fails, request again */
        if(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
            available_device->physical_device, 
            surface,
            &surface_capabilities
        ) != VK_SUCCESS) {
            continue;
        }
        /* if size is not proper, request again (for example hidden window 0, 0) */
        if(surface_capabilities.currentExtent.width == 0 || surface_capabilities.currentExtent.height == 0) {
            continue;
        }
        break;
    }

    /* destroy old swapchain image views and zero arrays */
    if(old_swapchain != NULL) {
        views_end = views + max_image_count;
        view_i    = views;
        image_i   = images;
        for(; view_i != views_end; view_i++, image_i++) {
            if(view_i != NULL) {
                vkDestroyImageView(device, *view_i, NULL);
            }
            *view_i  = NULL;
            *image_i = NULL;
        }
    } else {
        for(; view_i != views_end; view_i++, image_i++) {
            *view_i  = NULL;
            *image_i = NULL;
        }
    }

    /* calculate image count, if surface max image count = 0, there is no limit */
    if(surface_capabilities.maxImageCount == 0) {
        min_image_count = optimal_image_count;
    } else {
        min_image_count = CLAMP(surface_capabilities.minImageCount, surface_capabilities.maxImageCount, optimal_image_count);
        min_image_count = MIN(min_image_count, max_image_count);
    }

    /* create swapchain */
    *swapchain = NULL;
    swapchain_info = (VkSwapchainCreateInfoKHR) {
        .sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface          = surface,
        .oldSwapchain     = old_swapchain,
        .minImageCount    = min_image_count,
        .imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageExtent      = (VkExtent2D) {
            .width  = surface_capabilities.currentExtent.width,
            .height = surface_capabilities.currentExtent.height
        },
        .imageFormat      = available_device->surface_color_format,
        .imageColorSpace  = available_device->surface_color_space,
        .preTransform     = surface_capabilities.currentTransform,
        .compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .imageArrayLayers = 1
    };
    if(vkCreateSwapchainKHR(
        device,
        &swapchain_info,
        NULL,
        swapchain
    ) != VK_SUCCESS) {
        LOG_ERROR(
            "failed to create vulkan swapchain x: %u y: %u min image count: %u", 
            surface_capabilities.currentExtent.width,
            surface_capabilities.currentExtent.height,
            min_image_count
        );
        goto fail;
    }
    if(old_swapchain != NULL) {
        vkDestroySwapchainKHR(device, old_swapchain, NULL);
    }

    /* query images */
    vkGetSwapchainImagesKHR(device, *swapchain, image_count, NULL);
    if(*image_count == 0 || *image_count > max_image_count) {
        LOG_ERROR("swapchain image count is invalid: %u max image count: %u", *image_count, max_image_count);
        goto fail;
    }
    vkGetSwapchainImagesKHR(device, *swapchain, image_count, images);

    /* create image views */
    views_end = views + *image_count;
    view_i    = views;
    image_i   = images;
    id        = 0;
    for(; view_i != views_end; view_i++, image_i++, id++) {
        view_info = (VkImageViewCreateInfo) {
            .sType      = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image      = *image_i,
            .viewType   = VK_IMAGE_VIEW_TYPE_2D,
            .format     = available_device->surface_color_format,
            .components = (VkComponentMapping) {
                VK_COMPONENT_SWIZZLE_R,
                VK_COMPONENT_SWIZZLE_G,
                VK_COMPONENT_SWIZZLE_B,
                VK_COMPONENT_SWIZZLE_A
            },
            .subresourceRange = (VkImageSubresourceRange) {
                .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel   = 0,
                .baseArrayLayer = 0,
                .layerCount     = 1,
                .levelCount     = 1
            }
        };
        if(vkCreateImageView(
            device,
            &view_info,
            NULL,
            view_i
        ) != VK_SUCCESS) {
            LOG_ERROR("failed to create swapchain image view id: %u", id);
            goto fail;
        }
    }

    LOG_MESSAGE(
        "created vulkan swapchain x: %u y: %u image count: %u",
        surface_capabilities.currentExtent.width,
        surface_capabilities.currentExtent.height,
        *image_count
    );
    return TRUE;

    fail: {
        for(; (u64)view_i >= (u64)views; view_i--, image_i--) {
            if(*view_i != NULL) {
                vkDestroyImageView(device, *view_i, NULL);
            }
            *view_i = NULL;
            *image_i = NULL;
        }
        if(old_swapchain != NULL) {
            vkDestroySwapchainKHR(device, old_swapchain, NULL);
        }
        if(*swapchain != NULL) {
            vkDestroySwapchainKHR(device, *swapchain, NULL);
        }
        *swapchain   = NULL;
        *image_count = 0;
        return FALSE;
    }
}

static void destroyVulkanSwapchain(
    VkDevice       device,
    VkSwapchainKHR swapchain,
    VkImage*       images,
    VkImageView*   views,
    u32            image_count
) {
    VkImageView* views_end = views + image_count;
    VkImageView* view_i    = views;
    for(; view_i != views_end; view_i++) {
        vkDestroyImageView(device, *view_i, NULL);
    }
    vkDestroySwapchainKHR(device, swapchain, NULL);
}

/* in           = valid pointer to OpenRenderWindowIn struct
   page_address = valid pointer to 4096 memory space that will be used for storing VulkanCtx
   return       = valid pointer to VulkanCtx struct (success) / NULL (fail)                               */
CtxHandle openRenderWindow(
    const OpenRenderWindowIn* in, 
    void*                     page_address
) {
    VkPhysicalDeviceMemoryProperties    memory_properties       = (VkPhysicalDeviceMemoryProperties){0};
    const char*                         app_name                = NULL;
    VulkanCtx*                          vk_ctx                  = NULL;
    AvailableDevice*                    available_device_i      = NULL;
    AvailableDevice*                    available_devices_end   = NULL;
    PFN_vkDestroyDebugUtilsMessengerEXT destroy_debug_messenger = NULL;
    u32                                 id                      = 0;

    if(in == NULL || page_address == NULL) {
        LOG_ERROR("render open window invalid params");
        goto fail;
    }

    app_name = (in->name == NULL) ? " " : in->name;
    vk_ctx   = page_address;

    *vk_ctx  = (VulkanCtx) {
        .ctx_hash = VULKAN_CTX_HASH,
        .app_name = app_name
    };

    /* window */
    vk_ctx->window = createWindow(app_name, in->window_x, in->window_y);
    if(vk_ctx->window == NULL) {
        LOG_ERROR("failed to create vulkan window");
        goto fail;
    }

    /* instance */
    if(createVulkanInstance(
        app_name, 
        VK_MAKE_VERSION(1, 0, 0),
        &vk_ctx->instance,
        &vk_ctx->debug_messenger
    ) == FALSE) {
        LOG_ERROR("failed to create vulkan instance");
        goto fail;
    }

    /* surface */
    vk_ctx->surface = createSurface(vk_ctx->window, vk_ctx->instance);
    if(vk_ctx->surface == NULL) {
        LOG_ERROR("failed to create vulkan surface");
        goto fail;
    }

    /* query devices */
    vk_ctx->available_device_count = queryAvailableDevices(
        vk_ctx->instance,
        vk_ctx->surface,
        vk_ctx->available_devices,
        MAX_SCAN_GRAPHICS_DEVICE_COUNT
    );
    if(vk_ctx->available_device_count == 0) {
        LOG_ERROR("no available devices found");
        goto fail;
    }

    /* try to mount devices */
    available_devices_end = vk_ctx->available_devices + vk_ctx->available_device_count;
    available_device_i    = vk_ctx->available_devices;
    id                    = 0;
    for(; available_device_i != available_devices_end; available_device_i++, id++) {
        /* mount device */
        if(!createVulkanDevice(
            vk_ctx->instance,
            available_device_i,
            &vk_ctx->device,
            &vk_ctx->render_queue,
            &vk_ctx->compute_queue,
            &vk_ctx->transfer_queue,
            &vk_ctx->render_command_pool,
            &vk_ctx->compute_command_pool,
            &vk_ctx->transfer_command_pool
        )) {
            vk_ctx->current_device_id = id;
            continue;
        }
        /* create swapchain */
        if(!createVulkanSwapchain(
            vk_ctx->available_devices + vk_ctx->current_device_id,
            vk_ctx->window,
            vk_ctx->device,
            vk_ctx->surface,
            &vk_ctx->swapchain,
            vk_ctx->swapchain_images,
            vk_ctx->swapchain_views,
            &vk_ctx->swapchain_image_count,
            MAX_SWAPCHAIN_IMAGES,
            2
        )) {
            continue;
        }

        goto device_creation_succeed;
    }

    LOG_ERROR("failed to create all vulkan devices");
    goto fail;
    device_creation_succeed: {};

    /* get memory data */
    vkGetPhysicalDeviceMemoryProperties(
        vk_ctx->available_devices[vk_ctx->current_device_id].physical_device,
        &memory_properties
    );
    memcpy(vk_ctx->memory_heaps, memory_properties.memoryHeaps, sizeof(VkMemoryHeap) * VK_MAX_MEMORY_HEAPS);
    memcpy(vk_ctx->memory_types, memory_properties.memoryTypes, sizeof(VkMemoryType) * VK_MAX_MEMORY_TYPES);
    vk_ctx->memory_type_count = memory_properties.memoryTypeCount;
    vk_ctx->memory_mutex = createMutexHandle("vulkan vram mutex");
    if(vk_ctx->memory_mutex == NULL) {
        LOG_ERROR("failed to create vulkan vram mutex");
        goto fail;
    }

    return (CtxHandle)vk_ctx;

    fail: {
        if(vk_ctx != NULL) {
            if(vk_ctx->memory_mutex != NULL) {
                CloseHandle(vk_ctx->memory_mutex);
            }
            if(vk_ctx->device != NULL) {
                vkDestroyDevice(vk_ctx->device, NULL);
            } 
            if(vk_ctx->surface != NULL) {
                vkDestroySurfaceKHR(vk_ctx->instance, vk_ctx->surface, NULL);
            }
            if(vk_ctx->debug_messenger != NULL) {
                destroy_debug_messenger = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(vk_ctx->instance, "vkDestroyDebugUtilsMessengerEXT");
                if(destroy_debug_messenger) {
                    destroy_debug_messenger(vk_ctx->instance, vk_ctx->debug_messenger, NULL);
                }
            }
            if(vk_ctx->instance != NULL) {
                vkDestroyInstance(vk_ctx->instance, NULL);
            }
            if(vk_ctx->window != NULL) {
                destroyWindow(vk_ctx->window, app_name);
            }

            *vk_ctx = (VulkanCtx){0};
        }
        return NULL;
    }
}

/* ctx = valid CtxHandle (address of VulkanCtx struct) */
b32 closeRenderWindow(
    CtxHandle ctx
) {
    VulkanCtx*                          vk_ctx                  = NULL;
    PFN_vkDestroyDebugUtilsMessengerEXT destroy_debug_messenger = NULL;
    MutexResult                         wait_result             = 0;
    
    /* validation */
    if(ctx == NULL) {
        LOG_ERROR("ctx or(and) allocator is(are) NULL");
        goto fail;
    }
    if(*((u64*)ctx) == VULKAN_CTX_HASH) {
        vk_ctx = (VulkanCtx*)ctx;
    } else {
        LOG_ERROR("trying to destroy VulkanCtx with invalid hash");
        goto fail;
    }

    loadShaderPrograms(ctx, NULL);

    /* destroy mutex */
    wait_result = waitForMutex(vk_ctx->memory_mutex, 4096);
    if(wait_result == MUTEX_FAILED || wait_result == MUTEX_TIMEOUT) {
        LOG_ERROR("failed to wait for vulkan vram mutex");
        goto fail;
    }
    destroyMutexHandle(vk_ctx->memory_mutex);

    /* detsruction */
    destroyVulkanSwapchain(
        vk_ctx->device, 
        vk_ctx->swapchain, 
        vk_ctx->swapchain_images, 
        vk_ctx->swapchain_views,
        vk_ctx->swapchain_image_count
    ); LOG_MESSAGE("destroyed vulkan swapchain");

    if(vk_ctx->render_command_pool) {
        vkDestroyCommandPool(vk_ctx->device, vk_ctx->render_command_pool, NULL);
    }
    if(vk_ctx->compute_command_pool) {
        vkDestroyCommandPool(vk_ctx->device, vk_ctx->compute_command_pool, NULL);
    }
    if(vk_ctx->transfer_command_pool) {
        vkDestroyCommandPool(vk_ctx->device, vk_ctx->transfer_command_pool, NULL);
    }

    vkDestroyDevice(vk_ctx->device, NULL);                        LOG_MESSAGE("destroyed vulkan device");
    vkDestroySurfaceKHR(vk_ctx->instance, vk_ctx->surface, NULL); LOG_MESSAGE("destroyed vulkan surface");

    if(vk_ctx->debug_messenger != NULL) {
        destroy_debug_messenger = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
            vk_ctx->instance, 
            "vkDestroyDebugUtilsMessengerEXT"
        );
        if(destroy_debug_messenger) {
            destroy_debug_messenger(vk_ctx->instance, vk_ctx->debug_messenger, NULL);  
            LOG_MESSAGE("destroyed vulkan debug messenger");
        }
    }

    vkDestroyInstance(vk_ctx->instance, NULL);       LOG_MESSAGE("destroyed vulkan instance");
    destroyWindow(vk_ctx->window, vk_ctx->app_name); LOG_MESSAGE("destroyed vulkan window");

    /* memory free */
    *vk_ctx = (VulkanCtx){0};

    return TRUE;

    fail: {
        return FALSE;
    }
}

/* types are sorted in good to worse order */
const VkMemoryPropertyFlags c_device_memory_flags[] = {
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
const VkMemoryPropertyFlags c_device_host_memory_flags[] = {
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
const VkMemoryPropertyFlags c_host_memory_flags[] = {
    VK_MEMORY_PROPERTY_HOST_CACHED_BIT  ,

    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
    VK_MEMORY_PROPERTY_HOST_CACHED_BIT,

    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,

    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
    VK_MEMORY_PROPERTY_HOST_CACHED_BIT  |
    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
};

/* intenal function, hash check is unnecessary */
b32 (allocateVram)(
    VulkanCtx*      vk_ctx, 
    VramAllocation* allocation, 
    u64             size, 
    u32             type_bits, 
    VramType        type
) {
    VkMemoryAllocateInfo         alloc_info         = (VkMemoryAllocateInfo){0};
    VkDeviceMemory               device_memory      = NULL;
    const VkMemoryPropertyFlags* memory_flags       = NULL;
    VkMemoryType*                memory_types       = vk_ctx->memory_types;
    VkMemoryHeap*                memory_heaps       = vk_ctx->memory_heaps;
    MutexResult                  wait_result        = 0;
    u32                          memory_types_count = vk_ctx->memory_type_count;
    u32                          memory_flags_count = 0;
    u32                          i                  = 0;
    u32                          j                  = 0;
    u32                          memory_id          = U32_MAX;
    u32                          heap_id            = U32_MAX;

    wait_result = waitForMutex(vk_ctx->memory_mutex, 1000);
    if(wait_result == MUTEX_FAILED || wait_result == MUTEX_TIMEOUT) {
        LOG_ERROR("failed to wait for vram mutex");
        goto fail;
    }

    if(type == VRAM_TYPE_DEVICE) {
        memory_flags       = c_device_memory_flags;
        memory_flags_count = ARRAY_SIZE(c_device_memory_flags);
    }
    if(type == VRAM_TYPE_DEVICE_HOST) {
        memory_flags       = c_device_host_memory_flags;
        memory_flags_count = ARRAY_SIZE(c_device_host_memory_flags);
    }
    if(type == VRAM_TYPE_HOST) {
        memory_flags       = c_host_memory_flags;
        memory_flags_count = ARRAY_SIZE(c_host_memory_flags);
    }

    for(i = 0; i != memory_flags_count; i++) {
        for(j = 0; j != memory_types_count; j++) {
            if(
                memory_types[j].propertyFlags == memory_flags[i] &&
                memory_heaps[memory_types[j].heapIndex].size >= size
            ) {
                if(memory_id == U32_MAX) {
                    memory_id = j;
                    heap_id   = memory_types[j].heapIndex;
                }
                if(type_bits & (0x1 << j)) {
                    memory_id = j;
                    heap_id   = memory_types[j].heapIndex;
                    break;
                }
            }
        }
    }
    if(memory_id == U32_MAX) {
        LOG_ERROR("failed to find free memory type for allocation");
        goto fail;
    }

    /* allocate memory */
    alloc_info = (VkMemoryAllocateInfo) {
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .memoryTypeIndex = memory_id,
        .allocationSize  = size
    };
    if(vkAllocateMemory(
        vk_ctx->device, 
        &alloc_info, 
        NULL, 
        &device_memory
    ) != VK_SUCCESS) {
        LOG_ERROR("failed to allocate vulkan device memory");
        goto fail;
    }

    memory_heaps[heap_id].size -= size;
    *allocation = (VramAllocation) {
        .device_memory = device_memory,
        .size          = size,
        .memory_heap   = heap_id,
        .memory_type   = memory_id
    };
    releaseMutex(vk_ctx->memory_mutex);
    return TRUE;

    fail: {
        if(device_memory != NULL) {
            vkFreeMemory(vk_ctx->device, device_memory, NULL);
        }
        releaseMutex(vk_ctx->memory_mutex);
        return FALSE;
    }
}

/* intenal function, hash check is unnecessary */
void (freeVram)(
    VulkanCtx*      vk_ctx, 
    VramAllocation* allocation
) {
    MutexResult wait_result = 0;

    wait_result = waitForMutex(vk_ctx->memory_mutex, 1000);
    if(wait_result == MUTEX_FAILED || wait_result == MUTEX_TIMEOUT) {
        LOG_ERROR("failed to wait for vram mutex");
        goto fail;
    }

    vkFreeMemory(vk_ctx->device, allocation->device_memory, NULL);
    vk_ctx->memory_heaps[allocation->memory_heap].size += allocation->size;
    *allocation = (VramAllocation){0};

    releaseMutex(vk_ctx->memory_mutex);
    return;

    fail: {
        releaseMutex(vk_ctx->memory_mutex);
        return;
    }
}

static VkPipeline createGraphicsPipeline(
    VkDevice           device,
    VkPipelineLayout   pipeline_layout,
    VkShaderModule     vertex_shader,
    VkShaderModule     fragment_shader,
    const VkFormat*    color_attachments,
    VkFormat           depth_attachment,
    u32                color_attachment_count,
    ShaderProgramFlags flags
) {
    VkVertexInputBindingDescription        vertex_binding               = {0};
    VkVertexInputAttributeDescription      vertex_attributes[5]         = {0};
    VkPipelineShaderStageCreateInfo        shader_stages[2]             = {0};
    u32                                    vertex_binding_count         = 0;
    u32                                    vertex_attribute_count       = 0;
    VkPipelineDynamicStateCreateInfo       dynamic_state                = (VkPipelineDynamicStateCreateInfo){0};
    VkPipelineVertexInputStateCreateInfo   vertex_input_state           = (VkPipelineVertexInputStateCreateInfo){0};
    VkPipelineInputAssemblyStateCreateInfo input_assembly_state         = (VkPipelineInputAssemblyStateCreateInfo){0};
    VkPipelineRasterizationStateCreateInfo rasterization_state          = (VkPipelineRasterizationStateCreateInfo){0};
    VkPipelineMultisampleStateCreateInfo   multisample_state            = (VkPipelineMultisampleStateCreateInfo){0};
    VkPipelineColorBlendAttachmentState    color_blend_attachment_state = (VkPipelineColorBlendAttachmentState){0};
    VkPipelineColorBlendStateCreateInfo    color_blend_state            = (VkPipelineColorBlendStateCreateInfo){0};
    VkPipelineDepthStencilStateCreateInfo  depth_stencil_state          = (VkPipelineDepthStencilStateCreateInfo){0};
    VkPipelineRenderingCreateInfoKHR       rendering_info               = (VkPipelineRenderingCreateInfoKHR){0};
    VkPipelineViewportStateCreateInfo      viewport_state               = (VkPipelineViewportStateCreateInfo){0};
    VkGraphicsPipelineCreateInfo           pipeline_info                = (VkGraphicsPipelineCreateInfo){0};
    VkPipeline                             pipeline                     = NULL;

    vertex_binding = (VkVertexInputBindingDescription) {
        .binding   = 0,
        .stride    = sizeof(RenderMeshVertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
    };

    if(flags & SHADER_PROGRAM_FLAG_VERTEX_POSITION) {
        vertex_binding_count = 1;
        vertex_attributes[vertex_attribute_count++] = (VkVertexInputAttributeDescription) {
            .binding  = 0,
            .location = 0,
            .format   = VK_FORMAT_R32G32B32A32_SFLOAT,
            .offset   = 0
        };
    }
    if(flags & SHADER_PROGRAM_FLAG_VERTEX_NORMAL) {
        vertex_binding_count = 1;
        vertex_attributes[vertex_attribute_count++] = (VkVertexInputAttributeDescription) {
            .binding  = 0,
            .location = 1,
            .format   = VK_FORMAT_R32G32B32A32_SFLOAT,
            .offset   = 16
        };
    }
    if(flags & SHADER_PROGRAM_FLAG_VERTEX_TEXCOORD) {
        vertex_binding_count = 1;
        vertex_attributes[vertex_attribute_count++] = (VkVertexInputAttributeDescription) {
            .binding  = 0,
            .location = 2,
            .format   = VK_FORMAT_R32G32B32A32_SFLOAT,
            .offset   = 32
        };
    }
    if(flags & SHADER_PROGRAM_FLAG_VERTEX_WEIGHTS) {
        vertex_binding_count = 1;
        vertex_attributes[vertex_attribute_count++] = (VkVertexInputAttributeDescription) {
            .binding  = 0,
            .location = 3,
            .format   = VK_FORMAT_R32G32B32A32_SFLOAT,
            .offset   = 24
        };
    }
    if(flags & SHADER_PROGRAM_FLAG_VERTEX_BONES) {
        vertex_binding_count = 1;
        vertex_attributes[vertex_attribute_count++] = (VkVertexInputAttributeDescription) {
            .binding  = 0,
            .location = 4,
            .format   = VK_FORMAT_R32G32B32A32_SFLOAT,
            .offset   = 32
        };
    }

    /* shader stages */
    shader_stages[0] = (VkPipelineShaderStageCreateInfo) {
        .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pName  = SHADER_ENTRY_VERTEX,
        .stage  = VK_SHADER_STAGE_VERTEX_BIT,
        .module = vertex_shader
    };
    shader_stages[1] = (VkPipelineShaderStageCreateInfo) {
        .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pName  = SHADER_ENTRY_FRAGMENT,
        .stage  = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module = fragment_shader
    };

    /* these states will be changed on fly */
    dynamic_state = (VkPipelineDynamicStateCreateInfo) {
        .sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2,
        .pDynamicStates    = (const VkDynamicState[]){VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR}
    };    
    vertex_input_state = (VkPipelineVertexInputStateCreateInfo) {
        .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexAttributeDescriptionCount = vertex_attribute_count,
        .pVertexAttributeDescriptions    = vertex_attributes,
        .vertexBindingDescriptionCount   = vertex_binding_count,
        .pVertexBindingDescriptions      = &vertex_binding
    };
    input_assembly_state = (VkPipelineInputAssemblyStateCreateInfo) {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = FALSE
    };
    rasterization_state = (VkPipelineRasterizationStateCreateInfo) {
        .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable        = FALSE,
        .rasterizerDiscardEnable = FALSE,
        .polygonMode             = VK_POLYGON_MODE_FILL,
        .lineWidth               = 1.0f,
        .cullMode                = VK_CULL_MODE_NONE,
        .frontFace               = VK_FRONT_FACE_CLOCKWISE,
        .depthBiasEnable         = FALSE,
        .depthBiasConstantFactor = 0.0f,
        .depthBiasClamp          = 0.0f,
        .depthBiasSlopeFactor    = 0.0f
    };
    multisample_state = (VkPipelineMultisampleStateCreateInfo) {
        .sType                 = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .sampleShadingEnable   = FALSE,
        .rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT,
        .minSampleShading      = 1.0f,
        .pSampleMask           = NULL,
        .alphaToCoverageEnable = FALSE,
        .alphaToOneEnable      = FALSE
    };
    color_blend_attachment_state = (VkPipelineColorBlendAttachmentState) {
        .colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        .blendEnable         = VK_FALSE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
        .colorBlendOp        = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp        = VK_BLEND_OP_ADD
    };
    color_blend_state = (VkPipelineColorBlendStateCreateInfo) {
        .sType             = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable     = VK_FALSE,
        .logicOp           = VK_LOGIC_OP_COPY,
        .attachmentCount   = 1,
        .pAttachments      = &color_blend_attachment_state,
        .blendConstants[0] = 0.0f,
        .blendConstants[1] = 0.0f,
        .blendConstants[2] = 0.0f,
        .blendConstants[3] = 0.0f
    };
    depth_stencil_state = (VkPipelineDepthStencilStateCreateInfo) {
        .sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable  = TRUE,
        .depthWriteEnable = TRUE,
        .depthCompareOp   = VK_COMPARE_OP_LESS_OR_EQUAL
    };

    /* dynamic, will be replaced */
    viewport_state = (VkPipelineViewportStateCreateInfo) {
        .sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount  = 1
    };

    rendering_info = (VkPipelineRenderingCreateInfoKHR) {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
        .colorAttachmentCount    = color_attachment_count,
        .pColorAttachmentFormats = color_attachments,
        .depthAttachmentFormat   = depth_attachment
    };

    pipeline_info = (VkGraphicsPipelineCreateInfo) {
        .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .basePipelineHandle  = NULL,
        .basePipelineIndex   = -1,
        .stageCount          = 2,
        .pStages             = shader_stages,
        .pVertexInputState   = &vertex_input_state,
        .pInputAssemblyState = &input_assembly_state,
        .pViewportState      = &viewport_state,
        .pRasterizationState = &rasterization_state,
        .pMultisampleState   = &multisample_state,
        .pDepthStencilState  = &depth_stencil_state,
        .pColorBlendState    = &color_blend_state,
        .pDynamicState       = &dynamic_state,
        .layout              = pipeline_layout,
        .renderPass          = NULL,
        .pNext               = &rendering_info
    };

    if(vkCreateGraphicsPipelines(
        device, 
        NULL, 
        1, 
        &pipeline_info, 
        NULL, 
        &pipeline
    ) != VK_SUCCESS) {
        return NULL;
    }
    return pipeline;
}

static b32 createPipelines(
    VkDevice             device,
    VkPipelineLayout     pipeline_layout,
    VkFormat             surface_format,
    const ShaderProgram* programs,
    VkPipeline*          pipelines,
    u32                  pipeline_count
) {
    VkShaderModuleCreateInfo module_a_info                                  = (VkShaderModuleCreateInfo){0};
    VkShaderModuleCreateInfo module_b_info                                  = (VkShaderModuleCreateInfo){0};
    VkFormat                 color_attachments[MAX_RENDER_ATTACHMENT_COUNT] = {0};
    VkFormat                 depth_attachment                               = 0;
    u32                      color_attachment_count                         = 0;
    const u32*               color_attachment_ids_end                       = NULL;
    const u32*               color_attachment_id_j                          = NULL;
    VkFormat*                color_attachment_j                             = NULL;
    VkShaderModule           module_a                                       = NULL;
    VkShaderModule           module_b                                       = NULL;
    VkPipeline*              pipelines_end                                  = pipelines + pipeline_count;
    VkPipeline*              pipeline_i                                     = pipelines;
    const ShaderProgram*     program_i                                      = programs;
    u32                      i                                              = 0;
    u32                      j                                              = 0;


    for(; pipeline_i != pipelines_end; pipeline_i++, program_i++, i++) {
        if(program_i->type == SHADER_PROGRAM_TYPE_GRAPHICS) {
            /* load color attachments if needed */
            if(
                color_attachment_count   != program_i->color_attachment_count                                   ||
                color_attachment_ids_end != program_i->color_attachment_ids + program_i->color_attachment_count
            ) {
                color_attachment_count   = program_i->color_attachment_count;
                color_attachment_ids_end = program_i->color_attachment_ids + program_i->color_attachment_count;
                color_attachment_id_j    = program_i->color_attachment_ids;
                color_attachment_j       = color_attachments;
                for(; color_attachment_id_j != color_attachment_ids_end; color_attachment_id_j++, color_attachment_j++) {
                    if(*color_attachment_id_j == IMAGE_SURFACE_COLOR_ID) {
                        *color_attachment_j = surface_format;
                        continue;
                    }
                    
                    LOG_ERROR(
                        "invalid color attachment shader id: %u/%u attachment id: %u/%u", 
                        i, pipeline_count, j, color_attachment_count
                    );
                    goto fail;
                }
            }
            /* load depth attachment */
            switch (program_i->depth_attachment_id) {
                case U32_MAX: {
                    depth_attachment = VK_FORMAT_UNDEFINED;
                    break;
                }
                case IMAGE_SCREEN_DEPTH_ID: {
                    depth_attachment = VK_FORMAT_D32_SFLOAT;
                    break;
                }

                default: {
                    LOG_ERROR("invalid shader depth attachment id: %u/%u", i, pipeline_count);
                    goto fail;
                }
            }

            /* fill module infos */
            module_a_info = (VkShaderModuleCreateInfo) {
                .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                .pCode    = program_i->vertex,
                .codeSize = program_i->vertex_size
            };
            module_b_info = (VkShaderModuleCreateInfo) {
                .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                .pCode    = program_i->fragment,
                .codeSize = program_i->fragment_size
            };
            
            /* create modules */
            if(vkCreateShaderModule(
                device, 
                &module_a_info,
                NULL,
                &module_a
            ) != VK_SUCCESS) {
                LOG_ERROR("failed to create vertex shader module id: %u/%u", i, pipeline_count);
                goto fail;
            }
            if(vkCreateShaderModule(
                device, 
                &module_b_info,
                NULL,
                &module_b
            ) != VK_SUCCESS) {
                LOG_ERROR("failed to create fragment shader module id: %u/%u", i, pipeline_count);
                goto fail;
            }

            /* create pipeline */
            *pipeline_i = createGraphicsPipeline(
                device,
                pipeline_layout,
                module_a,
                module_b,
                color_attachments,
                depth_attachment,
                color_attachment_count,
                program_i->flags
            );
            if(*pipeline_i == NULL) {
                LOG_ERROR("failed to create graphics pipeline id: %u/%u", i, pipeline_count);
                goto fail;
            }

            /* detsroy not needed modules */
            vkDestroyShaderModule(device, module_a, NULL);
            vkDestroyShaderModule(device, module_b, NULL);
            module_a = NULL;
            module_b = NULL;

            continue;
        }

        LOG_ERROR("invalid shader program type id: %u/%u", i, pipeline_count);
        goto fail;
    }

    return TRUE;

    fail: {
        if(module_a != NULL) {
            vkDestroyShaderModule(device, module_a, NULL);
        }
        if(module_b != NULL) {
            vkDestroyShaderModule(device, module_b, NULL);
        }
        for(; (u64)pipeline_i != (u64)pipelines - 1; pipeline_i--) {
            vkDestroyPipeline(device, *pipeline_i, NULL);
        }
        return FALSE;
    }
}

b32 loadShaderPrograms(
    CtxHandle                   ctx, 
    const LoadShaderProgramsIn* in
) {
    VulkanCtx*                 vk_ctx               = (VulkanCtx*)ctx;
    VkPipelineLayoutCreateInfo pipeline_layout_info = (VkPipelineLayoutCreateInfo){0};
    VulkanShaders*             vk_shaders           = NULL;
    VkPipeline*                pipelines_end        = NULL;
    VkPipeline*                pipeline_i           = NULL;

    if(ctx == NULL) {
        LOG_ERROR("ivalid input params");
        goto fail;
    }
    if(vk_ctx->ctx_hash != VULKAN_CTX_HASH) {
        LOG_ERROR("vulkan ctx hash invalid");
        goto fail;
    }
    /* get shaders address */
    vk_shaders = &vk_ctx->shaders;

    /* detsroy old data */
    if(vk_shaders->pipeline_count != 0) {
        pipelines_end = vk_shaders->pipelines + vk_shaders->pipeline_count;
        pipeline_i    = vk_shaders->pipelines;
        for(; pipeline_i != pipelines_end; pipeline_i++) {
            vkDestroyPipeline(vk_ctx->device, *pipeline_i, NULL);
        }

        free(vk_shaders->pipelines);
        vkDestroyPipelineLayout(vk_ctx->device, vk_shaders->pipeline_layout, NULL);
        LOG_MESSAGE("destroyed old vulkan pipelines count: %u", vk_shaders->pipeline_count);

        *vk_shaders = (VulkanShaders){0};
    }

    /* if no programs are created its destruction call only */
    if(in != NULL && in->program_count != 0) {
        /* create pipeline layout */
        pipeline_layout_info = (VkPipelineLayoutCreateInfo) {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO
        };
        if(vkCreatePipelineLayout(
            vk_ctx->device,
            &pipeline_layout_info,
            NULL,
            &vk_shaders->pipeline_layout
        ) != VK_SUCCESS) {
            LOG_ERROR("failed to create vulkan pipeline layout");
            goto fail;
        }

        /* allocate pipelines array */
        vk_shaders->pipelines = malloc(sizeof(VkPipeline) * in->program_count);
        if(vk_shaders->pipelines == NULL) {
            LOG_ERROR("failed to allocate vulkan pipelines array");
            goto fail;
        }
        /* zero initialization is required for safe usage of createPipelines */
        pipelines_end = vk_shaders->pipelines + in->program_count;
        pipeline_i    = vk_shaders->pipelines;
        for(; pipeline_i != pipelines_end; pipeline_i++) {
            *pipeline_i = NULL;
        }
        /* create pipelines */
        if(!createPipelines(
            vk_ctx->device,
            vk_shaders->pipeline_layout,
            vk_ctx->available_devices[vk_ctx->current_device_id].surface_color_format,
            in->programs,
            vk_shaders->pipelines,
            in->program_count
        )) {
            LOG_ERROR("failed to create vulkan pipelines");
            goto fail;
        }
        vk_shaders->pipeline_count = in->program_count;

        LOG_MESSAGE("created vulkan pipelines count: %u", vk_shaders->pipeline_count);
    }

    return TRUE;

    fail: {
        if(vk_shaders) {
            if(vk_shaders->pipeline_layout != NULL) {
                vkDestroyPipelineLayout(vk_ctx->device, vk_shaders->pipeline_layout, NULL);
            }
            *vk_shaders = (VulkanShaders){0};
        }
        return FALSE;
    }
}


const VkDescriptorPoolSize c_descriptor_pool_sizes[] = {
    (VkDescriptorPoolSize) {
        .type            = VK_DESCRIPTOR_TYPE_SAMPLER,
        .descriptorCount = 4
    },
    (VkDescriptorPoolSize) {
        .type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = MAX_BINDINGS_PER_DESCRIPTOR
    },
    (VkDescriptorPoolSize) {
        .type            = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .descriptorCount = MAX_BINDINGS_PER_DESCRIPTOR
    },
    (VkDescriptorPoolSize) {
        .type            = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        .descriptorCount = MAX_BINDINGS_PER_DESCRIPTOR
    },
    (VkDescriptorPoolSize) {
        .type            = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        .descriptorCount = MAX_BINDINGS_PER_DESCRIPTOR * 2
    }
};

VkDescriptorSetLayoutBinding c_set_0_bindings[] = (VkDescriptorSetLayoutBinding) {
    (VkDescriptorSetLayoutBinding) {
        .binding         = 0,
        .stageFlags      = VK_SHADER_STAGE_ALL,
        .descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLER,
        .descriptorCount = 1
    },
    (VkDescriptorSetLayoutBinding) {
        .binding         = 1,
        .stageFlags      = VK_SHADER_STAGE_ALL,
        .descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLER,
        .descriptorCount = 1
    },
    (VkDescriptorSetLayoutBinding) {
        .binding         = 2,
        .stageFlags      = VK_SHADER_STAGE_ALL,
        .descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLER,
        .descriptorCount = 1
    },
    (VkDescriptorSetLayoutBinding) {
        .binding         = 3,
        .stageFlags      = VK_SHADER_STAGE_ALL,
        .descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLER,
        .descriptorCount = 1
    }
};

VkDescriptorSetLayoutBinding c_set_1_bindings_default[] = (VkDescriptorSetLayoutBinding) {
    (VkDescriptorSetLayoutBinding) {
        .binding         = 0,
        .stageFlags      = VK_SHADER_STAGE_ALL,
        .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .descriptorCount = 1
    }
};

VkDescriptorSetLayoutBinding c_set_2_bindings[] = (VkDescriptorSetLayoutBinding) {
    (VkDescriptorSetLayoutBinding) {
        .binding         = 0,
        .stageFlags      = VK_SHADER_STAGE_ALL,
        .descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        .descriptorCount = 1
    },
    (VkDescriptorSetLayoutBinding) {
        .binding         = 1,
        .stageFlags      = VK_SHADER_STAGE_ALL,
        .descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        .descriptorCount = 1
    },
    (VkDescriptorSetLayoutBinding) {
        .binding         = 2,
        .stageFlags      = VK_SHADER_STAGE_ALL,
        .descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        .descriptorCount = 1
    },
    (VkDescriptorSetLayoutBinding) {
        .binding         = 3,
        .stageFlags      = VK_SHADER_STAGE_ALL,
        .descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        .descriptorCount = 1
    },
    (VkDescriptorSetLayoutBinding) {
        .binding         = 4,
        .stageFlags      = VK_SHADER_STAGE_ALL,
        .descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        .descriptorCount = 1
    }
};

... createResources also first decide how they are stored

b32 createDescriptorSets(
    VkDevice                              device,
    VkDescriptorPool                      pool,
    const VkDescriptorSetLayoutBinding*   set_1_bindings,
    u32                                   set_1_binding_count,
    VkDescriptorSetLayout*                set_layouts,
    VkDescriptorSet*                      sets,
    u32                                   set_count
) {
    VkDescriptorSetLayoutCreateInfo layout_info = (VkDescriptorSetLayoutCreateInfo){0};
    VkDescriptorSetAllocateInfo     set_info    = (VkDescriptorSetAllocateInfo){0};

    /* create layout 0 */
    layout_info = (VkDescriptorSetLayoutCreateInfo) {
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = ARRAY_SIZE(c_set_0_bindings),
        .pBindings    = c_set_0_bindings
    };
    if(vkCreateDescriptorSetLayout(
        device,
        &layout_info,
        NULL,
        &set_layouts[0]
    ) != VK_SUCCESS) {
        LOG_ERROR("failed to create descriptor set layout 0");
        goto fail;
    }

    /* create layout 1 */
    layout_info = (VkDescriptorSetLayoutCreateInfo) {
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = (set_1_binding_count == 0) ? ARRAY_SIZE(c_set_1_bindings_default) : set_1_binding_count,
        .pBindings    = (set_1_binding_count == 0) ? c_set_1_bindings_default             : set_1_bindings
    };
    if(vkCreateDescriptorSetLayout(
        device,
        &layout_info,
        NULL,
        &set_layouts[1]
    ) != VK_SUCCESS) {
        LOG_ERROR("failed to create descriptor set layout 1");
        goto fail;
    }

    /* create layout 2 */
    layout_info = (VkDescriptorSetLayoutCreateInfo) {
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = ARRAY_SIZE(c_set_2_bindings),
        .pBindings    = c_set_2_bindings
    };
    if(vkCreateDescriptorSetLayout(
        device,
        &layout_info,
        NULL,
        &set_layouts[2]
    ) != VK_SUCCESS) {
        LOG_ERROR("failed to create descriptor set layout 2");
        goto fail;
    }

    /* allocate sets */
    set_info = (VkDescriptorSetAllocateInfo) {
        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool     = pool,
        .descriptorSetCount = MAX_DESCRIPTOR_SET_COUNT,
        .pSetLayouts        = set_layouts
    };
    if(vkAllocateDescriptorSets(
        device,
        &set_info,
        sets
    ) != VK_SUCCESS) {
        LOG_ERROR("failed to allocate descriptor sets");
        goto fail;
    }

    return TRUE;

    fail: {
        if(set_layouts[0] != NULL) {
            vkDestroyDescriptorSetLayout(device, set_layouts[0], NULL);
        }
        if(set_layouts[1] != NULL) {
            vkDestroyDescriptorSetLayout(device, set_layouts[1], NULL);
        }
        if(set_layouts[2] != NULL) {
            vkDestroyDescriptorSetLayout(device, set_layouts[2], NULL);
        }

        set_layouts[0] = NULL;
        set_layouts[1] = NULL;
        set_layouts[2] = NULL;
        sets       [0] = NULL;
        sets       [1] = NULL;
        sets       [2] = NULL;

        return FALSE;
    }
}

b32 layoutRenderBindings(
    CtxHandle                     ctx, 
    const LayoutRenderBindingsIn* in
) {
    VkDescriptorSetLayoutBindingFlagsCreateInfo custom_bindings[MAX_BINDINGS_PER_DESCRIPTOR] = {0}; 
    VkDescriptorPoolCreateInfo                  descriptor_pool_info                         = (VkDescriptorPoolCreateInfo){0};
    VulkanCtx*                                  vk_ctx                                       = (VulkanCtx*)ctx;
    VulkanBindings*                             vk_bindings                                  = NULL;

    if(ctx == NULL) {
        LOG_ERROR("ivalid input params");
        goto fail;
    }
    if(vk_ctx->ctx_hash != VULKAN_CTX_HASH) {
        LOG_ERROR("vulkan ctx hash invalid");
        goto fail;
    }

    vk_bindings = &vk_ctx->bindings;
    
    /* destroy old bindings */
    if(vk_bindings->descriptor_pool != NULL) {
        vkFreeDescriptorSets(vk_ctx->device, vk_bindings->descriptor_pool, 3, vk_bindings->sets);
        vkDestroyDescriptorSetLayout(vk_ctx->device, vk_bindings->set_layouts[0], NULL);
        vkDestroyDescriptorSetLayout(vk_ctx->device, vk_bindings->set_layouts[1], NULL);
        vkDestroyDescriptorSetLayout(vk_ctx->device, vk_bindings->set_layouts[2], NULL);
        vkDestroyDescriptorPool(vk_ctx->device, vk_bindings->descriptor_pool, NULL);
    }
    *vk_bindings = (VulkanBindings){0};

    /* create descriptor pool */
    descriptor_pool_info = (VkDescriptorPoolCreateInfo) {
        .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets       = MAX_DESCRIPTOR_SET_COUNT,
        .poolSizeCount = ARRAY_SIZE(c_descriptor_pool_sizes),
        .pPoolSizes    = c_descriptor_pool_sizes
    };
    if(vkCreateDescriptorPool(
        vk_ctx->device,
        &descriptor_pool_info, 
        NULL,
        &vk_bindings->descriptor_pool
    ) != VK_SUCCESS) {
        LOG_ERROR("failed to create vulkan descriptor pool");
        goto fail;
    }

    /* create resources */

    /* create descriptors */


    return TRUE;

    fail: {
        if(vk_bindings) {
            if(vk_bindings->descriptor_pool != NULL) {
                vkDestroyDescriptorPool(vk_ctx->device, vk_bindings->descriptor_pool, NULL);
            }
            *vk_bindings = (VulkanBindings){0};
        }
        return FALSE;
    }
}
