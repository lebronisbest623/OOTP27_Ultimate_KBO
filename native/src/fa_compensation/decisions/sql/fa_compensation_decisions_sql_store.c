#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "fa_compensation_decisions_sql_store.h"

#include <stdio.h>
#include <string.h>

#include "../../../core/logging/core_log.h"
#include "../../../core/sql/escape/core_sql_escape.h"
#include "../../../core/sql/save_state/save_state_sqlite.h"

typedef struct KboFaCompensationDecisionSqlResult {
    int found;
    KboFaCompensationDecisionRow row;
} KboFaCompensationDecisionSqlResult;

static int kbo_fa_compensation_decisions_sql_ensure_schema(const char* source)
{
    static const char* sql =
        "CREATE TABLE IF NOT EXISTS fa_compensation_decisions ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "fa_player_id INTEGER NOT NULL,"
        "season INTEGER NOT NULL,"
        "grade TEXT NOT NULL DEFAULT '',"
        "original_team_id INTEGER NOT NULL DEFAULT 0,"
        "signing_team_id INTEGER NOT NULL DEFAULT 0,"
        "signed_on_yyyymmdd INTEGER NOT NULL DEFAULT 0,"
        "due_on_yyyymmdd INTEGER NOT NULL DEFAULT 0,"
        "decided_on_yyyymmdd INTEGER NOT NULL DEFAULT 0,"
        "action TEXT NOT NULL DEFAULT '',"
        "selected_player_id INTEGER NOT NULL DEFAULT 0,"
        "selected_player_name TEXT NOT NULL DEFAULT '',"
        "selected_player_score INTEGER NOT NULL DEFAULT 0,"
        "unprotected_candidate_count INTEGER NOT NULL DEFAULT 0,"
        "cash_with_player INTEGER NOT NULL DEFAULT 0,"
        "cash_only INTEGER NOT NULL DEFAULT 0,"
        "source TEXT NOT NULL DEFAULT '',"
        "created_at TEXT NOT NULL DEFAULT (datetime('now'))"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_fa_compensation_decisions_player "
        "ON fa_compensation_decisions(fa_player_id, decided_on_yyyymmdd, id);"
        "INSERT OR REPLACE INTO kbo_schema(schema_key, schema_version, updated_at) "
        "VALUES('fa_compensation_decisions', 1, datetime('now'));";
    return kbo_save_state_exec(sql, source != NULL ? source : "fa_compensation_decisions_schema");
}

int kbo_fa_compensation_decisions_sql_path(char* out, size_t out_size)
{
    return kbo_save_state_db_path(out, out_size);
}

static uint32_t kbo_fa_compensation_decisions_sql_u32(char** vals, int index)
{
    unsigned int value = 0u;
    if (vals != NULL && vals[index] != NULL) {
        (void)sscanf(vals[index], "%u", &value);
    }
    return (uint32_t)value;
}

static int32_t kbo_fa_compensation_decisions_sql_i32(char** vals, int index)
{
    int value = 0;
    if (vals != NULL && vals[index] != NULL) {
        (void)sscanf(vals[index], "%d", &value);
    }
    return (int32_t)value;
}

static void kbo_fa_compensation_decisions_sql_text(char** vals, int index, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';
    if (vals != NULL && vals[index] != NULL) {
        snprintf(out, out_size, "%s", vals[index]);
    }
}

static int kbo_fa_compensation_decisions_sql_append_row(
    const KboFaCompensationRecord* rec,
    uint32_t due_yyyymmdd,
    uint32_t decided_yyyymmdd,
    const char* action,
    const KboFaProtectedCandidate* selected,
    int unprotected_candidate_count,
    const char* source)
{
    if (rec == NULL || rec->player_id == 0u || rec->season == 0u || action == NULL) {
        return 0;
    }

    char grade[48] = {0};
    char escaped_action[64] = {0};
    char selected_player_name[224] = {0};
    char escaped_source[128] = {0};
    if (!kbo_sql_escape_literal(grade, sizeof(grade), rec->grade)
            || !kbo_sql_escape_literal(escaped_action, sizeof(escaped_action), action)
            || !kbo_sql_escape_literal(
                selected_player_name,
                sizeof(selected_player_name),
                selected != NULL ? selected->player_name : "")
            || !kbo_sql_escape_literal(
                escaped_source,
                sizeof(escaped_source),
                source != NULL ? source : "")) {
        return 0;
    }

    char sql[1800] = {0};
    int len = snprintf(
        sql,
        sizeof(sql),
        "INSERT INTO fa_compensation_decisions("
        "fa_player_id, season, grade, original_team_id, signing_team_id, signed_on_yyyymmdd, "
        "due_on_yyyymmdd, decided_on_yyyymmdd, action, selected_player_id, selected_player_name, "
        "selected_player_score, unprotected_candidate_count, cash_with_player, cash_only, source, created_at"
        ") VALUES(%u, %u, '%s', %u, %u, %u, %u, %u, '%s', %u, '%s', %d, %d, %u, %u, '%s', datetime('now'));",
        rec->player_id,
        rec->season,
        grade,
        rec->original_team_id,
        rec->signing_team_id,
        rec->signed_on_yyyymmdd,
        due_yyyymmdd,
        decided_yyyymmdd,
        escaped_action,
        selected != NULL ? selected->player_id : 0u,
        selected_player_name,
        selected != NULL ? selected->score : 0,
        unprotected_candidate_count,
        rec->cash_with_player,
        rec->cash_only,
        escaped_source);
    if (len <= 0 || len >= (int)sizeof(sql)) {
        kbo_log_runtimef(
            "KBO FA compensation decisions sqlite append failed reason=sql_buffer_full player=%u",
            rec->player_id);
        return 0;
    }
    return kbo_save_state_exec(sql, "fa_compensation_decisions_append");
}

