#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../../amateur_player_quality/api/amateur_player_quality.h"
#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/dates/core_current_date.h"
#include "../../../core/dates/core_text_date.h"
#include "../../../core/dates/tick/current_date_tick_capture.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../../core/season/opening_day_storyline_guard.h"
#include "../../../core/logging/core_log.h"
#include "../../../core/files/save_paths/core_save_paths.h"
#include "../../../core/runtime_tuning/runtime_tuning_policy.h"
#include "../../../fa_market_classification/api/fa_market_classification.h"
#include "../../../foreign/replacement_seed/api/foreign_replacement_seed.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../team/lookup/team_lookup.h"
#include "../../military_service.h"
#include "../../players/loans/military_active_loan.h"
#include "../../players/state/military_player_state.h"
#include "../../returns/military_return.h"
#include "../../returns/military_return_preview_news.h"
#include "../../calendar/military_service_date.h"
#include "../../seed/registry/military_seed_registry.h"
#include "../assignment/military_service_assignment.h"
#include "../state/military_service_runtime_state.h"
#include "military_service_days_tick.h"
#include "player_pass/military_service_days_tick_player_pass.h"
#include "military_service_tick.h"

/* OOTP's player vector can hold at most a few tens of thousands of entries in
 * any realistic save; anything past 200k almost certainly indicates that the
 * memory we found is not actually the player vector. */
#define KBO_MILITARY_TICK_PLAYER_COUNT_MAX_PLAUSIBLE 200000

/* Process-lifetime burst limit for the per-tick summary log line. */
#define KBO_MILITARY_TICK_GENERAL_LOG_BURST 20

static uint32_t kbo_military_days_tick_serial_from_work_date(uint32_t date)
{
    return kbo_date_serial(
        date / 10000u,
        (date / 100u) % 100u,
        date % 100u);
}

