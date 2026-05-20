#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "fa_compensation_cash_transfer_sql_store.h"

#include <stdio.h>
#include <string.h>

#include "../../../core/logging/core_log.h"
#include "../../../core/sql/escape/core_sql_escape.h"
#include "../../../core/sql/save_state/save_state_sqlite.h"

typedef struct KboFaCompensationCashTransferExistsResult {
    int found;
} KboFaCompensationCashTransferExistsResult;

static int kbo_fa_compensation_cash_transfer_sql_ensure_schema(const char* source)
{
    static const char* sql =
        "CREATE TABLE IF NOT EXISTS fa_compensation_cash_transfers ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "season INTEGER NOT NULL,"
        "player_id INTEGER NOT NULL,"
        "signing_team_id INTEGER NOT NULL,"
        "original_team_id INTEGER NOT NULL,"
        "amount INTEGER NOT NULL DEFAULT 0,"
        "action TEXT NOT NULL DEFAULT '',"
        "applied_yyyymmdd INTEGER NOT NULL DEFAULT 0,"
        "signing_old_cash INTEGER NOT NULL DEFAULT 0,"
        "signing_new_cash INTEGER NOT NULL DEFAULT 0,"
        "original_old_cash INTEGER NOT NULL DEFAULT 0,"
        "original_new_cash INTEGER NOT NULL DEFAULT 0,"
        "source TEXT NOT NULL DEFAULT '',"
        "player_name TEXT NOT NULL DEFAULT '',"
        "created_at TEXT NOT NULL DEFAULT (datetime('now')),"
        "UNIQUE(season, player_id, signing_team_id, original_team_id, action)"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_fa_compensation_cash_transfers_player "
        "ON fa_compensation_cash_transfers(player_id, season);"
        "INSERT OR REPLACE INTO kbo_schema(schema_key, schema_version, updated_at) "
        "VALUES('fa_compensation_cash_transfers', 1, datetime('now'));";
    return kbo_save_state_exec(sql, source != NULL ? source : "fa_compensation_cash_transfers_schema");
}

int kbo_fa_compensation_cash_transfer_sql_path(char* out, size_t out_size)
{
    return kbo_save_state_db_path(out, out_size);
}

static int kbo_fa_compensation_cash_transfer_sql_exists_cb(void* user_data, int ncols, char** vals, char** names)
{
    (void)ncols;
    (void)vals;
    (void)names;
    KboFaCompensationCashTransferExistsResult* result =
        (KboFaCompensationCashTransferExistsResult*)user_data;
    if (result != NULL) {
        result->found = 1;
    }
    return 0;
}

int kbo_fa_compensation_cash_transfer_sql_already_applied(
    uint32_t season,
    uint32_t player_id,
    uint32_t signing_team_id,
    uint32_t original_team_id,
    const char* action)
{
    if (season == 0u || player_id == 0u || signing_team_id == 0u || original_team_id == 0u
            || !kbo_fa_compensation_cash_transfer_sql_ensure_schema("fa_compensation_cash_transfers_exists_schema")) {
        return 0;
    }

    char escaped_action[96] = {0};
    if (!kbo_sql_escape_literal(escaped_action, sizeof(escaped_action), action != NULL ? action : "")) {
        return 0;
    }

    char sql[512] = {0};
    int len = 0;
    if (action != NULL && action[0] != '\0') {
        len = snprintf(
            sql,
            sizeof(sql),
            "SELECT 1 FROM fa_compensation_cash_transfers "
            "WHERE season=%u AND player_id=%u AND signing_team_id=%u AND original_team_id=%u "
            "AND action='%s' LIMIT 1;",
            season,
            player_id,
            signing_team_id,
            original_team_id,
            escaped_action);
        if (len <= 0 || len >= (int)sizeof(sql)) {
            return 0;
        }
    } else {
        len = snprintf(
            sql,
            sizeof(sql),
            "SELECT 1 FROM fa_compensation_cash_transfers "
            "WHERE season=%u AND player_id=%u AND signing_team_id=%u AND original_team_id=%u "
            "LIMIT 1;",
            season,
            player_id,
            signing_team_id,
            original_team_id);
        if (len <= 0 || len >= (int)sizeof(sql)) {
            return 0;
        }
    }

    KboFaCompensationCashTransferExistsResult result = {0};
    (void)kbo_save_state_query(
        sql,
        kbo_fa_compensation_cash_transfer_sql_exists_cb,
        &result,
        "fa_compensation_cash_transfers_exists");
    return result.found;
}

int kbo_fa_compensation_cash_transfer_sql_append(
    const KboFaCompensationRecord* rec,
    uint32_t amount,
    uint32_t applied_yyyymmdd,
    const char* action,
    const char* source,
    int32_t signing_old_cash,
    int32_t signing_new_cash,
    int32_t original_old_cash,
    int32_t original_new_cash)
{
    if (rec == NULL || rec->player_id == 0u || rec->season == 0u
            || !kbo_fa_compensation_cash_transfer_sql_ensure_schema("fa_compensation_cash_transfers_append_schema")) {
        return 0;
    }

    char escaped_action[96] = {0};
    char escaped_source[160] = {0};
    char escaped_name[224] = {0};
    if (!kbo_sql_escape_literal(escaped_action, sizeof(escaped_action), action != NULL ? action : "")
            || !kbo_sql_escape_literal(escaped_source, sizeof(escaped_source), source != NULL ? source : "")
            || !kbo_sql_escape_literal(escaped_name, sizeof(escaped_name), rec->player_name)) {
        return 0;
    }

    char sql[1200] = {0};
    int len = snprintf(
        sql,
        sizeof(sql),
        "INSERT OR IGNORE INTO fa_compensation_cash_transfers("
        "season, player_id, signing_team_id, original_team_id, amount, action, applied_yyyymmdd, "
        "signing_old_cash, signing_new_cash, original_old_cash, original_new_cash, source, player_name, created_at"
        ") VALUES(%u, %u, %u, %u, %u, '%s', %u, %d, %d, %d, %d, '%s', '%s', datetime('now'));",
        rec->season,
        rec->player_id,
        rec->signing_team_id,
        rec->original_team_id,
        amount,
        escaped_action,
        applied_yyyymmdd,
        signing_old_cash,
        signing_new_cash,
        original_old_cash,
        original_new_cash,
        escaped_source,
        escaped_name);
    if (len <= 0 || len >= (int)sizeof(sql)) {
        kbo_log_runtimef(
            "KBO FA compensation cash transfer sqlite append failed reason=sql_buffer_full player=%u",
            rec->player_id);
        return 0;
    }
    return kbo_save_state_exec(sql, "fa_compensation_cash_transfers_append");
}
