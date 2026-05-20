#include "salary_snapshot_phase_events.h"

#include <stdint.h>
#include <stdio.h>
#include <windows.h>

#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../bootstrap/profiling/profiler.h"
#include "../../core/dates/core_current_date.h"
#include "../../core/dates/tick/current_date_tick_capture.h"
#include "../../core/core_flags/api/flags_api.h"
#include "../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../core/logging/core_log.h"
#include "../../core/runtime_tuning/runtime_tuning_policy.h"
#include "../../core/season/season_calendar.h"
#include "../../runtime_memory/runtime_memory.h"
#include "../paths/salary_snapshot_paths_dates.h"
#include "../state/salary_snapshot_state.h"
#include "../capture/salary_snapshot_write_capture.h"

static int kbo_fa_salary_snapshot_phase_event_league_is_kbo(uintptr_t league_ptr, uint32_t league_id)
{
    if (league_ptr == 0 || league_id == 0
            || !memory_range_readable((void*)league_ptr, OOTP27_KBO_LEAGUE_ID_OFFSET + 16u)) {
        return 0;
    }

    uint32_t legacy_id = *(uint32_t*)(league_ptr + OOTP27_KBO_LEAGUE_ID_OFFSET);
    uint32_t alternate_id = *(uint32_t*)(league_ptr + OOTP27_KBO_LEAGUE_ID_OFFSET + 8u);
    if (legacy_id == league_id
            && kbo_league_candidate_matches_id(league_ptr, league_id, OOTP27_KBO_LEAGUE_ID_OFFSET)) {
        return 1;
    }
    if (alternate_id == league_id
            && kbo_league_candidate_matches_id(league_ptr, league_id, OOTP27_KBO_LEAGUE_ID_OFFSET + 8u)) {
        return 1;
    }
    return 0;
}

static void kbo_fa_salary_snapshot_set_pending_phase_event(uintptr_t league_ptr, uint32_t value, uint32_t site_rva)
{
    g_kbo_fa_salary_snapshot_phase_pending_league_ptr = league_ptr;
    InterlockedExchange(&g_kbo_fa_salary_snapshot_phase_pending_value, (LONG)value);
    InterlockedExchange(&g_kbo_fa_salary_snapshot_phase_pending_site_rva, (LONG)site_rva);
    InterlockedExchange(&g_kbo_fa_salary_snapshot_phase_pending_active, 1);
    kbo_log_runtimef(
        "KBO FA salary opening-day phase hook observed site=0x%x league=%p value=%u",
        site_rva,
        (void*)league_ptr,
        value);
}

static void kbo_fa_salary_snapshot_drain_phase_events_once(void)
{
    LONG latest = InterlockedCompareExchange(&g_kbo_fa_salary_snapshot_phase_event_published_sequence, 0, 0);
    LONG consumed = InterlockedCompareExchange(&g_kbo_fa_salary_snapshot_phase_event_consumed_sequence, 0, 0);
    if (latest == consumed) {
        return;
    }

    if (latest - consumed > (LONG)KBO_FA_SALARY_PHASE_EVENT_RING_SIZE) {
        kbo_log_runtimef(
            "KBO FA salary opening-day phase hook event ring overflow consumed=%ld latest=%ld",
            (long)consumed,
            (long)latest);
        consumed = latest - (LONG)KBO_FA_SALARY_PHASE_EVENT_RING_SIZE;
    }

    uint32_t league_id = kbo_resolve_kbo_league_id();
    for (LONG event_no = consumed; event_no < latest; event_no++) {
        uint32_t index = (uint32_t)event_no & KBO_FA_SALARY_PHASE_EVENT_RING_MASK;
        uintptr_t league_ptr = g_kbo_fa_salary_snapshot_phase_event_league_ptrs[index];
        uint32_t value = g_kbo_fa_salary_snapshot_phase_event_values[index];
        uint32_t site_rva = g_kbo_fa_salary_snapshot_phase_event_site_rvas[index];
        if (value != 3u || !kbo_fa_salary_snapshot_phase_event_league_is_kbo(league_ptr, league_id)) {
            continue;
        }
        kbo_fa_salary_snapshot_set_pending_phase_event(league_ptr, value, site_rva);
    }

    InterlockedExchange(&g_kbo_fa_salary_snapshot_phase_event_consumed_sequence, latest);
}

