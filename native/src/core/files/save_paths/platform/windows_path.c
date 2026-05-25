/* Win32 UTF-8 path helpers for save-scoped files. */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../core_save_paths_internal.h"
#include "../../../product/ootp_product.h"

int kbo_utf8_to_wide_path(const char* path, WCHAR* out, DWORD out_count)
{
    if (path == NULL || path[0] == '\0' || out == NULL || out_count == 0) {
        return 0;
    }
    out[0] = L'\0';
    int wrote = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, out, (int)out_count);
    if (wrote <= 0) {
        wrote = MultiByteToWideChar(CP_ACP, 0, path, -1, out, (int)out_count);
    }
    return wrote > 0 && (DWORD)wrote <= out_count;
}

int kbo_wide_to_utf8_path(const WCHAR* path, char* out, size_t out_size)
{
    if (path == NULL || path[0] == L'\0' || out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';
    int wrote = WideCharToMultiByte(CP_UTF8, 0, path, -1, out, (int)out_size, NULL, NULL);
    return wrote > 0 && (size_t)wrote <= out_size;
}

int kbo_get_localappdata_utf8(char* out, size_t out_size)
{
    WCHAR local_w[KBO_WIDE_PATH_CHARS] = {0};
    DWORD got = GetEnvironmentVariableW(L"LOCALAPPDATA", local_w, KBO_WIDE_PATH_CHARS);
    return got > 0 && got < KBO_WIDE_PATH_CHARS && kbo_wide_to_utf8_path(local_w, out, out_size);
}

int kbo_create_directory_utf8(const char* path)
{
    WCHAR wide[KBO_WIDE_PATH_CHARS] = {0};
    if (!kbo_utf8_to_wide_path(path, wide, KBO_WIDE_PATH_CHARS)) {
        return 0;
    }
    return CreateDirectoryW(wide, NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
}

DWORD kbo_get_file_attributes_utf8(const char* path)
{
    WCHAR wide[KBO_WIDE_PATH_CHARS] = {0};
    if (!kbo_utf8_to_wide_path(path, wide, KBO_WIDE_PATH_CHARS)) {
        return INVALID_FILE_ATTRIBUTES;
    }
    return GetFileAttributesW(wide);
}

int kbo_delete_file_utf8(const char* path)
{
    WCHAR wide[KBO_WIDE_PATH_CHARS] = {0};
    if (!kbo_utf8_to_wide_path(path, wide, KBO_WIDE_PATH_CHARS)) {
        return 0;
    }
    return DeleteFileW(wide) || GetLastError() == ERROR_FILE_NOT_FOUND;
}

int kbo_move_file_replace_utf8(const char* source, const char* destination)
{
    WCHAR wide_source[KBO_WIDE_PATH_CHARS] = {0};
    WCHAR wide_destination[KBO_WIDE_PATH_CHARS] = {0};
    if (!kbo_utf8_to_wide_path(source, wide_source, KBO_WIDE_PATH_CHARS)
            || !kbo_utf8_to_wide_path(destination, wide_destination, KBO_WIDE_PATH_CHARS)) {
        return 0;
    }
    return MoveFileExW(wide_source, wide_destination, MOVEFILE_REPLACE_EXISTING) != 0;
}

int kbo_copy_file_utf8(const char* source, const char* destination, int fail_if_exists)
{
    WCHAR wide_source[KBO_WIDE_PATH_CHARS] = {0};
    WCHAR wide_destination[KBO_WIDE_PATH_CHARS] = {0};
    if (!kbo_utf8_to_wide_path(source, wide_source, KBO_WIDE_PATH_CHARS)
            || !kbo_utf8_to_wide_path(destination, wide_destination, KBO_WIDE_PATH_CHARS)) {
        return 0;
    }
    return CopyFileW(wide_source, wide_destination, fail_if_exists ? TRUE : FALSE) != 0;
}

HANDLE kbo_create_file_utf8(
    const char* path,
    DWORD desired_access,
    DWORD share_mode,
    DWORD creation_disposition,
    DWORD flags_and_attributes)
{
    WCHAR wide[KBO_WIDE_PATH_CHARS] = {0};
    if (!kbo_utf8_to_wide_path(path, wide, KBO_WIDE_PATH_CHARS)) {
        return INVALID_HANDLE_VALUE;
    }
    return CreateFileW(
        wide,
        desired_access,
        share_mode,
        NULL,
        creation_disposition,
        flags_and_attributes,
        NULL);
}

HANDLE kbo_create_file_read_utf8(const char* path)
{
    return kbo_create_file_utf8(
        path,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL);
}

int kbo_get_file_attributes_ex_utf8(const char* path, WIN32_FILE_ATTRIBUTE_DATA* out)
{
    WCHAR wide[KBO_WIDE_PATH_CHARS] = {0};
    if (!kbo_utf8_to_wide_path(path, wide, KBO_WIDE_PATH_CHARS)) {
        return 0;
    }
    return GetFileAttributesExW(wide, GetFileExInfoStandard, out);
}

int kbo_path_looks_like_absolute_save_path(const char* path)
{
    if (path == NULL || path[0] == '\0') {
        return 0;
    }
    int absolute = 0;
    if (((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z'))
            && path[1] == ':' && (path[2] == '\\' || path[2] == '/')) {
        absolute = 1;
    } else if (path[0] == '\\' && path[1] == '\\') {
        absolute = 1;
    }
    if (!absolute) {
        return 0;
    }

    size_t len = strlen(path);
    while (len > 0 && (path[len - 1] == '\\' || path[len - 1] == '/')) {
        len--;
    }
    size_t extension_len = strlen(KBO_OOTP_SAVE_EXTENSION);
    if (len < extension_len
            || _strnicmp(path + len - extension_len, KBO_OOTP_SAVE_EXTENSION, extension_len) != 0) {
        return 0;
    }

    char normalized[KBO_UTF8_PATH_BYTES] = {0};
    if (len >= sizeof(normalized)) {
        return 0;
    }
    memcpy(normalized, path, len);
    normalized[len] = '\0';

    DWORD attrs = kbo_get_file_attributes_utf8(normalized);
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
}
