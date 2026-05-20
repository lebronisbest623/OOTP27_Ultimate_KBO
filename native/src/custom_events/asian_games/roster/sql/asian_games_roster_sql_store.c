#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "asian_games_roster_sql_store.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "../../../../core/logging/core_log.h"
#include "../../../../core/sql/save_state/save_state_sqlite.h"

typedef struct KboAsianGamesRosterSqlLoadContext {
    KboAsianGamesRosterEntry* entries;
    int capacity;
    int count;
    int overflowed;
    uint32_t year;
    uint8_t result;
} KboAsianGamesRosterSqlLoadContext;

typedef struct KboAsianGamesRosterHistorySqlLoadContext {
    KboAsianGamesRosterHistoryEntry* rows;
    int capacity;
    int count;
    int overflowed;
} KboAsianGamesRosterHistorySqlLoadContext;

typedef struct KboAsianGamesTournamentSqlLoadContext {
    KboAsianGamesTournamentHistoryEntry* rows;
    int capacity;
    int count;
    int overflowed;
} KboAsianGamesTournamentSqlLoadContext;

static int kbo_asian_games_roster_sql_ensure_schema(const char* source)
{
    static const char* sql =
        "CREATE TABLE IF NOT EXISTS asian_games_roster ("
        "slot_index INTEGER NOT NULL PRIMARY KEY,"
        "year INTEGER NOT NULL,"
        "player_id INTEGER NOT NULL,"
        "original_team_id INTEGER NOT NULL DEFAULT 0,"
        "original_league_id INTEGER NOT NULL DEFAULT 0,"
        "departure_date INTEGER NOT NULL DEFAULT 0,"
        "return_date INTEGER NOT NULL DEFAULT 0,"
        "age INTEGER NOT NULL DEFAULT 0,"
        "role INTEGER NOT NULL DEFAULT 0,"
        "wildcard INTEGER NOT NULL DEFAULT 0,"
        "military_unserved INTEGER NOT NULL DEFAULT 0,"
        "old_restricted INTEGER NOT NULL DEFAULT 0,"
        "old_secondary_restricted INTEGER NOT NULL DEFAULT 0,"
        "old_injury_active INTEGER NOT NULL DEFAULT 0,"
        "old_injury_days_left INTEGER NOT NULL DEFAULT 0,"
        "departed INTEGER NOT NULL DEFAULT 0,"
        "returned INTEGER NOT NULL DEFAULT 0,"
        "exempted INTEGER NOT NULL DEFAULT 0,"
        "score INTEGER NOT NULL DEFAULT 0,"
        "tournament_result INTEGER NOT NULL DEFAULT 0,"
        "updated_at TEXT NOT NULL DEFAULT (datetime('now'))"
        ");"
        "CREATE TABLE IF NOT EXISTS asian_games_roster_history ("
        "year INTEGER NOT NULL,"
        "slot_index INTEGER NOT NULL,"
        "player_id INTEGER NOT NULL,"
        "original_team_id INTEGER NOT NULL DEFAULT 0,"
        "original_league_id INTEGER NOT NULL DEFAULT 0,"
        "departure_date INTEGER NOT NULL DEFAULT 0,"
        "return_date INTEGER NOT NULL DEFAULT 0,"
        "age INTEGER NOT NULL DEFAULT 0,"
        "role INTEGER NOT NULL DEFAULT 0,"
        "wildcard INTEGER NOT NULL DEFAULT 0,"
        "military_unserved INTEGER NOT NULL DEFAULT 0,"
        "old_restricted INTEGER NOT NULL DEFAULT 0,"
        "old_secondary_restricted INTEGER NOT NULL DEFAULT 0,"
        "old_injury_active INTEGER NOT NULL DEFAULT 0,"
        "old_injury_days_left INTEGER NOT NULL DEFAULT 0,"
        "departed INTEGER NOT NULL DEFAULT 0,"
        "returned INTEGER NOT NULL DEFAULT 0,"
        "exempted INTEGER NOT NULL DEFAULT 0,"
        "score INTEGER NOT NULL DEFAULT 0,"
        "tournament_result INTEGER NOT NULL DEFAULT 0,"
        "updated_at TEXT NOT NULL DEFAULT (datetime('now')),"
        "PRIMARY KEY(year, slot_index)"
        ");"
        "CREATE TABLE IF NOT EXISTS asian_games_tournament_history ("
        "year INTEGER NOT NULL PRIMARY KEY,"
        "final_date INTEGER NOT NULL,"
        "result INTEGER NOT NULL,"
        "updated_at TEXT NOT NULL DEFAULT (datetime('now'))"
        ");"
        "INSERT OR REPLACE INTO kbo_schema(schema_key, schema_version, updated_at) "
        "VALUES('asian_games_roster', 1, datetime('now'));"
        "INSERT OR REPLACE INTO kbo_schema(schema_key, schema_version, updated_at) "
        "VALUES('asian_games_roster_history', 1, datetime('now'));"
        "INSERT OR REPLACE INTO kbo_schema(schema_key, schema_version, updated_at) "
        "VALUES('asian_games_tournament_history', 1, datetime('now'));";
    return kbo_save_state_exec(sql, source != NULL ? source : "asian_games_roster_schema");
}

