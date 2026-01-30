#include "main.h"

void msgCallback(i32 code, const String *msg) {
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

static Arena s_context_arena = (Arena){0};
void *allocateContext(u64 size, u64 alignment) {
    return allocateArena(&s_context_arena, size, alignment);
}

typedef struct {
    f32 time[4];
    f32 screen_params[4];
} UniformBuffer;

typedef struct {
    f32 position[4];
    f32 rotation[4];
} UniformScaleObject;


typedef enum {
    SHADER_PROGRAM_TRIANGLE = 0,
    SHADER_PROGRAM_DEFAULT = 1,
    SHADER_PROGRAM_BODY = 2,
    SHADER_PROGRAM_EYE = 3,
    SHADER_PROGRAM_COUNT
} ShaderPorgrams;

typedef enum {
    MESH_BODY = 0,
    MESH_EYE = 1,
    MESH_COUNT
} Meshes;

typedef enum {
    STORAGE_BUFFER_UNIFORM_SCALE = 0,
    STORAGE_BUFFER_MUTABLE_COUNT,
    STORAGE_BUFFER_CUBE_GRID = 1,
    STORAGE_BUFFER_COUNT
} StorageBuffers;


const ShaderProgramInfo c_shader_programs[] = {
    [SHADER_PROGRAM_TRIANGLE] = (ShaderProgramInfo) {
        .flags = 0,
        .vertex_shader = CONST_STRING("out/data/triangle_v.spv"),
        .fragment_shader = CONST_STRING("out/data/triangle_f.spv")
    },
    [SHADER_PROGRAM_DEFAULT] = (ShaderProgramInfo) {
        .flags = SHADER_PRORGAM_FLAG_USE_VERTEX_POSITION,
        .vertex_shader = CONST_STRING("out/data/default_v.spv"),
        .fragment_shader = CONST_STRING("out/data/default_f.spv"),
    },
    [SHADER_PROGRAM_BODY] = (ShaderProgramInfo) {
        .flags = SHADER_PRORGAM_FLAG_USE_VERTEX_POSITION,
        .vertex_shader = CONST_STRING("out/data/body_v.spv"),
        .fragment_shader = CONST_STRING("out/data/body_f.spv")
    },
    [SHADER_PROGRAM_EYE] = (ShaderProgramInfo) {
        .flags = SHADER_PRORGAM_FLAG_USE_VERTEX_POSITION,
        .vertex_shader = CONST_STRING("out/data/eye_v.spv"),
        .fragment_shader = CONST_STRING("out/data/eye_f.spv")
    }
};

const MeshInfo c_mesh_infos[] = {
    [MESH_BODY] = (MeshInfo) {
        .file = CONST_STRING("out/data/body.model")
    },
    [MESH_EYE] = (MeshInfo) {
        .file = CONST_STRING("out/data/eye.model")
    }
};

const StorageBufferInfo c_storage_buffers[] = {
    [STORAGE_BUFFER_UNIFORM_SCALE] = (StorageBufferInfo) {
        .size = sizeof(UniformScaleObject) * 2,
        .stride = sizeof(UniformScaleObject)
    },
    [STORAGE_BUFFER_CUBE_GRID] = (StorageBufferInfo) {
        .size = sizeof(UniformScaleObject) * 2,
        .stride = sizeof(UniformScaleObject),
        .data = (UniformScaleObject[2]){0}
    }
};

const UniformBufferInfo c_uniform_buffer_info = {
    .size = sizeof(UniformBuffer),
    .data = (UniformBuffer[]){(UniformBuffer){0}}
};


void updateCallback(UpdateInfo *info, RenderCmd *render_cmd) {
    cmdBeginRendering(render_cmd, 1, (u32[]){RENDER_ATTACHMENT_SCREEN_COLOR_ID}, RENDER_ATTACHMENT_SCREEN_DEPTH_ID); 
    cmdDrawProcedural(render_cmd, SHADER_PROGRAM_TRIANGLE, 18, 1);
    cmdDrawMesh(render_cmd, SHADER_PROGRAM_DEFAULT, MESH_BODY, 1);
    cmdDrawMesh(render_cmd, SHADER_PROGRAM_DEFAULT, MESH_EYE, 1);
    cmdEndRendering(render_cmd);
}

i32 main(i32 argc, char **argv) {
    if(!createArena(&s_context_arena, 1024 * 1024 * 64, 1024 * 1024 * 4)) {
        MSG_ERROR(msgCallback, &TRACED_STR("failed to create s_context_arena allocator"));
        return -1;
    }

    /* vulkan context cration */
    const VulkanContextInfo vulkan_info = {
        .name = CONST_STRING("Wreck"),
        .resolution_x = 800,
        .resolution_y = 600,
        .flags = VULKAN_FLAG_RESIZABLE | VULKAN_FLAG_DEBUG,
        .msg_callback = &msgCallback
    };
    VulkanContext *vulkan_context = createVulkanContext(&allocateContext, &vulkan_info);
    if(!vulkan_context) {
        MSG_ERROR(msgCallback, &TRACED_STR("failed to create vulkan_context"));
        return -2;
    }

    /* render context creation */
    const RenderContextInfo render_info = {
        .vulkan_context = vulkan_context,
        .msg_callback = &msgCallback,
        /* programs */
        .programs = c_shader_programs,
        .program_count = SHADER_PROGRAM_COUNT,
        /* meshes */
        .meshes = c_mesh_infos,
        .mesh_count = MESH_COUNT,
        /* buffers */
        .uniform_buffer = &c_uniform_buffer_info,
        .storage_buffers = c_storage_buffers,
        .storage_host_mutable_buffer_count = 0,
        .storage_buffer_count = 0
    };
    RenderContext *render_context = createRenderContext(&allocateContext, &render_info);
    if(!render_context) {
        MSG_ERROR(msgCallback, &TRACED_STR("failed to create render_context"));
        return -3;
    }

    /* running render loop */
    if(!runRenderLoop(render_context, &updateCallback)) {
        MSG_ERROR(msgCallback, &TRACED_STR("render loop failed"));
        return -4;
    }

    /* destrcution of render context */
    destroyRenderContext(render_context);
    /* destrcution of vulkan context */
    destroyVulkanContext(vulkan_context);

    if(!freeArena(&s_context_arena)) {
        MSG_ERROR(msgCallback, &TRACED_STR("failed to free s_context_arena allcoator"))
    }
}
