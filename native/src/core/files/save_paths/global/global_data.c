/* Global LocalAppData\OOTP-KBO data directory and file helpers. */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define JSMN_STATIC
#include "../../../../../third_party/jsmn/jsmn.h"

#include "../core_save_paths.h"
#include "../core_save_paths_internal.h"
#include "../../../product/ootp_product.h"

#define KBO_DATA_BUNDLE_MAX_BYTES (8u * 1024u * 1024u)
#define KBO_DATA_BUNDLE_MAX_TOKENS 512

static int kbo_global_file_exists(const char* path)
{
    DWORD attributes = kbo_get_file_attributes_utf8(path);
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

static int kbo_data_bundle_key_equals(const char* json, const jsmntok_t* token, const char* file_name)
{
    if (json == NULL || token == NULL || file_name == NULL || token->type != JSMN_STRING
            || token->start < 0 || token->end < token->start) {
        return 0;
    }
    const char* key = json + token->start;
    size_t key_len = (size_t)(token->end - token->start);
    size_t name_len = strlen(file_name);
    if (key_len != name_len) {
        return 0;
    }
    for (size_t i = 0; i < key_len; i++) {
        char a = key[i] == '/' ? '\\' : key[i];
        char b = file_name[i] == '/' ? '\\' : file_name[i];
        if (a >= 'A' && a <= 'Z') { a = (char)(a + ('a' - 'A')); }
        if (b >= 'A' && b <= 'Z') { b = (char)(b + ('a' - 'A')); }
        if (a != b) {
            return 0;
        }
    }
    return 1;
}

static int kbo_data_bundle_read_file(const char* path, char** out_data, DWORD* out_size)
{
    if (path == NULL || out_data == NULL || out_size == NULL) {
        return 0;
    }
    *out_data = NULL;
    *out_size = 0;

    HANDLE file = kbo_create_file_read_utf8(path);
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }
    LARGE_INTEGER size = {0};
    if (!GetFileSizeEx(file, &size) || size.QuadPart <= 0 || size.QuadPart > KBO_DATA_BUNDLE_MAX_BYTES) {
        CloseHandle(file);
        return 0;
    }
    char* data = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)size.QuadPart + 1u);
    if (data == NULL) {
        CloseHandle(file);
        return 0;
    }
    DWORD read = 0;
    BOOL ok = ReadFile(file, data, (DWORD)size.QuadPart, &read, NULL);
    CloseHandle(file);
    if (!ok || read != (DWORD)size.QuadPart) {
        HeapFree(GetProcessHeap(), 0, data);
        return 0;
    }
    data[read] = '\0';
    *out_data = data;
    *out_size = read;
    return 1;
}

static int kbo_data_bundle_token_subtree_end(const jsmntok_t* tokens, int parsed, int index)
{
    int cursor = index + 1;
    while (cursor < parsed
            && tokens[cursor].start >= tokens[index].start
            && tokens[cursor].end <= tokens[index].end) {
        cursor++;
    }
    return cursor;
}

static int kbo_data_bundle_hex_value(char c)
{
    if (c >= '0' && c <= '9') { return c - '0'; }
    if (c >= 'a' && c <= 'f') { return 10 + c - 'a'; }
    if (c >= 'A' && c <= 'F') { return 10 + c - 'A'; }
    return -1;
}

