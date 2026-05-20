#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "fa_compensation_protected_lists_sql_store.h"

#include <stdio.h>
#include <string.h>

#include "../../../core/logging/core_log.h"
#include "../../../core/sql/escape/core_sql_escape.h"
#include "../../../core/sql/save_state/save_state_sqlite.h"

static int kbo_fa_compensation_protected_lists_sql_ensure_schema(const char* source)
{
    static const char* sql =
        "CREATE TABLE IF NOT EXISTS fa_compensation_protected_lists ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "fa_player_id INTEGER NOT NULL,"
        "season INTEGER NOT NULL,"
        "grade TEXT NOT NULL DEFAULT '',"
        "original_team_id INTEGER NOT NULL DEFAULT 0,"
        "signing_team_id INTEGER NOT NULL DEFAULT 0,"
        "signed_on_yyyymmdd INTEGER NOT NULL DEFAULT 0,"
        "due_on_yyyymmdd INTEGER NOT NULL DEFAULT 0,"
        "generated_on_yyyymmdd INTEGER NOT NULL DEFAULT 0,"
        "protect_count INTEGER NOT NULL DEFAULT 0,"
        "protected_count INTEGER NOT NULL DEFAULT 0,"
        "protected_player_ids TEXT NOT NULL DEFAULT '',"
        "protected_player_names TEXT NOT NULL DEFAULT '',"
        "source TEXT NOT NULL DEFAULT '',"
        "created_at TEXT NOT NULL DEFAULT (datetime('now'))"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_fa_compensation_protected_lists_player "
        "ON fa_compensation_protected_lists(fa_player_id, season, generated_on_yyyymmdd);"
        "INSERT OR REPLACE INTO kbo_schema(schema_key, schema_version, updated_at) "
        "VALUES('fa_compensation_protected_lists', 1, datetime('now'));";
    return kbo_save_state_exec(sql, source != NULL ? source : "fa_compensation_protected_lists_schema");
}

int kbo_fa_compensation_protected_lists_sql_path(char* out, size_t out_size)
{
    return kbo_save_state_db_path(out, out_size);
}

int kbo_fa_compensation_protected_lists_sql_append(
    const KboFaCompensationRecord* rec,
    uint32_t due_yyyymmdd,
    uint32_t generated_yyyymmdd,
    const KboFaProtectedCandidate* candidates,
    int candidate_count,
    const char* source)
{
    if (rec == NULL || candidates == NULL || candidate_count <= 0
            || !kbo_fa_compensation_protected_lists_sql_ensure_schema("fa_compensation_protected_lists_append_schema")) {
        return 0;
    }

    int protected_count = candidate_count;
    if (protected_count > (int)rec->protect_count) {
        protected_count = (int)rec->protect_count;
    }

    char ids[1024] = {0};
    char names[4096] = {0};
    for (int i = 0; i < protected_count; i++) {
        char chunk[32] = {0};
        snprintf(chunk, sizeof(chunk), "%s%u", i == 0 ? "" : ";", candidates[i].player_id);
        strncat(ids, chunk, sizeof(ids) - strlen(ids) - 1u);
        snprintf(chunk, sizeof(chunk), "%s", i == 0 ? "" : ";");
        strncat(names, chunk, sizeof(names) - strlen(names) - 1u);
        strncat(names, candidates[i].player_name, sizeof(names) - strlen(names) - 1u);
    }

    char grade[48] = {0};
    char escaped_ids[2200] = {0};
    char escaped_names[8300] = {0};
    char escaped_source[128] = {0};
    if (!kbo_sql_escape_literal(grade, sizeof(grade), rec->grade)
            || !kbo_sql_escape_literal(escaped_ids, sizeof(escaped_ids), ids)
            || !kbo_sql_escape_literal(escaped_names, sizeof(escaped_names), names)
            || !kbo_sql_escape_literal(
                escaped_source,
                sizeof(escaped_source),
                source != NULL ? source : "fa_compensation_protected_list_ai")) {
        return 0;
    }

    char sql[11600] = {0};
    int len = snprintf(
        sql,
        sizeof(sql),
        "INSERT INTO fa_compensation_protected_lists("
        "fa_player_id, season, grade, original_team_id, signing_team_id, signed_on_yyyymmdd, "
        "due_on_yyyymmdd, generated_on_yyyymmdd, protect_count, protected_count, "
        "protected_player_ids, protected_player_names, source, created_at"
        ") VALUES(%u, %u, '%s', %u, %u, %u, %u, %u, %u, %d, '%s', '%s', '%s', datetime('now'));",
        rec->player_id,
        rec->season,
        grade,
        rec->original_team_id,
        rec->signing_team_id,
        rec->signed_on_yyyymmdd,
        due_yyyymmdd,
        generated_yyyymmdd,
        rec->protect_count,
        protected_count,
        escaped_ids,
        escaped_names,
        escaped_source);
    if (len <= 0 || len >= (int)sizeof(sql)) {
        kbo_log_runtimef(
            "KBO FA protected list sqlite append failed reason=sql_buffer_full player=%u protected=%d",
            rec->player_id,
            protected_count);
        return 0;
    }
    return kbo_save_state_exec(sql, "fa_compensation_protected_lists_append");
}