int kbo_asian_games_roster_sql_path(char* out, size_t out_size)
{
    return kbo_save_state_db_path(out, out_size);
}

static uint32_t kbo_asian_games_roster_sql_u32(char** vals, int index)
{
    unsigned int value = 0u;
    if (vals != NULL && vals[index] != NULL) {
        (void)sscanf(vals[index], "%u", &value);
    }
    return (uint32_t)value;
}

static int32_t kbo_asian_games_roster_sql_i32(char** vals, int index)
{
    int value = 0;
    if (vals != NULL && vals[index] != NULL) {
        (void)sscanf(vals[index], "%d", &value);
    }
    return (int32_t)value;
}

static void kbo_asian_games_roster_sql_assign_entry(
    KboAsianGamesRosterEntry* entry,
    char** vals,
    int offset)
{
    entry->player_id = kbo_asian_games_roster_sql_u32(vals, offset + 0);
    entry->original_team_id = kbo_asian_games_roster_sql_u32(vals, offset + 1);
    entry->original_league_id = kbo_asian_games_roster_sql_u32(vals, offset + 2);
    entry->departure_date = kbo_asian_games_roster_sql_u32(vals, offset + 3);
    entry->return_date = kbo_asian_games_roster_sql_u32(vals, offset + 4);
    entry->age = (uint16_t)(kbo_asian_games_roster_sql_u32(vals, offset + 5) & 0xffffu);
    entry->role = (uint8_t)(kbo_asian_games_roster_sql_u32(vals, offset + 6) & 0xffu);
    entry->wildcard = (uint8_t)(kbo_asian_games_roster_sql_u32(vals, offset + 7) & 0xffu);
    entry->military_unserved = (uint8_t)(kbo_asian_games_roster_sql_u32(vals, offset + 8) & 0xffu);
    entry->old_restricted = (uint8_t)(kbo_asian_games_roster_sql_u32(vals, offset + 9) & 0xffu);
    entry->old_secondary_restricted = (uint8_t)(kbo_asian_games_roster_sql_u32(vals, offset + 10) & 0xffu);
    entry->old_injury_active = (uint8_t)(kbo_asian_games_roster_sql_u32(vals, offset + 11) & 0xffu);
    entry->old_injury_days_left = (int16_t)kbo_asian_games_roster_sql_i32(vals, offset + 12);
    entry->departed = (uint8_t)(kbo_asian_games_roster_sql_u32(vals, offset + 13) & 0xffu);
    entry->returned = (uint8_t)(kbo_asian_games_roster_sql_u32(vals, offset + 14) & 0xffu);
    entry->exempted = (uint8_t)(kbo_asian_games_roster_sql_u32(vals, offset + 15) & 0xffu);
    entry->score = kbo_asian_games_roster_sql_i32(vals, offset + 16);
    entry->player_ptr = 0u;
}

