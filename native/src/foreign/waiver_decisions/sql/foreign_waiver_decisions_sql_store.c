#include "foreign_waiver_decisions_sql_store.h"

#include <stdio.h>
#include <string.h>

#include "../../../core/logging/core_log.h"
#include "../../../core/sql/escape/core_sql_escape.h"
#include "../../../core/sql/save_state/save_state_sqlite.h"

typedef struct KboForeignWaiverDecisionExistsResult {
    int found;
} KboForeignWaiverDecisionExistsResult;

typedef struct KboForeignWaiverLatestActionResult {
    char* out_action;
    size_t out_action_size;
    int found;
} KboForeignWaiverLatestActionResult;

static int kbo_foreign_waiver_decisions_sql_ensure_schema(const char* source)
{
    static const char* sql =
        "CREATE TABLE IF NOT EXISTS foreign_waiver_decisions ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "decision_date INTEGER NOT NULL,"
        "window_start INTEGER NOT NULL,"
        "window_end INTEGER NOT NULL,"
        "source TEXT NOT NULL,"
        "action TEXT NOT NULL,"
        "team_id INTEGER NOT NULL,"
        "player_id INTEGER NOT NULL,"
        "value_score INTEGER NOT NULL,"
        "forced INTEGER NOT NULL,"
        "executed INTEGER NOT NULL,"
        "created_at TEXT NOT NULL DEFAULT (datetime('now'))"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_foreign_waiver_decisions_lookup "
        "ON foreign_waiver_decisions(window_end, team_id, player_id, id);"
        "CREATE INDEX IF NOT EXISTS idx_foreign_waiver_decisions_breakdown "
        "ON foreign_waiver_decisions(window_end, action, source);"
        "INSERT OR REPLACE INTO kbo_schema(schema_key, schema_version, updated_at) "
        "VALUES('foreign_waiver_decisions', 1, datetime('now'));";
    return kbo_save_state_exec(sql, source != NULL ? source : "foreign_waiver_decisions_schema");
}

static int kbo_foreign_waiver_decisions_exists_cb(void* user_data, int ncols, char** vals, char** names)
{
    (void)ncols;
    (void)vals;
    (void)names;
    KboForeignWaiverDecisionExistsResult* result = (KboForeignWaiverDecisionExistsResult*)user_data;
    if (result != NULL) {
        result->found = 1;
    }
    return 0;
}

static int kbo_foreign_waiver_decisions_latest_action_cb(void* user_data, int ncols, char** vals, char** names)
{
    (void)names;
    KboForeignWaiverLatestActionResult* result = (KboForeignWaiverLatestActionResult*)user_data;
    if (result == NULL || result->out_action == NULL || result->out_action_size == 0u
            || ncols <= 0 || vals == NULL || vals[0] == NULL) {
        return 0;
    }
    snprintf(result->out_action, result->out_action_size, "%s", vals[0]);
    result->found = 1;
    return 0;
}

static int kbo_foreign_waiver_decisions_breakdown_cb(void* user_data, int ncols, char** vals, char** names)
{
    (void)names;
    KboForeignWaiverDecisionBreakdown* breakdown = (KboForeignWaiverDecisionBreakdown*)user_data;
    if (breakdown == NULL || ncols < 2 || vals == NULL || vals[0] == NULL || vals[1] == NULL) {
        return 0;
    }

    const char* source = vals[0];
    const char* action = vals[1];
    if (_stricmp(action, "RETAIN") == 0) {
        breakdown->retained++;
        breakdown->available = 1;
        if (_stricmp(source, "ai") == 0) {
            breakdown->ai_retained++;
        } else if (_stricmp(source, "user") == 0) {
            breakdown->user_retained++;
        }
    } else if (_stricmp(action, "SKIP") == 0) {
        breakdown->skipped++;
        breakdown->available = 1;
        if (_stricmp(source, "ai") == 0) {
            breakdown->ai_skipped++;
        } else if (_stricmp(source, "user") == 0) {
            breakdown->user_skipped++;
        }
    }
    return 0;
}

