#ifndef _RES_INCLUDED
#define _RES_INCLUDED

#include "../base.h"

CtxHandle res_start(void);
void      res_stop(CtxHandle ctx);

void* res_load_shader(CtxHandle ctx, const char* name, u64* size);
void  res_free_shaders(CtxHandle ctx);

#endif
