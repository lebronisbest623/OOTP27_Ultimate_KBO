#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "season_calendar_opening_day_cache.h"

#include <stdio.h>
#include <string.h>

#include "../../dates/constants/kbo_date_constants.h"
#include "../../dates/core_text_date.h"
#include "../../logging/core_log.h"
#include "../../sync/lock.h"
#include "../../sql/save_state/save_state_sqlite.h"

enum {
    KBO_SEASON_CALENDAR_OPENING_DAY_CACHE_SIZE = 32
};

typedef struct KboSeasonCalendarOpeningDayCacheEntry {
    uint32_t league_id;
    uint32_t season;
    uint32_t opening_day;
    uint32_t observed_date;
    char source[64];
    uint8_t valid;
} KboSeasonCalendarOpeningDayCacheEntry;

static KboLock g_kbo_season_calendar_opening_day_cache_lock = KBO_LOCK_INIT;
static KboSeasonCalendarOpeningDayCacheEntry
    g_kbo_season_calendar_opening_day_cache[KBO_SEASON_CALENDAR_OPENING_DAY_CACHE_SIZE];
static char g_kbo_season_calendar_opening_day_cache_path[MAX_PATH];

int kbo_season_calendar_path(char* out, size_t out_size)
{
    return kbo_save_state_db_path(out, out_size);
}

static uint32_t kbo_season_calendar_opening_day_cache_slot(uint32_t league_id, uint32_t season)
{
    uint32_t h = league_id * 2654435761u;
    h ^= season * 2246822519u;
    h ^= h >> 16;
    return h & (KBO_SEASON_CALENDAR_OPENING_DAY_CACHE_SIZE - 1u);
}

static void kbo_season_calendar_opening_day_cache_refresh_scope_locked(const char* path)
{
    if (path == NULL || path[0] == '\0') {
        return;
    }
    if (strcmp(g_kbo_season_calendar_opening_day_cache_path, path) == 0) {
        return;
    }
    memset(g_kbo_season_calendar_opening_day_cache, 0, sizeof(g_kbo_season_calendar_opening_day_cache));
    snprintf(
        g_kbo_season_calendar_opening_day_cache_path,
        sizeof(g_kbo_season_calendar_opening_day_cache_path),
        "%s",
        path);
}

static int kbo_season_calendar_opening_day_cache_get(
    uint32_t league_id,
    uint32_t season,
    KboSeasonCalendarOpeningDayRow* out_row)
{
    if (out_row != NULL) {
        memset(out_row, 0, sizeof(*out_row));
    }
    if (out_row == NULL || season == 0u) {
        return 0;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_season_calendar_path(path, sizeof(path))) {
        return 0;
    }

    int hit = 0;
    kbo_lock_enter(&g_kbo_season_calendar_opening_day_cache_lock);
    kbo_season_calendar_opening_day_cache_refresh_scope_locked(path);
    KboSeasonCalendarOpeningDayCacheEntry* entry =
        &g_kbo_season_calendar_opening_day_cache[
            kbo_season_calendar_opening_day_cache_slot(league_id, season)];
    if (entry->valid
            && entry->league_id == league_id
            && entry->season == season
            && kbo_season_calendar_opening_day_valid(entry->season, entry->opening_day)) {
        out_row->league_id = entry->league_id;
        out_row->season = entry->season;
        out_row->opening_day = entry->opening_day;
        out_row->observed_date = entry->observed_date;
        snprintf(out_row->source, sizeof(out_row->source), "%s", entry->source);
        hit = 1;
    }
    kbo_lock_leave(&g_kbo_season_calendar_opening_day_cache_lock);
    return hit;
}

void kbo_season_calendar_opening_day_cache_store(
    uint32_t league_id,
    uint32_t season,
    uint32_t opening_day,
    uint32_t observed_date,
    const char* source)
{
    if (season == 0u || !kbo_season_calendar_opening_day_valid(season, opening_day)) {
        return;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_season_calendar_path(path, sizeof(path))) {
        return;
    }

    kbo_lock_enter(&g_kbo_season_calendar_opening_day_cache_lock);
    kbo_season_calendar_opening_day_cache_refresh_scope_locked(path);
    KboSeasonCalendarOpeningDayCacheEntry* entry =
        &g_kbo_season_calendar_opening_day_cache[
            kbo_season_calendar_opening_day_cache_slot(league_id, season)];
    entry->valid = 0u;
    entry->league_id = league_id;
    entry->season = season;
    entry->opening_day = opening_day;
    entry->observed_date = observed_date;
    snprintf(entry->source, sizeof(entry->source), "%s", source != NULL ? source : "");
    entry->valid = 1u;
    kbo_lock_leave(&g_kbo_season_calendar_opening_day_cache_lock);
}

int kbo_season_calendar_opening_day_valid(uint32_t season, uint32_t opening_day)
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

void kbo_season_calendar_sanitize_source(const char* source, char* out, size_t out_size)
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

int kbo_season_calendar_load_opening_day_details(
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

    KboSeasonCalendarOpeningDayRow cached_row = {0};
    if (kbo_season_calendar_opening_day_cache_get(league_id, season, &cached_row)
            && kbo_season_calendar_opening_day_valid(cached_row.season, cached_row.opening_day)) {
        *out_opening_day = cached_row.opening_day;
        if (out_observed_date != NULL) {
            *out_observed_date = cached_row.observed_date;
        }
        if (out_source != NULL && out_source_size > 0u) {
            snprintf(out_source, out_source_size, "%s", cached_row.source);
        }
        return 1;
    }

    KboSeasonCalendarOpeningDayRow row = {0};
    if (!kbo_season_calendar_sql_load_opening_day(league_id, season, &row)
            || !kbo_season_calendar_opening_day_valid(row.season, row.opening_day)) {
        return 0;
    }

    kbo_season_calendar_opening_day_cache_store(
        league_id,
        season,
        row.opening_day,
        row.observed_date,
        row.source);
    if (row.league_id != league_id) {
        kbo_season_calendar_opening_day_cache_store(
            row.league_id,
            row.season,
            row.opening_day,
            row.observed_date,
            row.source);
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

