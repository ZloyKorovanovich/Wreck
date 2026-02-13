#include "vulkan.h"

#ifdef _WIN32
    #define VK_USE_PLATFORM_WIN32_KHR
#endif

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

/*  ===============================================================
        VULKAN & SEGMENT
    =============================================================== */
/*  Segment layout representation:
    +----------------+---------------+-----------------+---------+---------+--------+----------+--------+
    | VULKAN OBJECTS | VULKAN DEVICE |                                                                  |
    +----------------+---------------+-----------------+---------+---------+--------+----------+--------+  */

/* on all platforms we cover window is a handle, see [WINDOW SYSTEMS] */
typedef void *Window;

typedef struct {
    VulkanInFlags flags;
    MsgCallback_pfn msg_callback;
    Window window;
    VkInstance instance;
    VkSurfaceKHR surface;
    VkDebugUtilsMessengerEXT debug_messenger;
    /* debug extension */
    PFN_vkCreateDebugUtilsMessengerEXT create_debug_messenger;
    PFN_vkDestroyDebugUtilsMessengerEXT destroy_debug_messenger;
} VulkanObjects;

typedef struct {
    VkPhysicalDevice physical_device;
    VulkanDeviceModel device_model;
    VulkanMemoryModel memory_model;

    VkDevice device;
    VkQueue render_queue;
    VkQueue transfer_queue;
    VkQueue compute_queue;

    u32 render_queue_id;
    u32 transfer_queue_id;
    u32 compute_queue_id;
} VulkanDevice;


typedef struct {
    void *segment_begin;
    void *vulkan_objects_begin;
    void *vulkan_objects_end_device_begin;
    void *vulkan_device_end;
    void *static_end_dynamic_begin;
    void *segment_end;
} VulkanSegment;


#define STATIC_PART_SIZE (ALIGN(sizeof(VulkanSegment) + sizeof(VulkanObjects) + sizeof(VulkanDevice), MEMORY_PAGE_SIZE))
#define MIN_VULKAN_SEGMENT_SIZE (ALIGN(STATIC_PART_SIZE, ALLOCATION_GRANULARITY))

/* Layouts virtual addresses of vulkan_objects segment,
    fills VulkanSegment struct with pointers to
    future data. Commits memory to the segment */
VulkanSegment * 
layoutSegment(
    const Segment *segment, 
    MsgCallback_pfn msg_callback
) {
    /* check if size of segment is appropreate */
    u64 segment_size = (u64)segment->end - (u64)segment->begin;
    if(segment_size < MIN_VULKAN_SEGMENT_SIZE) {
        MSG_ERROR(msg_callback, &TRACED_STR("too small vulkan segment size"));
        goto _validation_fail;
    }
    
    #ifdef _WIN32
        if(!VirtualAlloc(segment->begin, segment_size, MEM_COMMIT, PAGE_READWRITE)) {
            goto _mem_commit_fail;
        }
    #endif /* _WIN32 */

    /* initialize segment addresses, segment layout is stored in the segment itself */
    VulkanSegment *vulkan_segment = (VulkanSegment *)segment->begin;
    *vulkan_segment = (VulkanSegment) {
        .segment_begin                      = (byte *)segment->begin + sizeof(VulkanSegment),
        .vulkan_objects_begin               = (byte *)segment->begin + sizeof(VulkanSegment),
        .vulkan_objects_end_device_begin    = (byte *)segment->begin + sizeof(VulkanSegment) + sizeof(VulkanObjects),
        .vulkan_device_end                  = (byte *)segment->begin + sizeof(VulkanSegment) + sizeof(VulkanObjects) + sizeof(VulkanDevice),
        .static_end_dynamic_begin           = (byte *)segment->begin + ALIGN(sizeof(VulkanSegment) + sizeof(VulkanObjects) + sizeof(VulkanDevice), MEMORY_PAGE_SIZE),
        .segment_end                        = segment->end
    };

    /* success */
    return vulkan_segment;

    /* fails */
    _validation_fail: {
        return NULL;
    }
    _mem_commit_fail: {
        return NULL;
    }
}

/*  ===============================================================
        WINDOW SYSTEMS
    =============================================================== */
/* Window systems are OS specific. On windows we use winapi window and win32 surface. 
    In all cases window handle is represented through "Window" type, which is void *.
    Theoretically its possible to put struct with window data and asign handle as address
    of this struct, but our memory allocator doesnt account for that right now. I dont think its useful,
    we will adapt for linux later and see.                                                               */

