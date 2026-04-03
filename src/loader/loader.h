#ifndef _LOADER_INCLUDED
#define _LOADER_INCLUDED

#include "../base.h"

typedef struct {
    void* spv;
    u64   spv_size;
} ShaderData;

CtxHandle   openLoader(const char* app_path, void* page_address);
b32         closeLoader(CtxHandle ctx);
ShaderData* loadShader(CtxHandle ctx, const char* name);

#endif
