#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../../../core/files/save_paths/core_save_paths.h"
#include "../../../../core/dates/core_text_date.h"
#include "../../../state/league_lookup/api/state_league_lookup.h"
#include "support_logos.h"
#include "../names/support_names.h"
#include "../paths/ui_asset_paths.h"
#include "../../../../core/dates/constants/kbo_date_constants.h"

static void kbo_hub_sanitize_logo_prefix(const char* name, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';
    if (name == NULL) {
        return;
    }

    size_t write = 0;
    int last_was_sep = 1;
    for (size_t read = 0; name[read] != '\0' && write + 1 < out_size; read++) {
        char ch = name[read];
        if (kbo_hub_ascii_is_alnum(ch)) {
            out[write++] = ch;
            last_was_sep = 0;
        } else if (!last_was_sep) {
            out[write++] = '_';
            last_was_sep = 1;
        }
    }
    while (write > 0 && out[write - 1] == '_') {
        write--;
    }
    out[write] = '\0';
}

static int kbo_hub_parse_four_digits(const char* text, int index)
{
    int value = 0;
    for (int i = 0; i < 4; i++) {
        char ch = text[index + i];
        if (ch < '0' || ch > '9') {
            return -1;
        }
        value = (value * 10) + (ch - '0');
    }
    return value;
}

static int kbo_hub_logo_filename_score(const char* file_name, uint32_t year)
{
    if (file_name == NULL || file_name[0] == '\0') {
        return -1000000;
    }

    int has_year_token = 0;
    int best_year_score = year == 0 ? 0 : -100000;
    size_t len = strlen(file_name);
    for (size_t i = 0; i + 4 <= len; i++) {
        int start_year = kbo_hub_parse_four_digits(file_name, (int)i);
        if (start_year < (int)KBO_HISTORY_YEAR_MIN || start_year > (int)KBO_LOGO_YEAR_MAX) {
            continue;
        }
        has_year_token = 1;
        int score = -5000;
        if (i + 9 <= len && file_name[i + 4] == '-') {
            int end_year = kbo_hub_parse_four_digits(file_name, (int)i + 5);
            if (end_year >= start_year) {
                score = (year >= (uint32_t)start_year && year <= (uint32_t)end_year)
                    ? 10000 - abs((int)year - start_year) : -1000;
                i += 8;
            }
        } else if (year == (uint32_t)start_year) {
            score = 12000;
        }
        if (score > best_year_score) {
            best_year_score = score;
        }
    }

    int score = has_year_token ? best_year_score : 100;
    if (strstr(file_name, "_small_50") != NULL) { score += 700; }
    if (strstr(file_name, "_away")     != NULL
            || strstr(file_name, "_Away") != NULL) { score -= 500; }
    return score;
}

static int kbo_hub_find_best_logo_file(
    const char* directory, const char* search_prefix,
    uint32_t year, char* out, size_t out_size)
{
    if (directory == NULL || search_prefix == NULL || search_prefix[0] == '\0'
            || out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';

    char pattern[MAX_PATH];
    snprintf(pattern, sizeof(pattern), "%s\\%s*.*", directory, search_prefix);

    WIN32_FIND_DATAA find_data;
    HANDLE find = FindFirstFileA(pattern, &find_data);
    if (find == INVALID_HANDLE_VALUE) {
        return 0;
    }

    int best_score = -1000000;
    char best_path[MAX_PATH] = {0};
    do {
        if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            continue;
        }
        size_t name_len = strlen(find_data.cFileName);
        int is_png = name_len >= 4 && ascii_equals_ignore_case(find_data.cFileName + name_len - 4, ".png");
        int is_oi  = name_len >= 3 && ascii_equals_ignore_case(find_data.cFileName + name_len - 3, ".oi");
        if (!is_png && !is_oi) {
            continue;
        }
        int score = kbo_hub_logo_filename_score(find_data.cFileName, year);
        if (is_png) { score += 50; }
        if (score > best_score) {
            best_score = score;
            snprintf(best_path, sizeof(best_path), "%s\\%s", directory, find_data.cFileName);
        }
    } while (FindNextFileA(find, &find_data));

    FindClose(find);
    if (best_path[0] == '\0') {
        return 0;
    }
    snprintf(out, out_size, "%s", best_path);
    return 1;
}

