#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "cbt_rules.h"
#include "../../core/core_flags/api/flags_api.h"
#include "../../core/files/save_paths/core_save_paths.h"
#include "../../core/core_flags/json/json_bool_parser.h"
#include "../../core/files/save_paths/platform/core_path_io.h"
#include "../../core/logging/core_log.h"

#define KBO_CBT_RULES_FILE "cbt_rules.json"

static void kbo_cbt_rules_init_defaults(KboCbtRules* out)
{
    memset(out, 0, sizeof(*out));
}

static int kbo_cbt_rules_add_threshold(KboCbtRules* out, uint32_t season, int32_t threshold)
{
    if (out == NULL || season == 0u || threshold <= 0 || out->threshold_count >= KBO_CBT_THRESHOLD_MAX) {
        return 0;
    }
    for (uint32_t i = 0u; i < out->threshold_count; i++) {
        if (out->thresholds[i].season == season) {
            out->thresholds[i].threshold = threshold;
            return 1;
        }
    }
    out->thresholds[out->threshold_count].season = season;
    out->thresholds[out->threshold_count].threshold = threshold;
    out->threshold_count++;
    return 1;
}

static void kbo_cbt_rules_parse_thresholds(KboCbtRules* out, const char* json, DWORD json_size)
{
    const char* start = NULL;
    const char* end = NULL;
    const char* array_start = NULL;
    if (!kbo_find_json_value_span(json, json_size, "thresholds", &start, &end)
            || start == NULL || end == NULL || start >= end) {
        return;
    }
    array_start = kbo_json_skip_ws(start, end);
    if (array_start >= end || *array_start != '[') {
        return;
    }

    KboCbtThresholdRow parsed[KBO_CBT_THRESHOLD_MAX];
    uint32_t parsed_count = 0u;
    const char* p = start;
    while (p < end && parsed_count < KBO_CBT_THRESHOLD_MAX) {
        if (*p != '{') {
            p++;
            continue;
        }
        const char* object_start = p;
        int depth = 0;
        int in_string = 0;
        int escaped = 0;
        while (p < end) {
            char ch = *p++;
            if (in_string) {
                if (escaped) {
                    escaped = 0;
                } else if (ch == '\\') {
                    escaped = 1;
                } else if (ch == '"') {
                    in_string = 0;
                }
                continue;
            }
            if (ch == '"') {
                in_string = 1;
            } else if (ch == '{') {
                depth++;
            } else if (ch == '}') {
                depth--;
                if (depth == 0) {
                    const char* object_end = p;
                    int season = 0;
                    int threshold = 0;
                    DWORD object_size = (DWORD)(object_end - object_start);
                    if (kbo_find_int_value_in_json(object_start, object_size, "season", &season)
                            && kbo_find_int_value_in_json(object_start, object_size, "threshold", &threshold)
                            && season > 0
                            && threshold > 0) {
                        parsed[parsed_count].season = (uint32_t)season;
                        parsed[parsed_count].threshold = (int32_t)threshold;
                        parsed_count++;
                    }
                    break;
                }
            }
        }
    }
    if (parsed_count == 0u) {
        return;
    }

    out->threshold_count = 0u;
    for (uint32_t i = 0u; i < parsed_count; i++) {
        kbo_cbt_rules_add_threshold(out, parsed[i].season, parsed[i].threshold);
    }
}

static void kbo_cbt_rules_parse_json(KboCbtRules* out, const char* json, DWORD json_size)
{
    int v = 0;
    if (kbo_find_flag_value_in_json(json, json_size, "enabled", &v)) {
        out->enabled = v ? 1u : 0u;
    }
    if (kbo_find_int_value_in_json(json, json_size, "top_player_count", &v) && v > 0 && v <= 200) {
        out->top_player_count = (uint32_t)v;
    }
    if (kbo_find_int_value_in_json(json, json_size, "threshold_override", &v) && v > 0) {
        out->threshold_override = (int32_t)v;
    }
    if (kbo_find_int_value_in_json(json, json_size, "tax_rate_1", &v) && v > 0 && v <= 1000) {
        out->tax_rate_1 = (uint32_t)v;
    }
    if (kbo_find_int_value_in_json(json, json_size, "tax_rate_2", &v) && v > 0 && v <= 1000) {
        out->tax_rate_2 = (uint32_t)v;
    }
    if (kbo_find_int_value_in_json(json, json_size, "tax_rate_3plus", &v) && v > 0 && v <= 1000) {
        out->tax_rate_3plus = (uint32_t)v;
    }
    if (kbo_find_int_value_in_json(json, json_size, "annual_increase_pct", &v) && v >= 0 && v <= 100) {
        out->annual_increase_pct = (uint32_t)v;
    }
    if (kbo_find_int_value_in_json(json, json_size, "draft_penalty_min_consecutive", &v) && v > 0 && v <= 10) {
        out->draft_penalty_min_consecutive = (uint32_t)v;
    }
    if (kbo_find_int_value_in_json(json, json_size, "draft_penalty_stages", &v) && v > 0 && v <= 100) {
        out->draft_penalty_stages = (uint32_t)v;
    }
    if (kbo_find_int_value_in_json(json, json_size, "exception_deadline_days_after_opening", &v) && v >= 0 && v <= 60) {
        out->exception_deadline_days_after_opening = (uint32_t)v;
    }
    if (kbo_find_int_value_in_json(json, json_size, "announcement_days_after_opening", &v) && v >= 0 && v <= 60) {
        out->announcement_days_after_opening = (uint32_t)v;
    }
    if (kbo_find_int_value_in_json(json, json_size, "event_scheduler_max_attempts", &v) && v > 0 && v <= 10000) {
        out->event_scheduler_max_attempts = (uint32_t)v;
    }
    if (kbo_find_int_value_in_json(json, json_size, "event_scheduler_sleep_ms", &v) && v >= 100 && v <= 600000) {
        out->event_scheduler_sleep_ms = (uint32_t)v;
    }
    for (int i = 0; i < 5; i++) {
        char key[64] = {0};
        snprintf(key, sizeof(key), "event_scheduler_log_attempt_%d", i + 1);
        if (kbo_find_int_value_in_json(json, json_size, key, &v) && v > 0 && v <= 10000) {
            out->event_scheduler_log_attempts[i] = (uint32_t)v;
        }
    }
    kbo_cbt_rules_parse_thresholds(out, json, json_size);
}

