#include "cbt_draft_order_ledger_sql_store.h"

#include <stdio.h>

#include "../../../../../core/sql/escape/core_sql_escape.h"
#include "../../../../../core/sql/save_state/save_state_sqlite.h"

static int kbo_cbt_draft_order_ledger_sql_ensure_schema(const char* source)
{
    static const char* sql =
        "CREATE TABLE IF NOT EXISTS cbt_draft_order_penalties ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "season INTEGER NOT NULL,"
        "team_id INTEGER NOT NULL,"
        "round INTEGER NOT NULL,"
        "stages INTEGER NOT NULL DEFAULT 0,"
        "from_pick INTEGER NOT NULL DEFAULT 0,"
        "to_pick INTEGER NOT NULL DEFAULT 0,"
        "source TEXT NOT NULL DEFAULT '',"
        "created_at TEXT NOT NULL DEFAULT (datetime('now'))"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_cbt_draft_order_penalties_season_team "
        "ON cbt_draft_order_penalties(season, team_id);"
        "INSERT OR REPLACE INTO kbo_schema(schema_key, schema_version, updated_at) "
        "VALUES('cbt_draft_order_penalties', 1, datetime('now'));";
    return kbo_save_state_exec(sql, source != NULL ? source : "cbt_draft_order_penalties_schema");
}

int kbo_cbt_draft_order_ledger_sql_append(const KboCbtDraftOrderMove* move, const char* source)
{
    if (move == NULL || move->season == 0u || move->team_id == 0u
            || !kbo_cbt_draft_order_ledger_sql_ensure_schema("cbt_draft_order_penalties_append_schema")) {
        return 0;
    }

    char escaped_source[256] = {0};
    if (!kbo_sql_escape_literal(escaped_source, sizeof(escaped_source), source != NULL ? source : "")) {
        return 0;
    }

    char sql[1024] = {0};
    int len = snprintf(
        sql,
        sizeof(sql),
        "INSERT INTO cbt_draft_order_penalties("
        "season, team_id, round, stages, from_pick, to_pick, source, created_at"
        ") VALUES(%u, %u, %u, %u, %u, %u, '%s', datetime('now'));",
        move->season,
        move->team_id,
        KBO_CBT_DRAFT_ORDER_TARGET_ROUND,
        move->stages,
        move->from_pick,
        move->to_pick,
        escaped_source);
    if (len <= 0 || len >= (int)sizeof(sql)) {
        return 0;
    }
    return kbo_save_state_exec(sql, "cbt_draft_order_penalties_append");
}
