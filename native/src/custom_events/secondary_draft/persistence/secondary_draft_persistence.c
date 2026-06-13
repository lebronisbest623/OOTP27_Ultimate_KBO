#include "../secondary_draft_internal.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../../core/sql/escape/core_sql_escape.h"
#include "../../../core/sql/save_state/save_state_sqlite.h"

typedef struct KboSecondaryDraftSqlCount {
    int found;
    int count;
} KboSecondaryDraftSqlCount;

typedef struct KboSecondaryDraftRunLoadContext {
    KboSecondaryDraftRunSummary* out;
    int found;
} KboSecondaryDraftRunLoadContext;

typedef struct KboSecondaryDraftResultLoadContext {
    KboSecondaryDraftPickRow* rows;
    int max_rows;
    int count;
} KboSecondaryDraftResultLoadContext;

static int kbo_secondary_draft_sql_count_cb(void* user_data, int ncols, char** vals, char** names)
{
    (void)names;
    KboSecondaryDraftSqlCount* count = (KboSecondaryDraftSqlCount*)user_data;
    if (count == NULL || ncols <= 0 || vals == NULL || vals[0] == NULL) {
        return 0;
    }
    count->found = 1;
    count->count = (int)strtol(vals[0], NULL, 10);
    return 0;
}

static uint32_t kbo_secondary_draft_sql_u32(char** vals, int index)
{
    if (vals == NULL || vals[index] == NULL) {
        return 0u;
    }
    return (uint32_t)strtoul(vals[index], NULL, 10);
}

static int kbo_secondary_draft_sql_int(char** vals, int index)
{
    if (vals == NULL || vals[index] == NULL) {
        return 0;
    }
    return (int)strtol(vals[index], NULL, 10);
}

static int64_t kbo_secondary_draft_sql_i64(char** vals, int index)
{
    if (vals == NULL || vals[index] == NULL) {
        return 0;
    }
    return (int64_t)strtoll(vals[index], NULL, 10);
}

static void kbo_secondary_draft_sql_copy(char* out, size_t out_size, char** vals, int index)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    snprintf(out, out_size, "%s", vals != NULL && vals[index] != NULL ? vals[index] : "");
}

static int kbo_secondary_draft_sql_run_cb(void* user_data, int ncols, char** vals, char** names)
{
    (void)ncols;
    (void)names;
    KboSecondaryDraftRunLoadContext* ctx = (KboSecondaryDraftRunLoadContext*)user_data;
    if (ctx == NULL || ctx->out == NULL || vals == NULL) {
        return 0;
    }
    memset(ctx->out, 0, sizeof(*ctx->out));
    ctx->out->season = kbo_secondary_draft_sql_u32(vals, 0);
    ctx->out->event_yyyymmdd = kbo_secondary_draft_sql_u32(vals, 1);
    ctx->out->league_id = kbo_secondary_draft_sql_u32(vals, 2);
    ctx->out->pick_count = kbo_secondary_draft_sql_int(vals, 3);
    ctx->out->candidate_count = kbo_secondary_draft_sql_int(vals, 4);
    ctx->out->protected_count = kbo_secondary_draft_sql_int(vals, 5);
    ctx->out->cash_total = kbo_secondary_draft_sql_i64(vals, 6);
    ctx->found = 1;
    return 0;
}

static int kbo_secondary_draft_sql_result_cb(void* user_data, int ncols, char** vals, char** names)
{
    (void)ncols;
    (void)names;
    KboSecondaryDraftResultLoadContext* ctx = (KboSecondaryDraftResultLoadContext*)user_data;
    if (ctx == NULL || ctx->rows == NULL || vals == NULL || ctx->count >= ctx->max_rows) {
        return 0;
    }
    KboSecondaryDraftPickRow* row = &ctx->rows[ctx->count++];
    memset(row, 0, sizeof(*row));
    row->round = kbo_secondary_draft_sql_u32(vals, 0);
    row->pick_no = kbo_secondary_draft_sql_u32(vals, 1);
    row->player_id = kbo_secondary_draft_sql_u32(vals, 2);
    row->from_team_id = kbo_secondary_draft_sql_u32(vals, 3);
    row->to_team_id = kbo_secondary_draft_sql_u32(vals, 4);
    row->cash_amount = kbo_secondary_draft_sql_u32(vals, 5);
    row->moved = kbo_secondary_draft_sql_int(vals, 6);
    row->cash_applied = kbo_secondary_draft_sql_int(vals, 7);
    row->military_rights_transfer = kbo_secondary_draft_sql_int(vals, 8);
    kbo_secondary_draft_sql_copy(row->player_name, sizeof(row->player_name), vals, 9);
    kbo_secondary_draft_sql_copy(row->from_team_name, sizeof(row->from_team_name), vals, 10);
    kbo_secondary_draft_sql_copy(row->to_team_name, sizeof(row->to_team_name), vals, 11);
    return 0;
}

