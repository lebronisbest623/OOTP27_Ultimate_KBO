#include "salary_snapshot_thread.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

#include "../../bootstrap/profiling/profiler.h"
#include "../../competitive_balance_tax/api/competitive_balance_tax.h"
#include "../../competitive_balance_tax/exceptions/cbt_exceptions.h"
#include "../../competitive_balance_tax/rules/cbt_rules.h"
#include "../../core/dates/core_current_date.h"
#include "../../core/dates/core_text_date.h"
#include "../../core/dates/tick/current_date_tick_capture.h"
#include "../../core/core_flags/api/flags_api.h"
#include "../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../core/files/save_paths/core_save_paths.h"
#include "../../core/logging/core_log.h"
#include "../../core/runtime_tuning/runtime_tuning_policy.h"
#include "../../core/season/season_calendar.h"
#include "../../foreign/common/dates/foreign_waiver_date.h"
#include "../../runtime_memory/runtime_memory.h"
#include "../paths/salary_snapshot_paths_dates.h"
#include "../state/salary_snapshot_state.h"
#include "../capture/salary_snapshot_write_capture.h"

static int kbo_fa_salary_snapshot_process_date_sync(uint32_t date, const char* source)
{
    if (!kbo_fix_enabled()) {
        return 1;
    }
    if (get_ootp_cached_global_database() == 0u) {
        return 0;
    }

    char save_path[MAX_PATH] = {0};
    if (!kbo_get_current_save_path(save_path, sizeof(save_path))) {
        return 0;
    }

    uint32_t year = date / 10000u;
    uint32_t month = (date / 100u) % 100u;
    uint32_t league_id = kbo_resolve_kbo_league_id();
    uint32_t opening_day = 0u;
    if (!kbo_season_calendar_resolve_opening_day(
            league_id,
            year,
            date,
            &opening_day)
            || opening_day / 10000u != year) {
        if (!kbo_fa_salary_snapshot_today_has_opening_day_message(date)) {
            if (month < 5u) {
                kbo_log_runtimef(
                    "KBO FA salary snapshot sync waiting date=%u league=%u reason=opening_day_unavailable",
                    date,
                    league_id);
            }
            return 1;
        }
        opening_day = date;
        if (league_id != 0u) {
            (void)kbo_season_calendar_store_opening_day(
                league_id,
                year,
                opening_day,
                date,
                "opening_day_message");
        }
    }

    int in_opening_window = kbo_fa_salary_snapshot_current_date_in_opening_window(date, opening_day);
    int snapshot_exists = kbo_fa_salary_snapshot_file_exists(year);
    KboCbtRules rules;
    kbo_cbt_rules_load(&rules);
    uint32_t cbt_announcement_day = opening_day / 10000u == year
        ? kbo_add_days_yyyymmdd(opening_day, rules.announcement_days_after_opening)
        : 0u;

    if (snapshot_exists && opening_day / 10000u == year && date >= opening_day) {
        if (cbt_announcement_day == 0u || date >= cbt_announcement_day) {
            kbo_cbt_exception_auto_designate_missing(year, "snapshot_sync_default_exception");
        }
        return 1;
    }

    int after_opening_day = opening_day / 10000u == year && date > opening_day;
    int late_missing_snapshot_backfill =
        !in_opening_window
        && after_opening_day
        && !snapshot_exists;
    if (!in_opening_window) {
        if (!late_missing_snapshot_backfill) {
            return 1;
        }
        kbo_log_runtimef(
            "KBO FA salary snapshot sync late backfill date=%u opening_day=%u league=%u source=%s reason=missing_opening_day_snapshot",
            date,
            opening_day,
            league_id,
            source != NULL ? source : "");
    }

    int captured = kbo_capture_fa_salary_opening_day_snapshot(
        late_missing_snapshot_backfill ? "opening_day_sync_late_backfill" : source,
        date,
        year,
        opening_day,
        league_id);
    return captured || snapshot_exists || kbo_fa_salary_snapshot_file_exists(year);
}

static int kbo_fa_salary_snapshot_sync_consumer(
    uint32_t date,
    uint32_t site_rva,
    void* context)
{
    (void)context;
    const char* source = site_rva == KBO_CURRENT_DATE_TICK_SAVE_ENTER_SITE_RVA
        ? "opening_day_sync_save_enter"
        : "opening_day_sync_post_advance";
    return kbo_fa_salary_snapshot_process_date_sync(date, source);
}

static DWORD WINAPI kbo_fa_salary_snapshot_thread(LPVOID parameter)
{
    (void)parameter;
    kbo_log_runtime_line("KBO FA salary opening-day snapshot thread started");

    KboCurrentDateTickConsumer date_consumer = {0};
    kbo_current_date_tick_consumer_init(
        &date_consumer,
        "fa_salary_snapshot_thread",
        KBO_CURRENT_DATE_TICK_CONSUMER_EMIT_CURRENT_ON_SAVE_ENTER);

    while (kbo_runtime_threads_should_continue()) {
        if (!kbo_runtime_sleep_should_continue((uint32_t)kbo_runtime_tuning_policy()->fa_salary_snapshot_thread_sleep_ms)) {
            break;
        }
        LARGE_INTEGER profile_snapshot_thread_tick = {0};
        int profile_snapshot_thread_tick_active = kbo_profiler_begin(&profile_snapshot_thread_tick);
        if (!kbo_fix_enabled()) {
            if (profile_snapshot_thread_tick_active) {
                kbo_profiler_end("fa_salary_snapshot.thread.disabled_tick", &profile_snapshot_thread_tick);
            }
            continue;
        }

        if (get_ootp_cached_global_database() == 0u) {
            if (profile_snapshot_thread_tick_active) {
                kbo_profiler_end("fa_salary_snapshot.thread.no_global_cached", &profile_snapshot_thread_tick);
            }
            continue;
        }

        KboCurrentDateTickWork date_work = {0};
        int drained = 0;
        while (kbo_current_date_tick_consumer_next(&date_consumer, &date_work)) {
            (void)date_work;
            kbo_current_date_tick_consumer_mark_processed(&date_consumer);
            drained = 1;
        }
        if (profile_snapshot_thread_tick_active) {
            kbo_profiler_end(
                drained
                    ? "fa_salary_snapshot.thread.date_delegated_to_sync"
                    : "fa_salary_snapshot.thread.no_date",
                &profile_snapshot_thread_tick);
        }
    }
    InterlockedExchange(&g_kbo_fa_salary_snapshot_thread_started, 0);
    kbo_log_runtime_line("KBO FA salary opening-day snapshot thread stopped");

    return 0;
}

void start_kbo_fa_salary_snapshot_thread(void)
{
    if (!kbo_fix_enabled()) {
        return;
    }
    if (InterlockedCompareExchange(&g_kbo_fa_salary_snapshot_thread_started, 1, 0) != 0) {
        return;
    }
    kbo_current_date_tick_register_sync_consumer(
        "fa_salary_snapshot_thread",
        kbo_fa_salary_snapshot_sync_consumer,
        NULL);

    if (!kbo_start_runtime_thread(kbo_fa_salary_snapshot_thread, NULL, "FA salary snapshot")) {
        InterlockedExchange(&g_kbo_fa_salary_snapshot_thread_started, 0);
    }
}
