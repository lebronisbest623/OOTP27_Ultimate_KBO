#include "json_bool_parser.h"

#define JSMN_STATIC
#include "../../../../third_party/jsmn/jsmn.h"

#include <string.h>

#define KBO_JSON_MAX_TOKENS 512

static int kbo_ascii_lower_char(int c)
{
    return (c >= 'A' && c <= 'Z') ? c + ('a' - 'A') : c;
}

static int kbo_text_equals_ignore_case_n(const char* text, size_t len, const char* expected)
{
    if (text == NULL || expected == NULL) {
        return 0;
    }
    for (size_t i = 0; i < len; i++) {
        if (expected[i] == '\0'
                || kbo_ascii_lower_char((unsigned char)text[i]) != kbo_ascii_lower_char((unsigned char)expected[i])) {
            return 0;
        }
    }
    return expected[len] == '\0';
}

const char* kbo_json_skip_ws(const char* p, const char* end)
{
    while (p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) {
        p++;
    }
    return p;
}

const char* kbo_json_find_string_end(const char* quote, const char* end)
{
    if (quote == NULL || quote >= end || *quote != '"') {
        return NULL;
    }

    const char* p = quote + 1;
    while (p < end) {
        if (*p == '\\') {
            p += (p + 1 < end) ? 2 : 1;
            continue;
        }
        if (*p == '"') {
            return p;
        }
        p++;
    }
    return NULL;
}

int kbo_json_string_equals_key(const char* start, const char* stop, const char* key)
{
    if (start == NULL || stop == NULL || key == NULL) {
        return 0;
    }

    size_t len = (size_t)(stop - start);
    return strlen(key) == len && memcmp(start, key, len) == 0;
}

static int kbo_token_equals_key(const char* json, const jsmntok_t* token, const char* key)
{
    if (json == NULL || token == NULL || key == NULL || token->type != JSMN_STRING || token->start < 0 || token->end < token->start) {
        return 0;
    }

    size_t len = (size_t)(token->end - token->start);
    return strlen(key) == len && memcmp(json + token->start, key, len) == 0;
}

static int kbo_parse_json_tokens(const char* json, DWORD json_size, jsmntok_t* tokens, unsigned int token_count)
{
    if (json == NULL || tokens == NULL || json_size == 0 || json_size > KBO_FLAGS_JSON_MAX_BYTES) {
        return -1;
    }

    if (json_size >= 3
            && (unsigned char)json[0] == 0xEF
            && (unsigned char)json[1] == 0xBB
            && (unsigned char)json[2] == 0xBF) {
        json += 3;
        json_size -= 3;
        int parsed = kbo_parse_json_tokens(json, json_size, tokens, token_count);
        if (parsed <= 0) {
            return parsed;
        }
        for (int i = 0; i < parsed; i++) {
            if (tokens[i].start >= 0) {
                tokens[i].start += 3;
            }
            if (tokens[i].end >= 0) {
                tokens[i].end += 3;
            }
        }
        return parsed;
    }

    jsmn_parser parser;
    jsmn_init(&parser);
    int parsed = jsmn_parse(&parser, json, (size_t)json_size, tokens, token_count);
    if (parsed <= 0 || tokens[0].type != JSMN_OBJECT) {
        return -1;
    }
    return parsed;
}

static int kbo_json_token_subtree_end(const jsmntok_t* tokens, int parsed, int index)
{
    if (tokens == NULL || index < 0 || index >= parsed) {
        return index + 1;
    }

    if (tokens[index].start < 0 || tokens[index].end < tokens[index].start) {
        return index + 1;
    }

    int cursor = index + 1;
    while (cursor < parsed
            && tokens[cursor].start >= tokens[index].start
            && tokens[cursor].end <= tokens[index].end) {
        cursor++;
    }
    return cursor;
}

