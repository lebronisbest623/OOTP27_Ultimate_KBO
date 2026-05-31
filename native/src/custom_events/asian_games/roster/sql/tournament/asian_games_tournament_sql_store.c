#include "../asian_games_roster_sql_store_internal.h"

#include <stdio.h>

#include "../../../../../core/logging/core_log.h"
#include "../../../../../core/sql/save_state/save_state_sqlite.h"

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
