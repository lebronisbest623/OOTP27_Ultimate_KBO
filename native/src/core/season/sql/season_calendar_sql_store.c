#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "season_calendar_sql_store.h"

#include <stdio.h>
#include <string.h>

#include "../../logging/core_log.h"
#include "../../sql/escape/core_sql_escape.h"
#include "../../sql/save_state/save_state_sqlite.h"

typedef struct KboSeasonCalendarSqlLoadContext {
    KboSeasonCalendarOpeningDayRow* row;
    int found;
} KboSeasonCalendarSqlLoadContext;

static int kbo_season_calendar_sql_ensure_schema(const char* source)
{
    static const char* sql =
        "CREATE TABLE IF NOT EXISTS season_calendar ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "league_id INTEGER NOT NULL,"
        "season INTEGER NOT NULL,"
        "opening_day INTEGER NOT NULL,"
        "observed_date INTEGER NOT NULL DEFAULT 0,"
        "source TEXT NOT NULL DEFAULT '',"
        "updated_at TEXT NOT NULL DEFAULT (datetime('now')),"
        "UNIQUE(league_id, season)"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_season_calendar_season "
        "ON season_calendar(season);"
        "INSERT OR REPLACE INTO kbo_schema(schema_key, schema_version, updated_at) "
        "VALUES('season_calendar', 1, datetime('now'));";
    return kbo_save_state_exec(sql, source != NULL ? source : "season_calendar_schema");
}

static int kbo_season_calendar_sql_parse_u32(const char* text, uint32_t* out)
{
    if (text == NULL || out == NULL) {
        return 0;
    }
    unsigned int value = 0u;
    if (sscanf(text, "%u", &value) != 1) {
        return 0;
    }
    *out = (uint32_t)value;
    return 1;
}

static int kbo_season_calendar_sql_load_cb(void* user_data, int ncols, char** vals, char** names)
{
    (void)names;
    KboSeasonCalendarSqlLoadContext* ctx = (KboSeasonCalendarSqlLoadContext*)user_data;
    if (ctx == NULL || ctx->row == NULL || vals == NULL || ncols < 5) {
        return 0;
    }

    KboSeasonCalendarOpeningDayRow row = {0};
    if (!kbo_season_calendar_sql_parse_u32(vals[0], &row.league_id)
            || !kbo_season_calendar_sql_parse_u32(vals[1], &row.season)
            || !kbo_season_calendar_sql_parse_u32(vals[2], &row.opening_day)
            || !kbo_season_calendar_sql_parse_u32(vals[3], &row.observed_date)) {
        return 0;
    }
    if (vals[4] != NULL) {
        snprintf(row.source, sizeof(row.source), "%s", vals[4]);
    }

    *ctx->row = row;
    ctx->found = 1;
    return 0;
}

int kbo_season_calendar_sql_load_opening_day(
    uint32_t league_id,
    uint32_t season,
    KboSeasonCalendarOpeningDayRow* out_row)
{
    if (out_row == NULL || season == 0u
            || !kbo_season_calendar_sql_ensure_schema("season_calendar_load_schema")) {
        return 0;
    }
    memset(out_row, 0, sizeof(*out_row));

    char sql[384] = {0};
    if (league_id != 0u) {
        snprintf(
            sql,
            sizeof(sql),
            "SELECT league_id, season, opening_day, observed_date, source "
            "FROM season_calendar "
            "WHERE league_id=%u AND season=%u "
            "ORDER BY id DESC LIMIT 1;",
            league_id,
            season);
    } else {
        snprintf(
            sql,
            sizeof(sql),
            "SELECT league_id, season, opening_day, observed_date, source "
            "FROM season_calendar "
            "WHERE season=%u "
            "ORDER BY id DESC LIMIT 1;",
            season);
    }

    KboSeasonCalendarSqlLoadContext ctx = {out_row, 0};
    if (!kbo_save_state_query(sql, kbo_season_calendar_sql_load_cb, &ctx, "season_calendar_load")) {
        return 0;
    }
    return ctx.found;
}

int kbo_season_calendar_sql_store_opening_day(
    uint32_t league_id,
    uint32_t season,
    uint32_t opening_day,
    uint32_t observed_date,
    const char* source)
{
    if (league_id == 0u || season == 0u || opening_day == 0u
            || !kbo_season_calendar_sql_ensure_schema("season_calendar_store_schema")) {
        return 0;
    }

    char escaped_source[128] = {0};
    if (!kbo_sql_escape_literal(escaped_source, sizeof(escaped_source), source != NULL ? source : "")) {
        return 0;
    }

    char sql[768] = {0};
    int len = snprintf(
        sql,
        sizeof(sql),
        "INSERT OR REPLACE INTO season_calendar("
        "league_id, season, opening_day, observed_date, source, updated_at"
        ") VALUES(%u, %u, %u, %u, '%s', datetime('now'));",
        league_id,
        season,
        opening_day,
        observed_date,
        escaped_source);
    if (len <= 0 || len >= (int)sizeof(sql)) {
        kbo_log_runtimef(
            "KBO season calendar sqlite store skipped league=%u season=%u reason=sql_buffer_full",
            league_id,
            season);
        return 0;
    }

    return kbo_save_state_exec(sql, "season_calendar_store");
}
