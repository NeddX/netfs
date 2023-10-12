#ifndef CROSSPLATFORM_SYSTEM_INPUT_OUTPUT_H
#define CROSSPLATFORM_SYSTEM_INPUT_OUTPUT_H

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define DEF_DIR_ENTRY_COUNT 10

#define false 0
#define true 1

#ifdef _WIN32
#define CIO_PLATFORM_NT

// MSVC shenanigans.
#ifdef _MSC_VER
#define restrict __restrict
#define stdcall __stdcall

// gcc and clang shenanigans.
#elif defined(__clang__) || defined(__GNUC__)
#define stdcall __attribute__((stdcall))
#endif

#define WIN32_MEAN_AND_LEAN
#include <windows.h>

#elif defined(__linux__) || defined(__APPLE__)
#define CIO_PLATFORM_UNIX

#include <dirent.h>
#include <sys/stat.h>

// gcc and clang shenanigans.
#if defined(__clang__) || defined(__GNUC__)
#define stdcall __attribute__((stdcall))
#endif

#endif

typedef enum _cio_entry_type {
    EntryType_File,
    EntryType_Directory
} EntryType;

typedef struct _cio_entry_info {
    char path[PATH_MAX];
    EntryType type;
    size_t size;
    time_t atime;
    time_t mtime;
    time_t ctime;
    uid_t owner_id;
    gid_t group_id;
} EntryInfo;

typedef struct _cio_directory_info {
#ifdef CIO_PLATFORM_UNIX
    DIR* _dir_handle;
#elif CIO_PLATFORM_NT
#endif

    char path[PATH_MAX];
    EntryInfo* entries;
    size_t entries_count;
    size_t size;
    time_t atime;
    time_t mtime;
    time_t ctime;
    uid_t owner_id;
    gid_t group_id;

} DirectoryInfo;

DirectoryInfo* Directory_Open(const char* path) {
    DirectoryInfo* info = (DirectoryInfo*)malloc(sizeof(DirectoryInfo));

    info->_dir_handle = opendir(path);
    if (!info->_dir_handle) {
        fprintf(stderr, "CS_SystemIO: Failed to open directory %s\n", path);
        perror("native error");
        free(info);
        return NULL;
    }

    struct stat st;
    if (lstat(path, &st) == -1) {
        fprintf(stderr, "CS_SystemIO: Failed to access %s\n", path);
        perror("native error");
        free(info);
        return NULL;
    }

    strcpy(info->path, path);
    info->entries_count = DEF_DIR_ENTRY_COUNT;
    info->entries = (EntryInfo*)malloc(sizeof(EntryInfo) * info->entries_count);
    info->size = 0;
    info->atime = st.st_atime;
    info->ctime = st.st_ctime;
    info->mtime = st.st_mtime;
    info->owner_id = st.st_uid;
    info->group_id = st.st_gid;

    struct dirent* entry;
    size_t i = 0;
    while ((entry = readdir(info->_dir_handle))) {
        if (i >= info->entries_count) {
            info->entries_count *= 2;
            info->entries = (EntryInfo*)realloc(info->entries, info->entries_count * sizeof(EntryInfo));
        }
        snprintf(info->entries[i].path, sizeof(info->entries[i].path), "%s/%s", path, entry->d_name);

        if (lstat(info->entries[i].path, &st) == -1) {
            fprintf(stderr, "CS_SystemIO: Failed to access %s\n", info->entries[i].path);
            perror("native error");
            --i;
            continue;
        }

        info->entries[i].type = (entry->d_type == DT_REG) ? EntryType_File : EntryType_Directory;
        info->entries[i].size = st.st_size;
        info->entries[i].atime = st.st_atime;
        info->entries[i].ctime = st.st_ctime;
        info->entries[i].mtime = st.st_mtime;
        info->entries[i].owner_id = st.st_uid;
        info->entries[i].group_id = st.st_gid;

        info->size += st.st_size;
        ++i;
    }
    info->entries_count = i;
    info->entries = (EntryInfo*)realloc(info->entries, info->entries_count * sizeof(EntryInfo));
    return info;
}

void Directory_Close(DirectoryInfo* restrict d) {
    free(d->entries);
    free(d);
}

#endif // CROSSPLATFORM_SYSTEM_INPUT_OUTPUT_H
