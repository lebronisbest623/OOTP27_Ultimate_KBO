#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "foreign_waiver_rights_sql_store.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "../../../core/logging/core_log.h"
#include "../../../core/sql/save_state/save_state_sqlite.h"

typedef struct KboForeignWaiverRightsSqlLoadContext {
    KboForeignWaiverRetention* records;
    int capacity;
    int count;
    int deduped;
    int overflowed;
} KboForeignWaiverRightsSqlLoadContext;

static int kbo_foreign_waiver_rights_sql_ensure_schema(const char* source)
{
    static const char* sql =
        "CREATE TABLE IF NOT EXISTS foreign_reserve_rights ("
        "player_id INTEGER PRIMARY KEY,"
        "team_id INTEGER NOT NULL,"
        "league_id INTEGER NOT NULL,"
        "retained_on INTEGER NOT NULL,"
        "expires_on INTEGER NOT NULL,"
        "updated_at TEXT NOT NULL DEFAULT (datetime('now'))"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_foreign_reserve_rights_active "
        "ON foreign_reserve_rights(expires_on, retained_on);"
        "INSERT OR REPLACE INTO kbo_schema(schema_key, schema_version, updated_at) "
        "VALUES('foreign_reserve_rights', 1, datetime('now'));";
    return kbo_save_state_exec(sql, source != NULL ? source : "foreign_reserve_rights_schema");
}

static int kbo_foreign_waiver_rights_sql_parse_u32(const char* text, uint32_t* out)
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

static int kbo_foreign_waiver_rights_sql_load_cb(void* user_data, int ncols, char** vals, char** names)
{
    (void)names;
    KboForeignWaiverRightsSqlLoadContext* ctx = (KboForeignWaiverRightsSqlLoadContext*)user_data;
    if (ctx == NULL || ctx->records == NULL || ncols < 5 || vals == NULL) {
        return 0;
    }

    KboForeignWaiverRetention rec = {0};
    if (!kbo_foreign_waiver_rights_sql_parse_u32(vals[0], &rec.player_id)
            || !kbo_foreign_waiver_rights_sql_parse_u32(vals[1], &rec.team_id)
            || !kbo_foreign_waiver_rights_sql_parse_u32(vals[2], &rec.league_id)
            || !kbo_foreign_waiver_rights_sql_parse_u32(vals[3], &rec.retained_on_yyyymmdd)
            || !kbo_foreign_waiver_rights_sql_parse_u32(vals[4], &rec.expires_on_yyyymmdd)
            || rec.player_id == 0u
            || rec.team_id == 0u
            || rec.league_id == 0u) {
        return 0;
    }

    int existing_index = -1;
    for (int i = 0; i < ctx->count; i++) {
        if (ctx->records[i].player_id == rec.player_id) {
            existing_index = i;
            break;
        }
    }
    if (existing_index >= 0) {
        if (rec.retained_on_yyyymmdd >= ctx->records[existing_index].retained_on_yyyymmdd) {
            ctx->records[existing_index] = rec;
        }
        ctx->deduped++;
        return 0;
    }

    if (ctx->count >= ctx->capacity) {
        ctx->overflowed++;
        return 0;
    }
    ctx->records[ctx->count++] = rec;
    return 0;
}

int kbo_foreign_waiver_rights_sql_load(
    KboForeignWaiverRetention* out_records,
    int capacity,
    int* out_count,
    int* out_deduped)
{
    if (out_records == NULL || capacity <= 0 || out_count == NULL) {
        return 0;
    }
    *out_count = 0;
    if (out_deduped != NULL) {
        *out_deduped = 0;
    }

    if (!kbo_foreign_waiver_rights_sql_ensure_schema("foreign_reserve_rights_load_schema")) {
        return 0;
    }

    KboForeignWaiverRightsSqlLoadContext ctx = {out_records, capacity, 0, 0, 0};
    static const char* sql =
        "SELECT player_id, team_id, league_id, retained_on, expires_on "
        "FROM foreign_reserve_rights "
        "WHERE player_id != 0 AND team_id != 0 AND league_id != 0 "
        "ORDER BY player_id;";
    if (!kbo_save_state_query(sql, kbo_foreign_waiver_rights_sql_load_cb, &ctx, "foreign_reserve_rights_load")) {
        return 0;
    }

    if (ctx.overflowed > 0) {
        kbo_log_runtimef(
            "foreign reserve rights: sqlite load truncated rows=%d capacity=%d",
            ctx.overflowed,
            capacity);
    }
    *out_count = ctx.count;
    if (out_deduped != NULL) {
        *out_deduped = ctx.deduped;
    }
    return 1;
}

static int kbo_foreign_waiver_rights_sql_append(
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

int kbo_foreign_waiver_rights_sql_replace_all(
    const KboForeignWaiverRetention* records,
    int count)
{
    if (count < 0 || (count > 0 && records == NULL)
            || !kbo_foreign_waiver_rights_sql_ensure_schema("foreign_reserve_rights_replace_schema")) {
        return 0;
    }

    size_t sql_size = 512u + ((size_t)count * 192u);
    char* sql = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sql_size);
    if (sql == NULL) {
        kbo_log_runtime_line("foreign reserve rights: sqlite persist failed reason=alloc_sql");
        return 0;
    }

    size_t cursor = 0u;
    int ok = kbo_foreign_waiver_rights_sql_append(
        sql,
        sql_size,
        &cursor,
        "BEGIN IMMEDIATE;"
        "DELETE FROM foreign_reserve_rights;");
    for (int i = 0; ok && i < count; i++) {
        const KboForeignWaiverRetention* rec = &records[i];
        if (rec->player_id == 0u || rec->team_id == 0u || rec->league_id == 0u) {
            continue;
        }
        ok = kbo_foreign_waiver_rights_sql_append(
            sql,
            sql_size,
            &cursor,
            "INSERT OR REPLACE INTO foreign_reserve_rights("
            "player_id, team_id, league_id, retained_on, expires_on, updated_at"
            ") VALUES(%u, %u, %u, %u, %u, datetime('now'));",
            rec->player_id,
            rec->team_id,
            rec->league_id,
            rec->retained_on_yyyymmdd,
            rec->expires_on_yyyymmdd);
    }
    if (ok) {
        ok = kbo_foreign_waiver_rights_sql_append(sql, sql_size, &cursor, "COMMIT;");
    }
    if (!ok) {
        HeapFree(GetProcessHeap(), 0, sql);
        kbo_log_runtimef(
            "foreign reserve rights: sqlite persist failed reason=sql_buffer_full count=%d",
            count);
        return 0;
    }

    ok = kbo_save_state_exec(sql, "foreign_reserve_rights_replace");
    HeapFree(GetProcessHeap(), 0, sql);
    return ok;
}
