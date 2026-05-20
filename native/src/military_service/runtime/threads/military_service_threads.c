#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../../../core/core_flags/api/flags_api.h"
#include "../../../core/logging/core_log.h"
#include "../../military_service.h"
#include "../days_tick/military_service_days_tick.h"
#include "../state/military_service_runtime_state.h"

void start_kbo_military_seed_bootstrap_thread(void)
{
    if (!kbo_fix_enabled()) {
        return;
    }
    if (InterlockedCompareExchange(&g_military_seed_bootstrap_started, 1, 0) != 0) {
        return;
    }

    if (!kbo_start_runtime_thread(kbo_military_seed_bootstrap_thread, NULL, "military seed bootstrap")) {
        InterlockedExchange(&g_military_seed_bootstrap_started, 0);
    }
}

void start_kbo_military_days_tick_thread(void)
{
    if (!kbo_fix_enabled()) {
        return;
    }
    if (InterlockedCompareExchange(&g_military_days_tick_started, 1, 0) != 0) {
        return;
    }

    if (!kbo_start_runtime_thread(kbo_military_days_tick_thread, NULL, "military days tick")) {
        InterlockedExchange(&g_military_days_tick_started, 0);
    }
}