int kbo_secondary_draft_ensure_schema(const char* source)
{
    static const char* sql =
        "CREATE TABLE IF NOT EXISTS secondary_draft_runs ("
        "season INTEGER PRIMARY KEY,"
        "event_yyyymmdd INTEGER NOT NULL,"
        "league_id INTEGER NOT NULL,"
        "pick_count INTEGER NOT NULL,"
        "candidate_count INTEGER NOT NULL,"
        "protected_count INTEGER NOT NULL,"
        "cash_total INTEGER NOT NULL,"
        "source TEXT NOT NULL,"
        "completed_at TEXT NOT NULL DEFAULT (datetime('now'))"
        ");"
        "CREATE TABLE IF NOT EXISTS secondary_draft_results ("
        "season INTEGER NOT NULL,"
        "event_yyyymmdd INTEGER NOT NULL,"
        "league_id INTEGER NOT NULL,"
        "pick_no INTEGER NOT NULL,"
        "round INTEGER NOT NULL,"
        "player_id INTEGER NOT NULL,"
        "player_name TEXT NOT NULL,"
        "from_team_id INTEGER NOT NULL,"
        "from_team_name TEXT NOT NULL,"
        "to_team_id INTEGER NOT NULL,"
        "to_team_name TEXT NOT NULL,"
        "cash_amount INTEGER NOT NULL,"
        "cash_applied INTEGER NOT NULL,"
        "moved INTEGER NOT NULL,"
        "military_rights_transfer INTEGER NOT NULL DEFAULT 0,"
        "source TEXT NOT NULL,"
        "created_at TEXT NOT NULL DEFAULT (datetime('now')),"
        "PRIMARY KEY(season, pick_no),"
        "UNIQUE(season, player_id)"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_secondary_draft_results_player "
        "ON secondary_draft_results(player_id, season);"
        "CREATE TABLE IF NOT EXISTS secondary_draft_protected_players ("
        "season INTEGER NOT NULL,"
        "team_id INTEGER NOT NULL,"
        "player_id INTEGER NOT NULL,"
        "player_name TEXT NOT NULL,"
        "team_name TEXT NOT NULL,"
        "source TEXT NOT NULL,"
        "updated_at TEXT NOT NULL DEFAULT (datetime('now')),"
        "PRIMARY KEY(season, team_id, player_id)"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_secondary_draft_protected_team "
        "ON secondary_draft_protected_players(season, team_id);"
        "CREATE TABLE IF NOT EXISTS secondary_draft_list_submissions ("
        "season INTEGER NOT NULL,"
        "team_id INTEGER NOT NULL,"
        "team_name TEXT NOT NULL,"
        "player_count INTEGER NOT NULL,"
        "source TEXT NOT NULL,"
        "submitted_at TEXT NOT NULL DEFAULT (datetime('now')),"
        "PRIMARY KEY(season, team_id)"
        ");"
        "CREATE TABLE IF NOT EXISTS secondary_draft_windows ("
        "season INTEGER PRIMARY KEY,"
        "league_id INTEGER NOT NULL,"
        "protection_open_yyyymmdd INTEGER NOT NULL,"
        "protection_deadline_yyyymmdd INTEGER NOT NULL,"
        "draft_yyyymmdd INTEGER NOT NULL,"
        "source TEXT NOT NULL,"
        "updated_at TEXT NOT NULL DEFAULT (datetime('now'))"
        ");"
        "CREATE TABLE IF NOT EXISTS secondary_draft_news_marks ("
        "season INTEGER NOT NULL,"
        "news_key TEXT NOT NULL,"
        "source TEXT NOT NULL,"
        "created_at TEXT NOT NULL DEFAULT (datetime('now')),"
        "PRIMARY KEY(season, news_key)"
        ");"
        "INSERT OR REPLACE INTO kbo_schema(schema_key, schema_version, updated_at) "
        "VALUES('secondary_draft', 1, datetime('now'));";
    return kbo_save_state_exec(sql, source != NULL ? source : "secondary_draft_schema");
}

