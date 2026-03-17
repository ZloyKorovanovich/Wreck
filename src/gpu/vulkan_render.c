#include "gpu.h"

#define MAX_GPU_TABLE_RESOURCE_COUNT (64)

typedef enum {
    GPU_ACCESS_NONE = 0x0,
    GPU_ACCESS_READ = 0x1,
    GPU_ACCESS_WRITE = 0x2
} GPUTableAccess;

typedef enum {
    GPU_RESOURCE_TYPE_NONE = 0,
    GPU_RESOURCE_TYPE_BUFFER = 1,
    GPU_RESOURCE_TYPE_COLOR_IMAGE = 2,
    GPU_RESOURCE_TYPE_DEPTH_IMAGE = 3
} GPUResourceType;

typedef enum {
    GPU_OPERATION_NONE = 0,
    GPU_OPERATION_DRAW = 1,
    GPU_OPERATION_COMPUTE = 2,
    GPU_OPERATION_TRANSFER = 3
} GPUOperationType;

typedef struct {
    GPUResourceType type;
    u32 resource_id;
} GPUTableResource;

typedef struct {
    GPUOperationType type;
    union {
        struct {
            u32 program_id;
            u32 vertex_count;
            u32 instance_count;
            u32 mesh_id;
        } graphics;
        struct {
            u32 program_id;
            u32 groups_x;
            u32 groups_y;
            u32 groups_z;
        } compute;
    };   
} GPUTableOperation;

typedef struct {
    u32 resource_count;
    u32 operation_count;
    const GPUTableResource *resources;
    const GPUTableOperation *operations;
    const GPUTableAccess *access_table;
} GPURenderTable;

/* meshes and textures from host, that are dynamic rsources are not considered as data access, they can only be read */
b32 recordCommandBuffer(
    GPU *                  gpu, 
    const GPURenderTable * gpu_table,
    MsgCallback_pfn        msg_callback
) {
    const u32                 resource_count    = gpu_table->resource_count;
    const u32                 operation_count   = gpu_table->operation_count;
    const GPUTableResource *  resources         = gpu_table->resources;
    const GPUTableOperation * operations        = gpu_table->operations;
    const GPUTableAccess *    table             = gpu_table->access_table; 

    /* VALIDATION */ {
        if(gpu_table->resource_count > MAX_GPU_TABLE_RESOURCE_COUNT) {
            MSG_ERROR(msg_callback, &TRACED_STR("too many gpu table resources"));
            return FALSE;
        }

        for(u32 i = 0; i != operation_count; i++) {
            if(!(operations[i].type & (GPU_OPERATION_DRAW | GPU_OPERATION_COMPUTE | GPU_OPERATION_TRANSFER))) {
                MSG_ERROR(msg_callback, &TRACED_STR("gpu table operation type is invalid"));
                return FALSE;
            }
        }
        for(u32 j = 0; j != resource_count; j++) {
            if(!(resources[j].type & (GPU_RESOURCE_TYPE_BUFFER | GPU_RESOURCE_TYPE_COLOR_IMAGE | GPU_RESOURCE_TYPE_DEPTH_IMAGE))) {
                MSG_ERROR(msg_callback, &TRACED_STR("gpu table resource type is invalid"));
                return FALSE;
            }
        }
    }

    GPUTableAccess row[MAX_GPU_TABLE_RESOURCE_COUNT] = {0};
    b32 is_render_pass = FALSE;

    for(u32 op_id = 0; op_id != operation_count; op_id++) {
        if(operations[op_id].type == GPU_OPERATION_DRAW) {
            
        }
    }
}
