#include "secondary_draft_internal.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../core/dates/tick/current_date_tick_capture.h"
#include "../../core/sql/escape/core_sql_escape.h"
#include "../../core/sql/save_state/save_state_sqlite.h"

typedef struct KboSecondaryDraftWindowLoadContext {
    KboSecondaryDraftWindow* out;
    int found;
} KboSecondaryDraftWindowLoadContext;

static uint32_t kbo_secondary_draft_window_u32(char** vals, int index)
{
    if (vals == NULL || vals[index] == NULL) {
        return 0u;
    }
    return (uint32_t)strtoul(vals[index], NULL, 10);
}

static int kbo_secondary_draft_window_cb(void* user_data, int ncols, char** vals, char** names)
{
    (void)names;
    KboSecondaryDraftWindowLoadContext* ctx = (KboSecondaryDraftWindowLoadContext*)user_data;
    if (ctx == NULL || ctx->out == NULL || vals == NULL || ncols < 5) {
        return 0;
    }
    KboSecondaryDraftWindow window;
    memset(&window, 0, sizeof(window));
    window.season = kbo_secondary_draft_window_u32(vals, 0);
    window.league_id = kbo_secondary_draft_window_u32(vals, 1);
    window.protection_open_yyyymmdd = kbo_secondary_draft_window_u32(vals, 2);
    window.protection_deadline_yyyymmdd = kbo_secondary_draft_window_u32(vals, 3);
    window.draft_yyyymmdd = kbo_secondary_draft_window_u32(vals, 4);
    if (window.season != 0u && window.draft_yyyymmdd != 0u) {
        *ctx->out = window;
        ctx->found = 1;
    }
    return 0;
}

static int kbo_secondary_draft_current_yyyymmdd(uint32_t* out)
{
    if (out == NULL) {
        return 0;
    }
    *out = 0u;
    uint32_t year = 0u;
    uint32_t month = 0u;
    uint32_t day = 0u;
    if (!kbo_current_date_tick_latest_components(&year, &month, &day)
            || year == 0u
            || month == 0u
            || day == 0u) {
        return 0;
    }
    *out = year * 10000u + month * 100u + day;
    return 1;
}

int kbo_secondary_draft_register_window(
    uint32_t season,
    uint32_t league_id,
    uint32_t protection_open_yyyymmdd,
    uint32_t protection_deadline_yyyymmdd,
    uint32_t draft_yyyymmdd,
    const char* source)
{
    if (season == 0u
            || league_id == 0u
            || protection_open_yyyymmdd == 0u
            || protection_deadline_yyyymmdd == 0u
            || draft_yyyymmdd == 0u
            || protection_open_yyyymmdd > protection_deadline_yyyymmdd
            || protection_deadline_yyyymmdd >= draft_yyyymmdd
            || !kbo_secondary_draft_ensure_schema("secondary_draft_window_schema")) {
        return 0;
    }

    char escaped_source[256] = {0};
    if (!kbo_sql_escape_literal(
            escaped_source,
            sizeof(escaped_source),
            source != NULL ? source : "secondary_draft_window")) {
        return 0;
    }

    char sql[768] = {0};
    snprintf(
        sql,
        sizeof(sql),
        "INSERT OR REPLACE INTO secondary_draft_windows("
        "season,league_id,protection_open_yyyymmdd,protection_deadline_yyyymmdd,"
        "draft_yyyymmdd,source,updated_at"
        ") VALUES(%u,%u,%u,%u,%u,'%s',datetime('now'));",
        season,
        league_id,
        protection_open_yyyymmdd,
        protection_deadline_yyyymmdd,
        draft_yyyymmdd,
        escaped_source);
    return kbo_save_state_exec(sql, "secondary_draft_window_register");
}

int kbo_secondary_draft_load_window(uint32_t season, KboSecondaryDraftWindow* out)
{
    if (out == NULL || season == 0u
            || !kbo_secondary_draft_ensure_schema("secondary_draft_window_load_schema")) {
        return 0;
    }
    memset(out, 0, sizeof(*out));

    char sql[512] = {0};
    snprintf(
        sql,
        sizeof(sql),
        "SELECT season,league_id,protection_open_yyyymmdd,protection_deadline_yyyymmdd,"
        "draft_yyyymmdd FROM secondary_draft_windows WHERE season=%u LIMIT 1;",
        season);
    KboSecondaryDraftWindowLoadContext ctx = {out, 0};
    if (!kbo_save_state_query(sql, kbo_secondary_draft_window_cb, &ctx, "secondary_draft_window_load")) {
        return 0;
    }
    return ctx.found;
}

int kbo_secondary_draft_protection_window_open(uint32_t season)
{
    KboSecondaryDraftWindow window;
    uint32_t today = 0u;
    if (!kbo_secondary_draft_load_window(season, &window)
            || !kbo_secondary_draft_current_yyyymmdd(&today)
            || today < window.protection_open_yyyymmdd
            || today > window.protection_deadline_yyyymmdd
            || kbo_secondary_draft_sql_run_exists(season)
            || kbo_secondary_draft_sql_result_count(season) > 0) {
        return 0;
    }
    return 1;
}

int kbo_secondary_draft_draft_window_open(uint32_t season)
{
    KboSecondaryDraftWindow window;
    uint32_t today = 0u;
    if (!kbo_secondary_draft_load_window(season, &window)
            || !kbo_secondary_draft_current_yyyymmdd(&today)
            || today < window.draft_yyyymmdd) {
        return 0;
    }
    return 1;
}
