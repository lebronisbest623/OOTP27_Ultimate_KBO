#include "core_news_templates.h"
#include "core_news_templates_internal.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../core_flags/api/flags_api.h"
#include "../../files/save_paths/core_save_paths.h"
#include "../../logging/core_log.h"

static const char* kbo_news_template_normalize_language_dir(const char* language_dir)
{
    return language_dir != NULL && strcmp(language_dir, "en") == 0 ? "en" : "ko";
}

static int kbo_text_resource_save_path_for_language(
    const char* resource_dir,
    const char* language_dir,
    const char* file_name,
    char* out,
    size_t out_size)
{
    if (resource_dir == NULL || resource_dir[0] == '\0' || file_name == NULL || out == NULL || out_size == 0u) {
        return 0;
    }
    char relative[160] = {0};
    snprintf(
        relative,
        sizeof(relative),
        "config\\%s\\%s\\%s",
        resource_dir,
        kbo_news_template_normalize_language_dir(language_dir),
        file_name);
    return kbo_get_save_scoped_data_file(relative, out, out_size);
}

static int kbo_news_template_save_split_path_for_language(
    const char* language_dir,
    const char* file_name,
    char* out,
    size_t out_size)
{
    return kbo_text_resource_save_path_for_language(
        KBO_NEWS_TEMPLATES_DIR,
        language_dir,
        file_name,
        out,
        out_size);
}

static int kbo_news_template_save_split_path(const char* file_name, char* out, size_t out_size)
{
    return kbo_news_template_save_split_path_for_language(
        kbo_custom_news_language_dir(),
        file_name,
        out,
        out_size);
}

static int kbo_news_template_global_file_path(const char* file_name, char* out, size_t out_size)
{
    if (out == NULL || out_size < 2u) {
        return 0;
    }
    out[0] = '\0';
    return kbo_get_global_data_file(file_name, out, out_size);
}

static int kbo_text_resource_global_path_for_language(
    const char* resource_dir,
    const char* language_dir,
    const char* file_name,
    char* out,
    size_t out_size)
{
    if (resource_dir == NULL || resource_dir[0] == '\0' || file_name == NULL || out == NULL || out_size == 0u) {
        return 0;
    }
    char relative[160] = {0};
    snprintf(
        relative,
        sizeof(relative),
        "%s\\%s\\%s",
        resource_dir,
        kbo_news_template_normalize_language_dir(language_dir),
        file_name);
    return kbo_news_template_global_file_path(relative, out, out_size);
}

static int kbo_news_template_global_split_path_for_language(
    const char* language_dir,
    const char* file_name,
    char* out,
    size_t out_size)
{
    return kbo_text_resource_global_path_for_language(
        KBO_NEWS_TEMPLATES_DIR,
        language_dir,
        file_name,
        out,
        out_size);
}

static int kbo_news_template_global_split_path(const char* file_name, char* out, size_t out_size)
{
    return kbo_news_template_global_split_path_for_language(
        kbo_custom_news_language_dir(),
        file_name,
        out,
        out_size);
}

static int kbo_news_template_try_load_int(const char* key, int* out_value)
{
    if (key == NULL || out_value == NULL) {
        return 0;
    }
    *out_value = 0;

    char split_file[96] = {0};
    if (kbo_news_template_file_for_key(key, split_file, sizeof(split_file), "template_variant")) {
        char save_split_path[MAX_PATH] = {0};
        char global_split_path[MAX_PATH] = {0};
        if (kbo_news_template_save_split_path(split_file, save_split_path, sizeof(save_split_path))
                && kbo_news_template_file_exists(save_split_path)
                && kbo_news_template_load_int_from_path(save_split_path, key, out_value)) {
            return 1;
        }
        if (kbo_news_template_global_split_path(split_file, global_split_path, sizeof(global_split_path))
                && kbo_news_template_file_exists(global_split_path)
                && kbo_news_template_load_int_from_path(global_split_path, key, out_value)) {
            return 1;
        }
    }
    return 0;
}

