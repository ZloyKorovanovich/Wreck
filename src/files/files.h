#ifndef _FILES_INCLUDED
#define _FILES_INCLUDED

#include <base.h>
#include "shaders_auto.h"

typedef enum {
    RESOURCE_FILES_NONE = 0x0,
    RESOURCE_FILE_SHADERS = 0x1
} ResourceFileFlags;

typedef struct {
    const char *dir_path;
    ResourceFileFlags flags;
} LoadInitFilesIn;

typedef struct {
    const void *shaders_address;
} LoadInitFilesOut;

b32 loadInitFiles(LoadInitFilesIn *input, LoadInitFilesOut *output, MsgCallback_pfn msg_callback);
void closeInitFiles(void);

#endif
