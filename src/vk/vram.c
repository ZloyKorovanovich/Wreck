#include "vk.h"

#define VRAM_MAX_ALLOCATIONS 512

static VkPhysicalDeviceMemoryProperties s_memory_properties = (VkPhysicalDeviceMemoryProperties){0};
static VkDeviceMemory s_allocations[VRAM_MAX_ALLOCATIONS] = {0};
static u32 s_allocation_count = 0;
static VkDevice s_device = NULL;

i32 vramArenaInit(const VulkanContext* vulkan_context) {
    vkGetPhysicalDeviceMemoryProperties(vulkan_context->physical_device, &s_memory_properties);
    s_device = vulkan_context->device;
    return MSG_CODE_SUCCESS;
}

void vramArenaTemrinate(void) {
    for(u32 i = 0; i < s_allocation_count; i++) {
        vkFreeMemory(s_device, s_allocations[i], NULL);
        s_allocations[i] = 0;
    }
    s_allocation_count = 0;
    s_memory_properties = (VkPhysicalDeviceMemoryProperties){0};
    s_device = NULL;
}

VkDeviceMemory vramArenaAllocate(const VkMemoryRequirements* requirements, u32 positive_flags, u32 negative_flags) {
    if(s_allocation_count > VRAM_MAX_ALLOCATIONS) return NULL;
    
    u32 memory_id = U32_MAX;
    for(u32 i = 0; i < s_memory_properties.memoryTypeCount; i++) {
        VkMemoryType memory_type = s_memory_properties.memoryTypes[i];
        if(
            ((memory_type.propertyFlags & positive_flags) == positive_flags) && 
            !(memory_type.propertyFlags & negative_flags) &&
            (memory_type.propertyFlags & requirements->memoryTypeBits) &&
            s_memory_properties.memoryHeaps[memory_type.heapIndex].size > requirements->size
        ) {
            memory_id = i;
            s_memory_properties.memoryHeaps[memory_type.heapIndex].size -= requirements->size;
            goto _found_memory_id; 
        }
    }
    //_not_found_memory_id:
    return NULL;

    _found_memory_id:
    /* allocation call */
    VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .memoryTypeIndex = memory_id,
        .allocationSize = requirements->size
    };
    if(vkAllocateMemory(s_device, &alloc_info, NULL, &s_allocations[s_allocation_count]) != VK_SUCCESS) {
        return NULL;
    }
    return s_allocations[s_allocation_count++];
}
