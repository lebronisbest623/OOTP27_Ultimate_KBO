#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "amateur_reputation_sql_store.h"

#include <stdarg.h>
#include <stdio.h>

#include "../../../core/logging/core_log.h"
#include "../../../core/sql/escape/core_sql_escape.h"
#include "../../../core/sql/save_state/save_state_sqlite.h"

typedef struct KboAmateurReputationSqlExistsResult {
    int found;
} KboAmateurReputationSqlExistsResult;

typedef struct KboAmateurReputationSqlLoadContext {
    KboAmateurReputationHistoryRow* rows;
    int capacity;
    int count;
    int overflowed;
} KboAmateurReputationSqlLoadContext;

static int kbo_amateur_reputation_sql_ensure_schema(const char* source)
{
    static const char* sql =
        "CREATE TABLE IF NOT EXISTS amateur_reputation_history ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "year INTEGER NOT NULL,"
        "league_id INTEGER NOT NULL,"
        "team_id INTEGER NOT NULL,"
        "old_reputation INTEGER NOT NULL,"
        "delta INTEGER NOT NULL,"
        "new_reputation INTEGER NOT NULL,"
        "wins INTEGER NOT NULL,"
        "losses INTEGER NOT NULL,"
        "ties INTEGER NOT NULL,"
        "score INTEGER NOT NULL,"
        "rank INTEGER NOT NULL,"
        "source TEXT NOT NULL DEFAULT '',"
        "created_at TEXT NOT NULL DEFAULT (datetime('now')),"
        "UNIQUE(year, league_id, team_id)"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_amateur_reputation_history_year "
        "ON amateur_reputation_history(league_id, year);"
        "INSERT OR REPLACE INTO kbo_schema(schema_key, schema_version, updated_at) "
        "VALUES('amateur_reputation_history', 1, datetime('now'));";
    return kbo_save_state_exec(sql, source != NULL ? source : "amateur_reputation_history_schema");
}

static int kbo_amateur_reputation_sql_exists_cb(void* user_data, int ncols, char** vals, char** names)
{
    (void)ncols;
    (void)vals;
    (void)names;
    KboAmateurReputationSqlExistsResult* result = (KboAmateurReputationSqlExistsResult*)user_data;
    if (result != NULL) {
        result->found = 1;
    }
    return 0;
}

static int kbo_amateur_reputation_sql_parse_u32(const char* text, uint32_t* out)
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

static int kbo_amateur_reputation_sql_parse_i32(const char* text, int32_t* out)
{
    if (text == NULL || out == NULL) {
        return 0;
    }
    int value = 0;
    if (sscanf(text, "%d", &value) != 1) {
        return 0;
    }
    *out = (int32_t)value;
    return 1;
}

static int kbo_amateur_reputation_sql_load_cb(void* user_data, int ncols, char** vals, char** names)
{
    (void)names;
    KboAmateurReputationSqlLoadContext* ctx = (KboAmateurReputationSqlLoadContext*)user_data;
    if (ctx == NULL || ctx->rows == NULL || ncols < 6 || vals == NULL) {
        return 0;
    }
    if (ctx->count >= ctx->capacity) {
        ctx->overflowed++;
        return 0;
    }

    KboAmateurReputationHistoryRow row = {0};
    if (!kbo_amateur_reputation_sql_parse_u32(vals[0], &row.year)
            || !kbo_amateur_reputation_sql_parse_u32(vals[1], &row.league_id)
            || !kbo_amateur_reputation_sql_parse_u32(vals[2], &row.team_id)
            || !kbo_amateur_reputation_sql_parse_u32(vals[3], &row.old_reputation)
            || !kbo_amateur_reputation_sql_parse_i32(vals[4], &row.delta)
            || !kbo_amateur_reputation_sql_parse_u32(vals[5], &row.new_reputation)) {
        return 0;
    }
    if (row.year == 0u || row.league_id == 0u || row.team_id == 0u || row.new_reputation == 0u) {
        return 0;
    }

    ctx->rows[ctx->count++] = row;
    return 0;
}

