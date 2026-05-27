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

typedef struct KboSecondaryDraftIdLoadContext {
    uint32_t* ids;
    int max_ids;
    int count;
} KboSecondaryDraftIdLoadContext;

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

static int kbo_secondary_draft_sql_id_cb(void* user_data, int ncols, char** vals, char** names)
{
    (void)names;
    KboSecondaryDraftIdLoadContext* ctx = (KboSecondaryDraftIdLoadContext*)user_data;
    if (ctx == NULL || ctx->ids == NULL || ncols <= 0 || vals == NULL || vals[0] == NULL
            || ctx->count >= ctx->max_ids) {
        return 0;
    }
    uint32_t id = (uint32_t)strtoul(vals[0], NULL, 10);
    if (id != 0u) {
        ctx->ids[ctx->count++] = id;
    }
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

int kbo_secondary_draft_sql_protected_count(uint32_t season, uint32_t team_id)
{
    if (season == 0u || team_id == 0u
            || !kbo_secondary_draft_ensure_schema("secondary_draft_protected_count_schema")) {
        return 0;
    }
    char sql[256] = {0};
    snprintf(
        sql,
        sizeof(sql),
        "SELECT COUNT(1) FROM secondary_draft_protected_players WHERE season=%u AND team_id=%u;",
        season,
        team_id);
    return kbo_secondary_draft_sql_count_query(sql, "secondary_draft_protected_count");
}

int kbo_secondary_draft_sql_team_submitted(uint32_t season, uint32_t team_id, int* out_count)
{
    if (out_count != NULL) {
        *out_count = 0;
    }
    if (season == 0u || team_id == 0u
            || !kbo_secondary_draft_ensure_schema("secondary_draft_team_submitted_schema")) {
        return 0;
    }
    char sql[256] = {0};
    snprintf(
        sql,
        sizeof(sql),
        "SELECT COUNT(1) FROM secondary_draft_list_submissions WHERE season=%u AND team_id=%u;",
        season,
        team_id);
    int submitted_rows = kbo_secondary_draft_sql_count_query(sql, "secondary_draft_team_submitted_exists");
    int count = kbo_secondary_draft_sql_protected_count(season, team_id);
    if (out_count != NULL) {
        *out_count = count;
    }
    return submitted_rows > 0;
}

int kbo_secondary_draft_sql_load_protected_player_ids(
    uint32_t season,
    uint32_t team_id,
    uint32_t* ids,
    int max_ids)
{
    if (ids == NULL || max_ids <= 0) {
        return 0;
    }
    memset(ids, 0, (size_t)max_ids * sizeof(ids[0]));
    if (season == 0u || team_id == 0u
            || !kbo_secondary_draft_ensure_schema("secondary_draft_load_protected_schema")) {
        return 0;
    }
    char sql[384] = {0};
    snprintf(
        sql,
        sizeof(sql),
        "SELECT player_id FROM secondary_draft_protected_players "
        "WHERE season=%u AND team_id=%u ORDER BY updated_at ASC, player_id ASC LIMIT %d;",
        season,
        team_id,
        max_ids);
    KboSecondaryDraftIdLoadContext ctx = {ids, max_ids, 0};
    if (!kbo_save_state_query(sql, kbo_secondary_draft_sql_id_cb, &ctx, "secondary_draft_load_protected")) {
        return 0;
    }
    return ctx.count;
}

int kbo_secondary_draft_sql_write_protected_player(
    uint32_t season,
    uint32_t team_id,
    uint32_t player_id,
    const char* player_name,
    const char* team_name,
    const char* source)
{
    if (season == 0u || team_id == 0u || player_id == 0u
            || !kbo_secondary_draft_ensure_schema("secondary_draft_write_protected_schema")) {
        return 0;
    }
    char escaped_player[256] = {0};
    char escaped_team[256] = {0};
    char escaped_source[256] = {0};
    if (!kbo_sql_escape_literal(escaped_player, sizeof(escaped_player), player_name != NULL ? player_name : "")
            || !kbo_sql_escape_literal(escaped_team, sizeof(escaped_team), team_name != NULL ? team_name : "")
            || !kbo_sql_escape_literal(
                escaped_source,
                sizeof(escaped_source),
                source != NULL ? source : "secondary_draft_protected")) {
        return 0;
    }
    char sql[1024] = {0};
    snprintf(
        sql,
        sizeof(sql),
        "INSERT OR IGNORE INTO secondary_draft_protected_players("
        "season,team_id,player_id,player_name,team_name,source,updated_at"
        ") VALUES(%u,%u,%u,'%s','%s','%s',datetime('now'));",
        season,
        team_id,
        player_id,
        escaped_player,
        escaped_team,
        escaped_source);
    return kbo_save_state_exec(sql, "secondary_draft_write_protected");
}

int kbo_secondary_draft_sql_submit_team(
    uint32_t season,
    uint32_t team_id,
    const char* team_name,
    int player_count,
    const char* source)
{
    if (season == 0u || team_id == 0u
            || !kbo_secondary_draft_ensure_schema("secondary_draft_submit_team_schema")) {
        return 0;
    }
    char escaped_team[256] = {0};
    char escaped_source[256] = {0};
    if (!kbo_sql_escape_literal(escaped_team, sizeof(escaped_team), team_name != NULL ? team_name : "")
            || !kbo_sql_escape_literal(
                escaped_source,
                sizeof(escaped_source),
                source != NULL ? source : "secondary_draft_submit")) {
        return 0;
    }
    char sql[768] = {0};
    snprintf(
        sql,
        sizeof(sql),
        "INSERT OR REPLACE INTO secondary_draft_list_submissions("
        "season,team_id,team_name,player_count,source,submitted_at"
        ") VALUES(%u,%u,'%s',%d,'%s',datetime('now'));",
        season,
        team_id,
        escaped_team,
        player_count,
        escaped_source);
    return kbo_save_state_exec(sql, "secondary_draft_submit_team");
}

int kbo_secondary_draft_id_list_contains(const uint32_t* ids, int count, uint32_t player_id)
{
    if (ids == NULL || count <= 0 || player_id == 0u) {
        return 0;
    }
    for (int i = 0; i < count; i++) {
        if (ids[i] == player_id) {
            return 1;
        }
    }
    return 0;
}