static const char* kbo_hub_file_name_part(const char* path)
{
    if (path == NULL) {
        return NULL;
    }

    const char* name = path;
    for (const char* p = path; *p != '\0'; p++) {
        if (*p == '\\' || *p == '/') {
            name = p + 1;
        }
    }
    return name;
}

static void kbo_hub_copy_logo_prefix_from_file_name(const char* file_name, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';
    if (file_name == NULL || file_name[0] == '\0') {
        return;
    }

    snprintf(out, out_size, "%s", file_name);
    char* dot = strrchr(out, '.');
    if (dot != NULL) {
        *dot = '\0';
    }
}

static int kbo_hub_copy_existing_file(const char* path, char* out, size_t out_size)
{
    if (path == NULL || path[0] == '\0' || out == NULL || out_size == 0) {
        return 0;
    }

    DWORD attrs = GetFileAttributesA(path);
    if (attrs == INVALID_FILE_ATTRIBUTES || (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        return 0;
    }

    snprintf(out, out_size, "%s", path);
    return 1;
}

static int kbo_hub_find_cached_league_logo_in_dir(
    const char* directory,
    const char* logo_file,
    uint32_t year,
    char* out,
    size_t out_size)
{
    if (directory == NULL || directory[0] == '\0'
            || logo_file == NULL || logo_file[0] == '\0'
            || out == NULL || out_size == 0) {
        return 0;
    }

    char path[MAX_PATH] = {0};
    snprintf(path, sizeof(path), "%s\\%s", directory, logo_file);
    if (kbo_hub_copy_existing_file(path, out, out_size)) {
        return 1;
    }

    char prefix[128] = {0};
    kbo_hub_copy_logo_prefix_from_file_name(logo_file, prefix, sizeof(prefix));
    if (prefix[0] == '\0') {
        return 0;
    }

    snprintf(path, sizeof(path), "%s\\%s.oi", directory, prefix);
    if (kbo_hub_copy_existing_file(path, out, out_size)) {
        return 1;
    }

    return kbo_hub_find_best_logo_file(directory, prefix, year, out, out_size);
}

int kbo_hub_get_league_logo_path(uint32_t league_id, uint32_t year, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';

    char cached_logo_file_raw[128] = {0};
    if (!kbo_hub_try_copy_cached_league_logo_file(league_id, cached_logo_file_raw, sizeof(cached_logo_file_raw))) {
        return 0;
    }

    const char* cached_logo_file = kbo_hub_file_name_part(cached_logo_file_raw);
    if (cached_logo_file == NULL || cached_logo_file[0] == '\0') {
        return 0;
    }

    char save_path[MAX_PATH] = {0};
    if (kbo_get_current_save_path(save_path, sizeof(save_path))) {
        char save_logo_dir[MAX_PATH] = {0};
        snprintf(save_logo_dir, sizeof(save_logo_dir), "%s\\news\\html\\images\\league_logos", save_path);
        if (kbo_hub_find_cached_league_logo_in_dir(save_logo_dir, cached_logo_file, year, out, out_size)) {
            return 1;
        }
    }

    char data_logo_dir[MAX_PATH] = {0};
    kbo_hub_ootp_install_path("data\\logos", data_logo_dir, sizeof(data_logo_dir));
    if (kbo_hub_find_cached_league_logo_in_dir(data_logo_dir, cached_logo_file, year, out, out_size)) {
        return 1;
    }

    return 0;
}

int kbo_hub_get_team_logo_path(uint32_t team_id, uint32_t year, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';

    char team_name[96] = {0};
    char prefix[96] = {0};
    kbo_hub_copy_team_display_name_by_id(team_id, team_name, sizeof(team_name), NULL);
    kbo_hub_sanitize_logo_prefix(team_name, prefix, sizeof(prefix));
    if (prefix[0] == '\0') {
        return 0;
    }

    char data_logo_dir[MAX_PATH] = {0};
    kbo_hub_ootp_install_path("data\\logos", data_logo_dir, sizeof(data_logo_dir));
    if (kbo_hub_find_best_logo_file(data_logo_dir, prefix, year, out, out_size)) {
        return 1;
    }

    char cap_prefix[112] = {0};
    snprintf(cap_prefix, sizeof(cap_prefix), "caps_%s", prefix);
    char ballcap_dir[MAX_PATH] = {0};
    kbo_hub_ootp_install_path("data\\ballcaps", ballcap_dir, sizeof(ballcap_dir));
    return kbo_hub_find_best_logo_file(ballcap_dir, cap_prefix, year, out, out_size);
}

