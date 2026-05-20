#include "foreign_waiver_window_sql_store.h"

#include <stdio.h>

#include "../../../core/sql/escape/core_sql_escape.h"
#include "../../../core/sql/save_state/save_state_sqlite.h"

typedef struct KboForeignWaiverWindowSqlReadResult {
    uint32_t start_yyyymmdd;
    uint32_t end_yyyymmdd;
    int found;
} KboForeignWaiverWindowSqlReadResult;

static int kbo_foreign_waiver_window_sql_ensure_schema(const char* source)
{
    static const char* sql =
        "CREATE TABLE IF NOT EXISTS foreign_waiver_window ("
        "state_key TEXT PRIMARY KEY,"
        "start_yyyymmdd INTEGER NOT NULL,"
        "end_yyyymmdd INTEGER NOT NULL,"
        "reason TEXT NOT NULL DEFAULT '',"
        "updated_at TEXT NOT NULL DEFAULT (datetime('now'))"
        ");"
        "INSERT OR REPLACE INTO kbo_schema(schema_key, schema_version, updated_at) "
        "VALUES('foreign_waiver_window', 1, datetime('now'));";
    return kbo_save_state_exec(sql, source != NULL ? source : "foreign_waiver_window_schema");
}

static int kbo_foreign_waiver_window_sql_parse_u32(const char* text, uint32_t* out)
{
    if (text == NULL || out == NULL) {
        return 0;
    }
    unsigned int value = 0u;
    if (sscanf(text, "%u", &value) != 1) {
        return 0;
    }
    *out = (uint32_t)value;
    return 1;
}

static int kbo_foreign_waiver_window_sql_read_cb(void* user_data, int ncols, char** vals, char** names)
{
    (void)names;
    KboForeignWaiverWindowSqlReadResult* result = (KboForeignWaiverWindowSqlReadResult*)user_data;
    if (result == NULL || ncols < 2 || vals == NULL || vals[0] == NULL || vals[1] == NULL) {
        return 0;
    }
    uint32_t start = 0u;
    uint32_t end = 0u;
    if (!kbo_foreign_waiver_window_sql_parse_u32(vals[0], &start)
            || !kbo_foreign_waiver_window_sql_parse_u32(vals[1], &end)
            || start == 0u
            || end == 0u) {
        return 0;
    }
    result->start_yyyymmdd = start;
    result->end_yyyymmdd = end;
    result->found = 1;
    return 0;
}

int kbo_foreign_waiver_window_sql_read(uint32_t* out_start, uint32_t* out_end)
{
    if (out_start != NULL) {
        *out_start = 0u;
    }
    if (out_end != NULL) {
        *out_end = 0u;
    }
    if (out_start == NULL || out_end == NULL
            || !kbo_foreign_waiver_window_sql_ensure_schema("foreign_waiver_window_read_schema")) {
        return 0;
    }

    static const char* sql =
        "SELECT start_yyyymmdd, end_yyyymmdd FROM foreign_waiver_window "
        "WHERE state_key='current' LIMIT 1;";
    KboForeignWaiverWindowSqlReadResult result = {0};
    if (!kbo_save_state_query(sql, kbo_foreign_waiver_window_sql_read_cb, &result, "foreign_waiver_window_read")
            || !result.found) {
        return 0;
    }

    *out_start = result.start_yyyymmdd;
    *out_end = result.end_yyyymmdd;
    return 1;
}

int kbo_foreign_waiver_window_sql_write(
    uint32_t start_yyyymmdd,
    uint32_t end_yyyymmdd,
    const char* reason)
{
    if (start_yyyymmdd == 0u || end_yyyymmdd == 0u
            || !kbo_foreign_waiver_window_sql_ensure_schema("foreign_waiver_window_write_schema")) {
        return 0;
    }

    char escaped_reason[256] = {0};
    if (!kbo_sql_escape_literal(escaped_reason, sizeof(escaped_reason), reason != NULL ? reason : "")) {
        return 0;
    }

    char sql[640] = {0};
    snprintf(
        sql,
        sizeof(sql),
        "INSERT OR REPLACE INTO foreign_waiver_window("
        "state_key, start_yyyymmdd, end_yyyymmdd, reason, updated_at"
        ") VALUES('current', %u, %u, '%s', datetime('now'));",
        start_yyyymmdd,
        end_yyyymmdd,
        escaped_reason);
    return kbo_save_state_exec(sql, "foreign_waiver_window_write");
}
