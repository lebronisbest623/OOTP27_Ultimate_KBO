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
#include "military_service_tick.h"

/* OOTP's player vector can hold at most a few tens of thousands of entries in
 * any realistic save; anything past 200k almost certainly indicates that the
 * memory we found is not actually the player vector. */
#define KBO_MILITARY_TICK_PLAYER_COUNT_MAX_PLAUSIBLE 200000

/* Re-check kbo_runtime_save_in_progress() every Nth player in the main loop
 * so a save that starts mid-scan is noticed quickly without paying for the
 * check on every iteration. */
#define KBO_MILITARY_TICK_SAVE_CHECK_INTERVAL 128

/* Process-lifetime burst limits for two of the per-tick log lines.  These
 * counters never reset, so a long-running launcher will eventually only see
 * the rate-limited lines even after a save switch.  See the comments at the
 * counter declarations below for the trade-off. */
#define KBO_MILITARY_TICK_DISCOVERED_LOG_BURST 160
#define KBO_MILITARY_TICK_GENERAL_LOG_BURST 20

static uint32_t kbo_military_days_tick_serial_from_work_date(uint32_t date)
{
    return kbo_date_serial(
        date / 10000u,
        (date / 100u) % 100u,
        date % 100u);
}

/* SANG (Sangmu) and KPB (police club) are the two clubs KBO players can be
 * loaned to for military service.  service_team_id is guaranteed to match one
 * of the two by the caller (see the loop body that filters it). */
static uint8_t* kbo_military_service_team_for_id(
    uint32_t service_team_id,
    uint32_t sang_id, uint8_t* sang,
    uint32_t kpb_id,  uint8_t* kpb)
{
    if (service_team_id != 0u && service_team_id == sang_id) {
        return sang;
    }
    if (service_team_id != 0u && service_team_id == kpb_id) {
        return kpb;
    }
    return NULL;
}

/* Returns 1 if the action was either executed or intentionally deferred to a
 * later tick, signalling the caller to "continue" the player loop.  Returns 0
 * if the action was not applicable. */
static int kbo_military_tick_release_invalid_or_defer(
    uint8_t* player,
    uint8_t* service_team,
    uint32_t service_team_id,
    const char* source,
    uint32_t vector_offset,
    int source_allows_roster_mutation,
    int* out_deferred,
    int* out_released)
{
    if (!source_allows_roster_mutation) {
        (*out_deferred)++;
        return 1;
    }
    *out_released += kbo_release_invalid_military_service_team_assignment(
        player, service_team, service_team_id, source, vector_offset);
    return 1;
}

static int kbo_military_tick_return_or_defer(
    uint8_t* player,
    const char* source,
    uint32_t vector_offset,
    int source_allows_roster_mutation,
    int* out_deferred,
    int* out_returned)
{
    if (!source_allows_roster_mutation) {
        (*out_deferred)++;
        return 1;
    }
    *out_returned += kbo_return_completed_military_loan_player(
        player, source, vector_offset, 0);
    return 1;
}

/* Backfill loan bookkeeping fields that may be zeroed/stale after a save
 * round-trip so subsequent days_left math works against a consistent record. */
