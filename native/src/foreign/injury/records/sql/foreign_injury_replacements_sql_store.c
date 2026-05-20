#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "foreign_injury_replacements_sql_store.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "../../../../core/logging/core_log.h"
#include "../../../../core/sql/save_state/save_state_sqlite.h"

typedef struct KboForeignInjuryReplacementsSqlLoadContext {
    KboForeignInjuryReplacement* records;
    int capacity;
    int count;
    int overflowed;
} KboForeignInjuryReplacementsSqlLoadContext;

static int kbo_foreign_injury_replacements_sql_ensure_schema(const char* source)
{
    static const char* sql =
        "CREATE TABLE IF NOT EXISTS foreign_injury_replacements ("
        "injured_player_id INTEGER NOT NULL PRIMARY KEY,"
        "sort_index INTEGER NOT NULL DEFAULT 0,"
        "team_id INTEGER NOT NULL DEFAULT 0,"
        "league_id INTEGER NOT NULL DEFAULT 0,"
        "replacement_player_id INTEGER NOT NULL DEFAULT 0,"
        "opened_on_yyyymmdd INTEGER NOT NULL DEFAULT 0,"
        "expected_end_yyyymmdd INTEGER NOT NULL DEFAULT 0,"
        "injury_id INTEGER NOT NULL DEFAULT 0,"
        "closed_on_yyyymmdd INTEGER NOT NULL DEFAULT 0,"
        "slot_type INTEGER NOT NULL DEFAULT 0,"
        "status INTEGER NOT NULL DEFAULT 0,"
        "converted INTEGER NOT NULL DEFAULT 0,"
        "close_choice INTEGER NOT NULL DEFAULT 0,"
        "updated_at TEXT NOT NULL DEFAULT (datetime('now'))"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_foreign_injury_replacements_team_status "
        "ON foreign_injury_replacements(team_id, status);"
        "INSERT OR REPLACE INTO kbo_schema(schema_key, schema_version, updated_at) "
        "VALUES('foreign_injury_replacements', 1, datetime('now'));";
    return kbo_save_state_exec(sql, source != NULL ? source : "foreign_injury_replacements_schema");
}

int kbo_foreign_injury_replacements_sql_path(char* out, size_t out_size)
{
    return kbo_save_state_db_path(out, out_size);
}

static uint32_t kbo_foreign_injury_replacements_sql_u32(char** vals, int index)
{
    unsigned int value = 0u;
    if (vals != NULL && vals[index] != NULL) {
        (void)sscanf(vals[index], "%u", &value);
    }
    return (uint32_t)value;
}

static int kbo_foreign_injury_replacements_sql_load_cb(void* user_data, int ncols, char** vals, char** names)
{
    (void)names;
    KboForeignInjuryReplacementsSqlLoadContext* ctx =
        (KboForeignInjuryReplacementsSqlLoadContext*)user_data;
    if (ctx == NULL || ctx->records == NULL || vals == NULL || ncols < 12) {
        return 0;
    }
    if (ctx->count >= ctx->capacity) {
        ctx->overflowed++;
        return 0;
    }

    KboForeignInjuryReplacement rec = {0};
    rec.team_id = kbo_foreign_injury_replacements_sql_u32(vals, 0);
    rec.league_id = kbo_foreign_injury_replacements_sql_u32(vals, 1);
    rec.injured_player_id = kbo_foreign_injury_replacements_sql_u32(vals, 2);
    rec.replacement_player_id = kbo_foreign_injury_replacements_sql_u32(vals, 3);
    rec.opened_on_yyyymmdd = kbo_foreign_injury_replacements_sql_u32(vals, 4);
    rec.expected_end_yyyymmdd = kbo_foreign_injury_replacements_sql_u32(vals, 5);
    rec.slot_type = (uint8_t)(kbo_foreign_injury_replacements_sql_u32(vals, 6) & 0xffu);
    rec.status = (uint8_t)(kbo_foreign_injury_replacements_sql_u32(vals, 7) & 0xffu);
    rec.converted = kbo_foreign_injury_replacements_sql_u32(vals, 8) != 0u ? 1u : 0u;
    rec.injury_id = kbo_foreign_injury_replacements_sql_u32(vals, 9);
    rec.closed_on_yyyymmdd = kbo_foreign_injury_replacements_sql_u32(vals, 10);
    rec.close_choice = (uint8_t)(kbo_foreign_injury_replacements_sql_u32(vals, 11) & 0xffu);
    if (rec.team_id == 0u || rec.injured_player_id == 0u) {
        return 0;
    }

    ctx->records[ctx->count++] = rec;
    return 0;
}

