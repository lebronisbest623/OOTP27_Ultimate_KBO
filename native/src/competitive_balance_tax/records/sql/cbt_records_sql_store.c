#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "cbt_records_sql_store.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "../../../core/logging/core_log.h"
#include "../../../core/sql/escape/core_sql_escape.h"
#include "../../../core/sql/save_state/save_state_sqlite.h"

typedef struct KboCbtRecordsSqlLoadContext {
    KboCbtRecord* records;
    int capacity;
    int count;
    int overflowed;
} KboCbtRecordsSqlLoadContext;

static int kbo_cbt_records_sql_ensure_schema(const char* source)
{
    static const char* sql =
        "CREATE TABLE IF NOT EXISTS cbt_records ("
        "season INTEGER NOT NULL,"
        "team_id INTEGER NOT NULL,"
        "payroll INTEGER NOT NULL DEFAULT 0,"
        "threshold_amount INTEGER NOT NULL DEFAULT 0,"
        "overage INTEGER NOT NULL DEFAULT 0,"
        "tax_rate_pct INTEGER NOT NULL DEFAULT 0,"
        "tax_amount INTEGER NOT NULL DEFAULT 0,"
        "consecutive_count INTEGER NOT NULL DEFAULT 0,"
        "processed_date INTEGER NOT NULL DEFAULT 0,"
        "team_name TEXT NOT NULL DEFAULT '',"
        "updated_at TEXT NOT NULL DEFAULT (datetime('now')),"
        "PRIMARY KEY(season, team_id)"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_cbt_records_team_season "
        "ON cbt_records(team_id, season);"
        "INSERT OR REPLACE INTO kbo_schema(schema_key, schema_version, updated_at) "
        "VALUES('cbt_records', 1, datetime('now'));";
    return kbo_save_state_exec(sql, source != NULL ? source : "cbt_records_schema");
}

int kbo_cbt_records_sql_path(char* out, size_t out_size)
{
    return kbo_save_state_db_path(out, out_size);
}

static uint32_t kbo_cbt_records_sql_u32(char** vals, int index)
{
    unsigned int value = 0u;
    if (vals != NULL && vals[index] != NULL) {
        (void)sscanf(vals[index], "%u", &value);
    }
    return (uint32_t)value;
}

static int32_t kbo_cbt_records_sql_i32(char** vals, int index)
{
    int value = 0;
    if (vals != NULL && vals[index] != NULL) {
        (void)sscanf(vals[index], "%d", &value);
    }
    return (int32_t)value;
}

static void kbo_cbt_records_sql_text(char** vals, int index, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';
    if (vals != NULL && vals[index] != NULL) {
        snprintf(out, out_size, "%s", vals[index]);
    }
}

static int kbo_cbt_records_sql_load_cb(void* user_data, int ncols, char** vals, char** names)
{
    (void)names;
    KboCbtRecordsSqlLoadContext* ctx = (KboCbtRecordsSqlLoadContext*)user_data;
    if (ctx == NULL || ctx->records == NULL || vals == NULL || ncols < 10) {
        return 0;
    }
    if (ctx->count >= ctx->capacity) {
        ctx->overflowed++;
        return 0;
    }

    KboCbtRecord rec = {0};
    rec.season = kbo_cbt_records_sql_u32(vals, 0);
    rec.team_id = kbo_cbt_records_sql_u32(vals, 1);
    rec.payroll = kbo_cbt_records_sql_i32(vals, 2);
    rec.threshold = kbo_cbt_records_sql_i32(vals, 3);
    rec.overage = kbo_cbt_records_sql_i32(vals, 4);
    rec.tax_rate_pct = kbo_cbt_records_sql_u32(vals, 5);
    rec.tax_amount = kbo_cbt_records_sql_i32(vals, 6);
    rec.consecutive_count = kbo_cbt_records_sql_u32(vals, 7);
    rec.processed_date = kbo_cbt_records_sql_u32(vals, 8);
    kbo_cbt_records_sql_text(vals, 9, rec.team_name, sizeof(rec.team_name));
    if (rec.season == 0u || rec.team_id == 0u) {
        return 0;
    }

    ctx->records[ctx->count++] = rec;
    return 0;
}

int kbo_cbt_records_sql_load(KboCbtRecord* records, int max, int* out_count)
{
    if (records == NULL || max <= 0 || out_count == NULL) {
        return 0;
    }
    *out_count = 0;
    if (!kbo_cbt_records_sql_ensure_schema("cbt_records_load_schema")) {
        return 0;
    }

    KboCbtRecordsSqlLoadContext ctx = {records, max, 0, 0};
    static const char* sql =
        "SELECT season, team_id, payroll, threshold_amount, overage, "
        "tax_rate_pct, tax_amount, consecutive_count, processed_date, team_name "
        "FROM cbt_records "
        "WHERE season != 0 AND team_id != 0 "
        "ORDER BY season, team_id;";
    if (!kbo_save_state_query(sql, kbo_cbt_records_sql_load_cb, &ctx, "cbt_records_load")) {
        return 0;
    }
    if (ctx.overflowed > 0) {
        kbo_log_runtimef(
            "KBO CBT records sqlite load truncated rows=%d capacity=%d",
            ctx.overflowed,
            max);
    }
    *out_count = ctx.count;
    return 1;
}

static int kbo_cbt_records_sql_append(
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

int kbo_cbt_records_sql_replace_all(const KboCbtRecord* records, int count)
{
    if (count < 0 || (count > 0 && records == NULL)
            || !kbo_cbt_records_sql_ensure_schema("cbt_records_replace_schema")) {
        return 0;
    }

    size_t sql_size = 512u + ((size_t)count * 512u);
    char* sql = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sql_size);
    if (sql == NULL) {
        kbo_log_runtime_line("KBO CBT records sqlite persist failed reason=alloc_sql");
        return 0;
    }

    size_t cursor = 0u;
    int ok = kbo_cbt_records_sql_append(
        sql,
        sql_size,
        &cursor,
        "BEGIN IMMEDIATE;"
        "DELETE FROM cbt_records;");
    for (int i = 0; ok && i < count; i++) {
        const KboCbtRecord* rec = &records[i];
        if (rec->season == 0u || rec->team_id == 0u) {
            continue;
        }

        char team_name[160] = {0};
        if (!kbo_sql_escape_literal(team_name, sizeof(team_name), rec->team_name)) {
            ok = 0;
            break;
        }
        ok = kbo_cbt_records_sql_append(
            sql,
            sql_size,
            &cursor,
            "INSERT OR REPLACE INTO cbt_records("
            "season, team_id, payroll, threshold_amount, overage, tax_rate_pct, "
            "tax_amount, consecutive_count, processed_date, team_name, updated_at"
            ") VALUES(%u, %u, %d, %d, %d, %u, %d, %u, %u, '%s', datetime('now'));",
            rec->season,
            rec->team_id,
            rec->payroll,
            rec->threshold,
            rec->overage,
            rec->tax_rate_pct,
            rec->tax_amount,
            rec->consecutive_count,
            rec->processed_date,
            team_name);
    }
    if (ok) {
        ok = kbo_cbt_records_sql_append(sql, sql_size, &cursor, "COMMIT;");
    }
    if (!ok) {
        HeapFree(GetProcessHeap(), 0, sql);
        kbo_log_runtimef(
            "KBO CBT records sqlite persist failed reason=sql_buffer_full count=%d",
            count);
        return 0;
    }

    ok = kbo_save_state_exec(sql, "cbt_records_replace");
    HeapFree(GetProcessHeap(), 0, sql);
    return ok;
}
