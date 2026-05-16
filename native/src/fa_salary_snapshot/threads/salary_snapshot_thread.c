#include "salary_snapshot_thread.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

#include "../../bootstrap/profiling/profiler.h"
#include "../../competitive_balance_tax/api/competitive_balance_tax.h"
#include "../../competitive_balance_tax/events/cbt_events.h"
#include "../../competitive_balance_tax/exceptions/cbt_exceptions.h"
#include "../../competitive_balance_tax/rules/cbt_rules.h"
#include "../../core/dates/core_current_date.h"
#include "../../core/core_flags/api/flags_api.h"
#include "../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../core/files/save_paths/core_save_paths.h"
#include "../../core/logging/core_log.h"
#include "../../core/runtime_tuning/runtime_tuning_policy.h"
#include "../../foreign/common/dates/foreign_waiver_date.h"
#include "../../runtime_memory/runtime_memory.h"
#include "../paths/salary_snapshot_paths_dates.h"
#include "../state/salary_snapshot_state.h"
#include "../capture/salary_snapshot_write_capture.h"

static DWORD WINAPI kbo_fa_salary_snapshot_thread(LPVOID parameter)
{
    (void)parameter;
    kbo_log_runtime_line("KBO FA salary opening-day snapshot thread started");

    uint32_t last_log_date = 0u;
    uint32_t last_log_opening_day = 0u;
    uint32_t captured_season = 0u;
    uint32_t cbt_event_schedule_done_season = 0u;
    uint32_t cbt_auto_exception_done_season = 0u;
    char captured_save_path[MAX_PATH] = {0};
    uint32_t cached_message_date = 0u;
    int cached_message_found = 0;
    DWORD cached_message_checked_ms = 0u;
    uint32_t quiet_opening_unavailable_year = 0u;
    /* Per-tick caches to avoid repeated file I/O and memory scans. */
    uintptr_t cached_league_ptr = 0u;
    uint32_t cached_league_ptr_year = 0u;
    uint32_t cached_schedule_year = 0u;
    uint32_t cached_schedule_opening_day = 0u;
    KboCbtRules cached_cbt_rules = {0};
    uint32_t cached_cbt_rules_date = 0u;
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

        uint32_t year = 0;
        uint32_t month = 0;
        uint32_t day = 0;
        if (!kbo_current_date_is_valid(&year, &month, &day)) {
            if (profile_snapshot_thread_tick_active) {
                kbo_profiler_end("fa_salary_snapshot.thread.no_date", &profile_snapshot_thread_tick);
            }
            continue;
        }
        uint32_t date = year * 10000u + month * 100u + day;

        char save_path[MAX_PATH] = {0};
        if (!kbo_get_current_save_path(save_path, sizeof(save_path))) {
            if (profile_snapshot_thread_tick_active) {
                kbo_profiler_end("fa_salary_snapshot.thread.no_save", &profile_snapshot_thread_tick);
            }
            continue;
        }
        if (captured_save_path[0] == '\0' || strcmp(captured_save_path, save_path) != 0) {
            snprintf(captured_save_path, sizeof(captured_save_path), "%s", save_path);
            last_log_date = 0u;
            last_log_opening_day = 0u;
            captured_season = 0u;
            cbt_event_schedule_done_season = 0u;
            cbt_auto_exception_done_season = 0u;
            cached_message_date = 0u;
            cached_message_found = 0;
            cached_message_checked_ms = 0u;
            quiet_opening_unavailable_year = 0u;
            cached_league_ptr = 0u;
            cached_league_ptr_year = 0u;
            cached_schedule_year = 0u;
            cached_schedule_opening_day = 0u;
            memset(&cached_cbt_rules, 0, sizeof(cached_cbt_rules));
            cached_cbt_rules_date = 0u;
        }

        uint32_t league_id = kbo_resolve_kbo_league_id();

        /* Cache league_ptr per season: kbo_find_league_ptr_from_global_vectors
         * scans 18 memory regions per call which adds up when called every second. */
        uintptr_t league_ptr = 0u;
        if (cached_league_ptr_year == year && cached_league_ptr != 0u) {
            league_ptr = cached_league_ptr;
        } else {
            league_ptr = kbo_find_league_ptr_from_id(league_id);
            if (league_ptr != 0u) {
                cached_league_ptr = league_ptr;
                cached_league_ptr_year = year;
            }
        }

        uint32_t opening_day = 0u;
        if (league_ptr == 0 || !kbo_fa_salary_snapshot_read_opening_day(league_ptr, &opening_day)) {
            /* Cache schedule file result: reading the LSDL file every second is
             * wasteful; the schedule doesn't change during a session. */
            if (cached_schedule_year == year && cached_schedule_opening_day != 0u) {
                opening_day = cached_schedule_opening_day;
            } else if (kbo_fa_salary_snapshot_load_schedule_opening_day(year, &opening_day)) {
                cached_schedule_year = year;
                cached_schedule_opening_day = opening_day;
                if (date != last_log_date || opening_day != last_log_opening_day) {
                    kbo_log_runtimef(
                        "KBO FA salary snapshot opening day fallback date=%u opening_day=%u league=%u source=schedule",
                        date,
                        opening_day,
                        league_id);
                    last_log_date = date;
                    last_log_opening_day = opening_day;
                }
            }
        }
        if (opening_day == 0u) {
            DWORD now_ms = GetTickCount();
            if (cached_message_date != date
                    && (cached_message_checked_ms == 0u || now_ms - cached_message_checked_ms >= 30000u)) {
                cached_message_date = date;
                cached_message_checked_ms = now_ms;
                cached_message_found = kbo_fa_salary_snapshot_today_has_opening_day_message(date);
            }

            if (!cached_message_found) {
                if (quiet_opening_unavailable_year != year && date != last_log_date) {
                    kbo_log_runtimef("KBO FA salary snapshot waiting date=%u league=%u reason=opening_day_memory_unavailable", date, league_id);
                    last_log_date = date;
                    if (month >= 5u) {
                        quiet_opening_unavailable_year = year;
                    }
                }
                if (profile_snapshot_thread_tick_active) {
                    kbo_profiler_end("fa_salary_snapshot.thread.opening_day_unavailable", &profile_snapshot_thread_tick);
                }
                continue;
            }
            opening_day = date;
        }

        int in_opening_window = kbo_fa_salary_snapshot_current_date_in_opening_window(date, opening_day);
        int after_opening_day = opening_day / 10000u == year && date > opening_day;
        if (!in_opening_window && !after_opening_day) {
            if (date != last_log_date || opening_day != last_log_opening_day) {
                kbo_log_runtimef(
                    "KBO FA salary snapshot waiting date=%u opening_day=%u league=%u",
                    date,
                    opening_day,
                    league_id);
                last_log_date = date;
                last_log_opening_day = opening_day;
            }
            if (profile_snapshot_thread_tick_active) {
                kbo_profiler_end("fa_salary_snapshot.thread.outside_opening_window", &profile_snapshot_thread_tick);
            }
            continue;
        }

        if (cached_cbt_rules_date != date) {
            kbo_cbt_rules_load(&cached_cbt_rules);
            cached_cbt_rules_date = date;
        }

        int snapshot_exists = captured_season == year || kbo_fa_salary_snapshot_file_exists(year);
        uint32_t cbt_announcement_day = opening_day / 10000u == year
            ? kbo_add_days_yyyymmdd(opening_day, cached_cbt_rules.announcement_days_after_opening)
            : 0u;

        if (snapshot_exists && opening_day / 10000u == year && date >= opening_day) {
            captured_season = year;
            if (cbt_auto_exception_done_season != year) {
                cbt_auto_exception_done_season = year;
                kbo_cbt_exception_auto_designate_missing(year, "snapshot_thread_default_exception");
            }
            if (cbt_announcement_day != 0u && date < cbt_announcement_day) {
                if (date != last_log_date || opening_day != last_log_opening_day) {
                    kbo_log_runtimef(
                        "KBO CBT waiting for exception designation window date=%u season=%u opening_day=%u announce=%u",
                        date,
                        year,
                        opening_day,
                        cbt_announcement_day);
                    last_log_date = date;
                    last_log_opening_day = opening_day;
                }
                if (profile_snapshot_thread_tick_active) {
                    kbo_profiler_end("fa_salary_snapshot.thread.cbt_wait_exception_window", &profile_snapshot_thread_tick);
                }
                continue;
            }
            if (cbt_event_schedule_done_season != year) {
                cbt_event_schedule_done_season = year;
                kbo_log_runtimef(
                    "KBO FA salary snapshot CBT event schedule date=%u season=%u opening_day=%u announce=%u reason=snapshot_exists",
                    date,
                    year,
                    opening_day,
                    cbt_announcement_day);
                kbo_schedule_cbt_custom_events("snapshot_thread_cbt_schedule");
            }
            if (profile_snapshot_thread_tick_active) {
                kbo_profiler_end("fa_salary_snapshot.thread.already_captured", &profile_snapshot_thread_tick);
            }
            continue;
        }

        int late_missing_snapshot_backfill =
            !in_opening_window
            && after_opening_day
            && !snapshot_exists;
        if (!in_opening_window && !late_missing_snapshot_backfill) {
            if (date != last_log_date || opening_day != last_log_opening_day) {
                kbo_log_runtimef(
                    "KBO FA salary snapshot waiting date=%u opening_day=%u league=%u",
                    date,
                    opening_day,
                    league_id);
                last_log_date = date;
                last_log_opening_day = opening_day;
            }
            if (profile_snapshot_thread_tick_active) {
                kbo_profiler_end("fa_salary_snapshot.thread.outside_opening_window", &profile_snapshot_thread_tick);
            }
            continue;
        }
        if (late_missing_snapshot_backfill) {
            kbo_log_runtimef(
                "KBO FA salary snapshot late backfill date=%u opening_day=%u league=%u reason=missing_opening_day_snapshot",
                date,
                opening_day,
                league_id);
        }

        if (kbo_capture_fa_salary_opening_day_snapshot(
                late_missing_snapshot_backfill ? "opening_day_thread_late_backfill" : "opening_day_thread",
                date,
                year,
                opening_day,
                league_id)) {
            captured_season = year;
        }
        if (profile_snapshot_thread_tick_active) {
            kbo_profiler_end("fa_salary_snapshot.thread.capture_attempt", &profile_snapshot_thread_tick);
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

    if (!kbo_start_runtime_thread(kbo_fa_salary_snapshot_thread, NULL, "FA salary snapshot")) {
        InterlockedExchange(&g_kbo_fa_salary_snapshot_thread_started, 0);
    }
}
