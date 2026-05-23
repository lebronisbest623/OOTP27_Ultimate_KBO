#include "secondary_draft_internal.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "../../core/sql/escape/core_sql_escape.h"
#include "../../core/sql/save_state/save_state_sqlite.h"

typedef struct KboSecondaryDraftSqlCount {
    int found;
    int count;
} KboSecondaryDraftSqlCount;

static int kbo_secondary_draft_store_count_cb(void* user_data, int ncols, char** vals, char** names)
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

static int kbo_secondary_draft_store_scalar_count(const char* sql, const char* source)
{
    KboSecondaryDraftSqlCount count = {0};
    if (!kbo_save_state_query(sql, kbo_secondary_draft_store_count_cb, &count, source)) {
        return 0;
    }
    return count.found ? count.count : 0;
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
    return kbo_secondary_draft_store_scalar_count(sql, "secondary_draft_protected_count");
}

int kbo_secondary_draft_sql_player_protected(uint32_t season, uint32_t team_id, uint32_t player_id)
{
    if (season == 0u || team_id == 0u || player_id == 0u
            || !kbo_secondary_draft_ensure_schema("secondary_draft_player_protected_schema")) {
        return 0;
    }
    char sql[320] = {0};
    snprintf(
        sql,
        sizeof(sql),
        "SELECT COUNT(1) FROM secondary_draft_protected_players "
        "WHERE season=%u AND team_id=%u AND player_id=%u;",
        season,
        team_id,
        player_id);
    return kbo_secondary_draft_store_scalar_count(sql, "secondary_draft_player_protected") > 0;
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
    char sql[320] = {0};
    snprintf(
        sql,
        sizeof(sql),
        "SELECT player_count FROM secondary_draft_list_submissions WHERE season=%u AND team_id=%u;",
        season,
        team_id);
    KboSecondaryDraftSqlCount count = {0};
    if (!kbo_save_state_query(sql, kbo_secondary_draft_store_count_cb, &count, "secondary_draft_team_submitted")) {
        return 0;
    }
    if (out_count != NULL && count.found) {
        *out_count = count.count;
    }
    return count.found;
}

int kbo_secondary_draft_sql_result_player_exists(uint32_t season, uint32_t player_id)
{
    if (season == 0u || player_id == 0u
            || !kbo_secondary_draft_ensure_schema("secondary_draft_result_player_exists_schema")) {
        return 0;
    }
    char sql[320] = {0};
    snprintf(
        sql,
        sizeof(sql),
        "SELECT COUNT(1) FROM secondary_draft_results WHERE season=%u AND player_id=%u;",
        season,
        player_id);
    return kbo_secondary_draft_store_scalar_count(sql, "secondary_draft_result_player_exists") > 0;
}

int kbo_secondary_draft_sql_clear_protected_team(uint32_t season, uint32_t team_id)
{
    if (season == 0u || team_id == 0u
            || !kbo_secondary_draft_ensure_schema("secondary_draft_clear_protected_schema")) {
        return 0;
    }
    char sql[384] = {0};
    snprintf(
        sql,
        sizeof(sql),
        "DELETE FROM secondary_draft_protected_players WHERE season=%u AND team_id=%u;"
        "DELETE FROM secondary_draft_list_submissions WHERE season=%u AND team_id=%u;",
        season,
        team_id,
        season,
        team_id);
    return kbo_save_state_exec(sql, "secondary_draft_clear_protected");
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
            || !kbo_sql_escape_literal(escaped_source, sizeof(escaped_source), source != NULL ? source : "secondary_draft")) {
        return 0;
    }
    char sql[1024] = {0};
    snprintf(
        sql,
        sizeof(sql),
        "INSERT OR REPLACE INTO secondary_draft_protected_players("
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

int kbo_secondary_draft_sql_delete_protected_player(uint32_t season, uint32_t team_id, uint32_t player_id)
{
    if (season == 0u || team_id == 0u || player_id == 0u
            || !kbo_secondary_draft_ensure_schema("secondary_draft_delete_protected_schema")) {
        return 0;
    }
    char sql[320] = {0};
    snprintf(
        sql,
        sizeof(sql),
        "DELETE FROM secondary_draft_protected_players "
        "WHERE season=%u AND team_id=%u AND player_id=%u;",
        season,
        team_id,
        player_id);
    return kbo_save_state_exec(sql, "secondary_draft_delete_protected");
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
            || !kbo_sql_escape_literal(escaped_source, sizeof(escaped_source), source != NULL ? source : "secondary_draft")) {
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
