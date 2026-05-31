#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../asian_games_roster_sql_store_internal.h"

#include <stdio.h>

#include "../../../../../core/logging/core_log.h"
#include "../../../../../core/sql/save_state/save_state_sqlite.h"

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