#ifdef _WIN32
    /* window function that proceeds events */
    LRESULT 
    CALLBACK 
    windowProcedure(
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
    Window 
    createWindow(
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
        return (void *)window_handle;

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

    b32
    destroyWindow(
        Window window,
        MsgCallback_pfn msg_callback
    ) {
        /* check for null pointer to indicate if application has bugs in it */
        if(!window) {
            MSG_WARNING(msg_callback, &TRACED_STR("trying to destroy window that is a null pointer"));
            return FALSE;
        }
        DestroyWindow(window);
        return TRUE;
    }

#endif /* _WIN32 */

/*  ===============================================================
        VULKAN OBJECTS
    =============================================================== */
/* Vulkan objects is general name for any kind of object that application needs 
    to create before device creation. Vulkan Instance, Vulkan Surface, ect.
    Window is also part of vulkan_objects objects, as it is cloesly related to surface. 
    There is one optional vulkan_objects object, which is debug messegner, its only
    created when debug flag is set in creation function.                                */
/* At the moment of vulkan objects creation segment looks like that:
    +----------------+---------------------------+
    | VULKAN OBJECTS | TEMP |->                  |
    +----------------+---------------------------+
             vulkan objects end              segment end                

    We use space from [vulkan objects end] address to [segment end] address for temporary
    that are mostely created when using enumeration functions of vulkan, 
    for example "vkEnumerateInstanceExtensionProperties". That space is acting as an Arena  */

VKAPI_ATTR VkBool32 VKAPI_CALL 
validationDebugCallback(
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

/* Allocates extensions array on arena, initializes it
    and matches it to required extensions. Same is done 
    for kayers.                                         */
b32
matchExtensionsAndLayers(
    Arena *alloc_arena,
    u32 required_extension_count,
    u32 required_layer_count,
    const char **required_extensions,
    const char **required_layers,
    MsgCallback_pfn msg_callback
) {
    /* EXTENSIONS */ {
        if(required_extension_count == 0) {
            goto _extensions_end;
        }
        
        u32 extension_count = 0;
        VkExtensionProperties *extension_properties = NULL;

        vkEnumerateInstanceExtensionProperties(NULL, &extension_count, NULL);
        if(extension_count == 0) {
            MSG_ERROR(msg_callback, &TRACED_STR("zero extensions vulkan avaliable"));
            return FALSE;
        }
        
        /* try to allocate full extension array */
        u64 possible_size = 0;
        extension_properties = allocateArena(alloc_arena, extension_count * sizeof(VkExtensionProperties), 16, &possible_size);
        if(!extension_properties) {
            /* no way to allocate any of extensions */
            if(possible_size == 0) {
                MSG_ERROR(msg_callback, &TRACED_STR("no space for instance extensions array on arena"));
                return FALSE;
            } 
            /* can only allocate part of extensions array */
            MSG_WARNING(msg_callback, &TRACED_STR("not enough space for some of instance extensions, trying to scan less of them"));

            extension_count = possible_size / sizeof(VkExtensionProperties);
            extension_properties = allocateArena(alloc_arena, extension_count * sizeof(VkExtensionProperties), 16, &possible_size);
            if(!extension_properties) {
                MSG_ERROR(msg_callback, &TRACED_STR("no space for instance extensions array on arena"));
                return FALSE;
            }
        }
        /* initialize extension array */
        vkEnumerateInstanceExtensionProperties(NULL, &extension_count, extension_properties);

        u32 extensions_found = 0;
        for(u32 i = 0; i < extension_count; i++) {
            for(u32 j = 0; j < required_extension_count; j++) {
                if(cstringCmp(required_extensions[j], extension_properties[i].extensionName)) {
                    extensions_found++;
                    if(extensions_found == required_extension_count) {
                        goto _extensions_end;
                    }
                }
            }
        }
        /* if not found extensions */
        MSG_ERROR(msg_callback, &TRACED_STR("failed to match extensions"));
        return FALSE;

        _extensions_end: {};
    }

    freeArena(alloc_arena);

    /* LAYERS */ {
        if(required_layer_count == 0) {
            goto _layers_end;
        }

        u32 layer_count = 0;
        VkLayerProperties *layer_properties = NULL;
        
        vkEnumerateInstanceLayerProperties(&layer_count, NULL);
        if(layer_count == 0) {
            MSG_ERROR(msg_callback, &TRACED_STR("no layers available"));
            return FALSE;
        }

        u64 possible_size = 0;
        layer_properties = allocateArena(alloc_arena, layer_count * sizeof(VkLayerProperties), 16, &possible_size);
        if(!layer_properties) {
            /* no way to allocate any of layers */
            if(possible_size == 0) {
                MSG_ERROR(msg_callback, &TRACED_STR("no space for instance layers array on arena"));
                return FALSE;
            } 
            /* can only allocate part of layers array */
            MSG_WARNING(msg_callback, &TRACED_STR("not enough space for some of instance layers, trying to scan less of them"));

            layer_count = possible_size / sizeof(VkLayerProperties);
            layer_properties = allocateArena(alloc_arena, layer_count * sizeof(VkLayerProperties), 16, &possible_size);
            if(!layer_properties) {
                MSG_ERROR(msg_callback, &TRACED_STR("no space for instance layers array on arena"));
                return FALSE;
            }
        }
        /* initialize layers array */
        vkEnumerateInstanceLayerProperties(&layer_count, layer_properties);

        u32 layers_found = 0;
        for(u32 i = 0; i < layer_count; i++) {
            for(u32 j = 0; j < required_layer_count; j++) {
                if(cstringCmp(required_layers[j], layer_properties[i].layerName)) {
                    layers_found++;
                    if(layers_found == required_layer_count) {
                        goto _layers_end;
                    }
                }
            }
        }
        /* if not found layers */
        MSG_ERROR(msg_callback, &TRACED_STR("failed to match layers"));
        return FALSE;
        
        _layers_end: {};
    }

    return TRUE;
}

/* Creates vulkan_objects objects: window, instance, 
    debug messenger (optional) and surface.             */
b32 
createVulkanObjects(
    u32 flags,
    u32 window_x,
    u32 window_y,
    const char *name,
    VulkanObjects *vulkan_objects,
    Arena *alloc_arena,
    MsgCallback_pfn msg_callback
) {
    *vulkan_objects = (VulkanObjects){.flags = flags, .msg_callback = msg_callback};
    b32 is_debug = flags & VULKAN_IN_FLAG_DEBUG;
    
    /* create vulkan_objects */
    vulkan_objects->window = createWindow(name, window_x, window_y, flags, msg_callback);
    if(!vulkan_objects->window) {
        MSG_ERROR(msg_callback, &TRACED_STR("failed to create window"));
        goto _critical_fail;
    }

    /* INSTANCE + DEBUG MESSENGER (OPTIONAL) */ {
        /* extensions for surface are platform dependent */
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
        
        /* check if required extensions and layers are available */
        if(!matchExtensionsAndLayers(
            (Arena[1]){(Arena) {
                alloc_arena->edge,
                alloc_arena->edge, 
                alloc_arena->end
            }},
            required_extension_count,
            required_layer_count,
            required_extensions,
            required_layers,
            msg_callback
        )) {
            MSG_ERROR(msg_callback, &TRACED_STR("failed to match vulkan insatnce layers or extensions"));
            goto _critical_fail;
        }

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
        if(vkCreateInstance(&instance_info, NULL, &vulkan_objects->instance) != VK_SUCCESS) {
            MSG_ERROR(msg_callback, &TRACED_STR("failed to create vulkan instance"));
            goto _critical_fail;
        }

        /* load debug extension functions and create debug messenger */
        if(is_debug) {
            /* load extensions */
            vulkan_objects->create_debug_messenger = (void *)vkGetInstanceProcAddr(vulkan_objects->instance, "vkCreateDebugUtilsMessengerEXT");
            vulkan_objects->destroy_debug_messenger = (void *)vkGetInstanceProcAddr(vulkan_objects->instance, "vkDestroyDebugUtilsMessengerEXT");
            if(!vulkan_objects->create_debug_messenger || !vulkan_objects->destroy_debug_messenger) {
                MSG_ERROR(msg_callback, &TRACED_STR("failed to load vulkan debug messenger procedures"));
                goto _critical_fail;
            }

            /* create debug messenger */
            if(vulkan_objects->create_debug_messenger(vulkan_objects->instance, &debug_messenger_info, NULL, &vulkan_objects->debug_messenger) != VK_SUCCESS) {
                MSG_ERROR(msg_callback, &TRACED_STR("failed to create vulkan debug messenger"));
                goto _critical_fail;
            }
        }
    }

    /* SURFACE */ {
        #ifdef _WIN32
            VkWin32SurfaceCreateInfoKHR win32_surface_info = {
                .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
                .hwnd = (HWND)vulkan_objects->window,
                .hinstance = GetModuleHandle(NULL)
            };
            if(vkCreateWin32SurfaceKHR(vulkan_objects->instance, &win32_surface_info, NULL, &vulkan_objects->surface) != VK_SUCCESS) {
                MSG_ERROR(msg_callback, &TRACED_STR("failed to create win32 surface"));
                goto _critical_fail;
            }
        #endif
    }

    return TRUE;

    /* fails */
    _critical_fail: {
        return FALSE;
    }
}

b32
destroyVulkanObjects(
    VulkanObjects *vulkan,
    MsgCallback_pfn msg_callback
) {
    const b32 is_debug = vulkan->flags & VULKAN_IN_FLAG_DEBUG;
    b32 result = TRUE;

    /* destroy surface */
    if(vulkan->surface) {
        vkDestroySurfaceKHR(vulkan->instance, vulkan->surface, NULL);
    } else {
        MSG_WARNING(msg_callback, &TRACED_STR("can not destroy vulkan surface, pointer is null"));
    }

    /* if debug flag is set */
    if(is_debug) {
        /* destroy debug messenger */
        if(vulkan->debug_messenger) {
            vulkan->destroy_debug_messenger(vulkan->instance, vulkan->debug_messenger, NULL);
        } else {
            MSG_WARNING(msg_callback, &TRACED_STR("can not destroy vulkan debug messenger, pointer is null"));
            result = FALSE;
        }
    }

    /* detsroy instance */
    if(vulkan->instance) {
        vkDestroyInstance(vulkan->instance, NULL);
    } else {
        MSG_WARNING(msg_callback, &TRACED_STR("can not destroy vulkan instance, pointer is null"));
        result = FALSE;
    }

    /* window */
    if(!destroyWindow(vulkan->window, vulkan->msg_callback)) {
        MSG_WARNING(msg_callback, &TRACED_STR("failed to destroy vulkan window"));
        result = FALSE;
    }

    *vulkan = (VulkanObjects){0};

    return result;
}


/*  ===============================================================
        DEVICES
    =============================================================== */
/* At the moment of vulkan device creation segment looks like that:
    +----------------+---------------+------+--------+
    | VULKAN OBJECTS | VULKAN DEVICE | TEMP |->      |
    +----------------+---------------+------+--------+
                                                 segment end            */
typedef struct {
    /* device info */
    VkPhysicalDevice device;
    char name[VK_MAX_PHYSICAL_DEVICE_NAME_SIZE];
    u32 device_id;
    VulkanDeviceModel device_model;
    /* memory model*/
    VulkanMemoryModel memory_model;
    /* queue layout */
    u32 render_queue_id;
    u32 transfer_queue_id;
    u32 compute_queue_id;
} DeviceData;

b32 
getDeviceData(
    VkPhysicalDevice physical_device,
    u32 required_extension_count,
    const char **required_extensions,
    DeviceData *device_data,
    Arena *alloc_arena,
    MsgCallback_pfn msg_callback
) {
    *device_data = (DeviceData){.device = physical_device};

    /* DEVICE PROPERTIES */ {
        VkPhysicalDeviceProperties device_properties = (VkPhysicalDeviceProperties){0};
        vkGetPhysicalDeviceProperties(physical_device, &device_properties);
        cstringCpy(device_data->name, device_properties.deviceName);
        device_data->device_id = device_properties.deviceID;
        if(device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            device_data->device_model = VULKAN_DEVICE_MODEL_DESCRETE;
        }
        else if(device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
            device_data->device_model = VULKAN_DEVICE_MODEL_INTEGRATED;
        }
        else {
            return FALSE;
        }
    }

    /* MEMORY LAYOUT */ {
        /* take this spec as reference, for what you can do with memory: https://docs.vulkan.org/spec/latest/chapters/memory.html
            "   There must be at least one memory type with both the VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT and VK_MEMORY_PROPERTY_HOST_COHERENT_BIT bits set in its propertyFlags. 
                There must be at least one memory type with the VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT bit set in its propertyFlags. 
                If the deviceCoherentMemory feature is enabled, there must be at least one memory type with the VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD bit set in its propertyFlags.    "
        */

        VkPhysicalDeviceMemoryProperties memory_properties = (VkPhysicalDeviceMemoryProperties){0};
        vkGetPhysicalDeviceMemoryProperties(physical_device, &memory_properties);

        const u32 memory_type_count = memory_properties.memoryTypeCount;
        const VkMemoryType *memory_types = memory_properties.memoryTypes;
        const VkMemoryHeap *memory_heaps = memory_properties.memoryHeaps;

        b32 has_visible_device_local = FALSE;
        b32 has_invisble_device_local = FALSE;
        b32 can_use_host_staging = FALSE;
        b32 can_use_host_as_device = FALSE;

        for(u32 i = 0; i < memory_type_count; i++) {
            /* if has_visible_device_local */
            if(
                memory_types[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT &&
                memory_types[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT &&
                memory_heaps[memory_types[i].heapIndex].size > REQUIRED_DEVICE_VRAM_SIZE
            ) {
                has_visible_device_local = TRUE;
            }

            /* if has_invisble_device_local */
            if(
                memory_types[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT    &&
                !(memory_types[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) &&
                memory_heaps[memory_types[i].heapIndex].size > REQUIRED_DEVICE_VRAM_SIZE
            ) {
                has_invisble_device_local = TRUE;
            }

            /* if can_use_host_staging */
            if(
                !(memory_types[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) &&
                memory_types[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT   &&
                memory_heaps[memory_types[i].heapIndex].size > REQUIRED_HOST_VRAM_SIZE
            ) {
                can_use_host_staging = TRUE;
            }

            /* if can_use_host_as_device */
            if(
                !(memory_types[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) &&
                memory_types[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT   &&
                memory_heaps[memory_types[i].heapIndex].size > REQUIRED_DEVICE_VRAM_SIZE
            ) {
                can_use_host_as_device = TRUE;
            }
        }

        if(device_data->device_model == VULKAN_DEVICE_MODEL_DESCRETE) {
            if(has_invisble_device_local && can_use_host_staging) {
                device_data->memory_model = VULKAN_MEMORY_MODEL_HOST_DEVICE;
            } 
            else if(has_visible_device_local) {
                device_data->memory_model = VULKAN_MEMORY_MODEL_FUSED_DEVICE;
            }
            else if(can_use_host_as_device) {
                device_data->memory_model = VULKAN_MEMORY_MODEL_FUSED_HOST;
            } else {
                return FALSE;
            }
        } 
        /* the order for integrated is different */
        if(device_data->device_model == VULKAN_DEVICE_MODEL_INTEGRATED) {
            if(has_visible_device_local) {
                device_data->memory_model = VULKAN_MEMORY_MODEL_FUSED_DEVICE;
            }
            else if(can_use_host_as_device) {
                device_data->memory_model = VULKAN_MEMORY_MODEL_FUSED_HOST;
            } 
            else if(has_invisble_device_local && can_use_host_staging) {
                device_data->memory_model = VULKAN_MEMORY_MODEL_HOST_DEVICE;
            } 
            else {
                return FALSE;
            }
        }
    }

    /* QUEUE LAYOUT */ {
        #define QUEUE_FAMILY_MASK (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT | VK_QUEUE_COMPUTE_BIT)
        #define QUEUE_FAMILY_RENDER (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT | VK_QUEUE_COMPUTE_BIT)
        #define QUEUE_FAMILY_COMPUTE (VK_QUEUE_TRANSFER_BIT | VK_QUEUE_COMPUTE_BIT)
        #define QUEUE_FAMILY_TRANSFER (VK_QUEUE_TRANSFER_BIT)

        u32 queue_family_count = 0;
        VkQueueFamilyProperties *queue_families = NULL;
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, NULL);

        if(queue_family_count == 0) {
            MSG_WARNING(msg_callback, &TRACED_STR("queue family count of device is zero"));
            return FALSE;
        }

        u64 max_possible_allocation = 0;
        queue_families = allocateArena(alloc_arena, queue_family_count * sizeof(VkQueueFamilyProperties), 16, &max_possible_allocation);
        
        if(!queue_families) {
            if(max_possible_allocation == 0) {
                MSG_ERROR(msg_callback, &TRACED_STR("not enough space on arena for queue families allocation"));
                return FALSE;
            }

            MSG_WARNING(msg_callback, &TRACED_STR("not enough space for all of queue families, partial allocation is made"));

            queue_family_count = max_possible_allocation / sizeof(VkQueueFamilyProperties);
            queue_families = allocateArena(alloc_arena, queue_family_count * sizeof(VkQueueFamilyProperties), 16, &max_possible_allocation);
            if(!queue_families) {
                MSG_ERROR(msg_callback, &TRACED_STR("not enough space on arena for queue families allocation"));
                return FALSE;
            }
        }

        device_data->render_queue_id = U32_MAX;
        device_data->transfer_queue_id = U32_MAX;
        device_data->compute_queue_id = U32_MAX;

        vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, queue_families);
        for(u32 i = 0; i < queue_family_count; i++) {
            if(device_data->render_queue_id == U32_MAX && (queue_families[i].queueFlags & QUEUE_FAMILY_MASK) == QUEUE_FAMILY_RENDER) {
                device_data->render_queue_id = i;
            }
            if(device_data->transfer_queue_id == U32_MAX && (queue_families[i].queueFlags & QUEUE_FAMILY_MASK) == QUEUE_FAMILY_TRANSFER) {
                device_data->transfer_queue_id = i;
            }
            if(device_data->compute_queue_id == U32_MAX && (queue_families[i].queueFlags & QUEUE_FAMILY_MASK) == QUEUE_FAMILY_COMPUTE) {
                device_data->compute_queue_id = i;
            }
        }

        if(device_data->render_queue_id == U32_MAX) {
            return FALSE;
        }
    }

    freeArena(alloc_arena);

    /* EXTENSIONS */ {
        if(required_extension_count == 0) {
            goto _extensions_end;
        }

        u32 extension_count = 0;
        VkExtensionProperties *extension_properties = NULL;
        vkEnumerateDeviceExtensionProperties(physical_device, NULL, &extension_count, NULL);
        if(extension_count == 0) {
            MSG_WARNING(msg_callback, &TRACED_STR("physical device extension count is zero"));
            return FALSE;
        }
        
        /* try to allocate full extension array */
        u64 possible_size = 0;
        extension_properties = allocateArena(alloc_arena, extension_count * sizeof(VkExtensionProperties), 16, &possible_size);
        if(!extension_properties) {
            /* no way to allocate any of extensions */
            if(possible_size == 0) {
                MSG_ERROR(msg_callback, &TRACED_STR("no space for instance extensions array on arena"));
                return FALSE;
            } 
            /* can only allocate part of extensions array */
            MSG_WARNING(msg_callback, &TRACED_STR("not enough space for some of instance extensions, trying to scan less of them"));

            extension_count = possible_size / sizeof(VkExtensionProperties);
            extension_properties = allocateArena(alloc_arena, extension_count * sizeof(VkExtensionProperties), 16, &possible_size);
            if(!extension_properties) {
                MSG_ERROR(msg_callback, &TRACED_STR("no space for instance extensions array on arena"));
                return FALSE;
            }
        }
        /* initialize extension array */
        vkEnumerateDeviceExtensionProperties(physical_device, NULL, &extension_count, extension_properties);

        u32 extensions_found = 0;
        for(u32 i = 0; i < extension_count; i++) {
            for(u32 j = 0; j < required_extension_count; j++) {
                if(cstringCmp(required_extensions[j], extension_properties[i].extensionName)) {
                    extensions_found++;
                    if(extensions_found == required_extension_count) {
                        goto _extensions_end;
                    }
                }
            }
        }
        /* if not found extensions */
        return FALSE;

        _extensions_end: {};
    }

    return TRUE;
}

b32 
compareDevices(
    const DeviceData *device_a,
    const DeviceData *device_b
) {
    return FALSE;
}

b32 
createVulkanDevice(
    const VulkanObjects *vulkan,
    VulkanDevice *device,
    Arena *alloc_arena,
    MsgCallback_pfn msg_callback
) {
    *device = (VulkanDevice){0};

    u32 device_count = 0;
    DeviceData devices[MAX_PHYSICAL_DEVICE_COUNT] = {0};
    /* SCAN DEVICES */ {
        u32 physical_device_count = 0;
        VkPhysicalDevice physical_devices[MAX_PHYSICAL_DEVICE_COUNT] = {0};

        vkEnumeratePhysicalDevices(vulkan->instance, &physical_device_count, NULL);
        if(physical_device_count == 0) {
            MSG_ERROR(msg_callback, &TRACED_STR("no physical devices found"));
            goto _critical_fail;
        }
        if(physical_device_count > MAX_PHYSICAL_DEVICE_COUNT) {
            MSG_WARNING(msg_callback, &TRACED_STR("too many physical devices found, the rest of list is chopped off"));
            physical_device_count = MAX_PHYSICAL_DEVICE_COUNT;
        }
        vkEnumeratePhysicalDevices(vulkan->instance, &physical_device_count, physical_devices);

        /* required extensions */
        const u32 required_extension_count = 2;
        const char *required_extensions[] = {
            VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
            VK_KHR_SWAPCHAIN_EXTENSION_NAME
        };

        for(u32 i = 0; i < physical_device_count; i++) {
            if(getDeviceData(
                physical_devices[i], 
                required_extension_count, 
                required_extensions, 
                &devices[device_count], 
                alloc_arena, 
                msg_callback
            )) {
                device_count++;
            }
        }

        if(device_count == 0) {
            MSG_ERROR(msg_callback, &TRACED_STR("no suitable device count"));
            goto _critical_fail;
        }

        for(u32 i = 0; i < device_count; i++) {
            for(u32 j = 0; j < i; j++) {
                if(compareDevices(&devices[j], &devices[i])) {
                    DeviceData temp = devices[j];
                    devices[j] = devices[i];
                    devices[i] = temp;
                }
            }
        }
    }

    /* CREATE VULKAN DEVICE */ {
        const u32 required_extension_count = 2;
        const char *required_extensions[] = {
            VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
            VK_KHR_SWAPCHAIN_EXTENSION_NAME
        };

        VkDeviceQueueCreateInfo queue_infos[3] = {0};
        f32 queue_priority = 1.0f;
        /* attempt to create devices that are worse if creation fails */
        for(u32 i = 0; i < device_count; i++) {
            u32 queue_count = 0;
            queue_infos[queue_count++] = (VkDeviceQueueCreateInfo) {
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .queueFamilyIndex = devices[i].render_queue_id,
                .queueCount = 1,
                .pQueuePriorities = &queue_priority
            };
            if(devices[i].transfer_queue_id != U32_MAX) {
                queue_infos[queue_count++] = (VkDeviceQueueCreateInfo) {
                    .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                    .queueFamilyIndex = devices[i].transfer_queue_id,
                    .queueCount = 1,
                    .pQueuePriorities = &queue_priority
                };
            }
            if(devices[i].compute_queue_id != U32_MAX) {
                queue_infos[queue_count++] = (VkDeviceQueueCreateInfo) {
                    .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                    .queueFamilyIndex = devices[i].compute_queue_id,
                    .queueCount = 1,
                    .pQueuePriorities = &queue_priority
                };
            }

            VkDeviceCreateInfo device_info = {
                .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                .enabledExtensionCount = required_extension_count,
                .ppEnabledExtensionNames = required_extensions,
                .queueCreateInfoCount = queue_count,
                .pQueueCreateInfos = queue_infos
            };

            VkDevice created_device = NULL;
            if(vkCreateDevice(devices[i].device, &device_info, NULL, &created_device) == VK_SUCCESS) {
                *device = (VulkanDevice) {
                    .physical_device = devices[i].device,
                    .device = created_device,
                    .device_model = devices[i].device_model,
                    .memory_model = devices[i].memory_model,
                    .render_queue_id = devices[i].render_queue_id,
                    .transfer_queue_id = devices[i].transfer_queue_id,
                    .compute_queue_id = devices[i].compute_queue_id
                };

                vkGetDeviceQueue(created_device, devices[i].render_queue_id, 0, &device->render_queue);
                if(devices[i].transfer_queue_id != U32_MAX) {
                    vkGetDeviceQueue(created_device, devices[i].transfer_queue_id, 0, &device->transfer_queue);
                }
                if(devices[i].compute_queue_id != U32_MAX) {
                    vkGetDeviceQueue(created_device, devices[i].compute_queue_id, 0, &device->compute_queue);
                }

                goto _success;
            }
        }

        MSG_ERROR(msg_callback, &TRACED_STR("devices creation failed"));
        goto _critical_fail;
    }

    /* successful device creation */
    _success: {
        return TRUE;
    }
    /* something failed */
    _critical_fail: {
        return FALSE;
    }
}

b32
destroyVulkanDevice(
    const VulkanObjects *vulkan,
    VulkanDevice *device,
    MsgCallback_pfn msg_callback
) {
    b32 result = TRUE;

    if(device->device) {
        vkDestroyDevice(device->device, NULL);
    } else {
        MSG_WARNING(msg_callback, &TRACED_STR("can not destroy device, pointer is null"));
        result = FALSE;
    }

    *device = (VulkanDevice){0};

    return result;
}

/*  ===============================================================
        EXTERN INTERFACE
    =============================================================== */

/* Creates vulkan on given segment, layouts memory, 
    creates vulkan objects, and making  */
VulkanHandle
createVulkan(
    const CreateVulkanIn *input, 
    CreateVulkanOut *output
) {
    /* VALIDATE */ {
        if(!input) {
            goto _critical_fail;
        }
        if(!input->name) {
            MSG_ERROR(input->msg_callback, &TRACED_STR("invalid vulkan in name"));
            goto _critical_fail;
        }
        if(!input->segment) {
            MSG_ERROR(input->msg_callback, &TRACED_STR("invalid vulkan in segment"));
            goto _critical_fail;
        }
    }

    MsgCallback_pfn msg_callback = input->msg_callback;

    VulkanSegment *segment = layoutSegment(
        input->segment, 
        input->msg_callback
    );
    if(!segment) {
        MSG_ERROR(input->msg_callback, &TRACED_STR("failed to layout vulkan segment"));
        goto _critical_fail;
    }

    /* structs in segment */
    VulkanObjects *vulkan_objects = segment->vulkan_objects_begin;
    VulkanDevice *vulkan_device = segment->vulkan_objects_end_device_begin;

    /* create vulkan objects */
    if(!createVulkanObjects(
        input->flags, 
        input->x, 
        input->y, 
        input->name, 
        vulkan_objects,
        (Arena[1]){(Arena) {
            segment->vulkan_objects_end_device_begin,
            segment->vulkan_objects_end_device_begin,
            segment->segment_end
        }},
        msg_callback
    )) {
        MSG_ERROR(msg_callback, &TRACED_STR("failed to create vulkan objects"));
        goto _critical_fail;
    }

    if(!createVulkanDevice(
        vulkan_objects,
        vulkan_device,
        (Arena[1]){(Arena) {
            segment->vulkan_device_end,
            segment->vulkan_device_end,
            segment->segment_end
        }},
        msg_callback
    )) {
        MSG_ERROR(msg_callback, &TRACED_STR("failed to create vulkan device"));
        goto _critical_fail;
    }

    /* Protect write to static part of segment */ 
    if(input->flags & VULKAN_IN_PROTECT_MEMORY) {
        #ifdef _WIN32
            u32x old_protection = 0; /* why this exists? winapi! */
            if(!VirtualProtect(segment->segment_begin, (u64)segment->static_end_dynamic_begin - (u64)segment->segment_begin, PAGE_READONLY, &old_protection)) {
                MSG_ERROR(msg_callback, &TRACED_STR("failed to protect vulkan segment static part"));
                goto _critical_fail;
            }
        #endif /* _WIN32 */
    }

    return (VulkanHandle)segment;

    /* fails */
    _critical_fail: {
        return NULL;
    }
}

b32 
destroyVulkan(
    VulkanHandle vulkan
) {
    VulkanSegment *vulkan_segment = (VulkanSegment *)vulkan;
    VulkanObjects *vulkan_objects = (VulkanObjects *)vulkan_segment->vulkan_objects_begin;
    VulkanDevice *vulkan_device = (VulkanDevice *)vulkan_segment->vulkan_objects_end_device_begin;

    const MsgCallback_pfn msg_callback = vulkan_objects->msg_callback;
    b32 result = TRUE;

    /* remove protection from segment */
    if(vulkan_objects->flags & VULKAN_IN_PROTECT_MEMORY) {
        #ifdef _WIN32
            u32x old_protection = 0; /* why this exists? winapi! */
            if(!VirtualProtect(vulkan_segment->segment_begin, (u64)vulkan_segment->vulkan_device_end - (u64)vulkan_segment->segment_begin, PAGE_READWRITE, &old_protection)) {
                MSG_WARNING(msg_callback, &TRACED_STR("failed to turn off protection of vulkan segment static part"));   
            }
        #endif /* _WIN32 */
    }

    if(!destroyVulkanDevice(vulkan_objects, vulkan_device, msg_callback)) {
        MSG_WARNING(msg_callback, &TRACED_STR("failed to destroy vulkan device"));
        result = FALSE;
    }

    /* vulkan objetcs */
    if(!destroyVulkanObjects(vulkan_objects, msg_callback)) {
        MSG_WARNING(msg_callback, &TRACED_STR("failed to destroy vulkan objects"));
        result = FALSE;
    }
    
    return result;
}

b32
runVulkanLoop(
    VulkanHandle vulkan
) {
    const VulkanSegment *vulkan_segment = (VulkanSegment *)vulkan;

#ifdef _WIN32
    MSG win32_message = (MSG){0};
#endif /*_WIN32 */

    b32 is_running_window = TRUE;
    while(is_running_window) {

        #ifdef _WIN32
            /* 2nd param is NULL, it indicates that we peek 
                message from current Threads windows       */
            while(PeekMessage(
                &win32_message, 
                NULL, 
                0, 
                0, 
                PM_REMOVE
            )) {
                is_running_window = (win32_message.message == WM_QUIT) ? FALSE : TRUE;
                TranslateMessage(&win32_message);
                DispatchMessage(&win32_message);
            }
        #endif /* _WIN32 */
    }

    return TRUE;
}
