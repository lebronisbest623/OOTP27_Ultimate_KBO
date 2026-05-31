#include "../localappdata_internal.h"

#include "../../json/json_bool_parser.h"

#include <string.h>

int kbo_localappdata_json_find_span_in_table(
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

int kbo_localappdata_json_parse_value_spans(
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

