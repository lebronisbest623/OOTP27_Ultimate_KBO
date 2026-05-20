#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../core_news_templates_internal.h"

#include <stdio.h>
#include <string.h>

#include "../../../core_flags/json/json_bool_parser.h"
#include "../../../files/save_paths/core_save_paths.h"
#include "../../../logging/core_log.h"

#define KBO_NEWS_TEMPLATE_INDEX_FILE "index.json"

static int kbo_news_template_starts_with(const char* text, const char* prefix)
{
    if (text == NULL || prefix == NULL) {
        return 0;
    }
    size_t prefix_len = strlen(prefix);
    return strncmp(text, prefix, prefix_len) == 0;
}

static int kbo_news_template_index_save_path(char* out, size_t out_size)
{
    return kbo_get_save_scoped_data_file("config\\news_templates\\" KBO_NEWS_TEMPLATE_INDEX_FILE, out, out_size);
}

static int kbo_news_template_index_global_path(char* out, size_t out_size)
{
    return kbo_get_global_data_file("news_templates\\" KBO_NEWS_TEMPLATE_INDEX_FILE, out, out_size);
}

static int kbo_news_template_index_scan_json(
    const char* json,
    DWORD json_size,
    const char* key,
    char* out,
    size_t out_size)
{
    if (json == NULL || json_size == 0u || key == NULL || out == NULL || out_size == 0u) {
        return 0;
    }
    out[0] = '\0';

    const char* p = json;
    const char* end = json + json_size;
    if (json_size >= 3u
            && (unsigned char)json[0] == 0xEFu
            && (unsigned char)json[1] == 0xBBu
            && (unsigned char)json[2] == 0xBFu) {
        p += 3;
    }
    p = kbo_json_skip_ws(p, end);
    if (p >= end || *p != '{') {
        return 0;
    }
    p++;

    size_t best_prefix_len = 0u;
    while (p < end) {
        p = kbo_json_skip_ws(p, end);
        if (p < end && *p == ',') {
            p++;
            p = kbo_json_skip_ws(p, end);
        }
        if (p >= end || *p == '}') {
            break;
        }
        if (*p != '"') {
            return 0;
        }

        const char* prefix_quote = p;
        const char* prefix_stop = kbo_json_find_string_end(prefix_quote, end);
        if (prefix_stop == NULL) {
            return 0;
        }
        char prefix[96] = {0};
        if (!kbo_json_string_value_at(prefix_quote, prefix_stop + 1, prefix, sizeof(prefix))) {
            return 0;
        }

        p = kbo_json_skip_ws(prefix_stop + 1, end);
        if (p >= end || *p != ':') {
            return 0;
        }
        p = kbo_json_skip_ws(p + 1, end);
        if (p >= end || *p != '"') {
            return 0;
        }

        const char* file_quote = p;
        const char* file_stop = kbo_json_find_string_end(file_quote, end);
        if (file_stop == NULL) {
            return 0;
        }
        char file_name[96] = {0};
        if (!kbo_json_string_value_at(file_quote, file_stop + 1, file_name, sizeof(file_name))) {
            return 0;
        }

        size_t prefix_len = strlen(prefix);
        if (prefix_len > best_prefix_len && kbo_news_template_starts_with(key, prefix)) {
            snprintf(out, out_size, "%s", file_name);
            best_prefix_len = prefix_len;
        }
        p = file_stop + 1;
    }
    return best_prefix_len > 0u && out[0] != '\0';
}

static int kbo_news_template_file_for_key_from_path(
    const char* path,
    const char* key,
    char* out,
    size_t out_size)
{
    DWORD json_size = 0u;
    char* json = kbo_news_template_read_json_file(path, &json_size);
    if (json == NULL) {
        return 0;
    }
    int ok = kbo_news_template_index_scan_json(json, json_size, key, out, out_size);
    HeapFree(GetProcessHeap(), 0, json);
    return ok;
}

int kbo_news_template_file_for_key(
    const char* key,
    char* out,
    size_t out_size,
    const char* source)
{
    if (key == NULL || key[0] == '\0' || out == NULL || out_size == 0u) {
        return 0;
    }
    out[0] = '\0';

    char save_index_path[MAX_PATH] = {0};
    char global_index_path[MAX_PATH] = {0};
    int has_save_index = kbo_news_template_index_save_path(save_index_path, sizeof(save_index_path));
    int has_global_index = kbo_news_template_index_global_path(global_index_path, sizeof(global_index_path));

    if (has_save_index && kbo_news_template_file_exists(save_index_path)) {
        if (kbo_news_template_file_for_key_from_path(save_index_path, key, out, out_size)) {
            return 1;
        }
        kbo_log_runtimef(
            "KBO news template index invalid source=%s scope=save key=%s path=%s",
            source != NULL ? source : "",
            key,
            save_index_path);
    }
    if (has_global_index && kbo_news_template_file_exists(global_index_path)) {
        if (kbo_news_template_file_for_key_from_path(global_index_path, key, out, out_size)) {
            return 1;
        }
        kbo_log_runtimef(
            "KBO news template index invalid source=%s scope=global key=%s path=%s",
            source != NULL ? source : "",
            key,
            global_index_path);
    }

    kbo_log_runtimef(
        "KBO news template index missing source=%s key=%s save_index=%s global_index=%s",
        source != NULL ? source : "",
        key,
        has_save_index ? save_index_path : "",
        has_global_index ? global_index_path : "");
    return 0;
}
