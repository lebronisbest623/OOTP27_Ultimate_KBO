#include "../../runtime/common/custom_events_common.h"
#include "asian_games_roster_store.h"
#include "internal/asian_games_roster_store_internal.h"
#include <stdio.h>
#include <string.h>
#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/logging/core_log.h"
#include "sql/asian_games_roster_sql_store.h"

int kbo_get_asian_games_roster_csv_path(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';

    return kbo_asian_games_roster_sql_path(out, out_size);
}

int kbo_get_asian_games_roster_history_csv_path(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';

    return kbo_asian_games_roster_sql_path(out, out_size);
}

int kbo_get_asian_games_tournament_history_csv_path(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';

    return kbo_asian_games_roster_sql_path(out, out_size);
}

void kbo_clear_asian_games_roster_memory(const char* source)
{
    memset(g_kbo_asian_games_roster, 0, sizeof(g_kbo_asian_games_roster));
    g_kbo_asian_games_roster_count = 0;
    g_kbo_asian_games_roster_year = 0;
    g_kbo_asian_games_result = KBO_ASIAN_GAMES_RESULT_UNKNOWN;
    g_kbo_asian_games_roster_save_path[0] = '\0';
    kbo_log_runtimef("KBO Asian Games roster memory cleared source=%s", source != NULL ? source : "");
}

void kbo_clear_asian_games_roster_if_save_changed(const char* source)
{
    char path[MAX_PATH] = {0};
    if (!kbo_get_asian_games_roster_csv_path(path, sizeof(path))) {
        if (g_kbo_asian_games_roster_count > 0 || g_kbo_asian_games_roster_save_path[0] != '\0') {
            kbo_clear_asian_games_roster_memory(source);
        }
        return;
    }

    if (g_kbo_asian_games_roster_save_path[0] != '\0'
            && _stricmp(g_kbo_asian_games_roster_save_path, path) != 0) {
        kbo_clear_asian_games_roster_memory(source);
    }
}

int kbo_save_asian_games_roster_csv(const char* source)
{
    char path[MAX_PATH] = {0};
    if (!kbo_get_asian_games_roster_csv_path(path, sizeof(path))) {
        kbo_log_runtimef("KBO Asian Games roster csv save skipped source=%s reason=path_unavailable", source != NULL ? source : "");
        return 0;
    }

    LONG roster_count = g_kbo_asian_games_roster_count;
    if (roster_count < 0 || roster_count > KBO_ASIAN_GAMES_ROSTER_SIZE) {
        roster_count = 0;
    }

    int ok = kbo_asian_games_roster_sql_replace_current(
        g_kbo_asian_games_roster_year,
        g_kbo_asian_games_result,
        g_kbo_asian_games_roster,
        (int)roster_count);
    if (!ok) {
        kbo_log_runtimef("KBO Asian Games roster sqlite save failed path=%s", path);
        return 0;
    }
    snprintf(g_kbo_asian_games_roster_save_path, sizeof(g_kbo_asian_games_roster_save_path), "%s", path);
    int history_saved = ok ? kbo_save_asian_games_roster_history_csv(source) : 0;
    kbo_log_runtimef(
        "KBO Asian Games roster csv save source=%s ok=%d history=%d year=%u count=%ld path=%s",
        source != NULL ? source : "",
        ok,
        history_saved,
        g_kbo_asian_games_roster_year,
        roster_count,
        path);
    return ok;
}

int kbo_load_asian_games_roster_csv(const char* source)
{
    kbo_clear_asian_games_roster_if_save_changed(source);

    char path[MAX_PATH] = {0};
    if (!kbo_get_asian_games_roster_csv_path(path, sizeof(path))) {
        kbo_clear_asian_games_roster_memory(source);
        return 0;
    }

    KboAsianGamesRosterEntry loaded[KBO_ASIAN_GAMES_ROSTER_SIZE];
    memset(loaded, 0, sizeof(loaded));
    int loaded_count = 0;
    uint32_t loaded_year = 0;
    uint8_t loaded_result = KBO_ASIAN_GAMES_RESULT_UNKNOWN;

    if (!kbo_asian_games_roster_sql_load_current(
            loaded,
            KBO_ASIAN_GAMES_ROSTER_SIZE,
            &loaded_year,
            &loaded_result,
            &loaded_count)) {
        kbo_clear_asian_games_roster_memory(source);
        kbo_log_runtimef("KBO Asian Games roster csv load skipped source=%s reason=sqlite_load_failed path=%s", source != NULL ? source : "", path);
        return 0;
    }

    if (loaded_count <= 0) {
        return 0;
    }

    memset(g_kbo_asian_games_roster, 0, sizeof(g_kbo_asian_games_roster));
    memcpy(g_kbo_asian_games_roster, loaded, sizeof(KboAsianGamesRosterEntry) * (size_t)loaded_count);
    g_kbo_asian_games_roster_count = loaded_count;
    g_kbo_asian_games_roster_year = loaded_year;
    g_kbo_asian_games_result = loaded_result;
    snprintf(g_kbo_asian_games_roster_save_path, sizeof(g_kbo_asian_games_roster_save_path), "%s", path);
    kbo_log_runtimef("KBO Asian Games roster csv load source=%s year=%u count=%d result=%u path=%s", source != NULL ? source : "", loaded_year, loaded_count, (uint32_t)g_kbo_asian_games_result, path);
    return loaded_count;
}

