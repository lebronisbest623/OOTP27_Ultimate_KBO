#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "military_selection_results_sql_store.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "../../../../core/logging/core_log.h"
#include "../../../../core/sql/save_state/save_state_sqlite.h"

typedef struct KboMilitarySelectionResultsSqlLoadContext {
    KboMilitarySelectionResultEntry* rows;
    int capacity;
    int count;
    int overflowed;
} KboMilitarySelectionResultsSqlLoadContext;

static int kbo_military_selection_results_sql_ensure_schema(const char* source)
{
    static const char* sql =
        "CREATE TABLE IF NOT EXISTS military_selection_results ("
        "year INTEGER NOT NULL,"
        "announcement_date INTEGER NOT NULL DEFAULT 0,"
        "player_id INTEGER NOT NULL,"
        "original_team_id INTEGER NOT NULL DEFAULT 0,"
        "original_league_id INTEGER NOT NULL DEFAULT 0,"
        "service_team_id INTEGER NOT NULL DEFAULT 0,"
        "return_date INTEGER NOT NULL DEFAULT 0,"
        "age INTEGER NOT NULL DEFAULT 0,"
        "position_group INTEGER NOT NULL DEFAULT 0,"
        "position_role INTEGER NOT NULL DEFAULT 0,"
        "score INTEGER NOT NULL DEFAULT 0,"
        "updated_at TEXT NOT NULL DEFAULT (datetime('now')),"
        "PRIMARY KEY(year, player_id)"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_military_selection_results_player "
        "ON military_selection_results(player_id, year);"
        "INSERT OR REPLACE INTO kbo_schema(schema_key, schema_version, updated_at) "
        "VALUES('military_selection_results', 1, datetime('now'));";
    return kbo_save_state_exec(sql, source != NULL ? source : "military_selection_results_schema");
}

int kbo_military_selection_results_sql_path(char* out, size_t out_size)
{
    return kbo_save_state_db_path(out, out_size);
}

static uint32_t kbo_military_selection_results_sql_u32(char** vals, int index)
{
    unsigned int value = 0u;
    if (vals != NULL && vals[index] != NULL) {
        (void)sscanf(vals[index], "%u", &value);
    }
    return (uint32_t)value;
}

static int32_t kbo_military_selection_results_sql_i32(char** vals, int index)
{
    int value = 0;
    if (vals != NULL && vals[index] != NULL) {
        (void)sscanf(vals[index], "%d", &value);
    }
    return (int32_t)value;
}

static int kbo_military_selection_results_sql_load_cb(void* user_data, int ncols, char** vals, char** names)
{
    (void)names;
    KboMilitarySelectionResultsSqlLoadContext* ctx =
        (KboMilitarySelectionResultsSqlLoadContext*)user_data;
    if (ctx == NULL || ctx->rows == NULL || vals == NULL || ncols < 11) {
        return 0;
    }
    if (ctx->count >= ctx->capacity) {
        ctx->overflowed++;
        return 0;
    }

    KboMilitarySelectionResultEntry entry = {0};
    entry.year = kbo_military_selection_results_sql_u32(vals, 0);
    entry.announcement_date = kbo_military_selection_results_sql_u32(vals, 1);
    entry.player_id = kbo_military_selection_results_sql_u32(vals, 2);
    entry.original_team_id = kbo_military_selection_results_sql_u32(vals, 3);
    entry.original_league_id = kbo_military_selection_results_sql_u32(vals, 4);
    entry.service_team_id = kbo_military_selection_results_sql_u32(vals, 5);
    entry.return_date = kbo_military_selection_results_sql_u32(vals, 6);
    entry.age = (uint16_t)(kbo_military_selection_results_sql_u32(vals, 7) & 0xffffu);
    entry.position_group = (uint8_t)(kbo_military_selection_results_sql_u32(vals, 8) & 0xffu);
    entry.position_role = (uint8_t)(kbo_military_selection_results_sql_u32(vals, 9) & 0xffu);
    entry.score = kbo_military_selection_results_sql_i32(vals, 10);
    if (entry.year == 0u || entry.player_id == 0u) {
        return 0;
    }

    ctx->rows[ctx->count++] = entry;
    return 0;
}

int kbo_military_selection_results_sql_load(
    KboMilitarySelectionResultEntry* out,
    int max_count,
    int* out_count)
{
    if (out == NULL || max_count <= 0 || out_count == NULL) {
        return 0;
    }
    *out_count = 0;
    if (!kbo_military_selection_results_sql_ensure_schema("military_selection_results_load_schema")) {
        return 0;
    }

    KboMilitarySelectionResultsSqlLoadContext ctx = {out, max_count, 0, 0};
    static const char* sql =
        "SELECT year, announcement_date, player_id, original_team_id, original_league_id, "
        "service_team_id, return_date, age, position_group, position_role, score "
        "FROM military_selection_results WHERE year != 0 AND player_id != 0 "
        "ORDER BY year, player_id;";
    if (!kbo_save_state_query(sql, kbo_military_selection_results_sql_load_cb, &ctx, "military_selection_results_load")) {
        return 0;
    }
    if (ctx.overflowed > 0) {
        kbo_log_runtimef(
            "KBO military selection results sqlite load truncated rows=%d capacity=%d",
            ctx.overflowed,
            max_count);
    }
    *out_count = ctx.count;
    return 1;
}

static int kbo_military_selection_results_sql_append(
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

int kbo_military_selection_results_sql_upsert_many(
    const KboMilitarySelectionResultEntry* entries,
    int entry_count)
{
    if (entry_count <= 0 || entries == NULL
            || !kbo_military_selection_results_sql_ensure_schema("military_selection_results_upsert_schema")) {
        return 0;
    }

    size_t sql_size = 512u + ((size_t)entry_count * 512u);
    char* sql = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sql_size);
    if (sql == NULL) {
        return 0;
    }

    size_t cursor = 0u;
    int ok = kbo_military_selection_results_sql_append(sql, sql_size, &cursor, "BEGIN IMMEDIATE;");
    for (int i = 0; ok && i < entry_count; i++) {
        const KboMilitarySelectionResultEntry* entry = &entries[i];
        if (entry->year == 0u || entry->player_id == 0u) {
            continue;
        }
        ok = kbo_military_selection_results_sql_append(
            sql,
            sql_size,
            &cursor,
            "INSERT OR REPLACE INTO military_selection_results("
            "year, announcement_date, player_id, original_team_id, original_league_id, "
            "service_team_id, return_date, age, position_group, position_role, score, updated_at"
            ") VALUES(%u, %u, %u, %u, %u, %u, %u, %u, %u, %u, %d, datetime('now'));",
            entry->year,
            entry->announcement_date,
            entry->player_id,
            entry->original_team_id,
            entry->original_league_id,
            entry->service_team_id,
            entry->return_date,
            (uint32_t)entry->age,
            (uint32_t)entry->position_group,
            (uint32_t)entry->position_role,
            entry->score);
    }
    if (ok) {
        ok = kbo_military_selection_results_sql_append(sql, sql_size, &cursor, "COMMIT;");
    }
    if (!ok) {
        HeapFree(GetProcessHeap(), 0, sql);
        kbo_log_runtimef(
            "KBO military selection results sqlite upsert failed reason=sql_buffer_full rows=%d",
            entry_count);
        return 0;
    }

    ok = kbo_save_state_exec(sql, "military_selection_results_upsert");
    HeapFree(GetProcessHeap(), 0, sql);
    return ok;
}
