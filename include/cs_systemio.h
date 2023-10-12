#ifndef CROSSPLATFORM_SYSTEM_INPUT_OUTPUT_H
#define CROSSPLATFORM_SYSTEM_INPUT_OUTPUT_H

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define DEF_DIR_ENTRY_COUNT 10
#define CIO_FILE_ERROR -1
#define CIO_FILE_SUCCESS 0

#define false 0
#define true 1

#ifdef _WIN32
#define CIO_PLATFORM_NT
#define CIO_PATH_MAX MAX_PATH

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
#define CIO_PATH_MAX PATH_MAX

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
    char name[CIO_PATH_MAX];
    EntryType type;
    uint64_t size;
    time_t mtime;
} EntryInfo;

typedef struct _cio_directory_info {
#ifdef CIO_PLATFORM_UNIX
    DIR* _dir_handle;
#elif defined(CIO_PLATFORM_NT)
#endif

    char path[CIO_PATH_MAX];
    EntryInfo* entries;
    size_t entries_count;
    uint64_t size;
    time_t mtime;

} DirectoryInfo;

DirectoryInfo* Directory_Open(const char* path) {
    DirectoryInfo* info = (DirectoryInfo*)malloc(sizeof(DirectoryInfo));
#ifdef CIO_PLATFORM_UNIX
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
    info->mtime = st.st_mtime;

    struct dirent* entry;
    size_t i = 0;
    while ((entry = readdir(info->_dir_handle))) {
        if (i >= info->entries_count) {
            info->entries_count *= 2;
            info->entries = (EntryInfo*)realloc(info->entries, info->entries_count * sizeof(EntryInfo));
        }
        strcpy(info->entries[i].name, entry->d_name);

        if (lstat(info->entries[i].path, &st) == -1) {
            fprintf(stderr, "CS_SystemIO: Failed to access %s\n", info->entries[i].path);
            perror("native error");
            --i;
            continue;
        }

        info->entries[i].type = (entry->d_type == DT_REG) ? EntryType_File : EntryType_Directory;
        info->entries[i].size = st.st_size;
        info->entries[i].mtime = st.st_mtime;

        info->size += st.st_size;
        ++i;
    }
    info->entries_count = i;
    info->entries = (EntryInfo*)realloc(info->entries, info->entries_count * sizeof(EntryInfo));
#elif defined(CIO_PLATFORM_NT)
#endif
    char t_path[CIO_PATH_MAX];
    strcpy(t_path, path);
    strcpy(t_path, "\\*.*");
    WIN32_FIND_DATA find_data;
    HANDLE h_find = FindFirstFile(t_path, &find_data);

    if (h_find == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "CS_SystemIO: Failed to open %s for reading.\n", path);
        return NULL;
    }

    strcpy(info->path, path);
    info->entries_count = DEF_DIR_ENTRY_COUNT;
    info->entries = (EntryInfo*)malloc(sizeof(EntryInfo) * info->entries_count);
    info->size = 0;

    FILETIME ft = find_data.ftLastWriteTime;
    SYSTEMTIME st;
    FileTimeToSystemTime(&ft, &st);

    struct tm file_time;
    file_time.tm_sec = st.wSecond;
    file_time.tm_min = st.wMinute;
    file_time.tm_hour = st.wHour;
    file_time.tm_mday = st.wDay;
    file_time.tm_mon = st.wMonth - 1; // Adjust for 0-based months.
    file_time.tm_year = st.wYear - 1900; // Adjust for years since 1900.

    info->mtime = mktime(&file_time);

    size_t i = 0;
    while (FindNextFile(h_find, &find_data) != 0) {
        if (i >= info->entries_count) {
            info->entries_count *= 2;
            info->entries = (EntryInfo*)realloc(info->entries, info->entries_count * sizeof(EntryInfo));
        }
        strcpy(info->entries[i].name, find_data.cFileName);

        info->entries[i].type = (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? EntryType_Directory : EntryType_File;
        info->entries[i].size = (uint64_t)(((uint64_t)find_data.nFileSizeHigh << 32) | find_data.nFileSizeLow);

        ft = find_data.ftLastWriteTime;
        FileTimeToSystemTime(&ft, &st);

        struct tm file_time;
        file_time.tm_sec = st.wSecond;
        file_time.tm_min = st.wMinute;
        file_time.tm_hour = st.wHour;
        file_time.tm_mday = st.wDay;
        file_time.tm_mon = st.wMonth - 1; // Adjust for 0-based months.
        file_time.tm_year = st.wYear - 1900; // Adjust for years since 1900.
        
        info->entries[i].mtime = mktime(&file_time);

        info->size += info->entries[i].size;
        ++i;
    }

    return info;
}

void Directory_Close(DirectoryInfo* restrict d) {
    free(d->entries);
    free(d);
}

int32_t File_GetCurrentDirectory(char* buffer, const size_t size) {
#ifdef CIO_PLATFORM_UNIX
    if (!getcwd(buffer, size)) {
        fputs("CIO_SystemIO: Failed to retreive current working directory.\n", stderr);
        return CIO_FILE_ERROR;
    }
#elif defined(CIO_PLATFORM_NT)
    if (!GetCurrentDirectory(size, buffer)) {
        fputs("CIO_SystemIO: Failed to retrieve current working directory.\n", stderr);
        return CIO_FILE_ERROR;
    }
#endif
    return CIO_FILE_SUCCESS;
}

#endif // CROSSPLATFORM_SYSTEM_INPUT_OUTPUT_H
