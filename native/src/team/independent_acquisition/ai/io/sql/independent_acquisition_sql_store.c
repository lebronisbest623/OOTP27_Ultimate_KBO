#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "independent_acquisition_sql_store.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "../../../../../core/logging/core_log.h"
#include "../../../../../core/sql/escape/core_sql_escape.h"
#include "../../../../../core/sql/save_state/save_state_sqlite.h"
#include "../../../../../core/sync/lock.h"

typedef struct KboIndependentAcquisitionSqlExistsResult {
    int found;
} KboIndependentAcquisitionSqlExistsResult;

typedef struct KboIndependentAcquisitionSqlCountResult {
    int count;
} KboIndependentAcquisitionSqlCountResult;

typedef struct KboIndependentAcquisitionSqlDateResult {
    uint32_t date;
} KboIndependentAcquisitionSqlDateResult;

typedef struct KboIndependentAcquisitionSqlQueuedLoadContext {
    KboIndependentAcquisitionQueuedRequest* rows;
    int max_count;
    int count;
} KboIndependentAcquisitionSqlQueuedLoadContext;

typedef struct KboIndependentAcquisitionSqlRequestLoadContext {
    KboIndependentAcquisitionSqlRequestRow* rows;
    int max_count;
    int count;
} KboIndependentAcquisitionSqlRequestLoadContext;

typedef struct KboIndependentAcquisitionSqlDecisionKeyLoadContext {
    KboIndependentAcquisitionDecisionKey* rows;
    int max_count;
    int count;
} KboIndependentAcquisitionSqlDecisionKeyLoadContext;

typedef struct KboIndependentAcquisitionSqlDecisionLoadContext {
    KboIndependentAcquisitionSqlDecisionRow* rows;
    int max_count;
    int count;
} KboIndependentAcquisitionSqlDecisionLoadContext;

typedef struct KboIndependentAcquisitionSqlTransferSummaryLoadContext {
    KboIndependentAcquisitionTransferSummary* rows;
    int max_count;
    int count;
} KboIndependentAcquisitionSqlTransferSummaryLoadContext;

static KboLock g_kbo_independent_acquisition_sql_schema_lock = KBO_LOCK_INIT;
static char g_kbo_independent_acquisition_sql_schema_path[MAX_PATH];
static int g_kbo_independent_acquisition_sql_schema_ready = 0;

int kbo_independent_acquisition_sql_ensure_schema(const char* source)
{
    char path[MAX_PATH] = {0};
    if (!kbo_save_state_db_path(path, sizeof(path))) {
        return 0;
    }

    kbo_lock_enter(&g_kbo_independent_acquisition_sql_schema_lock);
    if (g_kbo_independent_acquisition_sql_schema_ready
            && strcmp(g_kbo_independent_acquisition_sql_schema_path, path) == 0) {
        kbo_lock_leave(&g_kbo_independent_acquisition_sql_schema_lock);
        return 1;
    }
    kbo_lock_leave(&g_kbo_independent_acquisition_sql_schema_lock);

    static const char* sql =
        "CREATE TABLE IF NOT EXISTS independent_acquisition_requests ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "date INTEGER NOT NULL,"
        "season INTEGER NOT NULL,"
        "buyer_team_id INTEGER NOT NULL,"
        "seller_team_id INTEGER NOT NULL,"
        "seller_csv_id TEXT NOT NULL DEFAULT '',"
        "player_id INTEGER NOT NULL,"
        "nation_id INTEGER NOT NULL DEFAULT 0,"
        "pitcher INTEGER NOT NULL DEFAULT 0,"
        "asian_quota INTEGER NOT NULL DEFAULT 0,"
        "cash_cost INTEGER NOT NULL DEFAULT 0,"
        "value_score INTEGER NOT NULL DEFAULT 0,"
        "request_score INTEGER NOT NULL DEFAULT 0,"
        "effective_before INTEGER NOT NULL DEFAULT 0,"
        "effective_after INTEGER NOT NULL DEFAULT 0,"
        "effective_limit INTEGER NOT NULL DEFAULT 0,"
        "slot_type TEXT NOT NULL DEFAULT '',"
        "injured_player_id INTEGER NOT NULL DEFAULT 0,"
        "buyer_active_count INTEGER NOT NULL DEFAULT 0,"
        "buyer_foreign_effective INTEGER NOT NULL DEFAULT 0,"
        "source TEXT NOT NULL DEFAULT '',"
        "created_at TEXT NOT NULL DEFAULT (datetime('now')),"
        "UNIQUE(season, buyer_team_id, seller_team_id, player_id)"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_independent_acquisition_requests_pending "
        "ON independent_acquisition_requests(season, seller_team_id, player_id);"
        "CREATE TABLE IF NOT EXISTS independent_acquisition_decisions ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "date INTEGER NOT NULL,"
        "season INTEGER NOT NULL,"
        "seller_team_id INTEGER NOT NULL,"
        "player_id INTEGER NOT NULL,"
        "buyer_team_id INTEGER NOT NULL,"
        "request_score INTEGER NOT NULL DEFAULT 0,"
        "value_score INTEGER NOT NULL DEFAULT 0,"
        "cash_cost INTEGER NOT NULL DEFAULT 0,"
        "old_cash INTEGER NOT NULL DEFAULT 0,"
        "new_cash INTEGER NOT NULL DEFAULT 0,"
        "seller_transfer_fee INTEGER NOT NULL DEFAULT 0,"
        "seller_old_cash INTEGER NOT NULL DEFAULT 0,"
        "seller_new_cash INTEGER NOT NULL DEFAULT 0,"
        "transferred INTEGER NOT NULL DEFAULT 0,"
        "source TEXT NOT NULL DEFAULT '',"
        "created_at TEXT NOT NULL DEFAULT (datetime('now')),"
        "UNIQUE(season, seller_team_id, player_id)"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_independent_acquisition_decisions_buyer "
        "ON independent_acquisition_decisions(season, buyer_team_id, transferred);"
        "INSERT OR REPLACE INTO kbo_schema(schema_key, schema_version, updated_at) "
        "VALUES('independent_acquisition_requests', 1, datetime('now'));"
        "INSERT OR REPLACE INTO kbo_schema(schema_key, schema_version, updated_at) "
        "VALUES('independent_acquisition_decisions', 1, datetime('now'));";
    int ok = kbo_save_state_exec(sql, source != NULL ? source : "independent_acquisition_sql_schema");
    if (!ok) {
        return 0;
    }

    kbo_lock_enter(&g_kbo_independent_acquisition_sql_schema_lock);
    snprintf(
        g_kbo_independent_acquisition_sql_schema_path,
        sizeof(g_kbo_independent_acquisition_sql_schema_path),
        "%s",
        path);
    g_kbo_independent_acquisition_sql_schema_ready = 1;
    kbo_lock_leave(&g_kbo_independent_acquisition_sql_schema_lock);
    return 1;
}

