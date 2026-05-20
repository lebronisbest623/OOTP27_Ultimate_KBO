#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "season_calendar.h"

#include <stdio.h>
#include <string.h>

#include "../core_league_context_parts/api/league_context_lookup.h"
#include "../dates/core_text_date.h"
#include "../dates/tick/current_date_tick_capture.h"
#include "../logging/core_log.h"
#include "../sql/save_state/save_state_sqlite.h"
#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../runtime_memory/runtime_memory.h"
#include "../dates/constants/kbo_date_constants.h"
#include "sql/season_calendar_sql_store.h"

static int kbo_season_calendar_path(char* out, size_t out_size)
{
    return kbo_save_state_db_path(out, out_size);
}

static int kbo_season_calendar_opening_day_valid(uint32_t season, uint32_t opening_day)
{
    uint32_t year = opening_day / 10000u;
    uint32_t month = (opening_day / 100u) % 100u;
    uint32_t day = opening_day % 100u;
    return season >= KBO_SEASON_YEAR_MIN
        && season <= KBO_SIM_YEAR_MAX
        && year == season
        && month >= 1u
        && month <= 12u
        && day >= 1u
        && day <= 31u
        && kbo_date_serial(year, month, day) != 0u;
}

static void kbo_season_calendar_sanitize_source(const char* source, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';
    const char* text = source != NULL && source[0] != '\0' ? source : "unknown";
    size_t used = 0u;
    for (const char* p = text; *p != '\0' && used + 1u < out_size; ++p) {
        char ch = *p;
        if (ch == ',' || ch == '\r' || ch == '\n') {
            ch = '_';
        }
        out[used++] = ch;
    }
    out[used] = '\0';
}

int kbo_season_calendar_read_league_opening_day(uintptr_t league_ptr, uint32_t* out_date)
{
    if (out_date != NULL) {
        *out_date = 0u;
    }
    if (league_ptr == 0u
            || !memory_range_readable(
                (void*)(league_ptr + OOTP27_SEASON_START_DATE_YEAR_OFFSET),
                OOTP27_SEASON_START_DATE_SEC_OFFSET - OOTP27_SEASON_START_DATE_YEAR_OFFSET + sizeof(uint8_t))) {
        return 0;
    }

    uint32_t year = *(uint16_t*)(league_ptr + OOTP27_SEASON_START_DATE_YEAR_OFFSET);
    uint32_t day = *(uint8_t*)(league_ptr + OOTP27_SEASON_START_DATE_DAY_OFFSET);
    uint32_t month = *(uint8_t*)(league_ptr + OOTP27_SEASON_START_DATE_MONTH_OFFSET);
    if (year < KBO_SEASON_YEAR_MIN || year > KBO_SIM_YEAR_MAX || month < 1u || month > 12u || day < 1u || day > 31u) {
        return 0;
    }

    char scratch[16] = {0};
    if (!kbo_format_history_date(scratch, sizeof(scratch), year, month, day)) {
        return 0;
    }

    if (out_date != NULL) {
        *out_date = year * 10000u + month * 100u + day;
    }
    return 1;
}

static int kbo_season_calendar_load_opening_day_details(
    uint32_t league_id,
    uint32_t season,
    uint32_t* out_opening_day,
    uint32_t* out_observed_date,
    char* out_source,
    size_t out_source_size)
{
    if (out_opening_day != NULL) {
        *out_opening_day = 0u;
    }
    if (out_observed_date != NULL) {
        *out_observed_date = 0u;
    }
    if (out_source != NULL && out_source_size > 0u) {
        out_source[0] = '\0';
    }
    if (out_opening_day == NULL || season < KBO_SEASON_YEAR_MIN || season > KBO_SIM_YEAR_MAX) {
        return 0;
    }

    KboSeasonCalendarOpeningDayRow row = {0};
    if (!kbo_season_calendar_sql_load_opening_day(league_id, season, &row)
            || !kbo_season_calendar_opening_day_valid(row.season, row.opening_day)) {
        return 0;
    }

    *out_opening_day = row.opening_day;
    if (out_observed_date != NULL) {
        *out_observed_date = row.observed_date;
    }
    if (out_source != NULL && out_source_size > 0u) {
        snprintf(out_source, out_source_size, "%s", row.source);
    }
    return 1;
}

int kbo_season_calendar_load_opening_day(
    uint32_t league_id,
    uint32_t season,
    uint32_t* out_opening_day)
{
    return kbo_season_calendar_load_opening_day_details(
        league_id,
        season,
        out_opening_day,
        NULL,
        NULL,
        0u);
}

