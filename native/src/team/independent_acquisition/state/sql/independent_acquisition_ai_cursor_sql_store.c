#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "independent_acquisition_ai_cursor_sql_store.h"

#include <stdio.h>

#include "../../../../core/logging/core_log.h"
#include "../../../../core/sql/escape/core_sql_escape.h"
#include "../../../../core/sql/save_state/save_state_sqlite.h"

typedef struct KboIndependentAcquisitionAiCursorSqlLoadContext {
    uint32_t processed_date;
    int found;
} KboIndependentAcquisitionAiCursorSqlLoadContext;

static int kbo_independent_acquisition_ai_cursor_sql_ensure_schema(const char* source)
{
    static const char* sql =
        "CREATE TABLE IF NOT EXISTS independent_acquisition_ai_cursor ("
        "state_key TEXT PRIMARY KEY,"
        "processed_date INTEGER NOT NULL,"
        "source TEXT NOT NULL DEFAULT '',"
        "updated_at TEXT NOT NULL DEFAULT (datetime('now'))"
        ");"
        "INSERT OR REPLACE INTO kbo_schema(schema_key, schema_version, updated_at) "
        "VALUES('independent_acquisition_ai_cursor', 1, datetime('now'));";
    return kbo_save_state_exec(sql, source != NULL ? source : "independent_acquisition_ai_cursor_schema");
}

static int kbo_independent_acquisition_ai_cursor_sql_load_cb(
    void* user_data,
    int ncols,
    char** vals,
    char** names)
{
    (void)ncols;
    (void)names;
    KboIndependentAcquisitionAiCursorSqlLoadContext* ctx =
        (KboIndependentAcquisitionAiCursorSqlLoadContext*)user_data;
    if (ctx == NULL || vals == NULL || vals[0] == NULL) {
        return 0;
    }

    unsigned int value = 0u;
    if (sscanf(vals[0], "%u", &value) == 1) {
        ctx->processed_date = (uint32_t)value;
        ctx->found = 1;
    }
    return 0;
}

int kbo_independent_acquisition_ai_cursor_sql_load(uint32_t* out_processed_date)
{
    if (out_processed_date == NULL
            || !kbo_independent_acquisition_ai_cursor_sql_ensure_schema("independent_acquisition_ai_cursor_load_schema")) {
        return 0;
    }
    *out_processed_date = 0u;

    static const char* sql =
        "SELECT processed_date FROM independent_acquisition_ai_cursor "
        "WHERE state_key='last_processed' LIMIT 1;";
    KboIndependentAcquisitionAiCursorSqlLoadContext ctx = {0};
    if (!kbo_save_state_query(
            sql,
            kbo_independent_acquisition_ai_cursor_sql_load_cb,
            &ctx,
            "independent_acquisition_ai_cursor_load")) {
        return 0;
    }
    if (!ctx.found) {
        return 0;
    }
    *out_processed_date = ctx.processed_date;
    return 1;
}

int kbo_independent_acquisition_ai_cursor_sql_store(
    uint32_t processed_date,
    const char* source)
{
    if (processed_date == 0u
            || !kbo_independent_acquisition_ai_cursor_sql_ensure_schema("independent_acquisition_ai_cursor_store_schema")) {
        return 0;
    }

    char escaped_source[256] = {0};
    if (!kbo_sql_escape_literal(escaped_source, sizeof(escaped_source), source != NULL ? source : "")) {
        return 0;
    }

    char sql[768] = {0};
    int len = snprintf(
        sql,
        sizeof(sql),
        "INSERT OR REPLACE INTO independent_acquisition_ai_cursor("
        "state_key, processed_date, source, updated_at"
        ") VALUES('last_processed', %u, '%s', datetime('now'));",
        processed_date,
        escaped_source);
    if (len <= 0 || len >= (int)sizeof(sql)) {
        kbo_log_runtimef(
            "independent acquisition AI cursor sqlite store skipped date=%u reason=sql_buffer_full",
            processed_date);
        return 0;
    }

    return kbo_save_state_exec(sql, "independent_acquisition_ai_cursor_store");
}
