#include "../common/custom_events_common.h"
#include "custom_event_monitor.h"
#include <stdio.h>
#include <string.h>
#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/logging/core_log.h"
#include "../../../core/files/save_paths/core_save_paths.h"
#include "../../../core/dates/core_text_date.h"
#include "../../../core/dates/tick/current_date_tick_capture.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../core/runtime_tuning/runtime_tuning_policy.h"
#include "../../../bootstrap/profiling/profiler.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../foreign/common/dates/foreign_waiver_date.h"
#include "../../../core/season/phase/capture/season_phase_capture.h"
#include "../../schedules/independent/independent_team_acquisition_schedule.h"
#include "../calendar/custom_event_calendar_due.h"

static volatile LONG64 g_kbo_custom_event_schedule_deferred_log_ms = 0;
static volatile LONG64 g_kbo_custom_event_scan_deferred_log_ms = 0;
static volatile LONG g_kbo_custom_event_global_scheduled_yyyymmdd = 0;
static volatile LONG g_kbo_custom_event_global_scheduled_processing_yyyymmdd = 0;
static volatile LONG g_kbo_custom_event_global_scanned_yyyymmdd = 0;
static volatile LONG g_kbo_custom_event_global_fa_comp_yyyymmdd = 0;
static volatile LONG g_kbo_custom_event_global_fa_comp_processing_yyyymmdd = 0;
static volatile LONG g_kbo_custom_event_monitor_live_delegated_log_count = 0;
static uint32_t g_kbo_custom_event_sync_last_scheduled_yyyymmdd = 0u;
static uint32_t g_kbo_custom_event_sync_last_scanned_yyyymmdd = 0u;
static uint32_t g_kbo_custom_event_sync_last_fa_comp_yyyymmdd = 0u;

#define KBO_CUSTOM_EVENT_MONITOR_PULSE_MS 100u

static int kbo_custom_event_monitor_should_log_throttled(volatile LONG64* last_log_ms)
{
    ULONGLONG now = GetTickCount64();
    LONG64 last = InterlockedCompareExchange64(last_log_ms, 0, 0);
    if (last > 0 && now >= (ULONGLONG)last && now - (ULONGLONG)last < 30000ull) {
        return 0;
    }
    return InterlockedCompareExchange64(last_log_ms, (LONG64)now, last) == last;
}

