#include "localappdata_reader.h"

#include "../json/json_bool_parser.h"
#include "../../files/save_paths/platform/core_path_io.h"
#include "../../product/ootp_product.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef int (*KboLocalappdataJsonParseFn)(const char* json, DWORD json_size, void* context);
typedef int (*KboLocalappdataJsonValueParseFn)(const char* value, const char* end, void* context);

typedef struct KboLocalappdataJsonValueSpan {
    DWORD key_start;
    DWORD key_size;
    DWORD value_start;
    DWORD value_size;
} KboLocalappdataJsonValueSpan;

typedef struct KboLocalappdataJsonCacheEntry {
    WCHAR path[KBO_WIDE_PATH_CHARS];
    FILETIME last_write_time;
    DWORD size;
    char* buffer;
    KboLocalappdataJsonValueSpan values[256];
    DWORD value_count;
    uint8_t values_valid;
    uint8_t valid;
} KboLocalappdataJsonCacheEntry;

enum {
    KBO_LOCALAPPDATA_JSON_CACHE_SLOTS = 8
};

static SRWLOCK g_kbo_localappdata_json_cache_lock = SRWLOCK_INIT;
static KboLocalappdataJsonCacheEntry
    g_kbo_localappdata_json_cache[KBO_LOCALAPPDATA_JSON_CACHE_SLOTS];

static int kbo_filetime_equal(FILETIME a, FILETIME b)
{
    return a.dwLowDateTime == b.dwLowDateTime && a.dwHighDateTime == b.dwHighDateTime;
}

static uint32_t kbo_localappdata_json_cache_slot(const WCHAR* path)
{
    uint32_t hash = 2166136261u;
    if (path == NULL) {
        return 0u;
    }
    for (const WCHAR* p = path; *p != L'\0'; p++) {
        hash ^= (uint32_t)*p;
        hash *= 16777619u;
    }
    return hash % KBO_LOCALAPPDATA_JSON_CACHE_SLOTS;
}

static void kbo_localappdata_json_cache_clear_entry(KboLocalappdataJsonCacheEntry* entry)
{
    if (entry == NULL) {
        return;
    }
    if (entry->buffer != NULL) {
        HeapFree(GetProcessHeap(), 0, entry->buffer);
    }
    memset(entry, 0, sizeof(*entry));
}

static KboLocalappdataJsonCacheEntry* kbo_localappdata_json_cache_find_locked(const WCHAR* path)
{
    if (path == NULL || path[0] == L'\0') {
        return NULL;
    }
    for (int i = 0; i < KBO_LOCALAPPDATA_JSON_CACHE_SLOTS; i++) {
        KboLocalappdataJsonCacheEntry* entry = &g_kbo_localappdata_json_cache[i];
        if (entry->valid && wcscmp(entry->path, path) == 0) {
            return entry;
        }
    }
    return NULL;
}

static KboLocalappdataJsonCacheEntry* kbo_localappdata_json_cache_slot_locked(const WCHAR* path)
{
    KboLocalappdataJsonCacheEntry* existing = kbo_localappdata_json_cache_find_locked(path);
    if (existing != NULL) {
        return existing;
    }
    for (int i = 0; i < KBO_LOCALAPPDATA_JSON_CACHE_SLOTS; i++) {
        if (!g_kbo_localappdata_json_cache[i].valid) {
            return &g_kbo_localappdata_json_cache[i];
        }
    }
    return &g_kbo_localappdata_json_cache[kbo_localappdata_json_cache_slot(path)];
}

static int kbo_localappdata_json_cache_entry_matches(
    const KboLocalappdataJsonCacheEntry* entry,
    DWORD size,
    FILETIME last_write_time)
{
    return entry != NULL
        && entry->valid
        && entry->buffer != NULL
        && entry->size == size
        && kbo_filetime_equal(entry->last_write_time, last_write_time);
}

static int kbo_localappdata_json_find_span_in_table(
    const char* buffer,
    DWORD size,
    const KboLocalappdataJsonValueSpan* spans,
    DWORD span_count,
    const char* key,
    const char** out_value,
    const char** out_end)
{
    if (out_value != NULL) { *out_value = NULL; }
    if (out_end != NULL) { *out_end = NULL; }
    if (buffer == NULL
            || spans == NULL
            || key == NULL
            || key[0] == '\0') {
        return 0;
    }

    size_t key_len = strlen(key);
    for (DWORD i = 0; i < span_count; i++) {
        const KboLocalappdataJsonValueSpan* span = &spans[i];
        if (span->key_size == key_len
                && span->key_start + span->key_size <= size
                && span->value_start + span->value_size <= size
                && memcmp(buffer + span->key_start, key, key_len) == 0) {
            if (out_value != NULL) { *out_value = buffer + span->value_start; }
            if (out_end != NULL) { *out_end = buffer + span->value_start + span->value_size; }
            return 1;
        }
    }
    return 0;
}