static int kbo_json_token_span_value(const char* json, const jsmntok_t* token, const char** out_start, const char** out_end)
{
    if (json == NULL || token == NULL || out_start == NULL || out_end == NULL || token->start < 0 || token->end < token->start) {
        return 0;
    }

    *out_start = json + token->start;
    *out_end = json + token->end;
    if (token->type == JSMN_STRING) {
        *out_start = json + token->start - 1;
        *out_end = json + token->end + 1;
    }
    return 1;
}

static int kbo_json_bool_text_value(const char* text, size_t len, int* out_value)
{
    if (out_value == NULL) {
        return 0;
    }

    while (len > 0 && (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n')) {
        text++;
        len--;
    }
    while (len > 0 && (text[len - 1] == ' ' || text[len - 1] == '\t' || text[len - 1] == '\r' || text[len - 1] == '\n')) {
        len--;
    }

    if (kbo_text_equals_ignore_case_n(text, len, "1")
            || kbo_text_equals_ignore_case_n(text, len, "true")
            || kbo_text_equals_ignore_case_n(text, len, "yes")
            || kbo_text_equals_ignore_case_n(text, len, "on")
            || kbo_text_equals_ignore_case_n(text, len, "enabled")) {
        *out_value = 1;
        return 1;
    }
    if (kbo_text_equals_ignore_case_n(text, len, "0")
            || kbo_text_equals_ignore_case_n(text, len, "false")
            || kbo_text_equals_ignore_case_n(text, len, "no")
            || kbo_text_equals_ignore_case_n(text, len, "off")
            || kbo_text_equals_ignore_case_n(text, len, "disabled")) {
        *out_value = 0;
        return 1;
    }
    return 0;
}

static int kbo_json_int_text_value(const char* text, size_t len, int* out_value)
{
    if (text == NULL || out_value == NULL || len == 0) {
        return 0;
    }

    int sign = 1;
    size_t i = 0;
    while (i < len && (text[i] == ' ' || text[i] == '\t' || text[i] == '\r' || text[i] == '\n')) {
        i++;
    }
    if (i >= len) {
        return 0;
    }
    if (text[i] == '-' || text[i] == '+') {
        sign = text[i] == '-' ? -1 : 1;
        i++;
    }
    if (i >= len || text[i] < '0' || text[i] > '9') {
        return 0;
    }

    long long value = 0;
    while (i < len && text[i] >= '0' && text[i] <= '9') {
        value = value * 10 + (long long)(text[i] - '0');
        if (value > 2147483647LL) {
            return 0;
        }
        i++;
    }
    while (i < len && (text[i] == ' ' || text[i] == '\t' || text[i] == '\r' || text[i] == '\n')) {
        i++;
    }
    if (i != len) {
        return 0;
    }

    value *= sign;
    if (value < -2147483647LL - 1LL || value > 2147483647LL) {
        return 0;
    }
    *out_value = (int)value;
    return 1;
}

static int kbo_json_bool_token_value(const char* json, const jsmntok_t* token, int* out_value)
{
    if (json == NULL || token == NULL || out_value == NULL || token->start < 0 || token->end < token->start) {
        return 0;
    }

    const char* start = json + token->start;
    size_t len = (size_t)(token->end - token->start);
    if (token->type == JSMN_STRING) {
        return kbo_json_bool_text_value(start, len, out_value);
    }
    if (token->type == JSMN_PRIMITIVE) {
        if (kbo_text_equals_ignore_case_n(start, len, "true")) {
            *out_value = 1;
            return 1;
        }
        if (kbo_text_equals_ignore_case_n(start, len, "false")) {
            *out_value = 0;
            return 1;
        }
        if (len > 0 && ((*start >= '0' && *start <= '9') || *start == '-' || *start == '+')) {
            int digit_seen = 0;
            int nonzero = 0;
            const char* p = start;
            const char* end = start + len;
            if (p < end && (*p == '-' || *p == '+')) {
                p++;
            }
            while (p < end && *p >= '0' && *p <= '9') {
                digit_seen = 1;
                if (*p != '0') {
                    nonzero = 1;
                }
                p++;
            }
            if (digit_seen && p == end) {
                *out_value = nonzero ? 1 : 0;
                return 1;
            }
        }
    }
    return 0;
}

static int kbo_json_int_token_value(const char* json, const jsmntok_t* token, int* out_value)
{
    if (json == NULL || token == NULL || out_value == NULL || token->start < 0 || token->end < token->start) {
        return 0;
    }
    if (token->type != JSMN_STRING && token->type != JSMN_PRIMITIVE) {
        return 0;
    }

    return kbo_json_int_text_value(json + token->start, (size_t)(token->end - token->start), out_value);
}

int kbo_json_bool_value_at(const char* value, const char* end, int* out_value)
{
    value = kbo_json_skip_ws(value, end);
    if (value >= end || out_value == NULL) {
        return 0;
    }

    jsmntok_t token = {0};
    jsmn_parser parser;
    jsmn_init(&parser);
    int parsed = jsmn_parse(&parser, value, (size_t)(end - value), &token, 1);
    return parsed == 1 && kbo_json_bool_token_value(value, &token, out_value);
}

int kbo_json_int_value_at(const char* value, const char* end, int* out_value)
{
    value = kbo_json_skip_ws(value, end);
    if (value >= end || out_value == NULL) {
        return 0;
    }

    jsmntok_t token = {0};
    jsmn_parser parser;
    jsmn_init(&parser);
    int parsed = jsmn_parse(&parser, value, (size_t)(end - value), &token, 1);
    return parsed == 1 && kbo_json_int_token_value(value, &token, out_value);
}

static int kbo_find_value_token_in_json(const char* json, DWORD json_size, const char* key, jsmntok_t* out_value)
{
    if (json == NULL || key == NULL || out_value == NULL) {
        return 0;
    }

    jsmntok_t tokens[KBO_JSON_MAX_TOKENS];
    int parsed = kbo_parse_json_tokens(json, json_size, tokens, KBO_JSON_MAX_TOKENS);
    if (parsed < 3) {
        return 0;
    }

    if (tokens[0].type != JSMN_OBJECT || tokens[0].size <= 0) {
        return 0;
    }

    int cursor = 1;
    while (cursor + 1 < parsed
            && tokens[cursor].start >= tokens[0].start
            && tokens[cursor].end <= tokens[0].end) {
        int key_index = cursor;
        int value_index = cursor + 1;
        if (tokens[key_index].type == JSMN_STRING && kbo_token_equals_key(json, &tokens[key_index], key)) {
            *out_value = tokens[value_index];
            return 1;
        }
        cursor = kbo_json_token_subtree_end(tokens, parsed, value_index);
    }
    return 0;
}

int kbo_find_flag_value_in_json(const char* json, DWORD json_size, const char* key, int* out_value)
{
    jsmntok_t value = {0};
    return kbo_find_value_token_in_json(json, json_size, key, &value)
        && kbo_json_bool_token_value(json, &value, out_value);
}

int kbo_find_int_value_in_json(const char* json, DWORD json_size, const char* key, int* out_value)
{
    jsmntok_t value = {0};
    return kbo_find_value_token_in_json(json, json_size, key, &value)
        && kbo_json_int_token_value(json, &value, out_value);
}

int kbo_find_string_value_in_json(const char* json, DWORD json_size, const char* key, char* out, size_t out_size)
{
    const char* start = NULL;
    const char* end = NULL;
    return kbo_find_json_value_span(json, json_size, key, &start, &end)
        && kbo_json_string_value_at(start, end, out, out_size);
}

int kbo_find_json_value_span(const char* json, DWORD json_size, const char* key, const char** out_start, const char** out_end)
{
    jsmntok_t value = {0};
    return kbo_find_value_token_in_json(json, json_size, key, &value)
        && kbo_json_token_span_value(json, &value, out_start, out_end);
}