int kbo_foreign_waiver_decisions_sql_append(
    const char* source,
    const char* action,
    uint32_t decision_date,
    uint32_t window_start,
    uint32_t window_end,
    uint32_t team_id,
    uint32_t player_id,
    int score,
    int forced,
    int executed)
{
    if (source == NULL || source[0] == '\0' || action == NULL || action[0] == '\0'
            || team_id == 0u || player_id == 0u
            || !kbo_foreign_waiver_decisions_sql_ensure_schema("foreign_waiver_decisions_append_schema")) {
        return 0;
    }

    char escaped_source[128] = {0};
    char escaped_action[64] = {0};
    if (!kbo_sql_escape_literal(escaped_source, sizeof(escaped_source), source)
            || !kbo_sql_escape_literal(escaped_action, sizeof(escaped_action), action)) {
        kbo_log_runtimef(
            "foreign waiver decision: sqlite append failed reason=escape source=%s action=%s team=%u player=%u",
            source,
            action,
            team_id,
            player_id);
        return 0;
    }

    char sql[768] = {0};
    snprintf(
        sql,
        sizeof(sql),
        "INSERT INTO foreign_waiver_decisions("
        "decision_date, window_start, window_end, source, action, team_id, player_id, value_score, forced, executed"
        ") VALUES(%u, %u, %u, '%s', '%s', %u, %u, %d, %d, %d);",
        decision_date,
        window_start,
        window_end,
        escaped_source,
        escaped_action,
        team_id,
        player_id,
        score,
        forced ? 1 : 0,
        executed ? 1 : 0);
    return kbo_save_state_exec(sql, "foreign_waiver_decisions_append");
}

int kbo_foreign_waiver_decisions_sql_exists(
    uint32_t window_end,
    uint32_t team_id,
    uint32_t player_id)
{
    if (window_end == 0u || team_id == 0u || player_id == 0u
            || !kbo_foreign_waiver_decisions_sql_ensure_schema("foreign_waiver_decisions_exists_schema")) {
        return 0;
    }

    char sql[256] = {0};
    snprintf(
        sql,
        sizeof(sql),
        "SELECT 1 FROM foreign_waiver_decisions "
        "WHERE window_end=%u AND team_id=%u AND player_id=%u LIMIT 1;",
        window_end,
        team_id,
        player_id);
    KboForeignWaiverDecisionExistsResult result = {0};
    kbo_save_state_query(sql, kbo_foreign_waiver_decisions_exists_cb, &result, "foreign_waiver_decisions_exists");
    return result.found;
}

int kbo_foreign_waiver_decisions_sql_latest_action(
    uint32_t window_end,
    uint32_t team_id,
    uint32_t player_id,
    char* out_action,
    size_t out_action_size)
{
    if (out_action != NULL && out_action_size > 0u) {
        out_action[0] = '\0';
    }
    if (window_end == 0u || team_id == 0u || player_id == 0u
            || out_action == NULL || out_action_size == 0u
            || !kbo_foreign_waiver_decisions_sql_ensure_schema("foreign_waiver_decisions_latest_schema")) {
        return 0;
    }

    char sql[320] = {0};
    snprintf(
        sql,
        sizeof(sql),
        "SELECT action FROM foreign_waiver_decisions "
        "WHERE window_end=%u AND team_id=%u AND player_id=%u "
        "ORDER BY id DESC LIMIT 1;",
        window_end,
        team_id,
        player_id);
    KboForeignWaiverLatestActionResult result = {out_action, out_action_size, 0};
    kbo_save_state_query(
        sql,
        kbo_foreign_waiver_decisions_latest_action_cb,
        &result,
        "foreign_waiver_decisions_latest");
    return result.found;
}

int kbo_foreign_waiver_decisions_sql_breakdown(
    uint32_t window_end,
    KboForeignWaiverDecisionBreakdown* out_breakdown)
{
    if (out_breakdown != NULL) {
        memset(out_breakdown, 0, sizeof(*out_breakdown));
    }
    if (window_end == 0u || out_breakdown == NULL
            || !kbo_foreign_waiver_decisions_sql_ensure_schema("foreign_waiver_decisions_breakdown_schema")) {
        return 0;
    }

    char sql[224] = {0};
    snprintf(
        sql,
        sizeof(sql),
        "SELECT source, action FROM foreign_waiver_decisions "
        "WHERE window_end=%u ORDER BY id;",
        window_end);
    return kbo_save_state_query(
        sql,
        kbo_foreign_waiver_decisions_breakdown_cb,
        out_breakdown,
        "foreign_waiver_decisions_breakdown");
}