static void kbo_fa_salary_snapshot_try_pending_phase_event(uint32_t date)
{
    if (InterlockedCompareExchange(&g_kbo_fa_salary_snapshot_phase_pending_active, 1, 1) == 0) {
        return;
    }

    uintptr_t league_ptr = g_kbo_fa_salary_snapshot_phase_pending_league_ptr;
    uint32_t value = (uint32_t)InterlockedCompareExchange(&g_kbo_fa_salary_snapshot_phase_pending_value, 0, 0);
    uint32_t site_rva = (uint32_t)InterlockedCompareExchange(&g_kbo_fa_salary_snapshot_phase_pending_site_rva, 0, 0);
    if (value != 3u) {
        InterlockedExchange(&g_kbo_fa_salary_snapshot_phase_pending_active, 0);
        return;
    }

    uint32_t league_id = kbo_resolve_kbo_league_id();
    if (!kbo_fa_salary_snapshot_phase_event_league_is_kbo(league_ptr, league_id)) {
        kbo_log_runtimef(
            "KBO FA salary opening-day phase hook ignored site=0x%x league=%p value=%u reason=not_kbo_league",
            site_rva,
            (void*)league_ptr,
            value);
        InterlockedExchange(&g_kbo_fa_salary_snapshot_phase_pending_active, 0);
        return;
    }

    uint32_t opening_day = 0u;
    if (!kbo_season_calendar_read_league_opening_day(league_ptr, &opening_day)) {
        return;
    }

    if (date == 0u) {
        return;
    }

    if (date < opening_day) {
        return;
    }

    uint32_t season = opening_day / 10000u;
    if (season < 1982u || season > 2200u) {
        InterlockedExchange(&g_kbo_fa_salary_snapshot_phase_pending_active, 0);
        return;
    }
    (void)kbo_season_calendar_store_opening_day(
        league_id,
        season,
        opening_day,
        date,
        "season_phase_hook");

    if (kbo_fa_salary_snapshot_file_exists(season)) {
        InterlockedExchange(&g_kbo_fa_salary_snapshot_phase_pending_active, 0);
        return;
    }

    if (date > opening_day) {
        kbo_log_runtimef(
            "KBO FA salary opening-day phase hook missed exact opening day site=0x%x date=%u opening_day=%u league=%u",
            site_rva,
            date,
            opening_day,
            league_id);
        InterlockedExchange(&g_kbo_fa_salary_snapshot_phase_pending_active, 0);
        return;
    }

    char source[64] = {0};
    snprintf(source, sizeof(source), "season_phase_hook_0x%x", site_rva);
    if (kbo_capture_fa_salary_opening_day_snapshot(source, date, season, opening_day, league_id)) {
        InterlockedExchange(&g_kbo_fa_salary_snapshot_phase_pending_active, 0);
    }
}

static int kbo_fa_salary_snapshot_phase_sync_consumer(
    uint32_t date,
    uint32_t site_rva,
    void* context)
{
    (void)site_rva;
    (void)context;
    if (!kbo_fix_enabled()) {
        return 1;
    }
    kbo_fa_salary_snapshot_drain_phase_events_once();
    kbo_fa_salary_snapshot_try_pending_phase_event(date);
    return 1;
}

static DWORD WINAPI kbo_fa_salary_snapshot_phase_event_thread(LPVOID parameter)
{
    (void)parameter;
    kbo_log_runtime_line("KBO FA salary opening-day phase hook event thread started");

    KboCurrentDateTickConsumer date_consumer = {0};
    kbo_current_date_tick_consumer_init(
        &date_consumer,
        "fa_salary_snapshot_phase_events",
        KBO_CURRENT_DATE_TICK_CONSUMER_EMIT_CURRENT_ON_SAVE_ENTER);

    while (kbo_runtime_threads_should_continue()) {
        if (!kbo_runtime_sleep_should_continue((uint32_t)kbo_runtime_tuning_policy()->fa_salary_snapshot_phase_event_sleep_ms)) {
            break;
        }
        LARGE_INTEGER profile_phase_thread_tick = {0};
        int profile_phase_thread_tick_active = kbo_profiler_begin(&profile_phase_thread_tick);
        if (!kbo_fix_enabled()) {
            if (profile_phase_thread_tick_active) {
                kbo_profiler_end("fa_salary_snapshot.phase_thread.disabled_tick", &profile_phase_thread_tick);
            }
            continue;
        }

        KboCurrentDateTickWork date_work = {0};
        while (kbo_current_date_tick_consumer_next(&date_consumer, &date_work)) {
            (void)date_work;
            kbo_current_date_tick_consumer_mark_processed(&date_consumer);
        }

        kbo_fa_salary_snapshot_drain_phase_events_once();
        uint32_t current_date = 0u;
        if (kbo_current_date_tick_latest_published_date(&current_date)) {
            kbo_fa_salary_snapshot_try_pending_phase_event(current_date);
        }
        if (profile_phase_thread_tick_active) {
            kbo_profiler_end("fa_salary_snapshot.phase_thread.tick", &profile_phase_thread_tick);
        }
    }
    InterlockedExchange(&g_kbo_fa_salary_snapshot_phase_event_thread_started, 0);
    kbo_log_runtime_line("KBO FA salary opening-day phase hook event thread stopped");

    return 0;
}

void start_kbo_fa_salary_snapshot_phase_event_thread(void)
{
    if (!kbo_fix_enabled()) {
        return;
    }
    if (InterlockedCompareExchange(&g_kbo_fa_salary_snapshot_phase_event_thread_started, 1, 0) != 0) {
        return;
    }
    kbo_current_date_tick_register_sync_consumer(
        "fa_salary_snapshot_phase_events",
        kbo_fa_salary_snapshot_phase_sync_consumer,
        NULL);

    if (!kbo_start_runtime_thread(
            kbo_fa_salary_snapshot_phase_event_thread,
            NULL,
            "FA salary snapshot phase events")) {
        InterlockedExchange(&g_kbo_fa_salary_snapshot_phase_event_thread_started, 0);
    }
}