static int kbo_asian_games_roster_sql_current_cb(void* user_data, int ncols, char** vals, char** names)
{
    (void)names;
    KboAsianGamesRosterSqlLoadContext* ctx = (KboAsianGamesRosterSqlLoadContext*)user_data;
    if (ctx == NULL || ctx->entries == NULL || vals == NULL || ncols < 20) {
        return 0;
    }
    if (ctx->count >= ctx->capacity) {
        ctx->overflowed++;
        return 0;
    }

    KboAsianGamesRosterEntry entry = {0};
    uint32_t year = kbo_asian_games_roster_sql_u32(vals, 0);
    kbo_asian_games_roster_sql_assign_entry(&entry, vals, 2);
    uint8_t result = (uint8_t)(kbo_asian_games_roster_sql_u32(vals, 19) & 0xffu);
    if (year == 0u || entry.player_id == 0u) {
        return 0;
    }

    ctx->year = year;
    if (result == KBO_ASIAN_GAMES_RESULT_GOLD || result == KBO_ASIAN_GAMES_RESULT_NO_GOLD) {
        ctx->result = result;
    }
    ctx->entries[ctx->count++] = entry;
    return 0;
}

int kbo_asian_games_roster_sql_load_current(
    KboAsianGamesRosterEntry* entries,
    int max_entries,
    uint32_t* out_year,
    uint8_t* out_result,
    int* out_count)
{
    if (entries == NULL || max_entries <= 0 || out_year == NULL || out_result == NULL || out_count == NULL) {
        return 0;
    }
    *out_year = 0u;
    *out_result = KBO_ASIAN_GAMES_RESULT_UNKNOWN;
    *out_count = 0;
    if (!kbo_asian_games_roster_sql_ensure_schema("asian_games_roster_load_schema")) {
        return 0;
    }

    KboAsianGamesRosterSqlLoadContext ctx = {
        entries,
        max_entries,
        0,
        0,
        0u,
        KBO_ASIAN_GAMES_RESULT_UNKNOWN
    };
    static const char* sql =
        "SELECT year, slot_index, player_id, original_team_id, original_league_id, departure_date, return_date, "
        "age, role, wildcard, military_unserved, old_restricted, old_secondary_restricted, old_injury_active, "
        "old_injury_days_left, departed, returned, exempted, score, tournament_result "
        "FROM asian_games_roster WHERE player_id != 0 ORDER BY slot_index;";
    if (!kbo_save_state_query(sql, kbo_asian_games_roster_sql_current_cb, &ctx, "asian_games_roster_load")) {
        return 0;
    }
    if (ctx.overflowed > 0) {
        kbo_log_runtimef(
            "KBO Asian Games roster sqlite load truncated rows=%d capacity=%d",
            ctx.overflowed,
            max_entries);
    }
    *out_year = ctx.year;
    *out_result = ctx.result;
    *out_count = ctx.count;
    return 1;
}

static int kbo_asian_games_roster_sql_append(
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

static int kbo_asian_games_roster_sql_append_entry(
    char* sql,
    size_t sql_size,
    size_t* cursor,
    const char* table,
    uint32_t year,
    uint32_t slot_index,
    uint8_t result,
    const KboAsianGamesRosterEntry* entry)
{
    if (entry == NULL || entry->player_id == 0u) {
        return 1;
    }

    return kbo_asian_games_roster_sql_append(
        sql,
        sql_size,
        cursor,
        "INSERT OR REPLACE INTO %s("
        "year, slot_index, player_id, original_team_id, original_league_id, departure_date, return_date, "
        "age, role, wildcard, military_unserved, old_restricted, old_secondary_restricted, old_injury_active, "
        "old_injury_days_left, departed, returned, exempted, score, tournament_result, updated_at"
        ") VALUES(%u, %u, %u, %u, %u, %u, %u, %u, %u, %u, %u, %u, %u, %u, %d, %u, %u, %u, %d, %u, datetime('now'));",
        table,
        year,
        slot_index,
        entry->player_id,
        entry->original_team_id,
        entry->original_league_id,
        entry->departure_date,
        entry->return_date,
        (uint32_t)entry->age,
        (uint32_t)entry->role,
        (uint32_t)entry->wildcard,
        (uint32_t)entry->military_unserved,
        (uint32_t)entry->old_restricted,
        (uint32_t)entry->old_secondary_restricted,
        (uint32_t)entry->old_injury_active,
        (int)entry->old_injury_days_left,
        (uint32_t)entry->departed,
        (uint32_t)entry->returned,
        (uint32_t)entry->exempted,
        entry->score,
        (uint32_t)result);
}

int kbo_asian_games_roster_sql_replace_current(
    uint32_t year,
    uint8_t result,
    const KboAsianGamesRosterEntry* entries,
    int entry_count)
{
    if (year == 0u || entry_count < 0 || (entry_count > 0 && entries == NULL)
            || !kbo_asian_games_roster_sql_ensure_schema("asian_games_roster_replace_schema")) {
        return 0;
    }

    size_t sql_size = 1024u + ((size_t)entry_count * 640u);
    char* sql = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sql_size);
    if (sql == NULL) {
        return 0;
    }

    size_t cursor = 0u;
    int ok = kbo_asian_games_roster_sql_append(sql, sql_size, &cursor, "BEGIN IMMEDIATE;DELETE FROM asian_games_roster;");
    for (int i = 0; ok && i < entry_count; i++) {
        ok = kbo_asian_games_roster_sql_append_entry(
            sql,
            sql_size,
            &cursor,
            "asian_games_roster",
            year,
            (uint32_t)i + 1u,
            result,
            &entries[i]);
    }
    if (ok) {
        ok = kbo_asian_games_roster_sql_append(sql, sql_size, &cursor, "COMMIT;");
    }
    if (!ok) {
        HeapFree(GetProcessHeap(), 0, sql);
        kbo_log_runtimef("KBO Asian Games roster sqlite replace failed reason=sql_buffer_full rows=%d", entry_count);
        return 0;
    }

    ok = kbo_save_state_exec(sql, "asian_games_roster_replace");
    HeapFree(GetProcessHeap(), 0, sql);
    return ok;
}

