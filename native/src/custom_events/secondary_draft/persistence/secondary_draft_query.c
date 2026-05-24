#include "../secondary_draft.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../../core/sql/save_state/save_state_sqlite.h"

typedef struct KboSecondaryDraftSeasonLoadContext {
    uint32_t* seasons;
    int max_count;
    int count;
} KboSecondaryDraftSeasonLoadContext;

typedef struct KboSecondaryDraftSummaryLoadContext {
    KboSecondaryDraftRunSummary* out;
    int found;
} KboSecondaryDraftSummaryLoadContext;

typedef struct KboSecondaryDraftRowLoadContext {
    KboSecondaryDraftResultRow* rows;
    int max_count;
    int count;
} KboSecondaryDraftRowLoadContext;

static uint32_t kbo_secondary_draft_sql_u32(char** vals, int index)
{
    if (vals == NULL || vals[index] == NULL) {
        return 0u;
    }
    return (uint32_t)strtoul(vals[index], NULL, 10);
}

static int64_t kbo_secondary_draft_sql_i64(char** vals, int index)
{
    if (vals == NULL || vals[index] == NULL) {
        return 0;
    }
    return (int64_t)_strtoi64(vals[index], NULL, 10);
}

static void kbo_secondary_draft_sql_copy_text(const char* text, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    snprintf(out, out_size, "%s", text != NULL ? text : "");
}

static int kbo_secondary_draft_sql_season_cb(void* user_data, int ncols, char** vals, char** names)
{
    (void)names;
    KboSecondaryDraftSeasonLoadContext* ctx = (KboSecondaryDraftSeasonLoadContext*)user_data;
    if (ctx == NULL || ctx->seasons == NULL || ncols < 1 || ctx->count >= ctx->max_count) {
        return 0;
    }
    uint32_t season = kbo_secondary_draft_sql_u32(vals, 0);
    if (season != 0u) {
        ctx->seasons[ctx->count++] = season;
    }
    return 0;
}

int kbo_secondary_draft_load_seasons(uint32_t* seasons, int max_seasons)
{
    if (seasons == NULL || max_seasons <= 0
            || !kbo_secondary_draft_ensure_schema("secondary_draft_ui_seasons_schema")) {
        return 0;
    }
    memset(seasons, 0, (size_t)max_seasons * sizeof(seasons[0]));

    char sql[384] = {0};
    snprintf(
        sql,
        sizeof(sql),
        "SELECT season FROM ("
        "SELECT season FROM secondary_draft_runs "
        "UNION "
        "SELECT season FROM secondary_draft_results "
        "UNION "
        "SELECT season FROM secondary_draft_windows"
        ") WHERE season>0 ORDER BY season DESC LIMIT %d;",
        max_seasons);

    KboSecondaryDraftSeasonLoadContext ctx = {seasons, max_seasons, 0};
    if (!kbo_save_state_query(sql, kbo_secondary_draft_sql_season_cb, &ctx, "secondary_draft_ui_seasons")) {
        return 0;
    }
    return ctx.count;
}

static int kbo_secondary_draft_sql_summary_cb(void* user_data, int ncols, char** vals, char** names)
{
    (void)names;
    KboSecondaryDraftSummaryLoadContext* ctx = (KboSecondaryDraftSummaryLoadContext*)user_data;
    if (ctx == NULL || ctx->out == NULL || vals == NULL || ncols < 7) {
        return 0;
    }
    KboSecondaryDraftRunSummary summary;
    memset(&summary, 0, sizeof(summary));
    summary.season = kbo_secondary_draft_sql_u32(vals, 0);
    summary.event_yyyymmdd = kbo_secondary_draft_sql_u32(vals, 1);
    summary.league_id = kbo_secondary_draft_sql_u32(vals, 2);
    summary.pick_count = (int)kbo_secondary_draft_sql_u32(vals, 3);
    summary.candidate_count = (int)kbo_secondary_draft_sql_u32(vals, 4);
    summary.protected_count = (int)kbo_secondary_draft_sql_u32(vals, 5);
    summary.cash_total = kbo_secondary_draft_sql_i64(vals, 6);
    if (summary.season != 0u) {
        *ctx->out = summary;
        ctx->found = 1;
    }
    return 0;
}