static char* kbo_data_bundle_unescape_json_string(const char* start, const char* end, DWORD* out_size)
{
    if (start == NULL || end == NULL || end < start || out_size == NULL) {
        return NULL;
    }
    size_t input_len = (size_t)(end - start);
    char* out = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, input_len * 4u + 1u);
    if (out == NULL) {
        return NULL;
    }
    char* w = out;
    const char* p = start;
    while (p < end) {
        unsigned char c = (unsigned char)*p++;
        if (c != '\\' || p >= end) {
            *w++ = (char)c;
            continue;
        }
        char esc = *p++;
        switch (esc) {
            case '"': *w++ = '"'; break;
            case '\\': *w++ = '\\'; break;
            case '/': *w++ = '/'; break;
            case 'b': *w++ = '\b'; break;
            case 'f': *w++ = '\f'; break;
            case 'n': *w++ = '\n'; break;
            case 'r': *w++ = '\r'; break;
            case 't': *w++ = '\t'; break;
            case 'u': {
                if (p + 4 > end) { HeapFree(GetProcessHeap(), 0, out); return NULL; }
                int h0 = kbo_data_bundle_hex_value(p[0]);
                int h1 = kbo_data_bundle_hex_value(p[1]);
                int h2 = kbo_data_bundle_hex_value(p[2]);
                int h3 = kbo_data_bundle_hex_value(p[3]);
                if (h0 < 0 || h1 < 0 || h2 < 0 || h3 < 0) { HeapFree(GetProcessHeap(), 0, out); return NULL; }
                unsigned int code = (unsigned int)((h0 << 12) | (h1 << 8) | (h2 << 4) | h3);
                p += 4;
                if (code < 0x80u) {
                    *w++ = (char)code;
                } else if (code < 0x800u) {
                    *w++ = (char)(0xC0u | (code >> 6));
                    *w++ = (char)(0x80u | (code & 0x3Fu));
                } else {
                    *w++ = (char)(0xE0u | (code >> 12));
                    *w++ = (char)(0x80u | ((code >> 6) & 0x3Fu));
                    *w++ = (char)(0x80u | (code & 0x3Fu));
                }
                break;
            }
            default:
                *w++ = esc;
                break;
        }
    }
    *w = '\0';
    *out_size = (DWORD)(w - out);
    return out;
}

static int kbo_data_bundle_write_file(const char* path, const char* data, DWORD size)
{
    HANDLE file = kbo_create_file_utf8(
        path,
        GENERIC_WRITE,
        FILE_SHARE_READ,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL);
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }
    DWORD wrote = 0;
    BOOL ok = WriteFile(file, data, size, &wrote, NULL);
    CloseHandle(file);
    return ok && wrote == size;
}

static void kbo_data_bundle_create_parent_dirs(char* path)
{
    if (path == NULL) {
        return;
    }
    for (char* p = path; *p != '\0'; p++) {
        if (*p != '\\' && *p != '/') {
            continue;
        }
        char saved = *p;
        *p = '\0';
        if (strlen(path) > 2u) {
            kbo_create_directory_utf8(path);
        }
        *p = saved;
    }
}

