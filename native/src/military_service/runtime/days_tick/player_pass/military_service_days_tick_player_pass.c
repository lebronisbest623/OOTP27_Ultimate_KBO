#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>

#include "../../../../bootstrap/abi/ootp_offsets.h"
#include "../../../../core/core_flags/api/flags_api.h"
#include "../../../../core/logging/core_log.h"
#include "../../../../runtime_memory/runtime_memory.h"
#include "../../../../team/lookup/team_lookup.h"
#include "../../../military_service.h"
#include "../../../players/loans/military_active_loan.h"
#include "../../../players/state/military_player_state.h"
#include "../../../returns/military_return.h"
#include "../../../seed/parse/military_service_seed_parse.h"
#include "../../assignment/military_service_assignment.h"
#include "military_service_days_tick_player_pass.h"

/* Re-check kbo_runtime_save_in_progress() every Nth player in the main loop
 * so a save that starts mid-scan is noticed quickly without paying for the
 * check on every iteration. */
#define KBO_MILITARY_TICK_SAVE_CHECK_INTERVAL 128

/* Process-lifetime burst limit for discovered-loan trace lines. */
#define KBO_MILITARY_TICK_DISCOVERED_LOG_BURST 160

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

static void kbo_military_tick_release_invalid_or_defer(
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
        return;
    }
    *out_released += kbo_release_invalid_military_service_team_assignment(
        player, service_team, service_team_id, source, vector_offset);
}

static void kbo_military_tick_return_or_defer(
    uint8_t* player,
    const char* source,
    uint32_t vector_offset,
    int source_allows_roster_mutation,
    int* out_deferred,
    int* out_returned)
{
    if (!source_allows_roster_mutation) {
        (*out_deferred)++;
        return;
    }
    *out_returned += kbo_return_completed_military_loan_player(
        player, source, vector_offset, 0);
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

void kbo_military_days_tick_player_pass(
    const KboMilitaryDaysTickPlayerPassInput* input,
    KboMilitaryDaysTickPlayerPassResult* result)
{
    if (input == NULL || result == NULL || input->player_snapshot == NULL) {
        return;
    }

    for (int32_t i = 0; i < input->player_count; i++) {
        if ((i % KBO_MILITARY_TICK_SAVE_CHECK_INTERVAL) == 0
                && kbo_runtime_save_in_progress()) {
            result->aborted_for_save = 1;
            kbo_log_runtimef(
                "KBO military service day tick aborted source=%s reason=save_in_progress stage=player_loop date_serial=%u scanned=%d count=%d",
                input->source != NULL ? input->source : "",
                input->today_serial,
                (int)i,
                input->player_count);
            break;
        }
        uintptr_t player_ptr = input->player_snapshot[i];
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }

        uint8_t* player = (uint8_t*)player_ptr;
        uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        uint32_t loan_team_id = *(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET);
        uint32_t service_team_id = 0u;
        if (current_team_id == input->sang_id || current_team_id == input->kpb_id) {
            service_team_id = current_team_id;
        } else if (loan_team_id == input->sang_id || loan_team_id == input->kpb_id) {
            service_team_id = loan_team_id;
        }
        if (service_team_id == 0u) {
            continue;
        }

        result->tracked++;
        uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        int active_index = find_active_kbo_military_loan_index(player_id);
        int32_t direct_days_left = kbo_military_days_left(player);
        uint8_t military_active = player[OOTP27_PLAYER_MILITARY_ACTIVE_OFFSET];
        if (active_index < 0 && military_active == 0u && direct_days_left <= 0) {
            uint8_t* service_team = kbo_military_service_team_for_id(
                service_team_id, input->sang_id, input->sang, input->kpb_id, input->kpb);
            kbo_military_tick_release_invalid_or_defer(
                player, service_team, service_team_id, input->source, input->vector_offset,
                input->source_allows_roster_mutation,
                &result->deferred_invalid_releases, &result->invalid_released);
            continue;
        }

        uint32_t original_team_id = 0u;
        uint32_t original_league_id = 0u;
        kbo_military_resolve_original_team(
            player,
            service_team_id,
            input->sang_id,
            input->kpb_id,
            &original_team_id,
            &original_league_id);
        if (original_team_id != 0u
                && original_team_id != service_team_id
                && original_team_id != input->sang_id
                && original_team_id != input->kpb_id) {
            kbo_military_repair_original_team_memory(
                player,
                original_team_id,
                original_league_id,
                service_team_id,
                input->sang_id,
                input->kpb_id);
        }

        if (active_index < 0
                && direct_days_left > 0
                && original_team_id != 0
                && original_team_id != service_team_id
                && original_team_id != input->sang_id
                && original_team_id != input->kpb_id) {
            uint8_t* service_team = kbo_military_service_team_for_id(
                service_team_id, input->sang_id, input->sang, input->kpb_id, input->kpb);
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
            result->newly_registered++;
            static volatile LONG discovered_register_log_count = 0;
            LONG discovered_slot = InterlockedIncrement(&discovered_register_log_count);
            if (discovered_slot <= KBO_MILITARY_TICK_DISCOVERED_LOG_BURST) {
                kbo_log_runtimef(
                    "KBO military discovered active loan registered source=%s player=%u service_team=%u original_team=%u original_league=%u days_left=%d military_active=%u",
                    input->source != NULL ? input->source : "",
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
                    service_team_id, input->sang_id, input->sang, input->kpb_id, input->kpb);
                kbo_military_tick_release_invalid_or_defer(
                    player, service_team, service_team_id, input->source, input->vector_offset,
                    input->source_allows_roster_mutation,
                    &result->deferred_invalid_releases, &result->invalid_released);
                continue;
            }
            if (days_left <= 0) {
                kbo_military_tick_return_or_defer(
                    player, input->source, input->vector_offset,
                    input->source_allows_roster_mutation,
                    &result->deferred_returns, &result->returned);
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
            loan, player_ptr, kbo_military_days_left(player), input->today_serial);

        int32_t days_left = kbo_military_effective_days_left(player);
        int32_t stored_days_left = kbo_military_days_left(player);
        int32_t managed_days_left = kbo_military_days_left_from_return_serial(
            loan->service_return_date_serial,
            input->today_serial);
        if (managed_days_left != days_left) {
            days_left = managed_days_left;
        }
        if (stored_days_left != managed_days_left) {
            kbo_set_military_days_left(player, managed_days_left);
            result->days_left_resynced++;
        }
        if (days_left <= 0) {
            kbo_military_tick_return_or_defer(
                player, input->source, input->vector_offset,
                input->source_allows_roster_mutation,
                &result->deferred_returns, &result->returned);
            continue;
        }

        kbo_clear_military_unavailable_flags(player);
        result->monitored++;
    }
}