int kbo_secondary_draft_load_run_summary(uint32_t season, KboSecondaryDraftRunSummary* out)
{
    if (out == NULL || season == 0u
            || !kbo_secondary_draft_ensure_schema("secondary_draft_ui_summary_schema")) {
        return 0;
    }
    memset(out, 0, sizeof(*out));

    char sql[640] = {0};
    snprintf(
        sql,
        sizeof(sql),
        "SELECT season,event_yyyymmdd,league_id,pick_count,candidate_count,protected_count,cash_total "
        "FROM secondary_draft_runs WHERE season=%u LIMIT 1;",
        season);

    KboSecondaryDraftSummaryLoadContext ctx = {out, 0};
    if (!kbo_save_state_query(sql, kbo_secondary_draft_sql_summary_cb, &ctx, "secondary_draft_ui_summary")) {
        return 0;
    }
    if (ctx.found) {
        return 1;
    }

    snprintf(
        sql,
        sizeof(sql),
        "SELECT season,MAX(event_yyyymmdd),MAX(league_id),COUNT(*),0,0,COALESCE(SUM(cash_amount),0) "
        "FROM secondary_draft_results WHERE season=%u GROUP BY season;",
        season);
    memset(out, 0, sizeof(*out));
    ctx.out = out;
    ctx.found = 0;
    if (!kbo_save_state_query(sql, kbo_secondary_draft_sql_summary_cb, &ctx, "secondary_draft_ui_summary_fallback")) {
        return 0;
    }
    return ctx.found;
}

static int kbo_secondary_draft_sql_row_cb(void* user_data, int ncols, char** vals, char** names)
{
    (void)names;
    KboSecondaryDraftRowLoadContext* ctx = (KboSecondaryDraftRowLoadContext*)user_data;
    if (ctx == NULL || ctx->rows == NULL || vals == NULL || ncols < 14 || ctx->count >= ctx->max_count) {
        return 0;
    }

    KboSecondaryDraftResultRow row;
    memset(&row, 0, sizeof(row));
    row.season = kbo_secondary_draft_sql_u32(vals, 0);
    row.event_yyyymmdd = kbo_secondary_draft_sql_u32(vals, 1);
    row.league_id = kbo_secondary_draft_sql_u32(vals, 2);
    row.pick_no = kbo_secondary_draft_sql_u32(vals, 3);
    row.round = kbo_secondary_draft_sql_u32(vals, 4);
    row.player_id = kbo_secondary_draft_sql_u32(vals, 5);
    kbo_secondary_draft_sql_copy_text(vals[6], row.player_name, sizeof(row.player_name));
    row.from_team_id = kbo_secondary_draft_sql_u32(vals, 7);
    kbo_secondary_draft_sql_copy_text(vals[8], row.from_team_name, sizeof(row.from_team_name));
    row.to_team_id = kbo_secondary_draft_sql_u32(vals, 9);
    kbo_secondary_draft_sql_copy_text(vals[10], row.to_team_name, sizeof(row.to_team_name));
    row.cash_amount = kbo_secondary_draft_sql_u32(vals, 11);
    row.cash_applied = (int)kbo_secondary_draft_sql_u32(vals, 12);
    row.moved = (int)kbo_secondary_draft_sql_u32(vals, 13);
    if (row.season != 0u && row.player_id != 0u) {
        ctx->rows[ctx->count++] = row;
    }
    return 0;
}

int kbo_secondary_draft_load_result_rows(
    uint32_t season,
    KboSecondaryDraftResultRow* rows,
    int max_rows)
{
    if (rows == NULL || max_rows <= 0 || season == 0u
            || !kbo_secondary_draft_ensure_schema("secondary_draft_ui_rows_schema")) {
        return 0;
    }
    memset(rows, 0, (size_t)max_rows * sizeof(rows[0]));

    char sql[768] = {0};
    snprintf(
        sql,
        sizeof(sql),
        "SELECT season,event_yyyymmdd,league_id,pick_no,round,player_id,player_name,"
        "from_team_id,from_team_name,to_team_id,to_team_name,cash_amount,cash_applied,moved "
        "FROM secondary_draft_results WHERE season=%u ORDER BY pick_no ASC LIMIT %d;",
        season,
        max_rows);

    KboSecondaryDraftRowLoadContext ctx = {rows, max_rows, 0};
    if (!kbo_save_state_query(sql, kbo_secondary_draft_sql_row_cb, &ctx, "secondary_draft_ui_rows")) {
        return 0;
    }
    return ctx.count;
}