int kbo_news_template_load_for_language(
    const char* language_dir,
    const char* key,
    char* out,
    size_t out_size,
    char* source_path,
    size_t source_path_size,
    const char* source)
{
    if (out == NULL || out_size == 0u || key == NULL || key[0] == '\0') {
        return 0;
    }
    out[0] = '\0';
    if (source_path != NULL && source_path_size > 0u) {
        source_path[0] = '\0';
    }

    char split_file[96] = {0};
    char save_split_path[MAX_PATH] = {0};
    char global_split_path[MAX_PATH] = {0};
    int has_save_split_path = 0;
    int has_global_split_path = 0;
    const char* normalized_language_dir = kbo_news_template_normalize_language_dir(language_dir);
    if (kbo_news_template_file_for_key(key, split_file, sizeof(split_file), source)) {
        has_save_split_path = kbo_news_template_save_split_path_for_language(
            normalized_language_dir,
            split_file,
            save_split_path,
            sizeof(save_split_path));
        has_global_split_path = kbo_news_template_global_split_path_for_language(
            normalized_language_dir,
            split_file,
            global_split_path,
            sizeof(global_split_path));
        if (has_save_split_path && kbo_news_template_file_exists(save_split_path)) {
            if (kbo_news_template_try_load_from_existing_path(
                    save_split_path,
                    key,
                    out,
                    out_size,
                    source_path,
                    source_path_size)) {
                return 1;
            }
            kbo_log_runtimef(
                "KBO news template missing or invalid source=%s scope=save_split key=%s path=%s",
                source != NULL ? source : "",
                key,
                save_split_path);
        }
        if (has_global_split_path && kbo_news_template_file_exists(global_split_path)) {
            if (kbo_news_template_try_load_from_existing_path(
                    global_split_path,
                    key,
                    out,
                    out_size,
                    source_path,
                    source_path_size)) {
                return 1;
            }
            kbo_log_runtimef(
                "KBO news template missing or invalid source=%s scope=global_split key=%s path=%s",
                source != NULL ? source : "",
                key,
                global_split_path);
        }
    }

    kbo_log_runtimef(
        "KBO news template unavailable source=%s key=%s lang=%s save_split=%s global_split=%s",
        source != NULL ? source : "",
        key,
        normalized_language_dir,
        has_save_split_path ? save_split_path : "",
        has_global_split_path ? global_split_path : "");
    return 0;
}

int kbo_text_resource_load_for_language(
    const char* resource_dir,
    const char* language_dir,
    const char* file_name,
    const char* key,
    char* out,
    size_t out_size,
    char* source_path,
    size_t source_path_size,
    const char* source)
{
    if (out == NULL || out_size == 0u || resource_dir == NULL || resource_dir[0] == '\0'
            || file_name == NULL || file_name[0] == '\0' || key == NULL || key[0] == '\0') {
        return 0;
    }
    out[0] = '\0';
    if (source_path != NULL && source_path_size > 0u) {
        source_path[0] = '\0';
    }

    char save_path[MAX_PATH] = {0};
    char global_path[MAX_PATH] = {0};
    const char* normalized_language_dir = kbo_news_template_normalize_language_dir(language_dir);
    int has_save_path = kbo_text_resource_save_path_for_language(
        resource_dir,
        normalized_language_dir,
        file_name,
        save_path,
        sizeof(save_path));
    int has_global_path = kbo_text_resource_global_path_for_language(
        resource_dir,
        normalized_language_dir,
        file_name,
        global_path,
        sizeof(global_path));

    if (has_save_path && kbo_news_template_file_exists(save_path)) {
        if (kbo_news_template_try_load_from_existing_path(
                save_path,
                key,
                out,
                out_size,
                source_path,
                source_path_size)) {
            return 1;
        }
        kbo_log_runtimef(
            "KBO text resource missing or invalid source=%s scope=save key=%s path=%s",
            source != NULL ? source : "",
            key,
            save_path);
    }
    if (has_global_path && kbo_news_template_file_exists(global_path)) {
        if (kbo_news_template_try_load_from_existing_path(
                global_path,
                key,
                out,
                out_size,
                source_path,
                source_path_size)) {
            return 1;
        }
        kbo_log_runtimef(
            "KBO text resource missing or invalid source=%s scope=global key=%s path=%s",
            source != NULL ? source : "",
            key,
            global_path);
    }
    return 0;
}

