#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "cbt_exceptions_sql_store.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "../../../core/logging/core_log.h"
#include "../../../core/sql/escape/core_sql_escape.h"
#include "../../../core/sql/save_state/save_state_sqlite.h"

typedef struct KboCbtExceptionsSqlLoadContext {
    KboCbtExceptionDesignation* rows;
    int capacity;
    int count;
    int overflowed;
} KboCbtExceptionsSqlLoadContext;

static int kbo_cbt_exceptions_sql_ensure_schema(const char* source)
{
    static const char* sql =
        "CREATE TABLE IF NOT EXISTS cbt_exception_players ("
        "season INTEGER NOT NULL,"
        "team_id INTEGER NOT NULL,"
        "player_key TEXT NOT NULL,"
        "player_name TEXT NOT NULL DEFAULT '',"
        "updated_at TEXT NOT NULL DEFAULT (datetime('now')),"
        "PRIMARY KEY(season, team_id)"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_cbt_exception_players_lookup "
        "ON cbt_exception_players(season, team_id, player_key);"
        "INSERT OR REPLACE INTO kbo_schema(schema_key, schema_version, updated_at) "
        "VALUES('cbt_exception_players', 1, datetime('now'));";
    return kbo_save_state_exec(sql, source != NULL ? source : "cbt_exception_players_schema");
}

static uint32_t kbo_cbt_exceptions_sql_u32(char** vals, int index)
{
    unsigned int value = 0u;
    if (vals != NULL && vals[index] != NULL) {
        (void)sscanf(vals[index], "%u", &value);
    }
    return (uint32_t)value;
}

static void kbo_cbt_exceptions_sql_text(char** vals, int index, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';
    if (vals != NULL && vals[index] != NULL) {
        snprintf(out, out_size, "%s", vals[index]);
    }
}

static int kbo_cbt_exceptions_sql_load_cb(void* user_data, int ncols, char** vals, char** names)
{
    (void)names;
    KboCbtExceptionsSqlLoadContext* ctx = (KboCbtExceptionsSqlLoadContext*)user_data;
    if (ctx == NULL || ctx->rows == NULL || vals == NULL || ncols < 4) {
        return 0;
    }
    if (ctx->count >= ctx->capacity) {
        ctx->overflowed++;
        return 0;
    }

    KboCbtExceptionDesignation row = {0};
    row.season = kbo_cbt_exceptions_sql_u32(vals, 0);
    row.team_id = kbo_cbt_exceptions_sql_u32(vals, 1);
    kbo_cbt_exceptions_sql_text(vals, 2, row.player_key, sizeof(row.player_key));
    kbo_cbt_exceptions_sql_text(vals, 3, row.player_name, sizeof(row.player_name));
    if (row.season == 0u || row.team_id == 0u || row.player_key[0] == '\0') {
        return 0;
    }

    ctx->rows[ctx->count++] = row;
    return 0;
}

int kbo_cbt_exceptions_sql_load_designations(KboCbtExceptionDesignation* rows, int max, int* out_count)
{
    if (rows == NULL || max <= 0 || out_count == NULL) {
        return 0;
    }
    *out_count = 0;
    if (!kbo_cbt_exceptions_sql_ensure_schema("cbt_exception_players_load_schema")) {
        return 0;
    }

    KboCbtExceptionsSqlLoadContext ctx = {rows, max, 0, 0};
    static const char* sql =
        "SELECT season, team_id, player_key, player_name "
        "FROM cbt_exception_players "
        "WHERE season != 0 AND team_id != 0 AND player_key != '' "
        "ORDER BY season, team_id;";
    if (!kbo_save_state_query(sql, kbo_cbt_exceptions_sql_load_cb, &ctx, "cbt_exception_players_load")) {
        return 0;
    }
    if (ctx.overflowed > 0) {
        kbo_log_runtimef(
            "KBO CBT exception players sqlite load truncated rows=%d capacity=%d",
            ctx.overflowed,
            max);
    }
    *out_count = ctx.count;
    return 1;
}

static int kbo_cbt_exceptions_sql_append(
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

int kbo_cbt_exceptions_sql_replace_designations(const KboCbtExceptionDesignation* rows, int count)
{
    if (count < 0 || (count > 0 && rows == NULL)
            || !kbo_cbt_exceptions_sql_ensure_schema("cbt_exception_players_replace_schema")) {
        return 0;
    }

    size_t sql_size = 512u + ((size_t)count * 384u);
    char* sql = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sql_size);
    if (sql == NULL) {
        kbo_log_runtime_line("KBO CBT exception players sqlite persist failed reason=alloc_sql");
        return 0;
    }

    size_t cursor = 0u;
    int ok = kbo_cbt_exceptions_sql_append(
        sql,
        sql_size,
        &cursor,
        "BEGIN IMMEDIATE;"
        "DELETE FROM cbt_exception_players;");
    for (int i = 0; ok && i < count; i++) {
        const KboCbtExceptionDesignation* row = &rows[i];
        if (row->season == 0u || row->team_id == 0u || row->player_key[0] == '\0') {
            continue;
        }

        char player_key[160] = {0};
        char player_name[224] = {0};
        if (!kbo_sql_escape_literal(player_key, sizeof(player_key), row->player_key)
                || !kbo_sql_escape_literal(player_name, sizeof(player_name), row->player_name)) {
            ok = 0;
            break;
        }
        ok = kbo_cbt_exceptions_sql_append(
            sql,
            sql_size,
            &cursor,
            "INSERT OR REPLACE INTO cbt_exception_players("
            "season, team_id, player_key, player_name, updated_at"
            ") VALUES(%u, %u, '%s', '%s', datetime('now'));",
            row->season,
            row->team_id,
            player_key,
            player_name);
    }
    if (ok) {
        ok = kbo_cbt_exceptions_sql_append(sql, sql_size, &cursor, "COMMIT;");
    }
    if (!ok) {
        HeapFree(GetProcessHeap(), 0, sql);
        kbo_log_runtimef(
            "KBO CBT exception players sqlite persist failed reason=sql_buffer_full count=%d",
            count);
        return 0;
    }

    ok = kbo_save_state_exec(sql, "cbt_exception_players_replace");
    HeapFree(GetProcessHeap(), 0, sql);
    return ok;
}