static int kbo_asian_games_roster_sql_history_cb(void* user_data, int ncols, char** vals, char** names)
{
    (void)names;
    KboAsianGamesRosterHistorySqlLoadContext* ctx =
        (KboAsianGamesRosterHistorySqlLoadContext*)user_data;
    if (ctx == NULL || ctx->rows == NULL || vals == NULL || ncols < 20) {
        return 0;
    }
    if (ctx->count >= ctx->capacity) {
        ctx->overflowed++;
        return 0;
    }

    KboAsianGamesRosterHistoryEntry row = {0};
    row.year = kbo_asian_games_roster_sql_u32(vals, 0);
    row.index = kbo_asian_games_roster_sql_u32(vals, 1);
    kbo_asian_games_roster_sql_assign_entry(&row.entry, vals, 2);
    row.tournament_result = (uint8_t)(kbo_asian_games_roster_sql_u32(vals, 19) & 0xffu);
    if (row.year == 0u || row.entry.player_id == 0u) {
        return 0;
    }
    if (row.index == 0u) {
        row.index = (uint32_t)ctx->count + 1u;
    }
    ctx->rows[ctx->count++] = row;
    return 0;
}

int kbo_asian_games_roster_sql_load_history(
    KboAsianGamesRosterHistoryEntry* out,
    int max_count,
    int* out_count)
{
    if (out == NULL || max_count <= 0 || out_count == NULL) {
        return 0;
    }
    *out_count = 0;
    if (!kbo_asian_games_roster_sql_ensure_schema("asian_games_roster_history_load_schema")) {
        return 0;
    }

    KboAsianGamesRosterHistorySqlLoadContext ctx = {out, max_count, 0, 0};
    static const char* sql =
        "SELECT year, slot_index, player_id, original_team_id, original_league_id, departure_date, return_date, "
        "age, role, wildcard, military_unserved, old_restricted, old_secondary_restricted, old_injury_active, "
        "old_injury_days_left, departed, returned, exempted, score, tournament_result "
        "FROM asian_games_roster_history WHERE player_id != 0 ORDER BY year, slot_index;";
    if (!kbo_save_state_query(sql, kbo_asian_games_roster_sql_history_cb, &ctx, "asian_games_roster_history_load")) {
        return 0;
    }
    if (ctx.overflowed > 0) {
        kbo_log_runtimef(
            "KBO Asian Games roster history sqlite load truncated rows=%d capacity=%d",
            ctx.overflowed,
            max_count);
    }
    *out_count = ctx.count;
    return 1;
}