int kbo_amateur_reputation_sql_history_has_year(uint32_t league_id, uint32_t year)
{
    if (league_id == 0u || year == 0u
            || !kbo_amateur_reputation_sql_ensure_schema("amateur_reputation_history_has_year_schema")) {
        return 0;
    }

    char sql[224] = {0};
    snprintf(
        sql,
        sizeof(sql),
        "SELECT 1 FROM amateur_reputation_history "
        "WHERE league_id=%u AND year=%u LIMIT 1;",
        league_id,
        year);
    KboAmateurReputationSqlExistsResult result = {0};
    kbo_save_state_query(sql, kbo_amateur_reputation_sql_exists_cb, &result, "amateur_reputation_history_has_year");
    return result.found;
}

int kbo_amateur_reputation_sql_history_load(
    KboAmateurReputationHistoryRow* rows,
    int capacity,
    int* out_count)
{
    if (rows == NULL || capacity <= 0 || out_count == NULL) {
        return 0;
    }
    *out_count = 0;
    if (!kbo_amateur_reputation_sql_ensure_schema("amateur_reputation_history_load_schema")) {
        return 0;
    }

    KboAmateurReputationSqlLoadContext ctx = {rows, capacity, 0, 0};
    static const char* sql =
        "SELECT year, league_id, team_id, old_reputation, delta, new_reputation "
        "FROM amateur_reputation_history ORDER BY year, id;";
    if (!kbo_save_state_query(sql, kbo_amateur_reputation_sql_load_cb, &ctx, "amateur_reputation_history_load")) {
        return 0;
    }
    if (ctx.overflowed > 0) {
        kbo_log_runtimef(
            "amateur reputation history sqlite load truncated rows=%d capacity=%d",
            ctx.overflowed,
            capacity);
    }
    *out_count = ctx.count;
    return 1;
}

static int kbo_amateur_reputation_sql_append_text(
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

int kbo_amateur_reputation_sql_history_append(
    uint32_t league_id,
    const KboAmateurReputationUpdateRow* rows,
    int row_count,
    const char* source,
    uint32_t year)
{
    if (league_id == 0u || year == 0u || rows == NULL || row_count <= 0
            || !kbo_amateur_reputation_sql_ensure_schema("amateur_reputation_history_append_schema")) {
        return 0;
    }

    char escaped_source[256] = {0};
    if (!kbo_sql_escape_literal(escaped_source, sizeof(escaped_source), source != NULL ? source : "")) {
        return 0;
    }

    size_t sql_size = 1024u + ((size_t)row_count * 448u);
    char* sql = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sql_size);
    if (sql == NULL) {
        kbo_log_runtime_line("amateur reputation history sqlite append failed reason=alloc_sql");
        return 0;
    }

    size_t cursor = 0u;
    int ok = kbo_amateur_reputation_sql_append_text(sql, sql_size, &cursor, "BEGIN IMMEDIATE;");
    for (int i = 0; ok && i < row_count; i++) {
        const KboAmateurReputationUpdateRow* row = &rows[i];
        int32_t delta = (int32_t)row->new_reputation - (int32_t)row->old_reputation;
        ok = kbo_amateur_reputation_sql_append_text(
            sql,
            sql_size,
            &cursor,
            "INSERT OR REPLACE INTO amateur_reputation_history("
            "year, league_id, team_id, old_reputation, delta, new_reputation, "
            "wins, losses, ties, score, rank, source"
            ") VALUES(%u, %u, %u, %u, %d, %u, %u, %u, %u, %d, %d, '%s');",
            year,
            row->league_id,
            row->team_id,
            (uint32_t)row->old_reputation,
            delta,
            (uint32_t)row->new_reputation,
            (uint32_t)row->wins,
            (uint32_t)row->losses,
            (uint32_t)row->ties,
            row->score,
            i + 1,
            escaped_source);
    }
    if (ok) {
        ok = kbo_amateur_reputation_sql_append_text(sql, sql_size, &cursor, "COMMIT;");
    }
    if (!ok) {
        HeapFree(GetProcessHeap(), 0, sql);
        kbo_log_runtimef(
            "amateur reputation history sqlite append failed reason=sql_buffer_full rows=%d",
            row_count);
        return 0;
    }

    ok = kbo_save_state_exec(sql, "amateur_reputation_history_append");
    HeapFree(GetProcessHeap(), 0, sql);
    return ok;
}
