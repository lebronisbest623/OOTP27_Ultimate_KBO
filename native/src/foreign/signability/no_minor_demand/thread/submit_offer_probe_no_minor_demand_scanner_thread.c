#include "../submit_offer_probe_no_minor_demand_internal.h"

DWORD WINAPI kbo_no_minor_contract_demand_floor_scanner_thread(LPVOID param)
{
    (void)param;
    const KboForeignPlayerPolicy* policy = kbo_foreign_player_policy();
    if (!kbo_runtime_sleep_should_continue((uint32_t)policy->no_minor_scan_initial_delay_ms)) {
        InterlockedExchange(&g_kbo_no_minor_contract_demand_floor_scanner_started, 0);
        kbo_log_runtime_line("stopped KBO no-minor demand floor scanner thread");
        return 0;
    }

    for (uint32_t attempt = 0; kbo_runtime_threads_should_continue(); attempt++) {
        KBO_PROFILE_BEGIN(profile_no_minor_scanner_tick);
        if (!kbo_runtime_pause_for_save_if_needed("no_minor_demand_floor_scanner")) {
            KBO_PROFILE_END(profile_no_minor_scanner_tick, "no_minor.scanner_tick.save_stop");
            break;
        }
        kbo_no_minor_scan_and_floor_teamless_fa_demands("background_prescan");
        KBO_PROFILE_END(profile_no_minor_scanner_tick, attempt < (uint32_t)policy->no_minor_scan_warmup_attempts
            ? "no_minor.scanner_tick.warmup"
            : "no_minor.scanner_tick.steady");
        if (!kbo_runtime_sleep_should_continue(attempt < (uint32_t)policy->no_minor_scan_warmup_attempts
            ? (uint32_t)policy->no_minor_scan_warmup_interval_ms
            : (uint32_t)policy->no_minor_scan_interval_ms)) {
            break;
        }
    }
    InterlockedExchange(&g_kbo_no_minor_contract_demand_floor_scanner_started, 0);
    kbo_log_runtime_line("stopped KBO no-minor demand floor scanner thread");
    return 0;
}

void start_kbo_no_minor_contract_demand_floor_scanner_thread(void)
{
    if (InterlockedCompareExchange(&g_kbo_no_minor_contract_demand_floor_scanner_started, 1, 0) != 0) {
        return;
    }

    if (!kbo_start_runtime_thread(
            kbo_no_minor_contract_demand_floor_scanner_thread,
            NULL,
            "no-minor demand floor scanner")) {
        InterlockedExchange(&g_kbo_no_minor_contract_demand_floor_scanner_started, 0);
        return;
    }
    kbo_log_runtime_line("started KBO no-minor demand floor scanner thread");
}
