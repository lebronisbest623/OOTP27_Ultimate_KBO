#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/dates/core_current_date.h"
#include "../../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../../core/season/opening_day_storyline_guard.h"
#include "../../../core/logging/core_log.h"
#include "../../../core/files/save_paths/core_save_paths.h"
#include "../../../team/assignment/assignment/team_assignment.h"
#include "../../../team/lookup/team_lookup.h"
#include "../../../team/assignment/roster_arrays/team_roster_arrays.h"
#include "../../history/military_service_player_history.h"
#include "../../players/loans/military_active_loan.h"
#include "../../players/state/military_player_state.h"
#include "../../selection/events/policy/military_selection_policy.h"
#include "../../seed/registry/military_seed_registry.h"
#include "../../calendar/military_service_date.h"
#include "../state/military_service_runtime_state.h"
#include "military_service_assignment.h"

#define KBO_MILITARY_SERVICE_DAYS kbo_military_service_days()

int kbo_military_daily_roster_mutation_window_ready(
    uint32_t today_serial,
    int32_t player_count)
{
    char save_path[MAX_PATH] = {0};
    if (today_serial == 0u
            || player_count <= 0
            || !kbo_get_current_save_path(save_path, sizeof(save_path))) {
        return 0;
    }
    if (kbo_opening_day_storyline_guard_active("military_daily_roster_mutation_window", NULL, NULL)) {
        return 0;
    }

    LONG slot = InterlockedIncrement(&g_military_daily_mutation_ready_log_count);
    if (slot <= 20 || (slot % 100) == 0) {
        kbo_log_runtimef(
            "KBO military daily roster mutation window immediate date_serial=%u player_count=%d save=%s slot=%ld",
            today_serial,
            player_count,
            save_path,
            slot);
    }
    return 1;
}

