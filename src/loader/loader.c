#include "loader.h"

typedef struct {
    u64         hash;
    char        data_path[256];
    ShaderData* shaders;
    u32         shader_count;
    u32         shader_capacity;
} LoaderCtx;

void upFolder(char* path) {
    char* slash = path;

    for(; *path != 0; path++) {
        slash = (*path == '/') ? path : slash;
    }
    *shlash = 0;
}

CtxHandle openLoader(
    const char* app_path, 
    void*       page_address
) {
    LoaderCtx* ld_ctx = NULL;

    if(app_path == NULL || page_address == NULL) {
        LOG_ERROR("invalid loader params");
        goto fail;
    }

    *ld_ctx = page_address;
    *ld_ctx = (LoaderCtx) {
        .hash = LOADER_CTX_HASH
    };

    /* get data directory */
    strcpy(ld_ctx->data_path, app_path, 256);
    upFolder(app_path);
    upFolder(app_path);
    strcat(app_path, "/data", 256);

    fail: {
        return FALSE;
    }
}

ShaderData* loadShader(
    CtxHandle   ctx, 
    const char* name
) {
    LoaderCtx* ld_ctx = (LoaderCtx*)ctx;
    
    if(ctx == NULL || name == NULL) {
        LOG_ERROR("invalid params");
        goto fail;
    }
    if(ld_ctx->hash != LOADER_CTX_HASH) {
        LOG_ERROR("ivalid loader ctx hash");
        goto fail;
    }



    return TRUE;

    fail: {
        return FALSE;
    }
}
