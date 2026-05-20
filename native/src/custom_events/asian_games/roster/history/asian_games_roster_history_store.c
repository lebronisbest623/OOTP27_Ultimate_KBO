#include "../internal/asian_games_roster_store_internal.h"
#include <stdio.h>
#include <string.h>
#include "../../../../bootstrap/abi/ootp_offsets.h"
#include "../../../../core/logging/core_log.h"
#include "../sql/asian_games_roster_sql_store.h"

int kbo_load_asian_games_roster_history(
    KboAsianGamesRosterHistoryEntry* out,
    int max_count,
    const char* source)
{
    if (out == NULL || max_count <= 0) {
        return 0;
    }
    memset(out, 0, sizeof(*out) * (size_t)max_count);

    char path[MAX_PATH] = {0};
    if (!kbo_get_asian_games_roster_history_csv_path(path, sizeof(path))) {
        return 0;
    }

    int count = 0;
    if (!kbo_asian_games_roster_sql_load_history(out, max_count, &count)) {
        return 0;
    }

    kbo_log_runtimef(
        "KBO Asian Games roster history load source=%s count=%d path=%s",
        source != NULL ? source : "",
        count,
        path);
    return count;
}

int kbo_save_asian_games_roster_history_csv(const char* source)
{
    if (g_kbo_asian_games_roster_year == 0u) {
        return 0;
    }
    LONG roster_count = g_kbo_asian_games_roster_count;
    if (roster_count <= 0 || roster_count > KBO_ASIAN_GAMES_ROSTER_SIZE) {
        return 0;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_get_asian_games_roster_history_csv_path(path, sizeof(path))) {
        return 0;
    }

    int ok = kbo_asian_games_roster_sql_replace_history_year(
        g_kbo_asian_games_roster_year,
        g_kbo_asian_games_result,
        g_kbo_asian_games_roster,
        (int)roster_count);
    if (!ok) {
        return 0;
    }

    kbo_log_runtimef(
        "KBO Asian Games roster history save source=%s year=%u roster=%ld total=%d path=%s",
        source != NULL ? source : "",
        g_kbo_asian_games_roster_year,
        roster_count,
        (int)roster_count,
        path);
    return 1;
}
