#include "localappdata_internal.h"

#include "../json/json_bool_parser.h"
#include "../../product/ootp_product.h"

#include <stdio.h>
#include <string.h>

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
