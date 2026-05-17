#include "../entrypoint_internal.h"

static volatile LONG64 g_kbo_runtime_marker_guard_started_filetime = 0;

static LONG64 kbo_filetime_to_i64(FILETIME time)
{
    ULARGE_INTEGER value;
    value.LowPart = time.dwLowDateTime;
    value.HighPart = time.dwHighDateTime;
    return (LONG64)value.QuadPart;
}

DWORD WINAPI kbo_full_runtime_marker_wait_thread(LPVOID parameter)
{
    HINSTANCE instance = (HINSTANCE)parameter;
    FILETIME guard_started_time = {0};
    GetSystemTimeAsFileTime(&guard_started_time);
    InterlockedExchange64(
        &g_kbo_runtime_marker_guard_started_filetime,
        kbo_filetime_to_i64(guard_started_time));
    kbo_log_runtime_line("KBO full runtime marker guard thread started");

    int early_amateur_team_add_guard_installed = 0;
    KboCurrentDateTickConsumer date_consumer = {0};
    kbo_current_date_tick_consumer_init(
        &date_consumer,
        "runtime_marker_wait",
        KBO_CURRENT_DATE_TICK_CONSUMER_EMIT_CURRENT_ON_SAVE_ENTER);
    const KboRuntimeTuningPolicy* tuning = kbo_runtime_tuning_policy();
    for (int attempt = 1; attempt <= tuning->runtime_marker_wait_attempts; attempt++) {
        int log_detail = kbo_runtime_tuning_runtime_marker_log_attempt(attempt);
        if (kbo_current_save_has_required_roster_marker("runtime_marker_wait", log_detail)) {
            if (read_kbo_localappdata_flag_file("enable_kbo_cbt_service_time_probe.txt")) {
                kbo_cbt_service_time_probe_once();
            }

            if (!early_amateur_team_add_guard_installed
                    && !read_kbo_localappdata_flag_file("disable_amateur_assignment_reroute.txt")
                    && !read_kbo_localappdata_flag_file("disable_kbo_military_team_add_guard_patch.txt")) {
                kbo_log_runtime_line("KBO early amateur team-add guard installing after roster marker");
                early_amateur_team_add_guard_installed = install_kbo_military_team_add_guard_patch();
            }

            KboCurrentDateTickWork date_work = {0};
            if (kbo_current_date_tick_consumer_next(&date_consumer, &date_work)) {
                kbo_current_date_tick_consumer_mark_processed(&date_consumer);
                InterlockedExchange(&g_kbo_runtime_date_stable_ready, 1);
                kbo_log_runtimef(
                    "KBO full runtime marker guard ready source=runtime_marker_wait date=%u",
                    date_work.date);
                install_kbo_full_runtime_after_roster_marker(instance);
                return 0;
            }

            if (log_detail) {
                kbo_log_runtimef(
                    "KBO full runtime marker guard waiting source=runtime_marker_wait reason=current_date_hook_unavailable");
            }
        }
        Sleep((DWORD)tuning->runtime_marker_wait_sleep_ms);
    }

    kbo_log_runtime_line("KBO full runtime marker guard gave up: required roster marker was not found");
    return 0;
}