int kbo_news_template_load(
    const char* key,
    char* out,
    size_t out_size,
    char* source_path,
    size_t source_path_size,
    const char* source)
{
    return kbo_news_template_load_for_language(kbo_custom_news_language_dir(), key, out, out_size, source_path, source_path_size, source);
}

static uint32_t kbo_news_template_hash_text(uint32_t hash, const char* text)
{
    if (text == NULL) {
        text = "";
    }
    while (*text != '\0') {
        hash ^= (uint8_t)*text;
        hash *= 16777619u;
        text++;
    }
    return hash;
}

static uint32_t kbo_news_template_variant_seed(const char* key, const KboNewsTemplateVar* vars, int var_count)
{
    uint32_t hash = 2166136261u;
    hash = kbo_news_template_hash_text(hash, key);
    hash = kbo_news_template_hash_text(hash, "|");
    if (vars == NULL || var_count <= 0) {
        return hash;
    }
    for (int i = 0; i < var_count; i++) {
        hash = kbo_news_template_hash_text(hash, vars[i].key);
        hash = kbo_news_template_hash_text(hash, "=");
        hash = kbo_news_template_hash_text(hash, vars[i].value);
        hash = kbo_news_template_hash_text(hash, ";");
    }
    return hash;
}

static int kbo_news_template_variant_key(const char* key, const KboNewsTemplateVar* vars, int var_count, char* out, size_t out_size)
{
    if (key == NULL || out == NULL || out_size == 0u) {
        return 0;
    }
    out[0] = '\0';

    char count_key[192] = {0};
    snprintf(count_key, sizeof(count_key), "%s.variants", key);

    int variant_count = 0;
    if (!kbo_news_template_try_load_int(count_key, &variant_count) || variant_count <= 1) {
        return 0;
    }
    if (variant_count > 16) {
        variant_count = 16;
    }

    uint32_t seed = kbo_news_template_variant_seed(key, vars, var_count);
    int variant = (int)(seed % (uint32_t)variant_count) + 1;
    snprintf(out, out_size, "%s.v%d", key, variant);
    return out[0] != '\0';
}

int kbo_news_template_render_key(
    const char* key,
    const KboNewsTemplateVar* vars,
    int var_count,
    char* out,
    size_t out_size,
    const char* source)
{
    return kbo_news_template_render_key_with_source(key, vars, var_count, out, out_size, NULL, 0u, source);
}

int kbo_news_template_render_key_with_source(
    const char* key,
    const KboNewsTemplateVar* vars,
    int var_count,
    char* out,
    size_t out_size,
    char* source_path,
    size_t source_path_size,
    const char* source)
{
    char tmpl[8192] = {0};
    char variant_key[192] = {0};
    if (source_path != NULL && source_path_size > 0u) {
        source_path[0] = '\0';
    }
    if (kbo_news_template_variant_key(key, vars, var_count, variant_key, sizeof(variant_key))
            && kbo_news_template_load(variant_key, tmpl, sizeof(tmpl), source_path, source_path_size, source)) {
        return kbo_news_template_render(tmpl, vars, var_count, out, out_size);
    }
    if (!kbo_news_template_load(key, tmpl, sizeof(tmpl), source_path, source_path_size, source)) {
        if (out != NULL && out_size > 0u) {
            out[0] = '\0';
        }
        return 0;
    }
    return kbo_news_template_render(tmpl, vars, var_count, out, out_size);
}
