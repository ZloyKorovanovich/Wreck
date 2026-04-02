#ifndef _LOADER_INCLUDED
#define _LOADER_INCLUDED

typedef struct {
    void* spv;
    u64   spv_size;
} ShaderData;

ShaderData* loadShader(const char* name);

#endif
