#include "core_policy.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>

#include "../core_flags/json/json_bool_parser.h"
#include "../files/save_paths/core_save_paths.h"
#include "../files/save_paths/platform/core_path_io.h"

static int kbo_read_policy_file_value(
    const char* path,
    const char* key,
    int* out_value,
    int flag_value)
{
    if (path == NULL || path[0] == '\0' || key == NULL || key[0] == '\0' || out_value == NULL) {
        return 0;
    }

    HANDLE file = kbo_create_file_read_utf8(path);
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }

    DWORD size = GetFileSize(file, NULL);
    if (size == INVALID_FILE_SIZE || size == 0u || size > KBO_FLAGS_JSON_MAX_BYTES) {
        CloseHandle(file);
        return 0;
    }

    char* buffer = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)size + 1u);
    if (buffer == NULL) {
        CloseHandle(file);
        return 0;
    }

    DWORD read = 0;
    int found = 0;
    if (ReadFile(file, buffer, size, &read, NULL) && read > 0u) {
        found = flag_value
            ? kbo_find_flag_value_in_json(buffer, read, key, out_value)
            : kbo_find_int_value_in_json(buffer, read, key, out_value);
    }
    CloseHandle(file);
    HeapFree(GetProcessHeap(), 0, buffer);
    return found;
}

static int kbo_save_scoped_config_file_path(const char* file_name, char* out, size_t out_size)
{
    if (file_name == NULL || file_name[0] == '\0' || out == NULL || out_size < 2u) {
        return 0;
    }
    out[0] = '\0';

    char relative[KBO_UTF8_PATH_BYTES] = {0};
    int len = snprintf(relative, sizeof(relative), "config\\%s", file_name);
    if (len <= 0 || (size_t)len >= sizeof(relative)) {
        return 0;
    }
    return kbo_get_save_scoped_data_file(relative, out, out_size);
}

static int kbo_read_policy_scoped_value(
    const char* file_name,
    const char* key,
    int* out_value,
    int flag_value)
{
    if (file_name == NULL || file_name[0] == '\0' || key == NULL || key[0] == '\0' || out_value == NULL) {
        return 0;
    }

    char path[KBO_UTF8_PATH_BYTES] = {0};
    if (kbo_save_scoped_config_file_path(file_name, path, sizeof(path))
            && kbo_read_policy_file_value(path, key, out_value, flag_value)) {
        return 1;
    }
    path[0] = '\0';
    if (kbo_get_global_data_file(file_name, path, sizeof(path))
            && kbo_read_policy_file_value(path, key, out_value, flag_value)) {
        return 1;
    }
    return 0;
}

static int kbo_read_policy_global_value(
    const char* file_name,
    const char* key,
    int* out_value,
    int flag_value)
{
    if (file_name == NULL || file_name[0] == '\0' || key == NULL || key[0] == '\0' || out_value == NULL) {
        return 0;
    }

    char path[KBO_UTF8_PATH_BYTES] = {0};
    return kbo_get_global_data_file(file_name, path, sizeof(path))
        && kbo_read_policy_file_value(path, key, out_value, flag_value);
}

int kbo_read_policy_int_value(const char* file_name, const char* key, int* out_value)
{
    return kbo_read_policy_scoped_value(file_name, key, out_value, 0);
}

int kbo_read_policy_flag_value(const char* file_name, const char* key, int* out_value)
{
    return kbo_read_policy_scoped_value(file_name, key, out_value, 1);
}

int kbo_read_global_policy_int_value(const char* file_name, const char* key, int* out_value)
{
    return kbo_read_policy_global_value(file_name, key, out_value, 0);
}

int32_t kbo_read_clamped_policy_int(
    const char* file_name,
    const char* key,
    int32_t fallback,
    int32_t min_value,
    int32_t max_value)
{
    int value = (int)fallback;
    if (kbo_read_policy_int_value(file_name, key, &value)
            && value >= min_value
            && value <= max_value) {
        return (int32_t)value;
    }
    return fallback;
}

int32_t kbo_read_clamped_global_policy_int(
    const char* file_name,
    const char* key,
    int32_t fallback,
    int32_t min_value,
    int32_t max_value)
{
    int value = (int)fallback;
    if (kbo_read_global_policy_int_value(file_name, key, &value)
            && value >= min_value
            && value <= max_value) {
        return (int32_t)value;
    }
    return fallback;
}