int kbo_asian_games_roster_sql_replace_history_year(
    uint32_t year,
    uint8_t result,
    const KboAsianGamesRosterEntry* entries,
    int entry_count)
{
    if (year == 0u || entry_count < 0 || (entry_count > 0 && entries == NULL)
            || !kbo_asian_games_roster_sql_ensure_schema("asian_games_roster_history_replace_schema")) {
        return 0;
    }

    size_t sql_size = 1024u + ((size_t)entry_count * 640u);
    char* sql = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sql_size);
    if (sql == NULL) {
        return 0;
    }

    size_t cursor = 0u;
    int ok = kbo_asian_games_roster_sql_append(
        sql,
        sql_size,
        &cursor,
        "BEGIN IMMEDIATE;DELETE FROM asian_games_roster_history WHERE year=%u;",
        year);
    for (int i = 0; ok && i < entry_count; i++) {
        ok = kbo_asian_games_roster_sql_append_entry(
            sql,
            sql_size,
            &cursor,
            "asian_games_roster_history",
            year,
            (uint32_t)i + 1u,
            result,
            &entries[i]);
    }
    if (ok) {
        ok = kbo_asian_games_roster_sql_append(sql, sql_size, &cursor, "COMMIT;");
    }
    if (!ok) {
        HeapFree(GetProcessHeap(), 0, sql);
        kbo_log_runtimef(
            "KBO Asian Games roster history sqlite replace failed reason=sql_buffer_full rows=%d",
            entry_count);
        return 0;
    }

    ok = kbo_save_state_exec(sql, "asian_games_roster_history_replace_year");
    HeapFree(GetProcessHeap(), 0, sql);
    return ok;
}

static int kbo_asian_games_tournament_sql_history_cb(void* user_data, int ncols, char** vals, char** names)
{
    (void)names;
    KboAsianGamesTournamentSqlLoadContext* ctx = (KboAsianGamesTournamentSqlLoadContext*)user_data;
    if (ctx == NULL || ctx->rows == NULL || vals == NULL || ncols < 3) {
        return 0;
    }
    if (ctx->count >= ctx->capacity) {
        ctx->overflowed++;
        return 0;
    }

    KboAsianGamesTournamentHistoryEntry row = {0};
    row.year = kbo_asian_games_roster_sql_u32(vals, 0);
    row.final_date = kbo_asian_games_roster_sql_u32(vals, 1);
    row.result = (uint8_t)(kbo_asian_games_roster_sql_u32(vals, 2) & 0xffu);
    if (row.year == 0u || row.final_date == 0u
            || (row.result != KBO_ASIAN_GAMES_RESULT_GOLD
                && row.result != KBO_ASIAN_GAMES_RESULT_NO_GOLD)) {
        return 0;
    }
    ctx->rows[ctx->count++] = row;
    return 0;
}

int kbo_asian_games_tournament_sql_load_history(
    KboAsianGamesTournamentHistoryEntry* out,
    int max_count,
    int* out_count)
{
    if (out == NULL || max_count <= 0 || out_count == NULL) {
        return 0;
    }
    *out_count = 0;
    if (!kbo_asian_games_roster_sql_ensure_schema("asian_games_tournament_history_load_schema")) {
        return 0;
    }

    KboAsianGamesTournamentSqlLoadContext ctx = {out, max_count, 0, 0};
    static const char* sql =
        "SELECT year, final_date, result FROM asian_games_tournament_history ORDER BY year;";
    if (!kbo_save_state_query(sql, kbo_asian_games_tournament_sql_history_cb, &ctx, "asian_games_tournament_history_load")) {
        return 0;
    }
    if (ctx.overflowed > 0) {
        kbo_log_runtimef(
            "KBO Asian Games tournament history sqlite load truncated rows=%d capacity=%d",
            ctx.overflowed,
            max_count);
    }
    *out_count = ctx.count;
    return 1;
}

int kbo_asian_games_tournament_sql_upsert_history(
    uint32_t year,
    uint32_t final_date,
    uint8_t result)
{
    if (year == 0u || final_date == 0u
            || (result != KBO_ASIAN_GAMES_RESULT_GOLD && result != KBO_ASIAN_GAMES_RESULT_NO_GOLD)
            || !kbo_asian_games_roster_sql_ensure_schema("asian_games_tournament_history_upsert_schema")) {
        return 0;
    }

    char sql[512] = {0};
    int len = snprintf(
        sql,
        sizeof(sql),
        "INSERT OR REPLACE INTO asian_games_tournament_history("
        "year, final_date, result, updated_at"
        ") VALUES(%u, %u, %u, datetime('now'));",
        year,
        final_date,
        (uint32_t)result);
    if (len <= 0 || len >= (int)sizeof(sql)) {
        return 0;
    }
    return kbo_save_state_exec(sql, "asian_games_tournament_history_upsert");
}