int kbo_custom_event_monitor_tick_for_date(
    uint32_t today_yyyymmdd,
    uint32_t* last_scheduled_yyyymmdd,
    uint32_t* last_scanned_yyyymmdd,
    uint32_t* last_fa_comp_yyyymmdd,
    const char* source)
{
    KBO_PROFILE_BEGIN(profile_custom_event_monitor_tick);
    if (!kbo_runtime_pause_for_save_if_needed(source != NULL ? source : "custom_event_monitor")) {
        KBO_PROFILE_END(profile_custom_event_monitor_tick, "custom_event.monitor.save_pause_abort");
        return 0;
    }

    if (!kbo_yyyymmdd_valid(today_yyyymmdd)) {
        KBO_PROFILE_END(profile_custom_event_monitor_tick, "custom_event.monitor.no_date");
        return 0;
    }
    int deferred = 0;

    KBO_PROFILE_BEGIN(profile_custom_event_monitor_transition);
    int offseason_transition_ready = kbo_custom_event_monitor_check_offseason_transition(
        today_yyyymmdd,
        source);
    KBO_PROFILE_END(profile_custom_event_monitor_transition, "custom_event.monitor.offseason_transition");
    if (offseason_transition_ready) {
        if (last_scanned_yyyymmdd != NULL) {
            *last_scanned_yyyymmdd = 0u;
        }
        InterlockedExchange(&g_kbo_custom_event_global_scanned_yyyymmdd, 0);
    }

    if (last_scheduled_yyyymmdd != NULL && today_yyyymmdd != *last_scheduled_yyyymmdd) {
        LONG global_scheduled = InterlockedCompareExchange(
            &g_kbo_custom_event_global_scheduled_yyyymmdd,
            0,
            0);
        LONG processing_date = 0;
        if ((uint32_t)global_scheduled != today_yyyymmdd) {
            processing_date = InterlockedCompareExchange(
                &g_kbo_custom_event_global_scheduled_processing_yyyymmdd,
                (LONG)today_yyyymmdd,
                0);
        }
        if ((uint32_t)global_scheduled == today_yyyymmdd) {
            *last_scheduled_yyyymmdd = today_yyyymmdd;
            KBO_PROFILE_BEGIN(profile_custom_event_monitor_due_skip);
            KBO_PROFILE_END(profile_custom_event_monitor_due_skip, "custom_event.monitor.process_due_cached");
        } else if (processing_date == 0) {
            KBO_PROFILE_BEGIN(profile_custom_event_monitor_due);
            int due_result = kbo_process_custom_events_due_through(today_yyyymmdd, source);
            KBO_PROFILE_END(profile_custom_event_monitor_due, "custom_event.monitor.process_due");
            InterlockedExchange(&g_kbo_custom_event_global_scheduled_processing_yyyymmdd, 0);
            if (due_result >= 0) {
                *last_scheduled_yyyymmdd = today_yyyymmdd;
                InterlockedExchange(&g_kbo_custom_event_global_scheduled_yyyymmdd, (LONG)today_yyyymmdd);
                if (due_result == KBO_CUSTOM_EVENT_DUE_RESULT_CHANGED
                        || due_result == KBO_CUSTOM_EVENT_DUE_RESULT_SCANNED_IDLE) {
                    if (last_scanned_yyyymmdd != NULL) {
                        *last_scanned_yyyymmdd = today_yyyymmdd;
                    }
                    InterlockedExchange(&g_kbo_custom_event_global_scanned_yyyymmdd, (LONG)today_yyyymmdd);
                }
            } else {
                deferred = 1;
                if (kbo_custom_event_monitor_should_log_throttled(&g_kbo_custom_event_schedule_deferred_log_ms)) {
                    kbo_log_runtimef(
                        "KBO custom event schedule deferred reason=state_not_ready today=%u due_result=%d",
                        today_yyyymmdd,
                        due_result);
                }
            }
        } else {
            KBO_PROFILE_BEGIN(profile_custom_event_monitor_due_busy);
            KBO_PROFILE_END(profile_custom_event_monitor_due_busy, "custom_event.monitor.process_due_busy");
            deferred = 1;
        }
    }
    LONG global_scanned = InterlockedCompareExchange(&g_kbo_custom_event_global_scanned_yyyymmdd, 0, 0);
    int should_scan = last_scanned_yyyymmdd == NULL || today_yyyymmdd != *last_scanned_yyyymmdd;
    if ((uint32_t)global_scanned == today_yyyymmdd) {
        should_scan = 0;
        if (last_scanned_yyyymmdd != NULL) {
            *last_scanned_yyyymmdd = today_yyyymmdd;
        }
    }
    if (should_scan) {
        KBO_PROFILE_BEGIN(profile_custom_event_monitor_scan);
        int triggered = scan_kbo_custom_events_once_for_date(today_yyyymmdd, source);
        KBO_PROFILE_END(profile_custom_event_monitor_scan, "custom_event.monitor.scan_once");
        if (triggered >= 0) {
            if (last_scanned_yyyymmdd != NULL) {
                *last_scanned_yyyymmdd = today_yyyymmdd;
            }
            InterlockedExchange(&g_kbo_custom_event_global_scanned_yyyymmdd, (LONG)today_yyyymmdd);
        } else {
            deferred = 1;
            if (kbo_custom_event_monitor_should_log_throttled(&g_kbo_custom_event_scan_deferred_log_ms)) {
                kbo_log_runtimef(
                    "KBO custom event monitor scan deferred reason=state_not_ready today=%u",
                    today_yyyymmdd);
            }
        }
    } else {
        KBO_PROFILE_BEGIN(profile_custom_event_monitor_scan_skip);
        KBO_PROFILE_END(profile_custom_event_monitor_scan_skip, "custom_event.monitor.scan_once_cached");
    }

    if (last_fa_comp_yyyymmdd != NULL && today_yyyymmdd != *last_fa_comp_yyyymmdd) {
        LONG global_fa_comp = InterlockedCompareExchange(&g_kbo_custom_event_global_fa_comp_yyyymmdd, 0, 0);
        LONG processing_date = 0;
        if ((uint32_t)global_fa_comp != today_yyyymmdd) {
            processing_date = InterlockedCompareExchange(
                &g_kbo_custom_event_global_fa_comp_processing_yyyymmdd,
                (LONG)today_yyyymmdd,
                0);
        }
        int fa_comp_complete = 0;
        if ((uint32_t)global_fa_comp != today_yyyymmdd && processing_date == 0) {
            KBO_PROFILE_BEGIN(profile_custom_event_monitor_fa_comp);
            kbo_process_due_fa_compensation_protected_lists_for_date(today_yyyymmdd, source);
            KBO_PROFILE_END(profile_custom_event_monitor_fa_comp, "custom_event.monitor.fa_comp_protected_lists");
            InterlockedExchange(&g_kbo_custom_event_global_fa_comp_yyyymmdd, (LONG)today_yyyymmdd);
            InterlockedExchange(&g_kbo_custom_event_global_fa_comp_processing_yyyymmdd, 0);
            fa_comp_complete = 1;
        } else {
            KBO_PROFILE_BEGIN(profile_custom_event_monitor_fa_comp_skip);
            KBO_PROFILE_END(profile_custom_event_monitor_fa_comp_skip, "custom_event.monitor.fa_comp_protected_lists_cached");
            fa_comp_complete = (uint32_t)global_fa_comp == today_yyyymmdd;
            if (!fa_comp_complete) {
                deferred = 1;
            }
        }
        if (fa_comp_complete) {
            *last_fa_comp_yyyymmdd = today_yyyymmdd;
        }
    }
    KBO_PROFILE_END(profile_custom_event_monitor_tick, "custom_event.monitor.tick");
    return deferred ? 0 : 1;
}