int kbo_fa_compensation_decisions_sql_append_player_decision(
    const KboFaCompensationRecord* rec,
    uint32_t due_yyyymmdd,
    uint32_t decided_yyyymmdd,
    const KboFaProtectedCandidate* selected,
    int unprotected_candidate_count,
    const char* source)
{
    if (rec == NULL || selected == NULL || selected->player_id == 0u
            || !kbo_fa_compensation_decisions_sql_ensure_schema("fa_compensation_decisions_player_schema")) {
        return 0;
    }
    return kbo_fa_compensation_decisions_sql_append_row(
        rec,
        due_yyyymmdd,
        decided_yyyymmdd,
        "PLAYER",
        selected,
        unprotected_candidate_count,
        source != NULL ? source : "fa_compensation_player_ai");
}

int kbo_fa_compensation_decisions_sql_append_cash_only_decision(
    const KboFaCompensationRecord* rec,
    uint32_t due_yyyymmdd,
    uint32_t decided_yyyymmdd,
    const char* source)
{
    if (rec == NULL || rec->player_id == 0u || rec->cash_only == 0u
            || !kbo_fa_compensation_decisions_sql_ensure_schema("fa_compensation_decisions_cash_schema")) {
        return 0;
    }
    return kbo_fa_compensation_decisions_sql_append_row(
        rec,
        due_yyyymmdd,
        decided_yyyymmdd,
        "CASH_ONLY",
        NULL,
        0,
        source != NULL ? source : "fa_compensation_cash_only_ai");
}

static int kbo_fa_compensation_decisions_sql_latest_cb(void* user_data, int ncols, char** vals, char** names)
{
    (void)names;
    KboFaCompensationDecisionSqlResult* result = (KboFaCompensationDecisionSqlResult*)user_data;
    if (result == NULL || vals == NULL || ncols < 16) {
        return 0;
    }
    result->row.fa_player_id = kbo_fa_compensation_decisions_sql_u32(vals, 0);
    result->row.season = kbo_fa_compensation_decisions_sql_u32(vals, 1);
    kbo_fa_compensation_decisions_sql_text(vals, 2, result->row.grade, sizeof(result->row.grade));
    result->row.original_team_id = kbo_fa_compensation_decisions_sql_u32(vals, 3);
    result->row.signing_team_id = kbo_fa_compensation_decisions_sql_u32(vals, 4);
    result->row.signed_on_yyyymmdd = kbo_fa_compensation_decisions_sql_u32(vals, 5);
    result->row.due_on_yyyymmdd = kbo_fa_compensation_decisions_sql_u32(vals, 6);
    result->row.decided_on_yyyymmdd = kbo_fa_compensation_decisions_sql_u32(vals, 7);
    kbo_fa_compensation_decisions_sql_text(vals, 8, result->row.action, sizeof(result->row.action));
    result->row.selected_player_id = kbo_fa_compensation_decisions_sql_u32(vals, 9);
    kbo_fa_compensation_decisions_sql_text(
        vals,
        10,
        result->row.selected_player_name,
        sizeof(result->row.selected_player_name));
    result->row.selected_player_score = kbo_fa_compensation_decisions_sql_i32(vals, 11);
    result->row.unprotected_candidate_count = (int)kbo_fa_compensation_decisions_sql_u32(vals, 12);
    result->row.cash_with_player = kbo_fa_compensation_decisions_sql_u32(vals, 13);
    result->row.cash_only = kbo_fa_compensation_decisions_sql_u32(vals, 14);
    kbo_fa_compensation_decisions_sql_text(vals, 15, result->row.source, sizeof(result->row.source));
    result->found = result->row.fa_player_id != 0u
        && (result->row.selected_player_id != 0u || strcmp(result->row.action, "CASH_ONLY") == 0);
    return 0;
}

int kbo_fa_compensation_decisions_sql_load_latest(
    uint32_t fa_player_id,
    KboFaCompensationDecisionRow* out)
{
    if (out != NULL) {
        memset(out, 0, sizeof(*out));
    }
    if (fa_player_id == 0u || out == NULL
            || !kbo_fa_compensation_decisions_sql_ensure_schema("fa_compensation_decisions_latest_schema")) {
        return 0;
    }

    char sql[768] = {0};
    int len = snprintf(
        sql,
        sizeof(sql),
        "SELECT fa_player_id, season, grade, original_team_id, signing_team_id, signed_on_yyyymmdd, "
        "due_on_yyyymmdd, decided_on_yyyymmdd, action, selected_player_id, selected_player_name, "
        "selected_player_score, unprotected_candidate_count, cash_with_player, cash_only, source "
        "FROM fa_compensation_decisions WHERE fa_player_id=%u "
        "ORDER BY decided_on_yyyymmdd DESC, id DESC LIMIT 1;",
        fa_player_id);
    if (len <= 0 || len >= (int)sizeof(sql)) {
        return 0;
    }

    KboFaCompensationDecisionSqlResult result = {0};
    if (!kbo_save_state_query(
            sql,
            kbo_fa_compensation_decisions_sql_latest_cb,
            &result,
            "fa_compensation_decisions_latest")) {
        return 0;
    }
    if (!result.found) {
        return 0;
    }
    *out = result.row;
    return 1;
}