static int kbo_localappdata_json_find_cached_span(
    const KboLocalappdataJsonCacheEntry* entry,
    const char* key,
    const char** out_value,
    const char** out_end)
{
    if (entry == NULL || !entry->values_valid) {
        if (out_value != NULL) { *out_value = NULL; }
        if (out_end != NULL) { *out_end = NULL; }
        return 0;
    }
    return kbo_localappdata_json_find_span_in_table(
        entry->buffer,
        entry->size,
        entry->values,
        entry->value_count,
        key,
        out_value,
        out_end);
}

static const char* kbo_localappdata_json_skip_nested_value(const char* p, const char* end)
{
    if (p == NULL || p >= end || (*p != '{' && *p != '[')) {
        return NULL;
    }

    int object_depth = 0;
    int array_depth = 0;
    for (; p < end; p++) {
        if (*p == '"') {
            const char* stop = kbo_json_find_string_end(p, end);
            if (stop == NULL) {
                return NULL;
            }
            p = stop;
            continue;
        }
        if (*p == '{') {
            object_depth++;
        } else if (*p == '[') {
            array_depth++;
        } else if (*p == '}') {
            object_depth--;
            if (object_depth < 0) {
                return NULL;
            }
        } else if (*p == ']') {
            array_depth--;
            if (array_depth < 0) {
                return NULL;
            }
        }
        if (object_depth == 0 && array_depth == 0) {
            return p + 1;
        }
    }
    return NULL;
}

static const char* kbo_localappdata_json_value_end(const char* value, const char* end)
{
    value = kbo_json_skip_ws(value, end);
    if (value == NULL || value >= end) {
        return NULL;
    }

    if (*value == '"') {
        const char* stop = kbo_json_find_string_end(value, end);
        return stop != NULL ? stop + 1 : NULL;
    }
    if (*value == '{' || *value == '[') {
        return kbo_localappdata_json_skip_nested_value(value, end);
    }

    const char* p = value;
    while (p < end && *p != ',' && *p != '}') {
        p++;
    }
    const char* stop = p;
    while (stop > value
            && (stop[-1] == ' ' || stop[-1] == '\t' || stop[-1] == '\r' || stop[-1] == '\n')) {
        stop--;
    }
    return stop > value ? stop : NULL;
}

static int kbo_localappdata_json_parse_value_spans(
    const char* json,
    DWORD size,
    KboLocalappdataJsonValueSpan* out_values,
    DWORD* out_count)
{
    if (out_count != NULL) { *out_count = 0u; }
    if (json == NULL || out_values == NULL || out_count == NULL || size == 0u || size > KBO_FLAGS_JSON_MAX_BYTES) {
        return 0;
    }

    const char* base = json;
    const char* end = json + size;
    const char* p = json;
    if (size >= 3u
            && (unsigned char)p[0] == 0xEFu
            && (unsigned char)p[1] == 0xBBu
            && (unsigned char)p[2] == 0xBFu) {
        p += 3;
    }
    p = kbo_json_skip_ws(p, end);
    if (p >= end || *p != '{') {
        return 0;
    }
    p++;

    DWORD count = 0u;
    for (;;) {
        p = kbo_json_skip_ws(p, end);
        if (p >= end) {
            return 0;
        }
        if (*p == '}') {
            *out_count = count;
            return 1;
        }
        if (*p != '"') {
            return 0;
        }

        const char* key_start = p + 1;
        const char* key_stop = kbo_json_find_string_end(p, end);
        if (key_stop == NULL) {
            return 0;
        }
        p = kbo_json_skip_ws(key_stop + 1, end);
        if (p >= end || *p != ':') {
            return 0;
        }

        const char* value_start = kbo_json_skip_ws(p + 1, end);
        const char* value_stop = kbo_localappdata_json_value_end(value_start, end);
        if (value_start == NULL || value_stop == NULL || value_stop < value_start) {
            return 0;
        }
        if (count >= 256u) {
            return 0;
        }

        out_values[count].key_start = (DWORD)(key_start - base);
        out_values[count].key_size = (DWORD)(key_stop - key_start);
        out_values[count].value_start = (DWORD)(value_start - base);
        out_values[count].value_size = (DWORD)(value_stop - value_start);
        count++;

        p = kbo_json_skip_ws(value_stop, end);
        if (p >= end) {
            return 0;
        }
        if (*p == ',') {
            p++;
            continue;
        }
        if (*p == '}') {
            *out_count = count;
            return 1;
        }
        return 0;
    }
}

