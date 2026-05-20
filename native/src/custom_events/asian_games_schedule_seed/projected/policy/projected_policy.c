#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "projected_policy.h"

#include <stdio.h>
#include <string.h>

#include "../../../../core/core_flags/json/json_bool_parser.h"
#include "../../../../core/files/save_paths/core_save_paths.h"
#include "../../../../core/dates/constants/kbo_date_constants.h"

#define KBO_ASIAN_GAMES_PROJECTED_POLICY_FILE "asian_games_projected_policy.json"

static void kbo_asian_games_projected_policy_defaults(KboAsianGamesProjectedPolicy* out)
{
    memset(out, 0, sizeof(*out));
    out->projected_start_year = 2039u;
    out->projected_end_year = KBO_SIM_YEAR_MAX;
    out->cycle_years = 4u;
    out->start_day_base = 10u;
    out->start_day_span = 19u;
    out->duration_days_base = 14u;
    out->duration_days_span = 5u;
    out->selection_lead_days_base = 42u;
    out->selection_lead_days_span = 18u;
    out->selection_fallback_month_day = 805u;
    out->start_day_hash_salt = 31u;
    out->duration_hash_salt = 43u;
    out->selection_lead_hash_salt = 59u;
    out->host_hash_salt = 17u;
    out->host_max_count = 64u;
    out->host_city_cooldown_years = 24u;
    out->host_country_cooldown_years = 16u;
    out->host_fallback_city_cooldown_years = 24u;
    out->host_fallback_country_cooldown_years = 0u;
    snprintf(out->projected_status, sizeof(out->projected_status), "%s", "projected");
}

static int kbo_asian_games_projected_policy_global_path(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2u) {
        return 0;
    }
    out[0] = '\0';
    return kbo_get_global_data_file(KBO_ASIAN_GAMES_PROJECTED_POLICY_FILE, out, out_size);
}

static int kbo_asian_games_projected_policy_save_path(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2u) {
        return 0;
    }
    out[0] = '\0';
    return kbo_get_save_scoped_data_file(KBO_ASIAN_GAMES_PROJECTED_POLICY_FILE, out, out_size);
}

static void kbo_asian_games_projected_policy_parse_value(
    const char* json,
    DWORD json_size,
    const char* key,
    uint32_t min_value,
    uint32_t max_value,
    uint32_t* out_value)
{
    int value = 0;
    if (out_value == NULL || !kbo_find_int_value_in_json(json, json_size, key, &value)) {
        return;
    }
    if (value < (int)min_value || value > (int)max_value) {
        return;
    }
    *out_value = (uint32_t)value;
}

