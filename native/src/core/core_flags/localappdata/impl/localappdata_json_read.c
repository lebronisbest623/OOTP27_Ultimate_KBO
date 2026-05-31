#include "../localappdata_internal.h"

#include <stdio.h>
#include <string.h>

int kbo_read_localappdata_named_json_cached(
    const char* file_name,
    const char* key,
    KboLocalappdataJsonParseFn parse,
    KboLocalappdataJsonValueParseFn parse_value,
    void* context)
{
    if (file_name == NULL || file_name[0] == '\0' || parse == NULL) {
        return 0;
    }

    WCHAR path[KBO_WIDE_PATH_CHARS] = {0};
    if (!kbo_get_localappdata_named_json_path_w(file_name, path, KBO_WIDE_PATH_CHARS)) {
        return 0;
    }

    WIN32_FILE_ATTRIBUTE_DATA attrs;
    memset(&attrs, 0, sizeof(attrs));
    if (!GetFileAttributesExW(path, GetFileExInfoStandard, &attrs)) {
        return 0;
    }
    if (attrs.nFileSizeHigh != 0u || attrs.nFileSizeLow > KBO_FLAGS_JSON_MAX_BYTES) {
        return 0;
    }

    AcquireSRWLockShared(&g_kbo_localappdata_json_cache_lock);
    KboLocalappdataJsonCacheEntry* cached = kbo_localappdata_json_cache_find_locked(path);
    if (kbo_localappdata_json_cache_entry_matches(
            cached,
            attrs.nFileSizeLow,
            attrs.ftLastWriteTime)) {
        const char* value = NULL;
        const char* value_end = NULL;
        int found = parse_value != NULL
            && kbo_localappdata_json_find_cached_span(cached, key, &value, &value_end)
            && parse_value(value, value_end, context);
        if (!found) {
            found = parse(cached->buffer, cached->size, context);
        }
        ReleaseSRWLockShared(&g_kbo_localappdata_json_cache_lock);
        return found;
    }
    ReleaseSRWLockShared(&g_kbo_localappdata_json_cache_lock);

    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }

    BY_HANDLE_FILE_INFORMATION info;
    memset(&info, 0, sizeof(info));
    if (!GetFileInformationByHandle(file, &info)
            || info.nFileSizeHigh != 0u
            || info.nFileSizeLow > KBO_FLAGS_JSON_MAX_BYTES) {
        CloseHandle(file);
        return 0;
    }

    DWORD size = info.nFileSizeLow;
    char* buffer = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)size + 1u);
    if (buffer == NULL) {
        CloseHandle(file);
        return 0;
    }

    DWORD read = 0;
    int read_ok = ReadFile(file, buffer, size, &read, NULL) && read == size;
    CloseHandle(file);
    if (!read_ok) {
        HeapFree(GetProcessHeap(), 0, buffer);
        return 0;
    }

    KboLocalappdataJsonValueSpan spans[256];
    DWORD span_count = 0u;
    int spans_valid = kbo_localappdata_json_parse_value_spans(buffer, read, spans, &span_count);

    int found = 0;
    if (spans_valid && parse_value != NULL && key != NULL && key[0] != '\0') {
        const char* value = NULL;
        const char* value_end = NULL;
        found = kbo_localappdata_json_find_span_in_table(
                buffer,
                read,
                spans,
                span_count,
                key,
                &value,
                &value_end)
            && parse_value(value, value_end, context);
    }
    if (!found) {
        found = parse(buffer, read, context);
    }

    AcquireSRWLockExclusive(&g_kbo_localappdata_json_cache_lock);
    KboLocalappdataJsonCacheEntry* entry = kbo_localappdata_json_cache_slot_locked(path);
    kbo_localappdata_json_cache_clear_entry(entry);
    _snwprintf(entry->path, KBO_WIDE_PATH_CHARS, L"%ls", path);
    entry->last_write_time = info.ftLastWriteTime;
    entry->size = read;
    if (spans_valid) {
        memcpy(entry->values, spans, sizeof(spans[0]) * span_count);
        entry->value_count = span_count;
        entry->values_valid = 1u;
    }
    entry->buffer = buffer;
    entry->valid = 1u;
    buffer = NULL;
    ReleaseSRWLockExclusive(&g_kbo_localappdata_json_cache_lock);

    return found;
}

