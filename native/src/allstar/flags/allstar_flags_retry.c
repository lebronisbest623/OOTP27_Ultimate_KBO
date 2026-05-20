/* All-Star startup retry thread. */

#include "allstar_flags.h"

#include <windows.h>

#include "../allstar_league_context/allstar_league_context.h"
#include "../allstar_native_events/schedule/schedule_dates.h"
#include "../team_patch/allstar_team_patch.h"
#include "../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../core/logging/core_log.h"
#include "../../core/runtime_tuning/runtime_tuning_policy.h"
#include "../../runtime_memory/runtime_memory.h"

static void kbo_allstar_prepare_for_native_hooks(uintptr_t league_ptr, uint32_t league_id, const char* source)
{
    InterlockedExchangePointer(
        (PVOID volatile*)&g_allstar_schedule_import_league_ptr,
        (PVOID)league_ptr);
    seed_kbo_allstar_schedule_dates(league_ptr, source);
    patch_kbo_allstar_team_names_for_league_id(league_id, source);
    patch_kbo_allstar_team_names_for_known_exhibition_teams(source);
}

static DWORD WINAPI kbo_allstar_force_retry_thread(LPVOID parameter)
{
    (void)parameter;

    const KboRuntimeTuningPolicy* tuning = kbo_runtime_tuning_policy();
    for (int attempt = 1; attempt <= tuning->allstar_force_retry_attempts; attempt++) {
        uint32_t league_id = kbo_resolve_kbo_league_id();

        uintptr_t captured = (uintptr_t)InterlockedCompareExchangePointer(
            (PVOID volatile*)&g_allstar_schedule_import_league_ptr,
            NULL,
            NULL);
        if (captured != 0
                && enable_kbo_allstar_raw_flags_if_kbo_context(captured, "startup_retry_captured_raw")) {
            kbo_log_runtimef(
                "KBO all-star force retry prepared captured raw league attempt=%d league_id=%u league=%p mode=hook_driven",
                attempt,
                league_id,
                (void*)captured);
            kbo_allstar_prepare_for_native_hooks(captured, league_id, "startup_retry_captured_raw");
            return 0;
        }

        patch_kbo_allstar_team_names_for_league_id(league_id, "startup_retry");
        patch_kbo_allstar_team_names_for_known_exhibition_teams("startup_retry");

        uintptr_t league_ptr = kbo_find_allstar_league_ptr(league_id);
        if (league_ptr != 0) {
            kbo_log_runtimef("KBO all-star force retry found league attempt=%d league_id=%u league=%p", attempt, league_id, (void*)league_ptr);
            enable_kbo_allstar_flags(league_ptr, "startup_retry");
            kbo_allstar_prepare_for_native_hooks(league_ptr, league_id, "startup_retry");
            kbo_log_runtime_line("KBO all-star force retry prepared hook-driven league");
            return 0;
        }

        league_ptr = find_kbo_allstar_core_fallback_league(league_id);
        if (league_ptr != 0
                && enable_kbo_allstar_flags_for_core_league(league_ptr, league_id, "startup_retry_core_fallback")) {
            kbo_log_runtimef(
                "KBO all-star force retry found league by core fallback attempt=%d league_id=%u league=%p",
                attempt,
                league_id,
                (void*)league_ptr);
            kbo_allstar_prepare_for_native_hooks(league_ptr, league_id, "startup_retry_core_fallback");
            kbo_log_runtime_line("KBO all-star force retry prepared hook-driven core fallback");
            return 0;
        }

        if (!kbo_runtime_sleep_should_continue((uint32_t)tuning->allstar_force_retry_sleep_ms)) {
            break;
        }
    }

    kbo_log_runtimef(
        "KBO all-star force retry gave up: league not found after %d attempts",
        tuning->allstar_force_retry_attempts);
    return 0;
}

void start_kbo_allstar_force_retry_thread(void)
{
    static LONG started = 0;
    if (InterlockedCompareExchange(&started, 1, 0) != 0) {
        return;
    }

    if (kbo_start_runtime_thread(kbo_allstar_force_retry_thread, NULL, "all-star force retry")) {
        kbo_log_runtime_line("KBO all-star force retry thread started");
    } else {
        InterlockedExchange(&started, 0);
    }
}