int kbo_apply_military_service_seed_assignments(uint8_t* sang, uint8_t* kpb, const char* source)
{
    if (sang == NULL && kpb == NULL) {
        return 0;
    }

    uint32_t sang_id = sang != NULL ? *(uint32_t*)(sang + OOTP27_KBO_TEAM_ID_OFFSET) : 0u;
    uint32_t kpb_id  = kpb  != NULL ? *(uint32_t*)(kpb  + OOTP27_KBO_TEAM_ID_OFFSET) : 0u;
    uint32_t today_serial = kbo_current_date_serial();
    int applied = 0;
    int history_recorded = 0;
    KboMilitaryServiceSeed seeds[KBO_MILITARY_SERVICE_SEED_MAX];
    int seed_count = kbo_snapshot_military_service_seeds(seeds, KBO_MILITARY_SERVICE_SEED_MAX);

    for (int i = 0; i < seed_count; i++) {
        KboMilitaryServiceSeed* seed = &seeds[i];
        if (seed->player_id == 0u) {
            continue;
        }

        uint8_t* player = kbo_military_find_player_by_id(seed->player_id);
        if (player == NULL || !kbo_player_pointer_plausible((uintptr_t)player)) {
            continue;
        }

        uint8_t* service_team = (seed->service_team_code[0] != '\0' && _stricmp(seed->service_team_code, "KPB") == 0)
            ? kpb
            : sang;
        uint32_t service_team_id = service_team != NULL
            ? *(uint32_t*)(service_team + OOTP27_KBO_TEAM_ID_OFFSET)
            : 0u;
        if (service_team_id == 0u) {
            continue;
        }

        uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        uint32_t loan_team_id    = *(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET);
        int was_registered = find_active_kbo_military_loan_index(seed->player_id) >= 0;
        int was_on_service_team = current_team_id == service_team_id || loan_team_id == service_team_id;
        uint32_t seed_return_serial = kbo_military_yyyymmdd_to_serial(seed->service_return_yyyymmdd);
        if (today_serial != 0u
                && seed_return_serial != 0u
                && seed_return_serial <= today_serial
                && current_team_id != service_team_id
                && loan_team_id != service_team_id) {
            LONG skip_log = InterlockedIncrement(&g_military_seed_expired_skip_log_count);
            if (skip_log <= 40) {
                kbo_log_runtimef(
                    "KBO military service seed skipped expired key=%s player=%u return=%u today_serial=%u current_team=%u service_team=%u",
                    seed->key,
                    seed->player_id,
                    seed->service_return_yyyymmdd,
                    today_serial,
                    current_team_id,
                    service_team_id);
            }
            continue;
        }

        uint32_t original_team_id = 0u;
        uint32_t original_league_id = 0u;
        if (seed->original_team_code[0] != '\0') {
            uint8_t* original_team = NULL;
            if (seed->original_team_code[0] >= '0' && seed->original_team_code[0] <= '9') {
                original_team = find_kbo_team_by_numeric_id_any_league((uint32_t)strtoul(seed->original_team_code, NULL, 10), 0);
            } else {
                original_team = find_kbo_team_by_csv_id_any_league(seed->original_team_code, 0);
            }
            if (original_team != NULL) {
                original_team_id = *(uint32_t*)(original_team + OOTP27_KBO_TEAM_ID_OFFSET);
                original_league_id = *(uint32_t*)(original_team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
            }
        }
        if (original_team_id == 0u) {
            original_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
            original_league_id = *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_LEAGUE_ID_OFFSET);
        }
        if (original_team_id == 0u
                || original_team_id == service_team_id
                || original_team_id == sang_id
                || original_team_id == kpb_id) {
            original_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET);
        }
        if (original_team_id == 0u
                || original_team_id == service_team_id
                || original_team_id == sang_id
                || original_team_id == kpb_id) {
            continue;
        }
        if (original_league_id == 0u) {
            uint8_t* original_team = find_kbo_team_by_numeric_id_any_league(original_team_id, 0);
            if (original_team != NULL) {
                original_league_id = *(uint32_t*)(original_team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
            }
        }

        if (current_team_id != service_team_id && loan_team_id != service_team_id) {
            int called_pre_change = 0;
            int called_register = 0;
            int called_attach = 0;
            uint32_t service_league_id = *(uint32_t*)(service_team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
            kbo_assign_player_to_team_internal(
                player,
                service_team,
                service_league_id,
                0,
                &called_pre_change,
                &called_register,
                &called_attach);
            *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET) = original_team_id;
            *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET) = original_team_id;
            if (original_league_id != 0u) {
                *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_LEAGUE_ID_OFFSET) = original_league_id;
            }
            kbo_log_runtimef(
                "KBO military service seed assigned to service key=%s player=%u old_team=%u service_team=%u original_team=%u pre=%d register=%d attach=%d",
                seed->key,
                seed->player_id,
                current_team_id,
                service_team_id,
                original_team_id,
                called_pre_change,
                called_register,
                called_attach);
        }

        register_active_kbo_military_loan(
            seed->player_id,
            (uintptr_t)player,
            original_team_id,
            original_league_id,
            service_team_id,
            *(uint32_t*)(service_team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET));

        int active_index = find_active_kbo_military_loan_index(seed->player_id);
        if (active_index >= 0) {
            KboMilitaryActiveLoan* loan = kbo_active_military_loan_at(active_index);
            if (loan == NULL) {
                continue;
            }
            uint32_t start_serial = kbo_military_yyyymmdd_to_serial(seed->service_start_yyyymmdd);
            if (start_serial != 0u) {
                loan->service_start_date_serial = start_serial;
            }
            loan->service_total_days = seed->service_total_days > 0
                ? seed->service_total_days
                : KBO_MILITARY_SERVICE_DAYS;
            uint32_t return_serial = seed_return_serial;
            if (return_serial == 0u && loan->service_start_date_serial != 0u) {
                return_serial = loan->service_start_date_serial + (uint32_t)loan->service_total_days;
            }
            if (return_serial != 0u) {
                loan->service_return_date_serial = return_serial;
            }
            if (today_serial != 0u && loan->service_return_date_serial != 0u) {
                kbo_set_military_days_left(
                    player,
                    kbo_military_days_left_from_return_serial(loan->service_return_date_serial, today_serial));
            }
            player[OOTP27_PLAYER_MILITARY_ACTIVE_OFFSET] = 1;
            kbo_clear_military_unavailable_flags(player);
            if (!was_registered && !was_on_service_team) {
                uint32_t history_yyyymmdd = seed->service_start_yyyymmdd;
                if (history_yyyymmdd == 0u && loan->service_start_date_serial != 0u) {
                    history_yyyymmdd = kbo_military_serial_to_yyyymmdd(loan->service_start_date_serial);
                }
                if (history_yyyymmdd == 0u && today_serial != 0u) {
                    history_yyyymmdd = kbo_military_serial_to_yyyymmdd(today_serial);
                }
                history_recorded += kbo_record_military_seed_assignment_player_history(
                    seed->player_id,
                    service_team_id,
                    original_team_id,
                    history_yyyymmdd,
                    source);
            }
            applied++;
        }
    }

    if (applied > 0) {
        kbo_log_runtimef(
            "KBO military service seed applied source=%s assignments=%d history=%d",
            source != NULL ? source : "",
            applied,
            history_recorded);
    }
    return applied;
}

