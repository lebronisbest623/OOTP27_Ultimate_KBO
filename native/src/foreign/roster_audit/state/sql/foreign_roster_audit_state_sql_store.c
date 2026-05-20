#include "foreign_roster_audit_state_sql_store.h"

#include <stdio.h>

#include "../../../../core/dates/constants/kbo_date_constants.h"
#include "../../../../core/logging/core_log.h"
#include "../../../../core/sql/escape/core_sql_escape.h"
#include "../../../../core/sql/save_state/save_state_sqlite.h"

typedef struct KboForeignRosterAuditSqlDateResult {
    uint32_t date;
} KboForeignRosterAuditSqlDateResult;

static int kbo_foreign_roster_audit_sql_ensure_schema(const char* source)
{
    static const char* sql =
        "CREATE TABLE IF NOT EXISTS foreign_roster_daily_audit_state ("
        "state_key TEXT PRIMARY KEY,"
        "last_audit_date INTEGER NOT NULL DEFAULT 0,"
        "source TEXT NOT NULL DEFAULT '',"
        "updated_at TEXT NOT NULL DEFAULT (datetime('now'))"
        ");"
        "INSERT OR REPLACE INTO kbo_schema(schema_key, schema_version, updated_at) "
        "VALUES('foreign_roster_daily_audit_state', 1, datetime('now'));";
    return kbo_save_state_exec(sql, source != NULL ? source : "foreign_roster_audit_sql_schema");
}

static int kbo_foreign_roster_audit_sql_date_cb(void* user_data, int ncols, char** vals, char** names)
{
    (void)names;
    KboForeignRosterAuditSqlDateResult* result = (KboForeignRosterAuditSqlDateResult*)user_data;
    if (result != NULL && vals != NULL && ncols >= 1 && vals[0] != NULL) {
        unsigned int value = 0u;
        if (sscanf(vals[0], "%u", &value) == 1) {
            result->date = (uint32_t)value;
        }
    }
    return 0;
}

uint32_t kbo_foreign_roster_audit_sql_load_last_audit_date(const char* source)
{
    if (!kbo_foreign_roster_audit_sql_ensure_schema("foreign_roster_audit_load_state_schema")) {
        return 0u;
    }

    KboForeignRosterAuditSqlDateResult result = {0};
    if (!kbo_save_state_query(
            "SELECT last_audit_date FROM foreign_roster_daily_audit_state "
            "WHERE state_key='daily_audit' LIMIT 1;",
            kbo_foreign_roster_audit_sql_date_cb,
            &result,
            "foreign_roster_audit_load_state")) {
        return 0u;
    }
    if (result.date != 0u
            && (result.date < KBO_SEASON_DATE_MIN || result.date > KBO_SIM_DATE_MAX)) {
        kbo_log_runtimef(
            "foreign roster daily audit state ignored source=%s reason=invalid_last_audit_date date=%u store=sqlite",
            source != NULL ? source : "",
            result.date);
        return 0u;
    }
    return result.date;
}

int kbo_foreign_roster_audit_sql_persist_last_audit_date(uint32_t today, const char* source)
{
    if (today == 0u || !kbo_foreign_roster_audit_sql_ensure_schema("foreign_roster_audit_persist_state_schema")) {
        return 0;
    }

    char escaped_source[512] = {0};
    if (!kbo_sql_escape_literal(escaped_source, sizeof(escaped_source), source != NULL ? source : "")) {
        return 0;
    }

    char sql[1024] = {0};
    int len = snprintf(
        sql,
        sizeof(sql),
        "INSERT OR REPLACE INTO foreign_roster_daily_audit_state("
        "state_key, last_audit_date, source, updated_at"
        ") VALUES('daily_audit', %u, '%s', datetime('now'));",
        today,
        escaped_source);
    if (len <= 0 || len >= (int)sizeof(sql)) {
        return 0;
    }
    return kbo_save_state_exec(sql, "foreign_roster_audit_persist_state");
}
