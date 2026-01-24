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

void *allocateContext(u64 size, u64 aligment) {
    return allocateArena(&s_context_arena, size, aligment);
}

typedef enum {
    SHADER_PROGRAM_TRIANGLE = 0,
    SHADER_PROGRAM_COUNT
} ShaderPorgrams;

const ShaderProgramInfo c_shader_programs[] = {
    [SHADER_PROGRAM_TRIANGLE] = (ShaderProgramInfo) {
        .vertex_shader = CONST_STRING("out/data/triangle_v.spv"),
        .fragment_shader = CONST_STRING("out/data/triangle_f.spv")
    }
};


typedef enum {
    MESH_SHITY_QUAD = 0,
    MESH_SHITY_QUAD_COUNT
} Meshes;

const MeshInfo c_mesh_infos[] = {
    [MESH_SHITY_QUAD] = (MeshInfo) {
        .file = CONST_STRING("out/data/shity_quad_v1.model")
    }
};

void updateCallback(UpdateInfo *info, RenderCmd *render_cmd) {
    beginRendering(render_cmd, 1, (u32[]){RENDER_ATTACHMENT_SCREEN_COLOR_ID}, RENDER_ATTACHMENT_SCREEN_DEPTH_ID); 
    drawProcedural(render_cmd, SHADER_PROGRAM_TRIANGLE, 18, 1);
    endRendering(render_cmd);
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
        .programs = c_shader_programs,
        .program_count = ARRAY_SIZE(c_shader_programs),
        .meshes = c_mesh_infos,
        .mesh_count = ARRAY_SIZE(c_mesh_infos),
        .msg_callback = &msgCallback
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