static void kbo_asian_games_projected_policy_parse(KboAsianGamesProjectedPolicy* out, const char* json, DWORD json_size)
{
    kbo_asian_games_projected_policy_parse_value(json, json_size, "projected_start_year", KBO_SEASON_YEAR_MIN, KBO_POLICY_YEAR_MAX, &out->projected_start_year);
    kbo_asian_games_projected_policy_parse_value(json, json_size, "projected_end_year", KBO_SEASON_YEAR_MIN, KBO_POLICY_YEAR_MAX, &out->projected_end_year);
    kbo_asian_games_projected_policy_parse_value(json, json_size, "cycle_years", 1u, 16u, &out->cycle_years);
    kbo_asian_games_projected_policy_parse_value(json, json_size, "start_day_base", 1u, 28u, &out->start_day_base);
    kbo_asian_games_projected_policy_parse_value(json, json_size, "start_day_span", 1u, 28u, &out->start_day_span);
    kbo_asian_games_projected_policy_parse_value(json, json_size, "duration_days_base", 1u, 60u, &out->duration_days_base);
    kbo_asian_games_projected_policy_parse_value(json, json_size, "duration_days_span", 1u, 60u, &out->duration_days_span);
    kbo_asian_games_projected_policy_parse_value(json, json_size, "selection_lead_days_base", 1u, 365u, &out->selection_lead_days_base);
    kbo_asian_games_projected_policy_parse_value(json, json_size, "selection_lead_days_span", 1u, 365u, &out->selection_lead_days_span);
    kbo_asian_games_projected_policy_parse_value(json, json_size, "selection_fallback_month_day", 101u, 1231u, &out->selection_fallback_month_day);
    kbo_asian_games_projected_policy_parse_value(json, json_size, "start_day_hash_salt", 0u, 100000u, &out->start_day_hash_salt);
    kbo_asian_games_projected_policy_parse_value(json, json_size, "duration_hash_salt", 0u, 100000u, &out->duration_hash_salt);
    kbo_asian_games_projected_policy_parse_value(json, json_size, "selection_lead_hash_salt", 0u, 100000u, &out->selection_lead_hash_salt);
    kbo_asian_games_projected_policy_parse_value(json, json_size, "host_hash_salt", 0u, 100000u, &out->host_hash_salt);
    kbo_asian_games_projected_policy_parse_value(json, json_size, "host_max_count", 1u, 256u, &out->host_max_count);
    kbo_asian_games_projected_policy_parse_value(json, json_size, "host_city_cooldown_years", 0u, 120u, &out->host_city_cooldown_years);
    kbo_asian_games_projected_policy_parse_value(json, json_size, "host_country_cooldown_years", 0u, 120u, &out->host_country_cooldown_years);
    kbo_asian_games_projected_policy_parse_value(json, json_size, "host_fallback_city_cooldown_years", 0u, 120u, &out->host_fallback_city_cooldown_years);
    kbo_asian_games_projected_policy_parse_value(json, json_size, "host_fallback_country_cooldown_years", 0u, 120u, &out->host_fallback_country_cooldown_years);
    char projected_status[KBO_ASIAN_GAMES_PROJECTED_STATUS_MAX] = {0};
    if (kbo_find_string_value_in_json(
            json,
            json_size,
            "projected_status",
            projected_status,
            sizeof(projected_status))
            && projected_status[0] != '\0') {
        snprintf(out->projected_status, sizeof(out->projected_status), "%s", projected_status);
    }
    char projected_notes[KBO_ASIAN_GAMES_PROJECTED_NOTES_MAX] = {0};
    if (kbo_find_string_value_in_json(
            json,
            json_size,
            "projected_notes",
            projected_notes,
            sizeof(projected_notes))
            && projected_notes[0] != '\0') {
        snprintf(out->projected_notes, sizeof(out->projected_notes), "%s", projected_notes);
    }
    if (out->projected_end_year < out->projected_start_year) {
        out->projected_end_year = out->projected_start_year;
    }
}

static void kbo_asian_games_projected_policy_import_file(KboAsianGamesProjectedPolicy* out, const char* path)
{
    if (out == NULL || path == NULL || path[0] == '\0') {
        return;
    }
    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }
    DWORD size = GetFileSize(file, NULL);
    if (size == INVALID_FILE_SIZE || size == 0u || size > KBO_FLAGS_JSON_MAX_BYTES) {
        CloseHandle(file);
        return;
    }
    char* raw = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)size + 1u);
    if (raw == NULL) {
        CloseHandle(file);
        return;
    }
    DWORD read = 0;
    if (ReadFile(file, raw, size, &read, NULL) && read > 0u) {
        raw[read] = '\0';
        kbo_asian_games_projected_policy_parse(out, raw, read);
    }
    HeapFree(GetProcessHeap(), 0, raw);
    CloseHandle(file);
}

void kbo_load_asian_games_projected_policy(KboAsianGamesProjectedPolicy* out)
{
    if (out == NULL) {
        return;
    }
    kbo_asian_games_projected_policy_defaults(out);
    char global_path[MAX_PATH] = {0};
    char save_path[MAX_PATH] = {0};
    kbo_asian_games_projected_policy_global_path(global_path, sizeof(global_path));
    kbo_asian_games_projected_policy_save_path(save_path, sizeof(save_path));
    kbo_asian_games_projected_policy_import_file(out, global_path);
    kbo_asian_games_projected_policy_import_file(out, save_path);
}