static int kbo_custom_event_monitor_sync_date_consumer(
    uint32_t today_yyyymmdd,
    uint32_t site_rva,
    void* context)
{
    (void)context;
    const char* source = site_rva == KBO_CURRENT_DATE_TICK_SAVE_ENTER_SITE_RVA
        ? "custom_event_monitor_sync_save_enter"
        : "custom_event_monitor_sync_post_advance";
    return kbo_custom_event_monitor_tick_for_date(
        today_yyyymmdd,
        &g_kbo_custom_event_sync_last_scheduled_yyyymmdd,
        &g_kbo_custom_event_sync_last_scanned_yyyymmdd,
        &g_kbo_custom_event_sync_last_fa_comp_yyyymmdd,
        source);
}

DWORD WINAPI kbo_custom_event_monitor_thread(LPVOID parameter)
{
    (void)parameter;
    kbo_log_runtime_line("KBO custom event monitor started");

    uint32_t last_scheduled_yyyymmdd = 0u;
    uint32_t last_scanned_yyyymmdd = 0u;
    uint32_t last_fa_comp_yyyymmdd = 0u;
    LONG last_phase_capture_sequence = InterlockedCompareExchange(
        &g_kbo_season_phase_capture_event_published_sequence,
        0,
        0);

    KboCurrentDateTickConsumer consumer = {0};
    kbo_current_date_tick_consumer_init(
        &consumer,
        "custom_event_monitor",
        KBO_CURRENT_DATE_TICK_CONSUMER_EMIT_CURRENT_ON_SAVE_ENTER);

    while (kbo_runtime_threads_should_continue()) {
        if (!kbo_runtime_sleep_should_continue(KBO_CUSTOM_EVENT_MONITOR_PULSE_MS)) {
            break;
        }
        if (!kbo_runtime_pause_for_save_if_needed("custom_event_monitor")) {
            break;
        }

        KboCurrentDateTickWork work = {0};
        while (kbo_current_date_tick_consumer_next(&consumer, &work)) {
            LONG log_no = InterlockedIncrement(
                &g_kbo_custom_event_monitor_live_delegated_log_count);
            if (log_no <= 20 || (log_no % 500) == 0) {
                kbo_log_runtimef(
                    "KBO custom event monitor date delegated to sync source=custom_event_monitor date=%u site=0x%x seq=%u",
                    work.date,
                    work.site_rva,
                    work.sequence);
            }
            kbo_current_date_tick_consumer_mark_processed(&consumer);
            continue;
        }

        LONG phase_capture_sequence = InterlockedCompareExchange(
            &g_kbo_season_phase_capture_event_published_sequence,
            0,
            0);
        uint32_t current_date = 0u;
        if (phase_capture_sequence != last_phase_capture_sequence
                && kbo_current_date_tick_latest_published_date(&current_date)) {
            last_phase_capture_sequence = phase_capture_sequence;
            kbo_custom_event_monitor_tick_for_date(
                current_date,
                &last_scheduled_yyyymmdd,
                &last_scanned_yyyymmdd,
                &last_fa_comp_yyyymmdd,
                "custom_event_monitor_phase_pulse");
        }
    }
    InterlockedExchange(&g_kbo_custom_event_monitor_started, 0);
    kbo_log_runtime_line("KBO custom event monitor stopped");

    return 0;
}

int start_kbo_custom_event_monitor(void)
{
    if (!kbo_fix_enabled()) {
        kbo_log_runtime_line("KBO custom event monitor skipped reason=fix_disabled");
        return 0;
    }
    kbo_current_date_tick_register_sync_consumer(
        "custom_event_monitor",
        kbo_custom_event_monitor_sync_date_consumer,
        NULL);
    if (InterlockedCompareExchange(&g_kbo_custom_event_monitor_started, 1, 0) != 0) {
        return 1;
    }

    if (!kbo_start_runtime_thread(kbo_custom_event_monitor_thread, NULL, "custom event monitor")) {
        InterlockedExchange(&g_kbo_custom_event_monitor_started, 0);
        return 0;
    }
    return 1;
}