static void kbo_invalidate_localappdata_named_json_cache_path(const WCHAR* path)
{
    if (path == NULL || path[0] == L'\0') {
        return;
    }
    AcquireSRWLockExclusive(&g_kbo_localappdata_json_cache_lock);
    KboLocalappdataJsonCacheEntry* entry = kbo_localappdata_json_cache_find_locked(path);
    if (entry != NULL) {
        kbo_localappdata_json_cache_clear_entry(entry);
    }
    ReleaseSRWLockExclusive(&g_kbo_localappdata_json_cache_lock);
}

static int kbo_get_localappdata_named_json_path_w(const char* file_name, WCHAR* out, DWORD out_count)
{
    if (file_name == NULL || file_name[0] == '\0' || out == NULL || out_count == 0) {
        return 0;
    }
    out[0] = L'\0';

    WCHAR local_app_data[KBO_WIDE_PATH_CHARS] = {0};
    DWORD got = GetEnvironmentVariableW(L"LOCALAPPDATA", local_app_data, KBO_WIDE_PATH_CHARS);
    if (got == 0 || got >= KBO_WIDE_PATH_CHARS) {
        return 0;
    }

    WCHAR wide_file_name[260] = {0};
    int converted = MultiByteToWideChar(CP_UTF8, 0, file_name, -1, wide_file_name, (int)(sizeof(wide_file_name) / sizeof(wide_file_name[0])));
    if (converted <= 0) {
        return 0;
    }

    int written = _snwprintf(out, out_count, L"%ls\\" KBO_PRODUCT_LOCAL_DATA_DIR_W L"\\%ls", local_app_data, wide_file_name);
    return written > 0 && (DWORD)written < out_count;
}

