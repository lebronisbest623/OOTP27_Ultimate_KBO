#include "cbt_cash_charge_sql_store.h"

#include <stdio.h>

#include "../../../core/sql/escape/core_sql_escape.h"
#include "../../../core/sql/save_state/save_state_sqlite.h"

typedef struct KboCbtCashChargeSqlExistsResult {
    int found;
} KboCbtCashChargeSqlExistsResult;

static int kbo_cbt_cash_charge_sql_ensure_schema(const char* source)
{
    static const char* sql =
        "CREATE TABLE IF NOT EXISTS cbt_cash_charges ("
        "season INTEGER NOT NULL,"
        "team_id INTEGER NOT NULL,"
        "tax_amount INTEGER NOT NULL DEFAULT 0,"
        "old_cash INTEGER NOT NULL DEFAULT 0,"
        "new_cash INTEGER NOT NULL DEFAULT 0,"
        "applied_date INTEGER NOT NULL DEFAULT 0,"
        "processed_date INTEGER NOT NULL DEFAULT 0,"
        "source TEXT NOT NULL DEFAULT '',"
        "team_name TEXT NOT NULL DEFAULT '',"
        "created_at TEXT NOT NULL DEFAULT (datetime('now')),"
        "PRIMARY KEY(season, team_id)"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_cbt_cash_charges_applied_date "
        "ON cbt_cash_charges(applied_date);"
        "INSERT OR REPLACE INTO kbo_schema(schema_key, schema_version, updated_at) "
        "VALUES('cbt_cash_charges', 1, datetime('now'));";
    return kbo_save_state_exec(sql, source != NULL ? source : "cbt_cash_charges_schema");
}

static int kbo_cbt_cash_charge_sql_exists_cb(void* user_data, int ncols, char** vals, char** names)
{
    (void)ncols;
    (void)vals;
    (void)names;
    KboCbtCashChargeSqlExistsResult* result = (KboCbtCashChargeSqlExistsResult*)user_data;
    if (result != NULL) {
        result->found = 1;
    }
    return 0;
}

int kbo_cbt_cash_charge_sql_already_applied(uint32_t season, uint32_t team_id)
{
    if (season == 0u || team_id == 0u
            || !kbo_cbt_cash_charge_sql_ensure_schema("cbt_cash_charges_exists_schema")) {
        return 0;
    }

    char sql[256] = {0};
    int len = snprintf(
        sql,
        sizeof(sql),
        "SELECT 1 FROM cbt_cash_charges "
        "WHERE season=%u AND team_id=%u LIMIT 1;",
        season,
        team_id);
    if (len <= 0 || len >= (int)sizeof(sql)) {
        return 0;
    }

    KboCbtCashChargeSqlExistsResult result = {0};
    kbo_save_state_query(sql, kbo_cbt_cash_charge_sql_exists_cb, &result, "cbt_cash_charges_exists");
    return result.found;
}

int kbo_cbt_cash_charge_sql_append_ledger(
    const KboCbtRecord* rec,
    int32_t old_cash,
    int32_t new_cash,
    uint32_t applied_yyyymmdd,
    const char* source)
{
    if (rec == NULL || rec->season == 0u || rec->team_id == 0u
            || !kbo_cbt_cash_charge_sql_ensure_schema("cbt_cash_charges_append_schema")) {
        return 0;
    }

    char escaped_source[256] = {0};
    char escaped_team_name[160] = {0};
    if (!kbo_sql_escape_literal(escaped_source, sizeof(escaped_source), source != NULL ? source : "")
            || !kbo_sql_escape_literal(escaped_team_name, sizeof(escaped_team_name), rec->team_name)) {
        return 0;
    }

    char sql[1024] = {0};
    int len = snprintf(
        sql,
        sizeof(sql),
        "INSERT OR IGNORE INTO cbt_cash_charges("
        "season, team_id, tax_amount, old_cash, new_cash, applied_date, "
        "processed_date, source, team_name, created_at"
        ") VALUES(%u, %u, %d, %d, %d, %u, %u, '%s', '%s', datetime('now'));",
        rec->season,
        rec->team_id,
        rec->tax_amount,
        old_cash,
        new_cash,
        applied_yyyymmdd,
        rec->processed_date,
        escaped_source,
        escaped_team_name);
    if (len <= 0 || len >= (int)sizeof(sql)) {
        return 0;
    }
    return kbo_save_state_exec(sql, "cbt_cash_charges_append");
}
