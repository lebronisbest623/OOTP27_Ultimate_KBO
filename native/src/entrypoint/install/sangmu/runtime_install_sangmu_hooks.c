#include "../../entrypoint_internal.h"

DWORD WINAPI kbo_delayed_sangmu_fa_hooks_install_thread(LPVOID parameter)
{
    KboSangmuFaHookInstallRequest request = {0};
    if (parameter != NULL) {
        request = *(KboSangmuFaHookInstallRequest*)parameter;
        HeapFree(GetProcessHeap(), 0, parameter);
    }

    kbo_log_runtimef(
        "KBO Sangmu FA hooks delayed install waiting signability=%d offer=%d",
        request.enable_signability,
        request.enable_offer);

    const KboRuntimeTuningPolicy* tuning = kbo_runtime_tuning_policy();
    int stable_ticks = 0;
    KboCurrentDateTickConsumer date_consumer = {0};
    kbo_current_date_tick_consumer_init(
        &date_consumer,
        "sangmu_delayed_install",
        KBO_CURRENT_DATE_TICK_CONSUMER_EMIT_CURRENT_ON_SAVE_ENTER);
    for (int attempt = 1; attempt <= tuning->sangmu_delayed_install_attempts; attempt++) {
        char save_path[MAX_PATH] = {0};
        int has_save = kbo_get_current_save_path(save_path, sizeof(save_path));
        KboCurrentDateTickWork date_work = {0};
        int has_date = has_save && kbo_current_date_tick_consumer_next(&date_consumer, &date_work);
        if (has_save && has_date) {
            stable_ticks++;
            if (stable_ticks >= tuning->sangmu_delayed_install_stable_ticks) {
                kbo_current_date_tick_consumer_mark_processed(&date_consumer);
                break;
            }
        } else {
            stable_ticks = 0;
        }

        if (kbo_runtime_tuning_sangmu_delayed_install_log_attempt(attempt)) {
            kbo_log_runtimef(
                "KBO Sangmu FA hooks delayed install not ready attempt=%d save=%d today=%u stable=%d",
                attempt,
                has_save,
                has_date ? date_work.date : 0u,
                stable_ticks);
        }
        if (!kbo_runtime_sleep_should_continue((uint32_t)tuning->sangmu_delayed_install_sleep_ms)) {
            kbo_log_runtime_line("KBO Sangmu FA hooks delayed install stopped before ready");
            return 0;
        }
    }

    kbo_load_military_service_team_policy_override_once();

    if (request.enable_signability) {
        install_kbo_player_team_signability_patch();
    } else {
        kbo_log_runtime_line("KBO player/team signability patch delayed disabled");
    }
    if (request.enable_offer) {
        install_kbo_player_offer_eligibility_patch();
    } else {
        kbo_log_runtime_line("KBO player offer eligibility patch delayed disabled");
    }

    kbo_log_runtime_line("KBO Sangmu FA hooks delayed install complete");
    return 0;
}

void start_kbo_delayed_sangmu_fa_hooks_install_thread(
    int enable_signability,
    int enable_offer)
{
    if (!enable_signability && !enable_offer) {
        return;
    }
    if (InterlockedCompareExchange(&g_kbo_sangmu_fa_hooks_install_started, 1, 0) != 0) {
        kbo_log_runtime_line("KBO Sangmu FA hooks delayed install already started");
        return;
    }

    KboSangmuFaHookInstallRequest* request =
        (KboSangmuFaHookInstallRequest*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(KboSangmuFaHookInstallRequest));
    if (request == NULL) {
        kbo_log_runtime_line("KBO Sangmu FA hooks delayed install request allocation failed");
        return;
    }
    request->enable_signability = enable_signability;
    request->enable_offer = enable_offer;

    if (kbo_start_runtime_thread(
            kbo_delayed_sangmu_fa_hooks_install_thread,
            request,
            "delayed sangmu FA hook install")) {
        kbo_log_runtime_line("KBO Sangmu FA hooks delayed install thread started");
    } else {
        HeapFree(GetProcessHeap(), 0, request);
    }
}
