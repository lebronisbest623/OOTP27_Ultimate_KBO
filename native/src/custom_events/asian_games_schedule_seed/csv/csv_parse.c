#include "../../runtime/common/custom_events_common.h"
#include "csv_parse.h"
#include <stdio.h>
#include <string.h>
#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/logging/core_log.h"
#include "../../../core/csv/core_csv.h"
#include "../../../core/dates/core_current_date.h"
#include "../../../core/files/save_paths/core_save_paths.h"
#include "../../../core/dates/core_text_date.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../foreign/common/dates/foreign_waiver_date.h"
#include "../../../core/dates/constants/kbo_date_constants.h"

void kbo_asian_games_schedule_copy_text(char* out, size_t out_size, const char* value)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';
    if (value == NULL) {
        return;
    }

    size_t len = strlen(value);
    if (len >= out_size) {
        len = out_size - 1u;
    }
    memcpy(out, value, len);
    out[len] = '\0';
}

uint32_t kbo_asian_games_schedule_parse_u32(const char* text)
{
    if (text == NULL || text[0] == '\0') {
        return 0u;
    }
    unsigned long long value = 0ull;
    const char* p = text;
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    if (*p < '0' || *p > '9') {
        return 0u;
    }
    while (*p >= '0' && *p <= '9') {
        value = value * 10ull + (unsigned long long)(*p - '0');
        if (value > UINT32_MAX) {
            return 0u;
        }
        p++;
    }
    return (uint32_t)value;
}

uint32_t kbo_asian_games_schedule_parse_date(const char* text)
{
    if (text == NULL || text[0] == '\0'
            || ascii_equals_ignore_case(text, "TBD")
            || ascii_equals_ignore_case(text, "NA")
            || ascii_equals_ignore_case(text, "N/A")) {
        return 0u;
    }

    uint32_t date = 0u;
    if (!kbo_parse_yyyymmdd(text, &date)) {
        return 0u;
    }
    if (kbo_date_serial(date / 10000u, (date / 100u) % 100u, date % 100u) == 0u) {
        return 0u;
    }
    return date;
}

uint8_t kbo_asian_games_schedule_parse_auto(const char* text)
{
    if (text == NULL || text[0] == '\0') {
        return 0u;
    }
    return (uint8_t)(ascii_equals_ignore_case(text, "1")
        || ascii_equals_ignore_case(text, "true")
        || ascii_equals_ignore_case(text, "yes")
        || ascii_equals_ignore_case(text, "auto")
        || ascii_equals_ignore_case(text, "schedule"));
}

int kbo_parse_asian_games_schedule_seed_fields(char fields[][128], int field_count, KboAsianGamesScheduleSeed* out)
{
    if (fields == NULL || out == NULL) {
        return 0;
    }
    memset(out, 0, sizeof(*out));
    if (field_count <= 0
            || fields[0][0] == '\0'
            || ascii_equals_ignore_case(fields[0], "year")
            || ascii_equals_ignore_case(fields[0], "event_year")) {
        return 0;
    }

    out->year = kbo_asian_games_schedule_parse_u32(fields[0]);
    if (out->year < KBO_SEASON_YEAR_MIN || out->year > KBO_SIM_YEAR_MAX) {
        return 0;
    }
    kbo_asian_games_schedule_copy_text(out->host_city, sizeof(out->host_city), field_count > 1 ? fields[1] : "");
    kbo_asian_games_schedule_copy_text(out->host_country, sizeof(out->host_country), field_count > 2 ? fields[2] : "");
    kbo_asian_games_schedule_copy_text(out->status, sizeof(out->status),
        field_count > 3 && fields[3][0] != '\0' ? fields[3] : "official");
    out->tournament_start = field_count > 4 ? kbo_asian_games_schedule_parse_date(fields[4]) : 0u;
    out->tournament_end = field_count > 5 ? kbo_asian_games_schedule_parse_date(fields[5]) : 0u;
    out->selection_date = field_count > 6 ? kbo_asian_games_schedule_parse_date(fields[6]) : 0u;
    out->departure_date = field_count > 7 ? kbo_asian_games_schedule_parse_date(fields[7]) : 0u;
    out->final_date = field_count > 8 ? kbo_asian_games_schedule_parse_date(fields[8]) : 0u;
    out->auto_schedule = field_count > 9 ? kbo_asian_games_schedule_parse_auto(fields[9]) : 0u;
    if (field_count > 11) {
        kbo_asian_games_schedule_copy_text(out->final_result, sizeof(out->final_result), fields[10]);
        kbo_asian_games_schedule_copy_text(out->notes, sizeof(out->notes), fields[11]);
    } else {
        kbo_asian_games_schedule_copy_text(out->notes, sizeof(out->notes), field_count > 10 ? fields[10] : "");
    }
    return 1;
}

int kbo_parse_asian_games_schedule_seed_line(const char* line, KboAsianGamesScheduleSeed* out)
{
    if (line == NULL || out == NULL) {
        return 0;
    }
    memset(out, 0, sizeof(*out));

    char copy[512] = {0};
    size_t len = strlen(line);
    if (len >= sizeof(copy)) {
        len = sizeof(copy) - 1u;
    }
    memcpy(copy, line, len);

    char* p = copy;
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    if (*p == '\0' || *p == '\r' || *p == '\n' || *p == '#' || *p == ';') {
        return 0;
    }

    char fields[12][128];
    int field_count = kbo_csv_read_trimmed_line_fields(p, (char*)fields, sizeof(fields[0]), 12);
    return kbo_parse_asian_games_schedule_seed_fields(fields, field_count, out);
}
