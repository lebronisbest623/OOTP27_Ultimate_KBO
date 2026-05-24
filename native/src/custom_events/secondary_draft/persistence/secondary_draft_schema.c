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

int kbo_secondary_draft_is_odd_season(uint32_t event_yyyymmdd)
{
    uint32_t year = event_yyyymmdd / 10000u;
    return year != 0u && (year % 2u) == 1u;
}

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
        "CREATE INDEX IF NOT EXISTS idx_secondary_draft_protected_team "
        "ON secondary_draft_protected_players(season, team_id);"
        "INSERT OR REPLACE INTO kbo_schema(schema_key, schema_version, updated_at) "
        "VALUES('secondary_draft', 4, datetime('now'));";
    return kbo_save_state_exec(sql, source != NULL ? source : "secondary_draft_schema");
}

int kbo_secondary_draft_sql_run_exists(uint32_t season)
{
    if (season == 0u || !kbo_secondary_draft_ensure_schema("secondary_draft_run_exists_schema")) {
        return 0;
    }
    char sql[256] = {0};
    snprintf(
        sql,
        sizeof(sql),
        "SELECT COUNT(1) FROM secondary_draft_runs WHERE season=%u;",
        season);
    KboSecondaryDraftSqlCount count = {0};
    if (!kbo_save_state_query(sql, kbo_secondary_draft_sql_count_cb, &count, "secondary_draft_run_exists")) {
        return 0;
    }
    return count.found && count.count > 0;
}

int kbo_secondary_draft_sql_result_count(uint32_t season)
{
    if (season == 0u || !kbo_secondary_draft_ensure_schema("secondary_draft_result_count_schema")) {
        return 0;
    }
    char sql[256] = {0};
    snprintf(
        sql,
        sizeof(sql),
        "SELECT COUNT(1) FROM secondary_draft_results WHERE season=%u;",
        season);
    KboSecondaryDraftSqlCount count = {0};
    if (!kbo_save_state_query(sql, kbo_secondary_draft_sql_count_cb, &count, "secondary_draft_result_count")) {
        return 0;
    }
    return count.found ? count.count : 0;
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
    (void)source;
    if (season == 0u || !kbo_secondary_draft_ensure_schema("secondary_draft_mark_run_schema")) {
        return 0;
    }
    char sql[512] = {0};
    snprintf(
        sql,
        sizeof(sql),
        "INSERT OR REPLACE INTO secondary_draft_runs("
        "season,event_yyyymmdd,league_id,pick_count,candidate_count,protected_count,cash_total,completed_at"
        ") VALUES(%u,%u,%u,%d,%d,%d,%lld,datetime('now'));",
        season,
        event_yyyymmdd,
        league_id,
        pick_count,
        candidate_count,
        protected_count,
        (long long)cash_total);
    return kbo_save_state_exec(sql, "secondary_draft_mark_run");
}

int kbo_secondary_draft_sql_append_pick(
    uint32_t season,
    uint32_t event_yyyymmdd,
    uint32_t league_id,
    const KboSecondaryDraftPick* pick,
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
            || !kbo_sql_escape_literal(escaped_source, sizeof(escaped_source), source != NULL ? source : "secondary_draft")) {
        return 0;
    }

    char sql[2048] = {0};
    snprintf(
        sql,
        sizeof(sql),
        "INSERT OR IGNORE INTO secondary_draft_results("
        "season,event_yyyymmdd,league_id,pick_no,round,player_id,player_name,"
        "from_team_id,from_team_name,to_team_id,to_team_name,cash_amount,cash_applied,moved,source"
        ") VALUES(%u,%u,%u,%u,%u,%u,'%s',%u,'%s',%u,'%s',%u,%d,%d,'%s');",
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
        escaped_source);
    return kbo_save_state_exec(sql, "secondary_draft_append_pick");
}

int kbo_secondary_draft_completion_valid(uint32_t league_id, uint32_t event_yyyymmdd)
{
    (void)league_id;
    if (!kbo_secondary_draft_is_odd_season(event_yyyymmdd)) {
        return 1;
    }
    uint32_t season = event_yyyymmdd / 10000u;
    return kbo_secondary_draft_sql_run_exists(season)
        || kbo_secondary_draft_sql_result_count(season) > 0;
}