int kbo_foreign_injury_replacements_sql_load(
    KboForeignInjuryReplacement* out,
    int max_count,
    int* out_count)
{
    if (out == NULL || max_count <= 0 || out_count == NULL) {
        return 0;
    }
    *out_count = 0;
    if (!kbo_foreign_injury_replacements_sql_ensure_schema("foreign_injury_replacements_load_schema")) {
        return 0;
    }

    KboForeignInjuryReplacementsSqlLoadContext ctx = {out, max_count, 0, 0};
    static const char* sql =
        "SELECT team_id, league_id, injured_player_id, replacement_player_id, opened_on_yyyymmdd, "
        "expected_end_yyyymmdd, slot_type, status, converted, injury_id, closed_on_yyyymmdd, close_choice "
        "FROM foreign_injury_replacements WHERE injured_player_id != 0 ORDER BY sort_index, injured_player_id;";
    if (!kbo_save_state_query(sql, kbo_foreign_injury_replacements_sql_load_cb, &ctx, "foreign_injury_replacements_load")) {
        return 0;
    }
    if (ctx.overflowed > 0) {
        kbo_log_runtimef(
            "foreign injury replacement: sqlite load truncated rows=%d capacity=%d",
            ctx.overflowed,
            max_count);
    }
    *out_count = ctx.count;
    return 1;
}

static int kbo_foreign_injury_replacements_sql_append(
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

int kbo_foreign_injury_replacements_sql_replace_all(
    const KboForeignInjuryReplacement* records,
    int record_count)
{
    if (record_count < 0 || (record_count > 0 && records == NULL)
            || !kbo_foreign_injury_replacements_sql_ensure_schema("foreign_injury_replacements_replace_schema")) {
        return 0;
    }

    size_t sql_size = 512u + ((size_t)record_count * 640u);
    char* sql = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sql_size);
    if (sql == NULL) {
        return 0;
    }

    size_t cursor = 0u;
    int ok = kbo_foreign_injury_replacements_sql_append(
        sql,
        sql_size,
        &cursor,
        "BEGIN IMMEDIATE;DELETE FROM foreign_injury_replacements;");
    for (int i = 0; ok && i < record_count; i++) {
        const KboForeignInjuryReplacement* rec = &records[i];
        if (rec->team_id == 0u || rec->injured_player_id == 0u) {
            continue;
        }
        ok = kbo_foreign_injury_replacements_sql_append(
            sql,
            sql_size,
            &cursor,
            "INSERT OR REPLACE INTO foreign_injury_replacements("
            "injured_player_id, sort_index, team_id, league_id, replacement_player_id, "
            "opened_on_yyyymmdd, expected_end_yyyymmdd, injury_id, closed_on_yyyymmdd, "
            "slot_type, status, converted, close_choice, updated_at"
            ") VALUES(%u, %u, %u, %u, %u, %u, %u, %u, %u, %u, %u, %u, %u, datetime('now'));",
            rec->injured_player_id,
            (uint32_t)i + 1u,
            rec->team_id,
            rec->league_id,
            rec->replacement_player_id,
            rec->opened_on_yyyymmdd,
            rec->expected_end_yyyymmdd,
            rec->injury_id,
            rec->closed_on_yyyymmdd,
            (uint32_t)rec->slot_type,
            (uint32_t)rec->status,
            (uint32_t)rec->converted,
            (uint32_t)rec->close_choice);
    }
    if (ok) {
        ok = kbo_foreign_injury_replacements_sql_append(sql, sql_size, &cursor, "COMMIT;");
    }
    if (!ok) {
        HeapFree(GetProcessHeap(), 0, sql);
        kbo_log_runtimef(
            "foreign injury replacement: sqlite persist failed reason=sql_buffer_full rows=%d",
            record_count);
        return 0;
    }

    ok = kbo_save_state_exec(sql, "foreign_injury_replacements_replace");
    HeapFree(GetProcessHeap(), 0, sql);
    return ok;
}