int kbo_release_invalid_military_service_team_assignment(
    uint8_t* player,
    uint8_t* service_team,
    uint32_t service_team_id,
    const char* source,
    uint32_t vector_offset)
{
    if (player == NULL
            || service_team == NULL
            || service_team_id == 0u
            || !kbo_player_pointer_plausible((uintptr_t)player)) {
        return 0;
    }

    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    if (player_id == 0u) {
        return 0;
    }

    uint32_t old_current_team = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t old_current_league = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
    uint32_t old_active_team = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    uint32_t old_original_slot_team = *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET);
    uint8_t old_contract_level = player[OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET];
    int32_t old_contract_status = *(int32_t*)(player + OOTP27_PLAYER_CONTRACT_STATUS_OFFSET);
    int32_t old_salary_y1 = *(int32_t*)(player + OOTP27_PLAYER_CONTRACT_SALARY_Y1_OFFSET);
    int32_t old_military_days_left = kbo_military_days_left(player);
    uint8_t old_military_active = player[OOTP27_PLAYER_MILITARY_ACTIVE_OFFSET];
    int removed = kbo_remove_player_id_from_known_team_roster_arrays(service_team, player_id);

    uint32_t fallback_league_id = kbo_resolve_kbo_league_id();
    if (fallback_league_id == 0u) {
        fallback_league_id = old_current_league;
    }

    *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET) = 0u;
    *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET) = fallback_league_id;
    if (old_active_team == service_team_id) {
        *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET) = 0u;
    }
    if (old_original_slot_team == service_team_id) {
        *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET) = 0u;
    }
    if (*(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET) == service_team_id) {
        *(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET) = 0u;
        *(uint32_t*)(player + OOTP27_PLAYER_LOAN_LEAGUE_ID_OFFSET) = 0u;
        player[OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET] = 0;
        player[OOTP27_PLAYER_LOAN_DIRTY_FLAG_OFFSET] = 1;
    }

    player[OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET] = 0u;
    *(int32_t*)(player + OOTP27_PLAYER_CONTRACT_STATUS_OFFSET) = 0;
    *(uint32_t*)(player + OOTP27_PLAYER_CONTRACT_START_YEAR_OFFSET) = 0u;
    memset(
        player + OOTP27_PLAYER_CONTRACT_SALARY_Y1_OFFSET,
        0,
        OOTP27_PLAYER_CONTRACT_SALARY_YEARS * sizeof(int32_t));
    kbo_clear_military_status_flags(player);
    kbo_set_military_days_left(player, 0);

    static volatile LONG invalid_release_log_count = 0;
    LONG slot = InterlockedIncrement(&invalid_release_log_count);
    if (slot <= 160) {
        kbo_log_runtimef(
            "KBO military invalid FA assignment released source=%s player=%u service_team=%u old_current_team=%u old_current_league=%u new_league=%u old_active_team=%u old_original_slot=%u removed=%d old_contract_level=%u old_contract_status=%d old_salary_y1=%d old_days_left=%d old_military_active=%u vector_off=0x%x",
            source != NULL ? source : "",
            player_id,
            service_team_id,
            old_current_team,
            old_current_league,
            fallback_league_id,
            old_active_team,
            old_original_slot_team,
            removed,
            (unsigned)old_contract_level,
            old_contract_status,
            old_salary_y1,
            old_military_days_left,
            (unsigned)old_military_active,
            vector_offset);
    }
    return 1;
}

