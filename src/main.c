#include "base.h"
#include "render/render.h"

const AllocationCallbacks c_allocator = (AllocationCallbacks) {
    .allocate = (malloc),
    .release  = (free)   
};

i32 main(
    i32    argc,
    char** argv
) {
    OpenWindowRenderIn open_window_in = {
        .name     = "Scuby",
        .window_x = 800,
        .window_y = 600
    };

    CtxHandle render_handle = openRenderWindow(&open_window_in, &c_allocator);
    if(render_handle == NULL) {
        LOG_ERROR("failed to open render window");
        goto fail;
    }

    if(!closeRenderWindow(render_handle, &c_allocator)) {
        LOG_ERROR("failed to cloe render window");
        goto fail;
    }

    return 0;

    fail: {

    }
}
