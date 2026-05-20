#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "military_resolved_seed_sql_store.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "../../../../core/logging/core_log.h"
#include "../../../../core/sql/escape/core_sql_escape.h"
#include "../../../../core/sql/save_state/save_state_sqlite.h"
#include "../../../calendar/military_service_date.h"

typedef struct KboMilitaryResolvedSeedSqlLoadContext {
    KboMilitaryServiceSeed* seeds;
    int capacity;
    int count;
    int overflowed;
} KboMilitaryResolvedSeedSqlLoadContext;

static int kbo_military_resolved_seed_sql_ensure_schema(const char* source)
{
    static const char* sql =
        "CREATE TABLE IF NOT EXISTS military_service_resolved ("
        "source_key TEXT NOT NULL PRIMARY KEY,"
        "service_team TEXT NOT NULL DEFAULT 'SANG',"
        "original_team TEXT NOT NULL DEFAULT '',"
        "service_return_yyyymmdd INTEGER NOT NULL DEFAULT 0,"
        "player_id INTEGER NOT NULL DEFAULT 0,"
        "updated_at TEXT NOT NULL DEFAULT (datetime('now'))"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_military_service_resolved_player "
        "ON military_service_resolved(player_id);"
        "INSERT OR REPLACE INTO kbo_schema(schema_key, schema_version, updated_at) "
        "VALUES('military_service_resolved', 1, datetime('now'));";
    return kbo_save_state_exec(sql, source != NULL ? source : "military_service_resolved_schema");
}

int kbo_military_resolved_seed_sql_path(char* out, size_t out_size)
{
    return kbo_save_state_db_path(out, out_size);
}

static uint32_t kbo_military_resolved_seed_sql_u32(char** vals, int index)
{
    unsigned int value = 0u;
    if (vals != NULL && vals[index] != NULL) {
        (void)sscanf(vals[index], "%u", &value);
    }
    return (uint32_t)value;
}

static void kbo_military_resolved_seed_sql_text(char** vals, int index, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';
    if (vals != NULL && vals[index] != NULL) {
        snprintf(out, out_size, "%s", vals[index]);
    }
}

static int kbo_military_resolved_seed_sql_load_cb(void* user_data, int ncols, char** vals, char** names)
{
    (void)names;
    KboMilitaryResolvedSeedSqlLoadContext* ctx = (KboMilitaryResolvedSeedSqlLoadContext*)user_data;
    if (ctx == NULL || ctx->seeds == NULL || vals == NULL || ncols < 5) {
        return 0;
    }
    if (ctx->count >= ctx->capacity) {
        ctx->overflowed++;
        return 0;
    }

    KboMilitaryServiceSeed seed = {0};
    kbo_military_resolved_seed_sql_text(vals, 0, seed.key, sizeof(seed.key));
    kbo_military_resolved_seed_sql_text(vals, 1, seed.service_team_code, sizeof(seed.service_team_code));
    kbo_military_resolved_seed_sql_text(vals, 2, seed.original_team_code, sizeof(seed.original_team_code));
    seed.service_return_yyyymmdd = kbo_military_resolved_seed_sql_u32(vals, 3);
    seed.player_id = kbo_military_resolved_seed_sql_u32(vals, 4);
    seed.service_total_days = KBO_MILITARY_SERVICE_DAYS;
    if (seed.key[0] == '\0' || seed.player_id == 0u) {
        return 0;
    }
    if (seed.service_team_code[0] == '\0') {
        snprintf(seed.service_team_code, sizeof(seed.service_team_code), "SANG");
    }

    ctx->seeds[ctx->count++] = seed;
    return 0;
}

int kbo_military_resolved_seed_sql_load(
    KboMilitaryServiceSeed* out,
    int max_count,
    int* out_count)
{
    if (out == NULL || max_count <= 0 || out_count == NULL) {
        return 0;
    }
    *out_count = 0;
    if (!kbo_military_resolved_seed_sql_ensure_schema("military_service_resolved_load_schema")) {
        return 0;
    }

    KboMilitaryResolvedSeedSqlLoadContext ctx = {out, max_count, 0, 0};
    static const char* sql =
        "SELECT source_key, service_team, original_team, service_return_yyyymmdd, player_id "
        "FROM military_service_resolved WHERE source_key != '' AND player_id != 0 "
        "ORDER BY source_key;";
    if (!kbo_save_state_query(sql, kbo_military_resolved_seed_sql_load_cb, &ctx, "military_service_resolved_load")) {
        return 0;
    }
    if (ctx.overflowed > 0) {
        kbo_log_runtimef(
            "KBO military service resolved sqlite load truncated rows=%d capacity=%d",
            ctx.overflowed,
            max_count);
    }
    *out_count = ctx.count;
    return 1;
}

static int kbo_military_resolved_seed_sql_append(
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

int kbo_military_resolved_seed_sql_replace_all(
    const KboMilitaryServiceSeed* seeds,
    int seed_count)
{
    if (seed_count < 0 || (seed_count > 0 && seeds == NULL)
            || !kbo_military_resolved_seed_sql_ensure_schema("military_service_resolved_replace_schema")) {
        return 0;
    }

    size_t sql_size = 512u + ((size_t)seed_count * 640u);
    char* sql = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sql_size);
    if (sql == NULL) {
        return 0;
    }

    size_t cursor = 0u;
    int ok = kbo_military_resolved_seed_sql_append(
        sql,
        sql_size,
        &cursor,
        "BEGIN IMMEDIATE;DELETE FROM military_service_resolved;");
    for (int i = 0; ok && i < seed_count; i++) {
        const KboMilitaryServiceSeed* seed = &seeds[i];
        if (seed->player_id == 0u || seed->key[0] == '\0') {
            continue;
        }

        uint32_t return_yyyymmdd = seed->service_return_yyyymmdd;
        if (return_yyyymmdd == 0u && seed->service_start_yyyymmdd != 0u) {
            return_yyyymmdd = kbo_military_yyyymmdd_add_days(
                seed->service_start_yyyymmdd,
                seed->service_total_days > 0 ? seed->service_total_days : KBO_MILITARY_SERVICE_DAYS);
        }

        char key[96] = {0};
        char service_team[48] = {0};
        char original_team[48] = {0};
        if (!kbo_sql_escape_literal(key, sizeof(key), seed->key)
                || !kbo_sql_escape_literal(
                    service_team,
                    sizeof(service_team),
                    seed->service_team_code[0] != '\0' ? seed->service_team_code : "SANG")
                || !kbo_sql_escape_literal(original_team, sizeof(original_team), seed->original_team_code)) {
            ok = 0;
            break;
        }

        ok = kbo_military_resolved_seed_sql_append(
            sql,
            sql_size,
            &cursor,
            "INSERT OR REPLACE INTO military_service_resolved("
            "source_key, service_team, original_team, service_return_yyyymmdd, player_id, updated_at"
            ") VALUES('%s', '%s', '%s', %u, %u, datetime('now'));",
            key,
            service_team,
            original_team,
            return_yyyymmdd,
            seed->player_id);
    }
    if (ok) {
        ok = kbo_military_resolved_seed_sql_append(sql, sql_size, &cursor, "COMMIT;");
    }
    if (!ok) {
        HeapFree(GetProcessHeap(), 0, sql);
        kbo_log_runtimef(
            "KBO military service resolved sqlite persist failed reason=sql_buffer_full rows=%d",
            seed_count);
        return 0;
    }

    ok = kbo_save_state_exec(sql, "military_service_resolved_replace");
    HeapFree(GetProcessHeap(), 0, sql);
    return ok;
}
