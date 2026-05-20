#ifndef KBOFIX_SRC_CORE_FILES_SAVE_PATHS_PLATFORM_CORE_PATH_IO_H_
#define KBOFIX_SRC_CORE_FILES_SAVE_PATHS_PLATFORM_CORE_PATH_IO_H_

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stddef.h>

#define KBO_WIDE_PATH_CHARS 32768
#define KBO_UTF8_PATH_BYTES 32768

int kbo_utf8_to_wide_path(const char* path, WCHAR* out, DWORD out_count);
int kbo_wide_to_utf8_path(const WCHAR* path, char* out, size_t out_size);
int kbo_get_localappdata_utf8(char* out, size_t out_size);
int kbo_create_directory_utf8(const char* path);
DWORD kbo_get_file_attributes_utf8(const char* path);
HANDLE kbo_create_file_utf8(
    const char* path,
    DWORD desired_access,
    DWORD share_mode,
    DWORD creation_disposition,
    DWORD flags_and_attributes);
HANDLE kbo_create_file_read_utf8(const char* path);
int kbo_get_file_attributes_ex_utf8(const char* path, WIN32_FILE_ATTRIBUTE_DATA* out);

#endif