static int kbo_secondary_draft_sql_count_query(const char* sql, const char* source)
{
    KboSecondaryDraftSqlCount count = {0};
    if (!kbo_save_state_query(sql, kbo_secondary_draft_sql_count_cb, &count, source)) {
        return 0;
    }
    return count.found ? count.count : 0;
}

int kbo_secondary_draft_sql_run_exists(uint32_t season)
{
    if (season == 0u || !kbo_secondary_draft_ensure_schema("secondary_draft_run_exists_schema")) {
        return 0;
    }
    char sql[256] = {0};
    snprintf(sql, sizeof(sql), "SELECT COUNT(1) FROM secondary_draft_runs WHERE season=%u;", season);
    return kbo_secondary_draft_sql_count_query(sql, "secondary_draft_run_exists") > 0;
}

int kbo_secondary_draft_sql_result_count(uint32_t season)
{
    if (season == 0u || !kbo_secondary_draft_ensure_schema("secondary_draft_result_count_schema")) {
        return 0;
    }
    char sql[256] = {0};
    snprintf(sql, sizeof(sql), "SELECT COUNT(1) FROM secondary_draft_results WHERE season=%u;", season);
    return kbo_secondary_draft_sql_count_query(sql, "secondary_draft_result_count");
}

int kbo_secondary_draft_sql_result_player_exists(uint32_t season, uint32_t player_id)
{
    if (season == 0u || player_id == 0u
            || !kbo_secondary_draft_ensure_schema("secondary_draft_result_player_schema")) {
        return 0;
    }
    char sql[256] = {0};
    snprintf(
        sql,
        sizeof(sql),
        "SELECT COUNT(1) FROM secondary_draft_results WHERE season=%u AND player_id=%u;",
        season,
        player_id);
    return kbo_secondary_draft_sql_count_query(sql, "secondary_draft_result_player_exists") > 0;
}

int kbo_secondary_draft_sql_news_mark_exists(uint32_t season, const char* news_key)
{
    if (season == 0u || news_key == NULL || news_key[0] == '\0'
            || !kbo_secondary_draft_ensure_schema("secondary_draft_news_exists_schema")) {
        return 0;
    }
    char escaped_key[128] = {0};
    if (!kbo_sql_escape_literal(escaped_key, sizeof(escaped_key), news_key)) {
        return 0;
    }
    char sql[384] = {0};
    snprintf(
        sql,
        sizeof(sql),
        "SELECT COUNT(1) FROM secondary_draft_news_marks WHERE season=%u AND news_key='%s';",
        season,
        escaped_key);
    return kbo_secondary_draft_sql_count_query(sql, "secondary_draft_news_exists") > 0;
}

int kbo_secondary_draft_sql_mark_news(uint32_t season, const char* news_key, const char* source)
{
    if (season == 0u || news_key == NULL || news_key[0] == '\0'
            || !kbo_secondary_draft_ensure_schema("secondary_draft_news_mark_schema")) {
        return 0;
    }
    char escaped_key[128] = {0};
    char escaped_source[256] = {0};
    if (!kbo_sql_escape_literal(escaped_key, sizeof(escaped_key), news_key)
            || !kbo_sql_escape_literal(
                escaped_source,
                sizeof(escaped_source),
                source != NULL ? source : "secondary_draft_news")) {
        return 0;
    }
    char sql[512] = {0};
    snprintf(
        sql,
        sizeof(sql),
        "INSERT OR IGNORE INTO secondary_draft_news_marks(season,news_key,source,created_at) "
        "VALUES(%u,'%s','%s',datetime('now'));",
        season,
        escaped_key,
        escaped_source);
    return kbo_save_state_exec(sql, "secondary_draft_news_mark");
}

int kbo_secondary_draft_sql_mark_run(
    uint32_t season,
    uint32_t event_yyyymmdd,
    uint32_t league_id,
    int pick_count,
    int candidate_count,
    int protected_count,
    int64_t cash_total,
    const char* source)
{
    if (season == 0u || !kbo_secondary_draft_ensure_schema("secondary_draft_mark_run_schema")) {
        return 0;
    }
    char escaped_source[256] = {0};
    if (!kbo_sql_escape_literal(
            escaped_source,
            sizeof(escaped_source),
            source != NULL ? source : "secondary_draft")) {
        return 0;
    }
    char sql[768] = {0};
    snprintf(
        sql,
        sizeof(sql),
        "INSERT OR REPLACE INTO secondary_draft_runs("
        "season,event_yyyymmdd,league_id,pick_count,candidate_count,protected_count,cash_total,source,completed_at"
        ") VALUES(%u,%u,%u,%d,%d,%d,%lld,'%s',datetime('now'));",
        season,
        event_yyyymmdd,
        league_id,
        pick_count,
        candidate_count,
        protected_count,
        (long long)cash_total,
        escaped_source);
    return kbo_save_state_exec(sql, "secondary_draft_mark_run");
}