static void kbo_military_tick_repair_loan_fields(
    KboMilitaryActiveLoan* loan,
    uintptr_t player_ptr,
    int32_t stored_days_left,
    uint32_t today_serial)
{
    loan->player_ptr = player_ptr;
    if (loan->service_total_days <= 0) {
        loan->service_total_days = KBO_MILITARY_SERVICE_DAYS;
    }
    if (loan->service_start_date_serial == 0
            || loan->service_start_date_serial > today_serial) {
        loan->service_start_date_serial = today_serial;
    }
    if (loan->service_return_date_serial == 0u) {
        loan->service_return_date_serial = today_serial + (uint32_t)(
            stored_days_left > 0 ? stored_days_left : loan->service_total_days);
    }
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

    int tracked          = 0;
    int monitored        = 0;
    /* days_left_resynced: count of players whose stored days_left field we
     * had to rewrite because it diverged from the value implied by the loan
     * record's return_date_serial. */
    int days_left_resynced = 0;
    int returned         = 0;
    int newly_registered = 0;
    int invalid_released = 0;
    int deferred_returns = 0;
    int deferred_invalid_releases = 0;
    int return_preview_news = 0;
    int aborted_for_save = 0;

    for (int32_t i = 0; i < player_count; i++) {
        if ((i % KBO_MILITARY_TICK_SAVE_CHECK_INTERVAL) == 0
                && kbo_runtime_save_in_progress()) {
            aborted_for_save = 1;
            kbo_log_runtimef(
                "KBO military service day tick aborted source=%s reason=save_in_progress stage=player_loop date_serial=%u scanned=%d count=%d",
                source != NULL ? source : "",
                today_serial,
                (int)i,
                player_count);
            break;
        }
        uintptr_t player_ptr = player_snapshot[i];
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }

        uint8_t* player = (uint8_t*)player_ptr;
        uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        uint32_t loan_team_id = *(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET);
        uint32_t service_team_id = 0u;
        if (current_team_id == sang_id || current_team_id == kpb_id) {
            service_team_id = current_team_id;
        } else if (loan_team_id == sang_id || loan_team_id == kpb_id) {
            service_team_id = loan_team_id;
        }
        if (service_team_id == 0u) {
            continue;
        }

        tracked++;
        uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        int active_index = find_active_kbo_military_loan_index(player_id);
        int32_t direct_days_left = kbo_military_days_left(player);
        uint8_t military_active = player[OOTP27_PLAYER_MILITARY_ACTIVE_OFFSET];
        if (active_index < 0 && military_active == 0u && direct_days_left <= 0) {
            uint8_t* service_team = kbo_military_service_team_for_id(
                service_team_id, sang_id, sang, kpb_id, kpb);
            kbo_military_tick_release_invalid_or_defer(
                player, service_team, service_team_id, source, vector_offset,
                source_allows_roster_mutation,
                &deferred_invalid_releases, &invalid_released);
            continue;
        }

        uint32_t original_team_id = 0u;
        uint32_t original_league_id = 0u;
        kbo_military_resolve_original_team(
            player,
            service_team_id,
            sang_id,
            kpb_id,
            &original_team_id,
            &original_league_id);
        if (original_team_id != 0u
                && original_team_id != service_team_id
                && original_team_id != sang_id
                && original_team_id != kpb_id) {
            kbo_military_repair_original_team_memory(
                player,
                original_team_id,
                original_league_id,
                service_team_id,
                sang_id,
                kpb_id);
        }

        if (active_index < 0
                && direct_days_left > 0
                && original_team_id != 0
                && original_team_id != service_team_id
                && original_team_id != sang_id
                && original_team_id != kpb_id) {
            uint8_t* service_team = kbo_military_service_team_for_id(
                service_team_id, sang_id, sang, kpb_id, kpb);
            uint32_t service_league = service_team != NULL
                ? *(uint32_t*)(service_team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET)
                : 0;
            register_active_kbo_military_loan(
                player_id, player_ptr,
                original_team_id, original_league_id,
                service_team_id,
                service_league != 0 ? service_league
                    : *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET));
            player[OOTP27_PLAYER_MILITARY_ACTIVE_OFFSET] = 1u;
            active_index = find_active_kbo_military_loan_index(player_id);
            newly_registered++;
            /* Process-lifetime counter: never reset across save switches.
             * A long-running launcher that visits many saves will eventually
             * lose this trace, but the trade-off is intentional - we'd rather
             * miss late lines than spam the log on every save round-trip. */
            static volatile LONG discovered_register_log_count = 0;
            LONG discovered_slot = InterlockedIncrement(&discovered_register_log_count);
            if (discovered_slot <= KBO_MILITARY_TICK_DISCOVERED_LOG_BURST) {
                kbo_log_runtimef(
                    "KBO military discovered active loan registered source=%s player=%u service_team=%u original_team=%u original_league=%u days_left=%d military_active=%u",
                    source != NULL ? source : "",
                    player_id,
                    service_team_id,
                    original_team_id,
                    original_league_id,
                    direct_days_left,
                    (unsigned)military_active);
            }
        }

        if (active_index < 0) {
            int32_t days_left = kbo_military_days_left(player);
            if (original_team_id == 0u
                    && days_left <= 0
                    && player[OOTP27_PLAYER_MILITARY_ACTIVE_OFFSET] == 0) {
                uint8_t* service_team = kbo_military_service_team_for_id(
                    service_team_id, sang_id, sang, kpb_id, kpb);
                kbo_military_tick_release_invalid_or_defer(
                    player, service_team, service_team_id, source, vector_offset,
                    source_allows_roster_mutation,
                    &deferred_invalid_releases, &invalid_released);
                continue;
            }
            if (days_left <= 0) {
                kbo_military_tick_return_or_defer(
                    player, source, vector_offset,
                    source_allows_roster_mutation,
                    &deferred_returns, &returned);
                continue;
            }
            kbo_clear_military_unavailable_flags(player);
            continue;
        }

        KboMilitaryActiveLoan* loan = kbo_active_military_loan_at(active_index);
        if (loan == NULL) {
            continue;
        }
        kbo_military_tick_repair_loan_fields(
            loan, player_ptr, kbo_military_days_left(player), today_serial);

        int32_t days_left = kbo_military_effective_days_left(player);
        int32_t stored_days_left = kbo_military_days_left(player);
        int32_t managed_days_left = kbo_military_days_left_from_return_serial(
            loan->service_return_date_serial,
            today_serial);
        if (managed_days_left != days_left) {
            days_left = managed_days_left;
        }
        if (stored_days_left != managed_days_left) {
            kbo_set_military_days_left(player, managed_days_left);
            days_left_resynced++;
        }
        if (days_left <= 0) {
            kbo_military_tick_return_or_defer(
                player, source, vector_offset,
                source_allows_roster_mutation,
                &deferred_returns, &returned);
            continue;
        }

        kbo_clear_military_unavailable_flags(player);
        monitored++;
    }

    if (!aborted_for_save && !kbo_runtime_save_in_progress()) {
        return_preview_news = kbo_emit_military_return_preview_news_if_due(today_serial, source);
    } else if (!aborted_for_save) {
        aborted_for_save = 1;
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
            || returned > 0
            || newly_registered > 0
            || days_left_resynced > 0
            || invalid_released > 0
            || return_preview_news > 0
            || deferred_returns > 0
            || deferred_invalid_releases > 0
            || aborted_for_save
            || log_index <= KBO_MILITARY_TICK_GENERAL_LOG_BURST) {
        kbo_log_runtimef(
            "KBO military service day tick source=%s date_serial=%u"
            " seeded=%d tracked=%d newly_registered=%d monitored=%d managed=%d returned=%d invalid_released=%d"
            " return_preview_news=%d deferred_returns=%d deferred_invalid=%d aborted_for_save=%d count=%d vector_off=0x%x",
            source != NULL ? source : "",
            today_serial,
            seeded_assignments,
            tracked, newly_registered, monitored, days_left_resynced, returned,
            invalid_released,
            return_preview_news,
            deferred_returns,
            deferred_invalid_releases,
            aborted_for_save,
            player_count, vector_offset);
    }

    HeapFree(GetProcessHeap(), 0, player_snapshot);
    return returned;
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
