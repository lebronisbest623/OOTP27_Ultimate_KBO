#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "independent_acquisition_window_sql_store.h"

#include <stdio.h>

#include "../../../../core/logging/core_log.h"
#include "../../../../core/sql/escape/core_sql_escape.h"
#include "../../../../core/sql/save_state/save_state_sqlite.h"

typedef struct KboIndependentAcquisitionWindowSqlLoadContext {
    uint32_t open_date;
    int found;
} KboIndependentAcquisitionWindowSqlLoadContext;

static int kbo_independent_acquisition_window_sql_ensure_schema(const char* source)
{
    static const char* sql =
        "CREATE TABLE IF NOT EXISTS independent_acquisition_window ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "season INTEGER NOT NULL UNIQUE,"
        "open_date INTEGER NOT NULL,"
        "source TEXT NOT NULL DEFAULT '',"
        "updated_at TEXT NOT NULL DEFAULT (datetime('now'))"
        ");"
        "INSERT OR REPLACE INTO kbo_schema(schema_key, schema_version, updated_at) "
        "VALUES('independent_acquisition_window', 1, datetime('now'));";
    return kbo_save_state_exec(sql, source != NULL ? source : "independent_acquisition_window_schema");
}

static int kbo_independent_acquisition_window_sql_load_cb(
    void* user_data,
    int ncols,
    char** vals,
    char** names)
{
    (void)ncols;
    (void)names;
    KboIndependentAcquisitionWindowSqlLoadContext* ctx =
        (KboIndependentAcquisitionWindowSqlLoadContext*)user_data;
    if (ctx == NULL || vals == NULL || vals[0] == NULL) {
        return 0;
    }

    unsigned int value = 0u;
    if (sscanf(vals[0], "%u", &value) == 1) {
        ctx->open_date = (uint32_t)value;
        ctx->found = 1;
    }
    return 0;
}

int kbo_independent_acquisition_window_sql_load_open_date(uint32_t* out_open_date)
{
    if (out_open_date == NULL
            || !kbo_independent_acquisition_window_sql_ensure_schema("independent_acquisition_window_load_schema")) {
        return 0;
    }
    *out_open_date = 0u;

    static const char* sql =
        "SELECT open_date FROM independent_acquisition_window "
        "ORDER BY season DESC, id DESC LIMIT 1;";
    KboIndependentAcquisitionWindowSqlLoadContext ctx = {0};
    if (!kbo_save_state_query(
            sql,
            kbo_independent_acquisition_window_sql_load_cb,
            &ctx,
            "independent_acquisition_window_load")) {
        return 0;
    }
    if (!ctx.found) {
        return 0;
    }
    *out_open_date = ctx.open_date;
    return 1;
}

int kbo_independent_acquisition_window_sql_store_open_date(
    uint32_t open_date,
    const char* source)
{
    if (open_date == 0u
            || !kbo_independent_acquisition_window_sql_ensure_schema("independent_acquisition_window_store_schema")) {
        return 0;
    }

    char escaped_source[256] = {0};
    if (!kbo_sql_escape_literal(escaped_source, sizeof(escaped_source), source != NULL ? source : "")) {
        return 0;
    }

    uint32_t season = open_date / 10000u;
    char sql[768] = {0};
    int len = snprintf(
        sql,
        sizeof(sql),
        "INSERT OR REPLACE INTO independent_acquisition_window("
        "season, open_date, source, updated_at"
        ") VALUES(%u, %u, '%s', datetime('now'));",
        season,
        open_date,
        escaped_source);
    if (len <= 0 || len >= (int)sizeof(sql)) {
        kbo_log_runtimef(
            "KBO independent futures acquisition window sqlite store skipped date=%u reason=sql_buffer_full",
            open_date);
        return 0;
    }

    return kbo_save_state_exec(sql, "independent_acquisition_window_store");
}
