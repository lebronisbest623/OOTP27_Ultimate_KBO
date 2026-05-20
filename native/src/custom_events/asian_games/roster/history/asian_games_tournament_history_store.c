#include "../asian_games_roster_store.h"
#include <stdio.h>
#include <string.h>
#include "../../../../core/logging/core_log.h"
#include "../sql/asian_games_roster_sql_store.h"

int kbo_load_asian_games_tournament_history(
    KboAsianGamesTournamentHistoryEntry* out,
    int max_count,
    const char* source)
{
    if (out == NULL || max_count <= 0) {
        return 0;
    }
    memset(out, 0, sizeof(*out) * (size_t)max_count);

    char path[MAX_PATH] = {0};
    if (!kbo_get_asian_games_tournament_history_csv_path(path, sizeof(path))) {
        return 0;
    }

    int count = 0;
    if (!kbo_asian_games_tournament_sql_load_history(out, max_count, &count)) {
        return 0;
    }

    kbo_log_runtimef("KBO Asian Games tournament history load source=%s count=%d path=%s", source != NULL ? source : "", count, path);
    return count;
}

int kbo_append_asian_games_tournament_history(
    uint32_t year,
    uint32_t final_date,
    uint8_t result,
    const char* source)
{
    if (year == 0u || final_date == 0u
            || (result != KBO_ASIAN_GAMES_RESULT_GOLD
                && result != KBO_ASIAN_GAMES_RESULT_NO_GOLD)) {
        return 0;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_get_asian_games_tournament_history_csv_path(path, sizeof(path))) {
        return 0;
    }

    if (!kbo_asian_games_tournament_sql_upsert_history(year, final_date, result)) {
        return 0;
    }

    kbo_log_runtimef(
        "KBO Asian Games tournament history save source=%s year=%u final=%u result=%u path=%s",
        source != NULL ? source : "",
        year,
        final_date,
        (uint32_t)result,
        path);
    return 1;
}
