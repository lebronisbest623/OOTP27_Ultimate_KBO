#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>

#include "../internal/amateur_assignment_internal.h"
#include "../../core/dates/tick/current_date_tick_capture.h"
#include "../../core/runtime_tuning/runtime_tuning_policy.h"

static volatile LONG g_kbo_amateur_reputation_update_thread_started = 0;

static const char* kbo_amateur_reputation_source_for_work(const KboCurrentDateTickWork* work)
{
    if (work != NULL && work->site_rva == KBO_CURRENT_DATE_TICK_SAVE_ENTER_SITE_RVA) {
        return "amateur_reputation_background_save_enter";
    }
    return "amateur_reputation_background_post_advance";
}

static int kbo_amateur_reputation_process_work(
    KboCurrentDateTickConsumer* consumer,
    const KboCurrentDateTickWork* work)
{
    if (consumer == NULL || work == NULL) {
        return 1;
    }
    if (!kbo_fix_enabled()) {
        kbo_current_date_tick_consumer_mark_processed(consumer);
        return 1;
    }
    if (work->date == 0u || (work->date % 10000u) != 101u) {
        kbo_current_date_tick_consumer_mark_processed(consumer);
        return 1;
    }
    if (get_ootp_cached_global_database() == 0u || kbo_runtime_save_in_progress()) {
        static volatile LONG s_deferred_log_count = 0;
        LONG log_index = InterlockedIncrement(&s_deferred_log_count);
        if (log_index <= 40) {
            kbo_log_runtimef(
                "amateur reputation update background deferred reason=not_ready date=%u site=0x%x",
                work->date,
                work->site_rva);
        }
        return 0;
    }

    kbo_update_amateur_reputation_from_team_records_for_date(
        work->date,
        kbo_amateur_reputation_source_for_work(work));
    if (kbo_runtime_save_in_progress()) {
        return 0;
    }
    kbo_current_date_tick_consumer_mark_processed(consumer);
    return 1;
}

static DWORD WINAPI kbo_amateur_reputation_update_thread(LPVOID parameter)
{
    (void)parameter;
    kbo_log_runtime_line("amateur reputation update thread started");

    KboCurrentDateTickConsumer consumer = {0};
    kbo_current_date_tick_consumer_init(
        &consumer,
        "amateur_reputation_update_thread",
        KBO_CURRENT_DATE_TICK_CONSUMER_EMIT_CURRENT_ON_SAVE_ENTER);

    while (kbo_runtime_threads_should_continue()) {
        if (!kbo_runtime_sleep_should_continue(
                (uint32_t)kbo_runtime_tuning_policy()->amateur_reputation_update_thread_sleep_ms)) {
            break;
        }

        KboCurrentDateTickWork work = {0};
        while (kbo_current_date_tick_consumer_next(&consumer, &work)) {
            if (!kbo_amateur_reputation_process_work(&consumer, &work)) {
                break;
            }
        }
    }
    InterlockedExchange(&g_kbo_amateur_reputation_update_thread_started, 0);
    kbo_log_runtime_line("amateur reputation update thread stopped");
    return 0;
}

void start_kbo_amateur_reputation_update_thread(void)
{
    if (!kbo_fix_enabled()) {
        return;
    }
    if (InterlockedCompareExchange(&g_kbo_amateur_reputation_update_thread_started, 1, 0) != 0) {
        return;
    }
    if (!kbo_start_runtime_thread(
            kbo_amateur_reputation_update_thread,
            NULL,
            "amateur reputation update")) {
        InterlockedExchange(&g_kbo_amateur_reputation_update_thread_started, 0);
    }
}
