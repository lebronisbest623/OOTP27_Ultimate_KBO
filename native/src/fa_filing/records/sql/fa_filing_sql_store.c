#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "fa_filing_sql_store.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "../../../core/logging/core_log.h"
#include "../../../core/sql/escape/core_sql_escape.h"
#include "../../../core/sql/save_state/save_state_sqlite.h"

typedef struct KboFaFilingSqlLoadContext {
    KboFaFilingRecord* rows;
    int capacity;
    int count;
    int overflowed;
} KboFaFilingSqlLoadContext;

static int kbo_fa_filing_sql_ensure_schema(const char* source)
{
    static const char* sql =
        "CREATE TABLE IF NOT EXISTS fa_filing ("
        "player_id INTEGER NOT NULL,"
        "filing_date INTEGER NOT NULL,"
        "season INTEGER NOT NULL,"
        "original_team_id INTEGER NOT NULL,"
        "league_id INTEGER NOT NULL,"
        "source_caller_rva INTEGER NOT NULL DEFAULT 0,"
        "notify INTEGER NOT NULL DEFAULT 0,"
        "contract_level INTEGER NOT NULL DEFAULT 0,"
        "player_name TEXT NOT NULL DEFAULT '',"
        "source TEXT NOT NULL DEFAULT '',"
        "updated_at TEXT NOT NULL DEFAULT (datetime('now')),"
        "PRIMARY KEY(player_id, season)"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_fa_filing_player_date "
        "ON fa_filing(player_id, filing_date);"
        "INSERT OR REPLACE INTO kbo_schema(schema_key, schema_version, updated_at) "
        "VALUES('fa_filing', 1, datetime('now'));";
    return kbo_save_state_exec(sql, source != NULL ? source : "fa_filing_schema");
}

int kbo_fa_filing_sql_path(char* out, size_t out_size)
{
    return kbo_save_state_db_path(out, out_size);
}

static uint32_t kbo_fa_filing_sql_u32(char** vals, int index)
{
    unsigned int value = 0u;
    if (vals != NULL && vals[index] != NULL) {
        (void)sscanf(vals[index], "%u", &value);
    }
    return (uint32_t)value;
}

static void kbo_fa_filing_sql_text(char** vals, int index, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';
    if (vals != NULL && vals[index] != NULL) {
        snprintf(out, out_size, "%s", vals[index]);
    }
}

static int kbo_fa_filing_sql_load_cb(void* user_data, int ncols, char** vals, char** names)
{
    (void)names;
    KboFaFilingSqlLoadContext* ctx = (KboFaFilingSqlLoadContext*)user_data;
    if (ctx == NULL || ctx->rows == NULL || vals == NULL || ncols < 10) {
        return 0;
    }
    if (ctx->count >= ctx->capacity) {
        ctx->overflowed++;
        return 0;
    }

    KboFaFilingRecord row = {0};
    row.player_id = kbo_fa_filing_sql_u32(vals, 0);
    row.filing_date = kbo_fa_filing_sql_u32(vals, 1);
    row.season = kbo_fa_filing_sql_u32(vals, 2);
    row.original_team_id = kbo_fa_filing_sql_u32(vals, 3);
    row.league_id = kbo_fa_filing_sql_u32(vals, 4);
    row.source_caller_rva = kbo_fa_filing_sql_u32(vals, 5);
    row.notify = (uint8_t)(kbo_fa_filing_sql_u32(vals, 6) & 0xffu);
    row.contract_level = (uint8_t)(kbo_fa_filing_sql_u32(vals, 7) & 0xffu);
    kbo_fa_filing_sql_text(vals, 8, row.player_name, sizeof(row.player_name));
    kbo_fa_filing_sql_text(vals, 9, row.source, sizeof(row.source));
    if (row.player_id == 0u || row.filing_date == 0u || row.season == 0u) {
        return 0;
    }

    ctx->rows[ctx->count++] = row;
    return 0;
}

int kbo_fa_filing_sql_load(KboFaFilingRecord* rows, int max_rows, int* out_count)
{
    if (rows == NULL || max_rows <= 0 || out_count == NULL) {
        return 0;
    }
    *out_count = 0;
    if (!kbo_fa_filing_sql_ensure_schema("fa_filing_load_schema")) {
        return 0;
    }

    KboFaFilingSqlLoadContext ctx = {rows, max_rows, 0, 0};
    static const char* sql =
        "SELECT player_id, filing_date, season, original_team_id, league_id, "
        "source_caller_rva, notify, contract_level, player_name, source "
        "FROM fa_filing "
        "WHERE player_id != 0 AND filing_date != 0 AND season != 0 "
        "ORDER BY season, player_id;";
    if (!kbo_save_state_query(sql, kbo_fa_filing_sql_load_cb, &ctx, "fa_filing_load")) {
        return 0;
    }
    if (ctx.overflowed > 0) {
        kbo_log_runtimef(
            "KBO FA filing sqlite load truncated rows=%d capacity=%d",
            ctx.overflowed,
            max_rows);
    }
    *out_count = ctx.count;
    return 1;
}

static int kbo_fa_filing_sql_append(
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

int kbo_fa_filing_sql_replace_all(const KboFaFilingRecord* rows, int row_count)
{
    if (row_count < 0 || row_count > KBO_FA_FILING_MAX || (row_count > 0 && rows == NULL)
            || !kbo_fa_filing_sql_ensure_schema("fa_filing_replace_schema")) {
        return 0;
    }

    size_t sql_size = 512u + ((size_t)row_count * 800u);
    char* sql = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sql_size);
    if (sql == NULL) {
        kbo_log_runtime_line("KBO FA filing sqlite persist failed reason=alloc_sql");
        return 0;
    }

    size_t cursor = 0u;
    int ok = kbo_fa_filing_sql_append(
        sql,
        sql_size,
        &cursor,
        "BEGIN IMMEDIATE;"
        "DELETE FROM fa_filing;");
    for (int i = 0; ok && i < row_count; i++) {
        const KboFaFilingRecord* row = &rows[i];
        if (row->player_id == 0u || row->filing_date == 0u || row->season == 0u) {
            continue;
        }

        char player_name[224] = {0};
        char source[128] = {0};
        if (!kbo_sql_escape_literal(player_name, sizeof(player_name), row->player_name)
                || !kbo_sql_escape_literal(source, sizeof(source), row->source)) {
            ok = 0;
            break;
        }
        ok = kbo_fa_filing_sql_append(
            sql,
            sql_size,
            &cursor,
            "INSERT OR REPLACE INTO fa_filing("
            "player_id, filing_date, season, original_team_id, league_id, "
            "source_caller_rva, notify, contract_level, player_name, source, updated_at"
            ") VALUES(%u, %u, %u, %u, %u, %u, %u, %u, '%s', '%s', datetime('now'));",
            row->player_id,
            row->filing_date,
            row->season,
            row->original_team_id,
            row->league_id,
            row->source_caller_rva,
            (uint32_t)row->notify,
            (uint32_t)row->contract_level,
            player_name,
            source);
    }
    if (ok) {
        ok = kbo_fa_filing_sql_append(sql, sql_size, &cursor, "COMMIT;");
    }
    if (!ok) {
        HeapFree(GetProcessHeap(), 0, sql);
        kbo_log_runtimef("KBO FA filing sqlite persist failed reason=sql_buffer_full rows=%d", row_count);
        return 0;
    }

    ok = kbo_save_state_exec(sql, "fa_filing_replace");
    HeapFree(GetProcessHeap(), 0, sql);
    return ok;
}
