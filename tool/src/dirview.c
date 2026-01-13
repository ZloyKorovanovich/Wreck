#define _CRT_SECURE_NO_DEPRECATE
#include <windows.h>
#include <stdio.h>
#include <string.h>

typedef int i32;
typedef unsigned u32;
typedef unsigned long long u64;
typedef float f32;

void reset_tab_buff(char* tab_buff, u32 depth) {
    for(u32 i = 0; i <= depth * 2; i++) {
        tab_buff[i] = (i & 0x1) ? ' ' : '|';
    }
    tab_buff[depth * 2 + 1] = '-';
    tab_buff[depth * 2 + 2] = '-';
    tab_buff[depth * 2 + 3] = ' ';
    tab_buff[depth * 2 + 4] = '\0';
}

void scan_directory(char* path, char* tab_buff, u32 depth, u32 max_depth, const char** ignore, u32 ignore_count) {
    if(depth == max_depth) return;

    reset_tab_buff(tab_buff, depth);

    WIN32_FIND_DATA find_data = (WIN32_FIND_DATA){0};
    HANDLE handle = FindFirstFile(path, &find_data);
    /* if no files in directory */
    if(handle == INVALID_HANDLE_VALUE) return;
    /* if not empty nor . nor .. */
    if(find_data.cFileName[0] && find_data.cFileName[0] != '.' && (find_data.cFileName[1] != '.' && find_data.cFileName[1])) {
        /* if directory */
        printf("  %s %s\n", tab_buff, find_data.cFileName);
        if((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY) {
            /* check if in igonre list */
            for(u32 i = 0; i < ignore_count; i++) {
                if(strcmp(find_data.cFileName, ignore[i]) == 0) {
                    goto skip_file;
                }
            }

            char* path_end = path + strlen(path) - 1;
            strcpy(path_end, find_data.cFileName);
            strcat(path_end, "/*");
            /* recurseviely scan directory */
            scan_directory(path, tab_buff, depth + 1, max_depth, ignore, ignore_count);
            /* remove file name from path */
            *path_end = '*';
            *(path_end + 1) = '\0';
            /* fix tab buffer */
            reset_tab_buff(tab_buff, depth);
        }
    }

    skip_file: {};

    while (FindNextFile(handle, &find_data)) {
        /* check for . or .. */
        if(!find_data.cFileName[0] || (find_data.cFileName[0] == '.' && (find_data.cFileName[1] == '.' || !find_data.cFileName[1]))) {
            continue;
        }
        /* if directory */
        printf("  %s %s\n", tab_buff, find_data.cFileName);
        if((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY) {
            /* check if in igonre list */
            for(u32 i = 0; i < ignore_count; i++) {
                if(strcmp(find_data.cFileName, ignore[i]) == 0) {
                    goto skip_file;
                }
            }

            char* path_end = path + strlen(path) - 1;
            strcpy(path_end, find_data.cFileName);
            strcat(path_end, "/*");
            /* recurseviely scan directory */
            scan_directory(path, tab_buff, depth + 1, max_depth, ignore, ignore_count);
            /* remove file name from path */
            *path_end = '*';
            *(path_end + 1) = '\0';
            /* fix tab buffer */
            reset_tab_buff(tab_buff, depth);
        }
    }
}

i32 main(i32 argc, char** argv) {
    if(argc < 3) {
        printf("wrong argument count, arguments should be: 1 <dir>, 2 <max search depth>, ... <ignore dirs>\n");
        return -1;
    }

    char path[256] = {0};
    char tab_buff[256] = {0};
    strcpy(tab_buff, "\t");
    strcpy(path, argv[1]);
    strcat(path, "/*");
    scan_directory(path, tab_buff, 0, (u32)strtol(argv[2], NULL, 10), (const char**)&argv[3], argc - 3);
    return 0;
}
