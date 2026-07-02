#include "res.h"
#include <windows.h>

#define RES_VIRTUAL_BASE          (0x0000000000000000)
#define RES_VIRTUAL_SHADERS_BASE  (0x0000000000000000)
#define RES_VIRTUAL_SHADERS_LIMIT (0x00000000001FF000)
#define RES_VIRTUAL_LIMIT         (0x0000000000200000)

typedef struct {
    void* base;
    void* limit;
    void* shaders_arena_base;
    void* shaders_arena_limit;
    u64   shaders_used_size;
    u64   shaders_commit_size;
} ResContext;

CtxHandle res_start(void) {
    const u64 context_size = ALIGN(sizeof(ResContext), 0x1000);
    const u64 virtual_size = context_size + 0x1000 + RES_VIRTUAL_LIMIT;

    ResContext* context = VirtualAlloc(NULL, virtual_size, MEM_RESERVE, PAGE_READWRITE);
    if(context == NULL) {
        LOG_ERROR("failed to allocate resources virtual address space");
        goto fail;
    }
    if(VirtualAlloc(context, context_size, MEM_COMMIT, PAGE_READWRITE) == NULL) {
        LOG_ERROR("failed to commit resources context memory");
        goto fail;
    }

    *context = (ResContext) {
        .base               = (u8*)context + 0x1000 + RES_VIRTUAL_BASE,
        .limit              = (u8*)context + 0x1000 + RES_VIRTUAL_LIMIT,
        .shaders_arena_base  = (u8*)context + 0x1000 + RES_VIRTUAL_SHADERS_BASE,
        .shaders_arena_limit = (u8*)context + 0x1000 + RES_VIRTUAL_SHADERS_LIMIT
    };

    return context;

    fail: {
        return NULL;
    }
}

void res_stop(
    CtxHandle ctx
) {
    ResContext* context = (ResContext*)ctx;
    if(!VirtualFree(context, 0, MEM_RELEASE)) {
        LOG_ERROR("failed to free resources virtual memory");
    }
}

void* res_load_shader(
    CtxHandle   ctx, 
    const char* name,
    u64*        size
) {
    ResContext* res_cxt = (ResContext*)ctx;

    LARGE_INTEGER file_size = (LARGE_INTEGER){0};
    HANDLE        file      = INVALID_HANDLE_VALUE;

    /* open file */
    file = CreateFileA(
        name,
        FILE_GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    if(file == INVALID_HANDLE_VALUE) {
        LOG_ERROR("failed to open shader file name: %s", name);
        goto fail;
    }

    if(!GetFileSizeEx(file, &file_size)) {
        LOG_ERROR("failed to get shader file size: %s", name);
        goto fail;
    }

    /* commit memory */
    const u64 shader_offset   = res_cxt->shaders_used_size;
    const u64 shader_size     = file_size.QuadPart;
    const u64 new_used_size   = shader_offset + shader_size;
    const u64 new_commit_size = ALIGN(new_used_size, 0x1000);
    
    if(new_commit_size > RES_VIRTUAL_SHADERS_LIMIT - RES_VIRTUAL_SHADERS_BASE) {
        LOG_ERROR(
            "shaders exceed dedicated memory space: %llu/%llu", 
            new_commit_size, (u64)(RES_VIRTUAL_SHADERS_LIMIT - RES_VIRTUAL_SHADERS_BASE)
        );
        goto fail;
    }

    if(res_cxt->shaders_commit_size < new_commit_size) {
        if(VirtualAlloc(res_cxt->shaders_arena_base, new_commit_size, MEM_COMMIT, PAGE_READWRITE) == NULL) {
            LOG_ERROR("failed to commit shaders memory");
            goto fail;
        }
    }
    res_cxt->shaders_used_size   = new_used_size;
    res_cxt->shaders_commit_size = new_commit_size;

    /* read file */
    void* shader_buffer = (u8*)res_cxt->shaders_arena_base + shader_offset;

    if(!ReadFile(file, shader_buffer, shader_size, NULL, NULL)) {
        LOG_ERROR("failed to read shader file: %s", name);
        goto fail;
    }

    *size = shader_size;
    return shader_buffer;

    fail: {
        return NULL;
    }
}

void res_free_shaders(CtxHandle ctx) {
    ResContext* res_cxt = (ResContext*)ctx;

    if(!VirtualFree(
        res_cxt->shaders_arena_base,
        res_cxt->shaders_commit_size,
        MEM_DECOMMIT
    )) {
        LOG_ERROR("failed to decommit shaders memory");
    }
    
    res_cxt->shaders_used_size   = 0;
    res_cxt->shaders_commit_size = 0;
}