static int kbo_data_bundle_extract_file(const char* dir, const char* file_name, char* out, size_t out_size)
{
    char bundle_path[KBO_UTF8_PATH_BYTES] = {0};
    snprintf(bundle_path, sizeof(bundle_path), "%s\\" KBO_PRODUCT_DATA_BUNDLE_FILE, dir);
    if (!kbo_global_file_exists(bundle_path)) {
        return 0;
    }

    char* json = NULL;
    DWORD json_size = 0;
    if (!kbo_data_bundle_read_file(bundle_path, &json, &json_size)) {
        return 0;
    }

    jsmntok_t tokens[KBO_DATA_BUNDLE_MAX_TOKENS];
    jsmn_parser parser;
    jsmn_init(&parser);
    int parsed = jsmn_parse(&parser, json, (size_t)json_size, tokens, KBO_DATA_BUNDLE_MAX_TOKENS);
    if (parsed <= 0 || tokens[0].type != JSMN_OBJECT) {
        HeapFree(GetProcessHeap(), 0, json);
        return 0;
    }

    const jsmntok_t* value = NULL;
    for (int i = 1; i + 1 < parsed; ) {
        jsmntok_t* key = &tokens[i];
        jsmntok_t* candidate = &tokens[i + 1];
        int next = kbo_data_bundle_token_subtree_end(tokens, parsed, i + 1);
        if (key->type == JSMN_STRING
                && (int)(key->end - key->start) == 5
                && memcmp(json + key->start, "files", 5) == 0
                && candidate->type == JSMN_OBJECT) {
            for (int f = i + 2; f + 1 < next; ) {
                jsmntok_t* file_key = &tokens[f];
                jsmntok_t* file_value = &tokens[f + 1];
                int file_next = kbo_data_bundle_token_subtree_end(tokens, parsed, f + 1);
                if (kbo_data_bundle_key_equals(json, file_key, file_name)
                        && file_value->type == JSMN_STRING) {
                    value = file_value;
                    break;
                }
                f = file_next;
            }
            break;
        }
        i = next;
    }
    if (value == NULL) {
        HeapFree(GetProcessHeap(), 0, json);
        return 0;
    }

    DWORD content_size = 0;
    char* content = kbo_data_bundle_unescape_json_string(json + value->start, json + value->end, &content_size);
    HeapFree(GetProcessHeap(), 0, json);
    if (content == NULL) {
        return 0;
    }

    WCHAR temp_w[KBO_WIDE_PATH_CHARS] = {0};
    DWORD temp_len = GetTempPathW(KBO_WIDE_PATH_CHARS, temp_w);
    char temp[KBO_UTF8_PATH_BYTES] = {0};
    if (temp_len == 0 || temp_len >= KBO_WIDE_PATH_CHARS || !kbo_wide_to_utf8_path(temp_w, temp, sizeof(temp))) {
        HeapFree(GetProcessHeap(), 0, content);
        return 0;
    }

    char cache_path[KBO_UTF8_PATH_BYTES] = {0};
    snprintf(cache_path, sizeof(cache_path), "%sOOTP-KBO-bundle-cache\\%s", temp, file_name);
    for (char* p = cache_path; *p != '\0'; p++) {
        if (*p == '/') { *p = '\\'; }
    }
    kbo_data_bundle_create_parent_dirs(cache_path);
    int wrote = kbo_data_bundle_write_file(cache_path, content, content_size);
    HeapFree(GetProcessHeap(), 0, content);
    if (!wrote) {
        return 0;
    }

    snprintf(out, out_size, "%s", cache_path);
    return out[0] != '\0';
}

int kbo_get_global_data_dir(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return 0;
    }
    out[0] = '\0';

    char local_app_data[KBO_UTF8_PATH_BYTES] = {0};
    if (!kbo_get_localappdata_utf8(local_app_data, sizeof(local_app_data))) {
        return 0;
    }

    snprintf(out, out_size, "%s\\" KBO_PRODUCT_LOCAL_DATA_DIR, local_app_data);
    if (out[0] == '\0') {
        return 0;
    }
    kbo_create_directory_utf8(out);
    return 1;
}

int kbo_get_global_data_file(const char* file_name, char* out, size_t out_size)
{
    if (file_name == NULL || file_name[0] == '\0' || out == NULL || out_size == 0u) {
        return 0;
    }
    out[0] = '\0';

    char dir[KBO_UTF8_PATH_BYTES] = {0};
    if (!kbo_get_global_data_dir(dir, sizeof(dir))) {
        return 0;
    }

    char config_path[KBO_UTF8_PATH_BYTES] = {0};
    snprintf(config_path, sizeof(config_path), "%s\\config\\%s", dir, file_name);
    if (config_path[0] != '\0' && kbo_global_file_exists(config_path)) {
        snprintf(out, out_size, "%s", config_path);
        return out[0] != '\0';
    }

    if (kbo_data_bundle_extract_file(dir, file_name, out, out_size)) {
        return 1;
    }

    snprintf(out, out_size, "%s\\%s", dir, file_name);
    return out[0] != '\0';
}

int kbo_get_global_data_subdir(const char* dir_name, char* out, size_t out_size)
{
    if (dir_name == NULL || dir_name[0] == '\0' || out == NULL || out_size == 0u) {
        return 0;
    }
    out[0] = '\0';

    char root[KBO_UTF8_PATH_BYTES] = {0};
    if (!kbo_get_global_data_dir(root, sizeof(root))) {
        return 0;
    }

    snprintf(out, out_size, "%s\\%s", root, dir_name);
    if (out[0] == '\0') {
        return 0;
    }
    kbo_create_directory_utf8(out);
    return 1;
}
