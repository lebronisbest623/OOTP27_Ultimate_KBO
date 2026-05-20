#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../core_news_templates.h"
#include "../core_news_templates_internal.h"

#include <stdio.h>

#include "../../../core_flags/json/json_bool_parser.h"
#include "../../../files/save_paths/platform/core_path_io.h"

int kbo_news_template_file_exists(const char* path)
{
    if (path == NULL || path[0] == '\0') {
        return 0;
    }
    DWORD attrs = kbo_get_file_attributes_utf8(path);
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

char* kbo_news_template_read_json_file(const char* path, DWORD* out_size)
{
    if (path == NULL || path[0] == '\0' || out_size == NULL) {
        return NULL;
    }
    *out_size = 0u;

    HANDLE file = kbo_create_file_read_utf8(path);
    if (file == INVALID_HANDLE_VALUE) {
        return NULL;
    }

    DWORD high = 0u;
    DWORD size = GetFileSize(file, &high);
    if (size == INVALID_FILE_SIZE || high != 0u || size == 0u || size > KBO_FLAGS_JSON_MAX_BYTES) {
        CloseHandle(file);
        return NULL;
    }

    char* buffer = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)size + 1u);
    if (buffer == NULL) {
        CloseHandle(file);
        return NULL;
    }

    DWORD read = 0u;
    if (!ReadFile(file, buffer, size, &read, NULL) || read != size) {
        HeapFree(GetProcessHeap(), 0, buffer);
        CloseHandle(file);
        return NULL;
    }
    CloseHandle(file);

    buffer[size] = '\0';
    *out_size = size;
    return buffer;
}

int kbo_news_template_load_from_path(const char* path, const char* key, char* out, size_t out_size)
{
    if (path == NULL || key == NULL || out == NULL || out_size == 0u) {
        return 0;
    }

    DWORD json_size = 0u;
    char* json = kbo_news_template_read_json_file(path, &json_size);
    if (json == NULL) {
        return 0;
    }

    const char* start = NULL;
    const char* end = NULL;
    int ok = kbo_find_json_value_span(json, json_size, key, &start, &end)
        && kbo_json_string_value_at(start, end, out, out_size);

    HeapFree(GetProcessHeap(), 0, json);
    return ok;
}

int kbo_news_template_load_int_from_path(const char* path, const char* key, int* out_value)
{
    if (path == NULL || key == NULL || out_value == NULL) {
        return 0;
    }

    DWORD json_size = 0u;
    char* json = kbo_news_template_read_json_file(path, &json_size);
    if (json == NULL) {
        return 0;
    }

    int ok = kbo_find_int_value_in_json(json, json_size, key, out_value);

    HeapFree(GetProcessHeap(), 0, json);
    return ok;
}

int kbo_news_template_try_load_from_existing_path(
    const char* path,
    const char* key,
    char* out,
    size_t out_size,
    char* source_path,
    size_t source_path_size)
{
    if (path == NULL || !kbo_news_template_file_exists(path)) {
        return 0;
    }
    if (!kbo_news_template_load_from_path(path, key, out, out_size)) {
        return 0;
    }
    if (source_path != NULL && source_path_size > 0u) {
        snprintf(source_path, source_path_size, "%s", path);
    }
    return 1;
}