static int kbo_create_parent_directory_w(const WCHAR* path)
{
    if (path == NULL || path[0] == L'\0') {
        return 0;
    }

    WCHAR dir[KBO_WIDE_PATH_CHARS] = {0};
    _snwprintf(dir, KBO_WIDE_PATH_CHARS, L"%ls", path);
    WCHAR* slash = wcsrchr(dir, L'\\');
    if (slash == NULL) {
        return 0;
    }

    *slash = L'\0';
    return CreateDirectoryW(dir, NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
}

static int kbo_read_localappdata_named_json_cached(
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

typedef struct KboLocalappdataJsonIntContext {
    const char* key;
    int* out_value;
} KboLocalappdataJsonIntContext;

typedef struct KboLocalappdataJsonStringContext {
    const char* key;
    char* out;
    size_t out_size;
} KboLocalappdataJsonStringContext;

static int kbo_parse_localappdata_json_flag(const char* json, DWORD json_size, void* context)
{
    KboLocalappdataJsonIntContext* ctx = (KboLocalappdataJsonIntContext*)context;
    if (ctx == NULL) {
        return 0;
    }
    return kbo_find_flag_value_in_json(json, json_size, ctx->key, ctx->out_value);
}

static int kbo_parse_localappdata_json_flag_value_at(const char* value, const char* end, void* context)
{
    KboLocalappdataJsonIntContext* ctx = (KboLocalappdataJsonIntContext*)context;
    if (ctx == NULL) {
        return 0;
    }
    return kbo_json_bool_value_at(value, end, ctx->out_value);
}

static int kbo_parse_localappdata_json_int(const char* json, DWORD json_size, void* context)
{
    KboLocalappdataJsonIntContext* ctx = (KboLocalappdataJsonIntContext*)context;
    if (ctx == NULL) {
        return 0;
    }
    return kbo_find_int_value_in_json(json, json_size, ctx->key, ctx->out_value);
}

static int kbo_parse_localappdata_json_int_value_at(const char* value, const char* end, void* context)
{
    KboLocalappdataJsonIntContext* ctx = (KboLocalappdataJsonIntContext*)context;
    if (ctx == NULL) {
        return 0;
    }
    return kbo_json_int_value_at(value, end, ctx->out_value);
}

static int kbo_parse_localappdata_json_string(const char* json, DWORD json_size, void* context)
{
    KboLocalappdataJsonStringContext* ctx = (KboLocalappdataJsonStringContext*)context;
    if (ctx == NULL) {
        return 0;
    }
    return kbo_find_string_value_in_json(json, json_size, ctx->key, ctx->out, ctx->out_size);
}

static int kbo_parse_localappdata_json_string_value_at(const char* value, const char* end, void* context)
{
    KboLocalappdataJsonStringContext* ctx = (KboLocalappdataJsonStringContext*)context;
    if (ctx == NULL) {
        return 0;
    }
    return kbo_json_string_value_at(value, end, ctx->out, ctx->out_size);
}

int kbo_read_localappdata_named_json_flag_value(const char* file_name, const char* key, int* out_value)
{
    if (key == NULL || key[0] == '\0' || out_value == NULL) {
        return 0;
    }
    KboLocalappdataJsonIntContext context = { key, out_value };
    return kbo_read_localappdata_named_json_cached(
        file_name,
        key,
        kbo_parse_localappdata_json_flag,
        kbo_parse_localappdata_json_flag_value_at,
        &context);
}

int kbo_read_localappdata_json_flag_value(const char* key, int* out_value)
{
    return kbo_read_localappdata_named_json_flag_value(KBO_PRODUCT_FLAGS_JSON_FILE, key, out_value);
}

int kbo_read_localappdata_setting_flag_value(const char* key, int* out_value)
{
    if (kbo_read_localappdata_named_json_flag_value(KBO_PRODUCT_SETTINGS_JSON_FILE, key, out_value)) {
        return 1;
    }
    return kbo_read_localappdata_json_flag_value(key, out_value);
}

int kbo_read_localappdata_named_json_int_value(const char* file_name, const char* key, int* out_value)
{
    if (key == NULL || key[0] == '\0' || out_value == NULL) {
        return 0;
    }
    KboLocalappdataJsonIntContext context = { key, out_value };
    return kbo_read_localappdata_named_json_cached(
        file_name,
        key,
        kbo_parse_localappdata_json_int,
        kbo_parse_localappdata_json_int_value_at,
        &context);
}

int kbo_read_localappdata_json_int_value(const char* key, int* out_value)
{
    return kbo_read_localappdata_named_json_int_value(KBO_PRODUCT_FLAGS_JSON_FILE, key, out_value);
}

int kbo_read_localappdata_setting_int_value(const char* key, int* out_value)
{
    if (kbo_read_localappdata_named_json_int_value(KBO_PRODUCT_SETTINGS_JSON_FILE, key, out_value)) {
        return 1;
    }
    return kbo_read_localappdata_json_int_value(key, out_value);
}

int kbo_read_localappdata_named_json_string_value(const char* file_name, const char* key, char* out, size_t out_size)
{
    if (out != NULL && out_size > 0) {
        out[0] = '\0';
    }
    if (key == NULL || key[0] == '\0' || out == NULL || out_size == 0) {
        return 0;
    }
    KboLocalappdataJsonStringContext context = { key, out, out_size };
    return kbo_read_localappdata_named_json_cached(
        file_name,
        key,
        kbo_parse_localappdata_json_string,
        kbo_parse_localappdata_json_string_value_at,
        &context);
}

static int kbo_write_all_bytes_to_file(const WCHAR* path, const char* data, DWORD size)
{
    if (path == NULL || data == NULL) {
        return 0;
    }

    kbo_create_parent_directory_w(path);

    HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }

    DWORD written = 0;
    int ok = WriteFile(file, data, size, &written, NULL) && written == size;
    CloseHandle(file);
    return ok;
}

static int kbo_write_localappdata_named_json_bytes(
    const WCHAR* path,
    const char* file_name,
    const char* data,
    DWORD size)
{
    int ok = kbo_write_all_bytes_to_file(path, data, size);
    if (ok) {
        kbo_invalidate_localappdata_named_json_cache_path(path);
    }
    (void)file_name;
    return ok;
}

int kbo_write_localappdata_named_json_int_value(const char* file_name, const char* key, int value)
{
    if (file_name == NULL || file_name[0] == '\0' || key == NULL || key[0] == '\0') {
        return 0;
    }

    WCHAR path[KBO_WIDE_PATH_CHARS] = {0};
    if (!kbo_get_localappdata_named_json_path_w(file_name, path, KBO_WIDE_PATH_CHARS)) {
        return 0;
    }

    char value_text[32] = {0};
    snprintf(value_text, sizeof(value_text), "%d", value);
    size_t value_len = strlen(value_text);

    char* input = NULL;
    DWORD input_size = 0;
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file != INVALID_HANDLE_VALUE) {
        DWORD size = GetFileSize(file, NULL);
        if (size != INVALID_FILE_SIZE && size <= KBO_FLAGS_JSON_MAX_BYTES) {
            input = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)size + 1u);
            if (input != NULL) {
                DWORD read = 0;
                if (ReadFile(file, input, size, &read, NULL)) {
                    input_size = read;
                } else {
                    HeapFree(GetProcessHeap(), 0, input);
                    input = NULL;
                }
            }
        }
        CloseHandle(file);
    }

    if (input == NULL || input_size == 0) {
        char fresh[160] = {0};
        int written = snprintf(fresh, sizeof(fresh), "{\r\n  \"%s\": %d\r\n}\r\n", key, value);
        int ok = written > 0 && kbo_write_localappdata_named_json_bytes(path, file_name, fresh, (DWORD)written);
        if (input != NULL) {
            HeapFree(GetProcessHeap(), 0, input);
        }
        return ok;
    }

    const char* value_start = NULL;
    const char* value_end = NULL;
    char* output = NULL;
    DWORD output_size = 0;

    if (kbo_find_json_value_span(input, input_size, key, &value_start, &value_end)
            && value_start != NULL && value_end != NULL && value_end >= value_start) {
        size_t prefix = (size_t)(value_start - input);
        size_t suffix = (size_t)((input + input_size) - value_end);
        output_size = (DWORD)(prefix + value_len + suffix);
        output = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)output_size + 1u);
        if (output != NULL) {
            memcpy(output, input, prefix);
            memcpy(output + prefix, value_text, value_len);
            memcpy(output + prefix + value_len, value_end, suffix);
        }
    } else {
        const char* end = input + input_size;
        const char* close = end;
        while (close > input && close[-1] != '}') {
            close--;
        }
        if (close > input && close[-1] == '}') {
            close--;
        } else {
            close = NULL;
        }

        if (close == NULL) {
            char fresh[160] = {0};
            int written = snprintf(fresh, sizeof(fresh), "{\r\n  \"%s\": %d\r\n}\r\n", key, value);
            int ok = written > 0 && kbo_write_localappdata_named_json_bytes(path, file_name, fresh, (DWORD)written);
            HeapFree(GetProcessHeap(), 0, input);
            return ok;
        }

        const char* object_start = input;
        while (object_start < close && *object_start != '{') {
            object_start++;
        }
        const char* body = object_start < close ? object_start + 1 : input;
        int has_body = 0;
        for (const char* p = body; p < close; p++) {
            if (*p != ' ' && *p != '\t' && *p != '\r' && *p != '\n') {
                has_body = 1;
                break;
            }
        }

        char insert[192] = {0};
        int insert_len = snprintf(insert, sizeof(insert), "%s\r\n  \"%s\": %d\r\n",
            has_body ? "," : "", key, value);
        if (insert_len <= 0) {
            HeapFree(GetProcessHeap(), 0, input);
            return 0;
        }

        size_t prefix = (size_t)(close - input);
        size_t suffix = (size_t)((input + input_size) - close);
        output_size = (DWORD)(prefix + (size_t)insert_len + suffix);
        output = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)output_size + 1u);
        if (output != NULL) {
            memcpy(output, input, prefix);
            memcpy(output + prefix, insert, (size_t)insert_len);
            memcpy(output + prefix + (size_t)insert_len, close, suffix);
        }
    }

    int ok = output != NULL && kbo_write_localappdata_named_json_bytes(path, file_name, output, output_size);
    if (output != NULL) {
        HeapFree(GetProcessHeap(), 0, output);
    }
    HeapFree(GetProcessHeap(), 0, input);
    return ok;
}

int kbo_write_localappdata_json_int_value(const char* key, int value)
{
    return kbo_write_localappdata_named_json_int_value(KBO_PRODUCT_FLAGS_JSON_FILE, key, value);
}

int kbo_write_localappdata_setting_int_value(const char* key, int value)
{
    return kbo_write_localappdata_named_json_int_value(KBO_PRODUCT_SETTINGS_JSON_FILE, key, value);
}
