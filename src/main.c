#include "base.h"
#include "render/render.h"

const AllocationCallbacks c_allocator = (AllocationCallbacks) {
    .allocate = (malloc),
    .release  = (free)   
};


static u8  vertex_shader  [4096] = {0};
static u8  fragment_shader[4096] = {0};
static u64 vertex_shader_size    = 0;
static u64 fragment_shader_size  = 0;

i32 main(
    i32    argc,
    char** argv
) {
    OpenRenderWindowIn open_window_in    = {0};
    LoadShaderProgramsIn load_shaders_in = {0};
    CtxHandle render_handle              = NULL;
    FILE* vertex_file                    = NULL;
    FILE* fragment_file                  = NULL;

    vertex_file   = fopen("res/spv/triangle_v.spv", "rb");
    fragment_file = fopen("res/spv/triangle_f.spv", "rb");
    if(vertex_file == NULL || fragment_file == NULL) {
        LOG_ERROR("failed to load shader files");
        goto fail;
    }
    fseek(vertex_file  , 0, SEEK_END);
    fseek(fragment_file, 0, SEEK_END);
    vertex_shader_size   = ftell(vertex_file);
    fragment_shader_size = ftell(fragment_file);
    fseek(vertex_file  , 0, SEEK_SET);
    fseek(fragment_file, 0, SEEK_SET);
    fread_s(vertex_shader  , 4096, 1, vertex_shader_size  , vertex_file  );
    fread_s(fragment_shader, 4096, 1, fragment_shader_size, fragment_file);
    fclose(vertex_file);
    fclose(fragment_file);

    open_window_in = (OpenRenderWindowIn) {
        .name     = "Scuby",
        .window_x = 800,
        .window_y = 600
    };
    load_shaders_in = (LoadShaderProgramsIn) {
        .program_count = 1,
        .programs      = (ShaderProgram[]) {
            (ShaderProgram) {
                .type                   = SHADER_PROGRAM_TYPE_GRAPHICS,
                .color_attachment_count = 1,
                .color_attachment_ids   = (u32[]){IMAGE_SURFACE_COLOR_ID},
                .depth_attachment_id    = IMAGE_SCREEN_DEPTH_ID,
                .vertex                 = vertex_shader,
                .fragment               = fragment_shader,
                .vertex_size            = vertex_shader_size,
                .fragment_size          = fragment_shader_size
            }
        }
    };

    render_handle = openRenderWindow(&open_window_in, &c_allocator);
    if(render_handle == NULL) {
        LOG_ERROR("failed to open render window");
        goto fail;
    }
    if(!loadShaderPrograms(
        render_handle, 
        &load_shaders_in, 
        &c_allocator
    )) {
        LOG_ERROR("failed to load shader programs");
        goto fail;
    }

    if(!loadShaderPrograms(
        render_handle, 
        NULL,
        &c_allocator
    )) {
        LOG_ERROR("failed to unload shader programs");
        goto fail;
    }
    if(!closeRenderWindow(render_handle, &c_allocator)) {
        LOG_ERROR("failed to close render window");
        goto fail;
    }

    return 0;

    fail: {
        return -1;
    }
}