static HANDLE kbo_cbt_rules_open_file(char* out_path, size_t out_path_size)
{
    if (out_path != NULL && out_path_size > 0u) {
        out_path[0] = '\0';
    }
    if (out_path == NULL || out_path_size == 0u) {
        return INVALID_HANDLE_VALUE;
    }

    if (kbo_get_save_scoped_data_file(KBO_CBT_RULES_FILE, out_path, out_path_size)) {
        HANDLE file = CreateFileA(out_path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (file != INVALID_HANDLE_VALUE) {
            return file;
        }
    }
    if (kbo_get_global_data_file(KBO_CBT_RULES_FILE, out_path, out_path_size)) {
        return CreateFileA(out_path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    }
    return INVALID_HANDLE_VALUE;
}

void kbo_cbt_rules_load(KboCbtRules* out)
{
    if (out == NULL) {
        return;
    }
    kbo_cbt_rules_init_defaults(out);

    char json_path[KBO_UTF8_PATH_BYTES] = {0};
    HANDLE file = kbo_cbt_rules_open_file(json_path, sizeof(json_path));
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }

    DWORD size = GetFileSize(file, NULL);
    if (size == INVALID_FILE_SIZE || size == 0u || size > 65536u) {
        CloseHandle(file);
        return;
    }

    char* buf = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)size + 1u);
    if (buf == NULL) {
        CloseHandle(file);
        return;
    }

    DWORD read = 0;
    if (ReadFile(file, buf, size, &read, NULL) && read > 0) {
        buf[read] = '\0';
        kbo_cbt_rules_parse_json(out, buf, read);
        kbo_log_runtimef(
            "KBO CBT rules loaded path=%s enabled=%u top=%u override=%d rates=%u/%u/%u windows=%u/%u thresholds=%u",
            json_path,
            (uint32_t)out->enabled,
            out->top_player_count,
            out->threshold_override,
            out->tax_rate_1,
            out->tax_rate_2,
            out->tax_rate_3plus,
            out->exception_deadline_days_after_opening,
            out->announcement_days_after_opening,
            out->threshold_count);
    }
    CloseHandle(file);
    HeapFree(GetProcessHeap(), 0, buf);
}

int32_t kbo_cbt_get_threshold(const KboCbtRules* rules, uint32_t season)
{
    if (rules == NULL) {
        return 0;
    }
    if (rules->threshold_override > 0) {
        return rules->threshold_override;
    }

    if (rules->threshold_count == 0u) {
        return 0;
    }

    int32_t best = rules->thresholds[0].threshold;
    uint32_t best_season = 0u;
    int32_t last_known_threshold = rules->thresholds[0].threshold;
    uint32_t last_known = rules->thresholds[0].season;
    for (uint32_t i = 0u; i < rules->threshold_count; i++) {
        if (season == rules->thresholds[i].season) {
            return rules->thresholds[i].threshold;
        }
        if (rules->thresholds[i].season <= season && rules->thresholds[i].season >= best_season) {
            best = rules->thresholds[i].threshold;
            best_season = rules->thresholds[i].season;
        }
        if (rules->thresholds[i].season >= last_known) {
            last_known = rules->thresholds[i].season;
            last_known_threshold = rules->thresholds[i].threshold;
        }
    }

    if (season > last_known && rules->annual_increase_pct > 0u) {
        uint32_t years_past = season - last_known;
        int32_t val = last_known_threshold;
        for (uint32_t y = 0u; y < years_past; y++) {
            val = val + (int32_t)((int64_t)val * (int32_t)rules->annual_increase_pct / 100LL);
        }
        return val;
    }

    return best;
}

uint32_t kbo_cbt_get_tax_rate(const KboCbtRules* rules, uint32_t consecutive_count)
{
    if (rules == NULL || consecutive_count == 0u) {
        return 0u;
    }
    if (consecutive_count == 1u) {
        return rules->tax_rate_1;
    }
    if (consecutive_count == 2u) {
        return rules->tax_rate_2;
    }
    return rules->tax_rate_3plus;
}