int kbo_independent_acquisition_sql_exists_cb(void* user_data, int ncols, char** vals, char** names)
{
    (void)ncols;
    (void)vals;
    (void)names;
    KboIndependentAcquisitionSqlExistsResult* result =
        (KboIndependentAcquisitionSqlExistsResult*)user_data;
    if (result != NULL) {
        result->found = 1;
    }
    return 0;
}

int kbo_independent_acquisition_sql_count_cb(void* user_data, int ncols, char** vals, char** names)
{
    (void)ncols;
    (void)names;
    KboIndependentAcquisitionSqlCountResult* result =
        (KboIndependentAcquisitionSqlCountResult*)user_data;
    if (result != NULL && vals != NULL && vals[0] != NULL) {
        result->count = atoi(vals[0]);
    }
    return 0;
}

int kbo_independent_acquisition_sql_date_cb(void* user_data, int ncols, char** vals, char** names)
{
    (void)ncols;
    (void)names;
    KboIndependentAcquisitionSqlDateResult* result =
        (KboIndependentAcquisitionSqlDateResult*)user_data;
    if (result != NULL && vals != NULL && vals[0] != NULL) {
        unsigned int value = 0u;
        if (sscanf(vals[0], "%u", &value) == 1) {
            result->date = (uint32_t)value;
        }
    }
    return 0;
}

uint32_t kbo_independent_acquisition_sql_u32(char** vals, int index)
{
    unsigned int value = 0u;
    if (vals != NULL && vals[index] != NULL) {
        (void)sscanf(vals[index], "%u", &value);
    }
    return (uint32_t)value;
}

int32_t kbo_independent_acquisition_sql_i32(char** vals, int index)
{
    int value = 0;
    if (vals != NULL && vals[index] != NULL) {
        (void)sscanf(vals[index], "%d", &value);
    }
    return (int32_t)value;
}

int64_t kbo_independent_acquisition_sql_i64(char** vals, int index)
{
    long long value = 0;
    if (vals != NULL && vals[index] != NULL) {
        (void)sscanf(vals[index], "%lld", &value);
    }
    return (int64_t)value;
}

void kbo_independent_acquisition_sql_text(char** vals, int index, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';
    if (vals != NULL && vals[index] != NULL) {
        snprintf(out, out_size, "%s", vals[index]);
    }
}

int kbo_independent_acquisition_sql_transfer_summary_cb(
    void* user_data,
    int ncols,
    char** vals,
    char** names)
{
    (void)names;
    KboIndependentAcquisitionSqlTransferSummaryLoadContext* ctx =
        (KboIndependentAcquisitionSqlTransferSummaryLoadContext*)user_data;
    if (ctx == NULL || ctx->rows == NULL || vals == NULL || ncols < 3 || ctx->count >= ctx->max_count) {
        return 0;
    }

    KboIndependentAcquisitionTransferSummary row;
    memset(&row, 0, sizeof(row));
    row.team_id = kbo_independent_acquisition_sql_u32(vals, 0);
    row.transferred_count = kbo_independent_acquisition_sql_i32(vals, 1);
    row.last_transfer_date = kbo_independent_acquisition_sql_u32(vals, 2);
    if (row.team_id != 0u && row.transferred_count > 0) {
        ctx->rows[ctx->count++] = row;
    }
    return 0;
}

int kbo_independent_acquisition_sql_append_text(
    char* out,
    size_t out_size,
    size_t* cursor,
    const char* fmt,
    ...)
{
    if (out == NULL || cursor == NULL || fmt == NULL || *cursor >= out_size) {
        return 0;
    }
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(out + *cursor, out_size - *cursor, fmt, args);
    va_end(args);
    if (len < 0 || (size_t)len >= out_size - *cursor) {
        return 0;
    }
    *cursor += (size_t)len;
    return 1;
}

int kbo_independent_acquisition_sql_escape(
    char* out,
    size_t out_size,
    const char* value)
{
    return kbo_sql_escape_literal(out, out_size, value != NULL ? value : "");
}
