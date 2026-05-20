#include "../../runtime/common/custom_events_common.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdint.h>
#include <stdio.h>

#include "state_and_paths.h"
#include "../../../core/files/save_paths/core_save_paths.h"
#include "../../../core/sync/lock.h"

KboAsianGamesScheduleSeed g_kbo_asian_games_schedule_seeds[KBO_ASIAN_GAMES_SCHEDULE_SEED_MAX];
int g_kbo_asian_games_schedule_seed_count = 0;
static KboLock g_kbo_asian_games_schedule_seed_lock = KBO_LOCK_INIT;
LONG g_kbo_asian_games_schedule_seed_loaded = 0;
char g_kbo_asian_games_schedule_seed_loaded_key[MAX_PATH * 6] = {0};

void kbo_lock_asian_games_schedule_seeds(void)
{
    kbo_lock_enter(&g_kbo_asian_games_schedule_seed_lock);
}

void kbo_unlock_asian_games_schedule_seeds(void)
{
    kbo_lock_leave(&g_kbo_asian_games_schedule_seed_lock);
}

int kbo_get_save_asian_games_schedule_seed_path(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2) {
        return 0;
    }
    out[0] = '\0';
    return kbo_get_save_scoped_data_file("config\\asian_games_schedule_seed.csv", out, out_size);
}

int kbo_get_global_asian_games_schedule_seed_path(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2) {
        return 0;
    }
    out[0] = '\0';
    return kbo_get_global_data_file("asian_games_schedule_seed.csv", out, out_size);
}
