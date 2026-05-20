#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>

#include "../../../../amateur_player_quality/api/amateur_player_quality.h"
#include "../../../../core/files/save_paths/core_save_paths.h"
#include "../../../../core/logging/core_log.h"
#include "../../../../fa_market_classification/api/fa_market_classification.h"
#include "../../../../foreign/replacement_seed/api/foreign_replacement_seed.h"
#include "../../../seed/registry/military_seed_registry.h"
#include "../military_service_days_tick.h"

void kbo_military_prewarm_save_scoped_bootstrap_files(const char* save_path)
{
    if (save_path == NULL || save_path[0] == '\0') {
        return;
    }

    kbo_ensure_military_service_seeds_loaded();
    kbo_ensure_foreign_replacement_player_seeds_loaded();
    kbo_ensure_amateur_reputation_seeds_loaded();

    KboFaMarketSeedCase fa_seed_cases[1];
    char fa_seed_path[MAX_PATH] = {0};
    kbo_load_fa_market_seed_cases(fa_seed_cases, 1, fa_seed_path, sizeof(fa_seed_path));

    kbo_log_runtimef(
        "KBO save bootstrap files prewarmed save=%s fa_seed=%s",
        save_path,
        fa_seed_path[0] != '\0' ? fa_seed_path : "-");
}