int kbo_season_calendar_store_opening_day(
    uint32_t league_id,
    uint32_t season,
    uint32_t opening_day,
    uint32_t observed_date,
    const char* source)
{
    if (league_id == 0u || !kbo_season_calendar_opening_day_valid(season, opening_day)) {
        return 0;
    }
    if (observed_date == 0u) {
        (void)kbo_current_date_tick_latest_published_date(&observed_date);
    }

    char path[MAX_PATH] = {0};
    if (!kbo_season_calendar_path(path, sizeof(path))) {
        return 0;
    }

    char safe_source[64] = {0};
    kbo_season_calendar_sanitize_source(source, safe_source, sizeof(safe_source));

    uint32_t cached_opening_day = 0u;
    uint32_t cached_observed_date = 0u;
    char cached_source[64] = {0};
    if (kbo_season_calendar_load_opening_day_details(
            league_id,
            season,
            &cached_opening_day,
            &cached_observed_date,
            cached_source,
            sizeof(cached_source))
            && cached_opening_day == opening_day) {
        return 1;
    }

    if (!kbo_season_calendar_sql_store_opening_day(
            league_id,
            season,
            opening_day,
            observed_date,
            safe_source)) {
        kbo_log_runtimef(
            "KBO season calendar opening day store skipped league=%u season=%u opening_day=%u reason=sqlite_write_failed path=%s",
            league_id,
            season,
            opening_day,
            path);
        return 0;
    }

    if (cached_opening_day != 0u && cached_opening_day != opening_day) {
        kbo_log_runtimef(
            "KBO season calendar opening day updated league=%u season=%u previous=%u previous_observed=%u previous_source=%s opening_day=%u observed_date=%u source=%s path=%s store=sqlite",
            league_id,
            season,
            cached_opening_day,
            cached_observed_date,
            cached_source,
            opening_day,
            observed_date,
            safe_source,
            path);
    } else {
        kbo_log_runtimef(
            "KBO season calendar opening day stored league=%u season=%u opening_day=%u observed_date=%u source=%s path=%s store=sqlite",
            league_id,
            season,
            opening_day,
            observed_date,
            safe_source,
            path);
    }
    return 1;
}

int kbo_season_calendar_resolve_opening_day_with_league_ptr(
    uint32_t league_id,
    uint32_t season,
    uint32_t today,
    uintptr_t league_ptr,
    const char* source,
    uint32_t* out_opening_day)
{
    if (out_opening_day != NULL) {
        *out_opening_day = 0u;
    }
    if (out_opening_day == NULL || season < KBO_SEASON_YEAR_MIN || season > KBO_SIM_YEAR_MAX) {
        return 0;
    }
    if (league_id == 0u) {
        league_id = kbo_resolve_kbo_league_id();
    }
    if (today == 0u) {
        (void)kbo_current_date_tick_latest_published_date(&today);
    }

    uint32_t opening_day = 0u;
    if (league_ptr == 0u && league_id != 0u) {
        league_ptr = kbo_find_league_ptr_from_id(league_id);
    }
    if (league_ptr != 0u
            && kbo_season_calendar_read_league_opening_day(league_ptr, &opening_day)
            && opening_day / 10000u == season) {
        if (league_id != 0u) {
            (void)kbo_season_calendar_store_opening_day(
                league_id,
                season,
                opening_day,
                today,
                source != NULL ? source : "memory");
        }
        *out_opening_day = opening_day;
        return 1;
    }

    if (kbo_season_calendar_load_opening_day(league_id, season, &opening_day)) {
        *out_opening_day = opening_day;
        return 1;
    }

    if (kbo_season_calendar_load_schedule_opening_day(season, &opening_day)
            && opening_day / 10000u == season) {
        if (league_id != 0u) {
            (void)kbo_season_calendar_store_opening_day(
                league_id,
                season,
                opening_day,
                today,
                "schedule_file");
        }
        *out_opening_day = opening_day;
        return 1;
    }

    return 0;
}

int kbo_season_calendar_resolve_opening_day(
    uint32_t league_id,
    uint32_t season,
    uint32_t today,
    uint32_t* out_opening_day)
{
    return kbo_season_calendar_resolve_opening_day_with_league_ptr(
        league_id,
        season,
        today,
        0u,
        "season_calendar_resolve",
        out_opening_day);
}
