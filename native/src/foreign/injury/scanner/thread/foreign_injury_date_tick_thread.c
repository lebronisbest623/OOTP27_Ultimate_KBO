#include "../foreign_injury_scanner_internal.h"

#include "../../../../core/dates/tick/current_date_tick_capture.h"

#define KBO_FOREIGN_INJURY_DATE_TICK_PULSE_MS 50u
#define KBO_FOREIGN_INJURY_DATE_TICK_CONSUMER_FLAGS \
    (KBO_CURRENT_DATE_TICK_CONSUMER_EMIT_CURRENT_ON_SAVE_ENTER \
        | KBO_CURRENT_DATE_TICK_CONSUMER_GAP_CATCHUP \
        | KBO_CURRENT_DATE_TICK_CONSUMER_OBSERVE_CURRENT_WHEN_IDLE)

static int kbo_foreign_injury_date_tick_process_work(
    KboCurrentDateTickConsumer* consumer,
    const KboCurrentDateTickWork* work)
{
    if (consumer == NULL || work == NULL) {
        return 1;
    }

    if (kbo_runtime_save_in_progress()) {
        kbo_log_runtimef(
            "foreign injury date tick deferred reason=save_in_progress date=%u gap=%d",
            work->date,
            work->gap);
        return 0;
    }

    if (work->gap) {
        kbo_foreign_injury_replacement_scan_discovery_for_date(
            "foreign_injury_current_date_tick_gap",
            work->date);
    } else {
        kbo_foreign_injury_replacement_scan_captured_date(
            "foreign_injury_current_date_tick",
            work->date);
    }

    kbo_current_date_tick_consumer_mark_processed(consumer);
    return 1;
}

DWORD WINAPI kbo_foreign_injury_date_tick_thread(LPVOID parameter)
{
    (void)parameter;
    kbo_log_runtime_line("foreign injury date tick thread started");

    KboCurrentDateTickConsumer consumer = {0};
    kbo_current_date_tick_consumer_init(
        &consumer,
        "foreign_injury_current_date_tick",
        KBO_FOREIGN_INJURY_DATE_TICK_CONSUMER_FLAGS);
    static volatile LONG accepted_log_count = 0;

    while (kbo_runtime_threads_should_continue()) {
        if (!kbo_runtime_sleep_should_continue(KBO_FOREIGN_INJURY_DATE_TICK_PULSE_MS)) {
            break;
        }
        if (!kbo_runtime_pause_for_save_if_needed("foreign_injury_date_tick")) {
            break;
        }

        KboCurrentDateTickWork work = {0};
        while (kbo_current_date_tick_consumer_next(&consumer, &work)) {
            LONG log_index = InterlockedIncrement(&accepted_log_count);
            if (log_index <= 80) {
                kbo_log_runtimef(
                    "foreign injury date tick event date=%u gap=%d",
                    work.date,
                    work.gap);
            }
            if (!kbo_foreign_injury_date_tick_process_work(&consumer, &work)) {
                break;
            }
        }
    }

    InterlockedExchange(&g_kbo_foreign_injury_date_tick_thread_started, 0);
    kbo_log_runtime_line("foreign injury date tick thread stopped");
    return 0;
}

void start_kbo_foreign_injury_date_tick_thread(void)
{
    if (!kbo_foreign_injury_replacement_enabled()) {
        kbo_log_runtime_line("foreign injury date tick: disabled");
        return;
    }
    if (InterlockedCompareExchange(&g_kbo_foreign_injury_date_tick_thread_started, 1, 0) != 0) {
        return;
    }

    if (kbo_start_runtime_thread(
            kbo_foreign_injury_date_tick_thread,
            NULL,
            "foreign injury date tick")) {
        kbo_log_runtime_line("foreign injury date tick thread requested");
    } else {
        InterlockedExchange(&g_kbo_foreign_injury_date_tick_thread_started, 0);
    }
}
