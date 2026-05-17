#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../../bootstrap/profiling/profiler.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../core/dates/tick/current_date_tick_capture.h"
#include "../../../core/files/save_paths/core_save_paths.h"
#include "../../../core/logging/core_log.h"
#include "../../../core/runtime_tuning/runtime_tuning_policy.h"
#include "../../common/dates/foreign_waiver_date.h"
#include "../../common/policy/foreign_waiver_policy.h"
#include "../../roster_audit/api/foreign_roster_audit.h"
#include "../api/foreign_waiver_core.h"
#include "../internal/foreign_waiver_core_ai_internal.h"
#include "../internal/foreign_waiver_core_io_internal.h"

LONG g_kbo_foreign_waiver_scanner_started = 0;

static DWORD WINAPI kbo_foreign_waiver_scanner_thread(LPVOID parameter)
{
    (void)parameter;
    uint32_t tick = 0;

    KboCurrentDateTickConsumer consumer = {0};
    kbo_current_date_tick_consumer_init(
        &consumer,
        "foreign_waiver_scanner",
        KBO_CURRENT_DATE_TICK_CONSUMER_EMIT_CURRENT_ON_SAVE_ENTER
            | KBO_CURRENT_DATE_TICK_CONSUMER_GAP_CATCHUP);

    while (kbo_runtime_threads_should_continue()) {
        if (!kbo_runtime_sleep_should_continue((uint32_t)kbo_runtime_tuning_policy()->foreign_waiver_scanner_sleep_ms)) {
            break;
        }
        if (!kbo_runtime_pause_for_save_if_needed("foreign_waiver_scanner")) {
            break;
        }
        KBO_PROFILE_BEGIN(profile_foreign_waiver_scanner_tick);
        tick++;
        char save_path[MAX_PATH] = {0};
        char readiness_path[MAX_PATH] = {0};
        if (!kbo_get_current_save_path(save_path, sizeof(save_path))
                || !kbo_get_save_scoped_data_file("foreign_waiver_commands.txt", readiness_path, sizeof(readiness_path))) {
            static LONG waiting_logged = 0;
            if (InterlockedCompareExchange(&waiting_logged, 1, 0) == 0) {
                kbo_log_runtime_line("foreign waiver worker waiting: save path not ready");
            }
            KBO_PROFILE_END(profile_foreign_waiver_scanner_tick, "foreign_waiver.scanner.not_ready");
            continue;
        }

        process_foreign_waiver_commands();
        if (!kbo_is_foreign_waiver_negotiation_window_open()) {
            kbo_current_date_tick_consumer_skip_to_latest(&consumer);
            KBO_PROFILE_END(profile_foreign_waiver_scanner_tick, "foreign_waiver.scanner.window_closed");
            continue;
        }

        int background_scanner_enabled = read_kbo_localappdata_flag_file("enable_foreign_waiver_background_scanner.txt");
        if (background_scanner_enabled) {
            audit_foreign_roster_state("foreign_roster_pre_tick", 0);
        }

        KboCurrentDateTickWork work = {0};
        while (kbo_current_date_tick_consumer_next(&consumer, &work)) {
            run_foreign_waiver_ai_core_once();
            if (kbo_runtime_save_in_progress()) {
                break;
            }
            kbo_current_date_tick_consumer_mark_processed(&consumer);
        }

        if (background_scanner_enabled && (tick % 6u) == 0u) {
            audit_foreign_roster_state("foreign_roster_post_tick", 1);
            write_foreign_waiver_candidates("foreign_waiver_scanner");
        }
        KBO_PROFILE_END(profile_foreign_waiver_scanner_tick, "foreign_waiver.scanner.tick");
    }
    InterlockedExchange(&g_kbo_foreign_waiver_scanner_started, 0);
    kbo_log_runtime_line("foreign waiver scanner thread stopped");
    return 0;
}

void start_kbo_foreign_waiver_scanner_thread(void)
{
    if (!kbo_foreign_waiver_ai_enabled()) {
        return;
    }
    int background_scanner_enabled = read_kbo_localappdata_flag_file("enable_foreign_waiver_background_scanner.txt");
    if (InterlockedCompareExchange(&g_kbo_foreign_waiver_scanner_started, 1, 0) != 0) {
        return;
    }

    if (kbo_start_runtime_thread(kbo_foreign_waiver_scanner_thread, NULL, "foreign waiver scanner")) {
        if (background_scanner_enabled) {
            kbo_log_runtime_line("foreign waiver scanner thread started");
        } else {
            kbo_log_runtime_line("foreign waiver lightweight retain worker started; candidate scanner disabled");
        }
    } else {
        InterlockedExchange(&g_kbo_foreign_waiver_scanner_started, 0);
    }
}
