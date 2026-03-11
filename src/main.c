#include <base.h>
#include "gpu/gpu.h"
#include "files/files.h"

void 
msgCallback(
    i32 code, 
    const String *msg
) {
    String print_str = (String) {
        .string = (char[512]){0},
        .capacity = 512
    };

    if(!msg) {
        printConsole(&CONST_STRING("got bad message"));
        return;
    }

    /* log message */
    if(code == 0) {
        stringPattern(&CONST_STRING(":: %s\n"), (const void *[]){msg}, &print_str);
        printConsole(&print_str);
    }
    /* warning message */
    if(code > 0) {
        stringPattern(&CONST_STRING(":? %s\n"), (const void *[]){msg}, &print_str);
        printConsole(&print_str);
    }
    /* error message */
    if(code < 0) {
        stringPattern(&CONST_STRING(":! %s\n"), (const void *[]){msg}, &print_str);
        errorConsole(&print_str);
    }
}

#define STRUCTS_BUFFER_SIZE (4088)
static struct {
    u64 edge;
    byte buffer[STRUCTS_BUFFER_SIZE];
} s_structs_buffer;

void *allocateStruct(
    u64 size,
    u64 alignment
) {
    u64 alloc_begin = ALIGN(s_structs_buffer.edge, alignment);
    u64 alloc_end = alloc_begin + size;
    /* not enough space */
    if(alloc_end > STRUCTS_BUFFER_SIZE) {
        MSG_WARNING(msgCallback, &TRACED_STR("structs buffer filled up"));
        return NULL;
    }
    /* all good, shift edge and return begin address */
    s_structs_buffer.edge = alloc_end;
    return (void *)(s_structs_buffer.buffer + alloc_begin);
}

/* empty proc, because arena */
void freeStruct(
    void *allocation
) {
    if((u64)allocation > (u64)(s_structs_buffer.buffer + s_structs_buffer.edge)) {
        MSG_WARNING(msgCallback, &TRACED_STR("trying to free structs buffer allocation, that is out of bounds already"));
    }
    s_structs_buffer.edge = ((u64)allocation - (u64)s_structs_buffer.buffer);
    return;
}

i32 
main(
    i32 argc, 
    char **argv
) {
    LoadInitFilesIn init_files_in = {
        .dir_path = "./out/data",
        .flags = RESOURCE_FILE_SHADERS
    };
    LoadInitFilesOut init_files_out = (LoadInitFilesOut){0};
    if(!loadInitFiles(
        &init_files_in, 
        &init_files_out, 
        msgCallback
    )) {
        MSG_ERROR(msgCallback, &TRACED_STR("failed to load init files"));
        return -1;
    }

    /* gpu mount */
    MountGPUIn mount_gpu_in = {
        .flags = GPU_FLAG_DEBUG,
        .window_name = "Scuby",
        .window_x = 800,
        .window_y = 600,
        .msg_callback = msgCallback
    };
    MountGPUOut mount_gpu_out = (MountGPUOut){0};
    CreateGPUStaticResourcesIn create_static_resources_in = {
        .program_count = 2,
        .programs = (GPUProgramInfo[]) {
            GPU_GRAPHICS_PROGRAM(SHADER_TRIANGLE, 0, init_files_out.shaders_address),
            GPU_GRAPHICS_PROGRAM(SHADER_TRIANGLE, 0, init_files_out.shaders_address)
        },
        .uniform_buffer = (GPUBufferInfo[1]) {
            (GPUBufferInfo) {.size = 64}
        },
        .mutable_storage_buffer_count = 1,
        .storage_buffer_count = 2,
        .storage_buffers = (GPUBufferInfo[]) {
            (GPUBufferInfo) {.size = 1024},
            (GPUBufferInfo) {.size = 1024 * 64}
        }
    };
    CreateGPUStaticResourcesOut create_static_resources_out = (CreateGPUStaticResourcesOut){0};

    GPU *gpu = mountGPU(
        &mount_gpu_in, 
        &mount_gpu_out,
        allocateStruct,
        freeStruct
    );
    if(!gpu) {
        MSG_ERROR(msgCallback, &TRACED_STR("failed to mount GPU"));
    }
    if(!createGPUStaticResources(
        gpu,
        &create_static_resources_in,
        &create_static_resources_out
    )) {
        MSG_ERROR(msgCallback, &TRACED_STR("failed to create GPU static resources"));
    }

    closeInitFiles();

    destroyGPUStaticResources(gpu);
    dismountGPU(gpu);
    return 0;
}