static int kbo_tick_military_service_days_for_serial(
    uint32_t today_serial,
    const char* source,
    int* out_seeded_assignments)
{
    if (out_seeded_assignments != NULL) {
        *out_seeded_assignments = 0;
    }
    if (!kbo_fix_enabled()) {
        return 0;
    }
    if (!kbo_runtime_pause_for_save_if_needed(source != NULL ? source : "military_days_tick")) {
        return 0;
    }
    if (today_serial == 0) {
        return 0;
    }
    if (kbo_opening_day_storyline_guard_active(source, NULL, NULL)) {
        return 0;
    }

    uintptr_t player_vector = 0;
    int32_t   player_count  = 0;
    uint32_t  vector_offset = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, &vector_offset)) {
        return 0;
    }
    if (player_vector == 0u
            || player_count <= 0
            || player_count > KBO_MILITARY_TICK_PLAYER_COUNT_MAX_PLAUSIBLE) {
        return 0;
    }
    SIZE_T player_vector_bytes = (SIZE_T)player_count * sizeof(uintptr_t);
    if (!memory_range_readable((void*)player_vector, player_vector_bytes)) {
        return 0;
    }
    uintptr_t* player_snapshot = (uintptr_t*)HeapAlloc(GetProcessHeap(), 0, player_vector_bytes);
    if (player_snapshot == NULL) {
        return 0;
    }
    /* OOTP can re-map or free the player vector mid-save; reading it through
     * a plain dereference would race with that and crash the whole process.
     * ReadProcessMemory wraps the copy in SEH so an unmap during the read
     * just returns FALSE instead of raising an access violation.  See commit
     * 69355b52 ("Fix save crash during all-star date seeding"). */
    SIZE_T bytes_read = 0u;
    if (!ReadProcessMemory(
            GetCurrentProcess(),
            (LPCVOID)player_vector,
            player_snapshot,
            player_vector_bytes,
            &bytes_read)
            || bytes_read != player_vector_bytes) {
        HeapFree(GetProcessHeap(), 0, player_snapshot);
        return 0;
    }
    /* Stage-by-stage save_in_progress checks: any work after this point would
     * race with OOTP serializing player state, so we bail out as soon as we
     * see a save start.  The duplication is intentional: each bailout points
     * back at the stage label so the runtime log shows where we gave up. */
    if (kbo_runtime_save_in_progress()) {
        kbo_log_runtimef(
            "KBO military service day tick aborted source=%s reason=save_in_progress stage=after_player_snapshot date_serial=%u count=%d",
            source != NULL ? source : "",
            today_serial,
            player_count);
        HeapFree(GetProcessHeap(), 0, player_snapshot);
        return 0;
    }

    kbo_update_amateur_reputation_from_team_records(source);

    uint8_t* sang = find_kbo_team_by_csv_id_any_league("SANG", 0);
    uint8_t* kpb  = find_kbo_team_by_csv_id_any_league("KPB",  0);
    uint32_t sang_id = sang != NULL ? *(uint32_t*)(sang + OOTP27_KBO_TEAM_ID_OFFSET) : 0;
    uint32_t kpb_id  = kpb  != NULL ? *(uint32_t*)(kpb  + OOTP27_KBO_TEAM_ID_OFFSET) : 0;
    if (sang_id == 0 && kpb_id == 0) {
        HeapFree(GetProcessHeap(), 0, player_snapshot);
        return 0;
    }
    int seeded_assignments = 0;
    /* The source identifier doubles as a policy switch: the daily-tick path
     * defers seed application and rate-limits roster mutations (only inside a
     * mutation window), whereas other entry points (save-enter, manual
     * triggers) are allowed to run the full pass.  See callers in
     * military_service_days_tick_sync.c and the runtime marker wait. */
    int source_is_daily_tick = source != NULL && strcmp(source, "military_days_tick") == 0;
    int source_allows_seed_assignment = !source_is_daily_tick;
    int source_allows_roster_mutation = !source_is_daily_tick
        || kbo_military_daily_roster_mutation_window_ready(today_serial, player_count);
    if (kbo_runtime_save_in_progress()) {
        kbo_log_runtimef(
            "KBO military service day tick aborted source=%s reason=save_in_progress stage=before_seed_assignment date_serial=%u count=%d",
            source != NULL ? source : "",
            today_serial,
            player_count);
        HeapFree(GetProcessHeap(), 0, player_snapshot);
        return 0;
    }
    if (source_allows_seed_assignment) {
        seeded_assignments = kbo_apply_military_service_seed_assignments(sang, kpb, source);
    }
    if (out_seeded_assignments != NULL) {
        *out_seeded_assignments = seeded_assignments;
    }

    KboMilitaryDaysTickPlayerPassInput pass_input = {
        .player_snapshot = player_snapshot,
        .player_count = player_count,
        .vector_offset = vector_offset,
        .today_serial = today_serial,
        .source = source,
        .sang = sang,
        .kpb = kpb,
        .sang_id = sang_id,
        .kpb_id = kpb_id,
        .source_allows_roster_mutation = source_allows_roster_mutation
    };
    KboMilitaryDaysTickPlayerPassResult pass_result = {0};
    kbo_military_days_tick_player_pass(&pass_input, &pass_result);

    int return_preview_news = 0;
    if (!pass_result.aborted_for_save && !kbo_runtime_save_in_progress()) {
        return_preview_news = kbo_emit_military_return_preview_news_if_due(today_serial, source);
    } else if (!pass_result.aborted_for_save) {
        pass_result.aborted_for_save = 1;
        kbo_log_runtimef(
            "KBO military service day tick aborted source=%s reason=save_in_progress stage=before_return_preview date_serial=%u count=%d",
            source != NULL ? source : "",
            today_serial,
            player_count);
    }

    /* Log key in the runtime log:
     *   managed=                <- kept for log-format compatibility, but the
     *                              underlying variable is days_left_resynced
     *                              ("number of players whose days_left was
     *                              rewritten because it diverged from the
     *                              loan's return_date_serial"). */
    LONG log_index = InterlockedIncrement(&g_military_days_tick_log_count);
    if (seeded_assignments > 0
            || pass_result.returned > 0
            || pass_result.newly_registered > 0
            || pass_result.days_left_resynced > 0
            || pass_result.invalid_released > 0
            || return_preview_news > 0
            || pass_result.deferred_returns > 0
            || pass_result.deferred_invalid_releases > 0
            || pass_result.aborted_for_save
            || log_index <= KBO_MILITARY_TICK_GENERAL_LOG_BURST) {
        kbo_log_runtimef(
            "KBO military service day tick source=%s date_serial=%u"
            " seeded=%d tracked=%d newly_registered=%d monitored=%d managed=%d returned=%d invalid_released=%d"
            " return_preview_news=%d deferred_returns=%d deferred_invalid=%d aborted_for_save=%d count=%d vector_off=0x%x",
            source != NULL ? source : "",
            today_serial,
            seeded_assignments,
            pass_result.tracked,
            pass_result.newly_registered,
            pass_result.monitored,
            pass_result.days_left_resynced,
            pass_result.returned,
            pass_result.invalid_released,
            return_preview_news,
            pass_result.deferred_returns,
            pass_result.deferred_invalid_releases,
            pass_result.aborted_for_save,
            player_count, vector_offset);
    }

    HeapFree(GetProcessHeap(), 0, player_snapshot);
    return pass_result.returned;
}

int kbo_tick_military_service_days_for_date(
    uint32_t today_yyyymmdd,
    const char* source,
    int* out_seeded_assignments)
{
    uint32_t today_serial = kbo_military_days_tick_serial_from_work_date(today_yyyymmdd);
    return kbo_tick_military_service_days_for_serial(today_serial, source, out_seeded_assignments);
}

/* This thread is the consumer side of the date-tick pipeline: it parks on the
 * consumer queue waiting for date-change events.  The actual military tick
 * work runs from kbo_tick_military_service_days_for_date(), which is called
 * by other entry points (the date-tick sync layer, the runtime marker wait,
 * etc.); this thread just keeps the consumer alive so save-enter events get
 * marked processed and date dispatch keeps flowing. */
DWORD WINAPI kbo_military_days_tick_thread(LPVOID parameter)
{
    (void)parameter;
    kbo_log_runtime_line("KBO military service day tick thread started");

    KboCurrentDateTickConsumer consumer = {0};
    kbo_current_date_tick_consumer_init(
        &consumer,
        "military_days_tick",
        KBO_CURRENT_DATE_TICK_CONSUMER_EMIT_CURRENT_ON_SAVE_ENTER);

    while (kbo_runtime_threads_should_continue()) {
        if (!kbo_runtime_sleep_should_continue((uint32_t)kbo_runtime_tuning_policy()->military_days_tick_sleep_ms)) {
            break;
        }

        KboCurrentDateTickWork work = {0};
        while (kbo_current_date_tick_consumer_next(&consumer, &work)) {
            (void)work;
            kbo_current_date_tick_consumer_mark_processed(&consumer);
        }
    }
    InterlockedExchange(&g_military_days_tick_started, 0);
    kbo_log_runtime_line("KBO military service day tick thread stopped");
    return 0;
}
