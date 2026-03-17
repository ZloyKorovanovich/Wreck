#include "files.h"

#define SHADERS_PATH "shaders.bin"

#ifdef _WIN32

typedef struct {
    HANDLE shaders_file;
    HANDLE shaders_file_map;
    HANDLE models_file;
    HANDLE models_file_map;
    void *shaders_file_address;
    void *models_file_address;
} InitFiles;

/* static data */
static InitFiles s_init_files = (InitFiles){0};
static MsgCallback_pfn s_msg_callback = NULL;

b32 loadInitFiles(
    LoadInitFilesIn *input, 
    LoadInitFilesOut *output, 
    MsgCallback_pfn msg_callback
) {
    /* VALIDATION */ {
        s_msg_callback = msg_callback;
        if(!input || !output) {
            MSG_ERROR(s_msg_callback, &TRACED_STR("invalid input or output addresses"));
            return FALSE;
        }
        if(!input->dir_path) {
            MSG_ERROR(s_msg_callback, &TRACED_STR("invalid resources directory path"));
            return FALSE;
        }
    }
    
    String file_path = STACK_STR(256);

    if(input->flags & RESOURCE_FILE_SHADERS) {
        /* get shaders file path */
        stringZero(&file_path);
        stringAddCstring(&file_path, input->dir_path);
        stringAddCstring(&file_path, "/" SHADERS_PATH);
        /* open shaders file */
        s_init_files.shaders_file = CreateFile(file_path.string, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if(s_init_files.shaders_file == INVALID_HANDLE_VALUE) {
            MSG_ERROR(s_msg_callback, &TRACED_STR("failed to open shader file"));
            return FALSE;
        }

        s_init_files.shaders_file_map = CreateFileMapping(s_init_files.shaders_file, NULL, PAGE_READONLY, 0, SHADER_BLOB_TOTAL_SIZE, "shaders file map");
        if(!s_init_files.shaders_file_map) {
            MSG_ERROR(s_msg_callback, &TRACED_STR("failed to create map for shader file"));
            return FALSE;
        }
        s_init_files.shaders_file_address = MapViewOfFile(s_init_files.shaders_file_map, FILE_MAP_READ, 0, 0, SHADER_BLOB_TOTAL_SIZE);
        if(!s_init_files.shaders_file_address) {
            MSG_ERROR(s_msg_callback, &TRACED_STR("failed to map view of shader file"));
            return FALSE;
        }
    }
    
    
    /* fill output */
    *output = (LoadInitFilesOut) {
        .shaders_address = s_init_files.shaders_file_address
    };

    return TRUE;
}

void closeInitFiles(void) {
    /* close shaders file */
    if(s_init_files.shaders_file_address) {
        UnmapViewOfFile(s_init_files.shaders_file_address);
    }
    if(s_init_files.shaders_file_map) {
        CloseHandle(s_init_files.shaders_file_map);
    }
    if(s_init_files.shaders_file && s_init_files.shaders_file != INVALID_HANDLE_VALUE) {
        CloseHandle(s_init_files.shaders_file_map);
    }

    s_init_files = (InitFiles){0};
}

#endif