int kbo_secondary_draft_sql_append_pick(
    uint32_t season,
    uint32_t event_yyyymmdd,
    uint32_t league_id,
    const KboSecondaryDraftPickRow* pick,
    const char* source)
{
    if (season == 0u || pick == NULL || pick->player_id == 0u
            || !kbo_secondary_draft_ensure_schema("secondary_draft_append_pick_schema")) {
        return 0;
    }
    char player_name[256] = {0};
    char from_name[256] = {0};
    char to_name[256] = {0};
    char escaped_source[256] = {0};
    if (!kbo_sql_escape_literal(player_name, sizeof(player_name), pick->player_name)
            || !kbo_sql_escape_literal(from_name, sizeof(from_name), pick->from_team_name)
            || !kbo_sql_escape_literal(to_name, sizeof(to_name), pick->to_team_name)
            || !kbo_sql_escape_literal(
                escaped_source,
                sizeof(escaped_source),
                source != NULL ? source : "secondary_draft")) {
        return 0;
    }
    char sql[2048] = {0};
    snprintf(
        sql,
        sizeof(sql),
        "INSERT OR IGNORE INTO secondary_draft_results("
        "season,event_yyyymmdd,league_id,pick_no,round,player_id,player_name,"
        "from_team_id,from_team_name,to_team_id,to_team_name,cash_amount,cash_applied,moved,"
        "military_rights_transfer,source"
        ") VALUES(%u,%u,%u,%u,%u,%u,'%s',%u,'%s',%u,'%s',%u,%d,%d,%d,'%s');",
        season,
        event_yyyymmdd,
        league_id,
        pick->pick_no,
        pick->round,
        pick->player_id,
        player_name,
        pick->from_team_id,
        from_name,
        pick->to_team_id,
        to_name,
        pick->cash_amount,
        pick->cash_applied,
        pick->moved,
        pick->military_rights_transfer,
        escaped_source);
    return kbo_save_state_exec(sql, "secondary_draft_append_pick");
}

int kbo_secondary_draft_load_run_summary(uint32_t season, KboSecondaryDraftRunSummary* out)
{
    if (out != NULL) {
        memset(out, 0, sizeof(*out));
    }
    if (season == 0u || out == NULL
            || !kbo_secondary_draft_ensure_schema("secondary_draft_load_run_summary_schema")) {
        return 0;
    }

    char sql[512] = {0};
    snprintf(
        sql,
        sizeof(sql),
        "SELECT season,event_yyyymmdd,league_id,pick_count,candidate_count,protected_count,cash_total "
        "FROM secondary_draft_runs WHERE season=%u LIMIT 1;",
        season);
    KboSecondaryDraftRunLoadContext ctx = {out, 0};
    if (!kbo_save_state_query(sql, kbo_secondary_draft_sql_run_cb, &ctx, "secondary_draft_load_run_summary")) {
        memset(out, 0, sizeof(*out));
        return 0;
    }
    return ctx.found;
}

int kbo_secondary_draft_load_results(uint32_t season, KboSecondaryDraftPickRow* out, int max_rows)
{
    if (out == NULL || max_rows <= 0) {
        return 0;
    }
    memset(out, 0, (size_t)max_rows * sizeof(out[0]));
    if (season == 0u || !kbo_secondary_draft_ensure_schema("secondary_draft_load_results_schema")) {
        return 0;
    }

    char sql[768] = {0};
    snprintf(
        sql,
        sizeof(sql),
        "SELECT round,pick_no,player_id,from_team_id,to_team_id,cash_amount,moved,cash_applied,"
        "military_rights_transfer,player_name,from_team_name,to_team_name "
        "FROM secondary_draft_results WHERE season=%u ORDER BY pick_no ASC LIMIT %d;",
        season,
        max_rows);
    KboSecondaryDraftResultLoadContext ctx = {out, max_rows, 0};
    if (!kbo_save_state_query(sql, kbo_secondary_draft_sql_result_cb, &ctx, "secondary_draft_load_results")) {
        memset(out, 0, (size_t)max_rows * sizeof(out[0]));
        return 0;
    }
    return ctx.count;
}
