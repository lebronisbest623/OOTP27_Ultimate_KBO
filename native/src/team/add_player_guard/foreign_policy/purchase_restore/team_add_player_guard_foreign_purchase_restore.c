#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>

#include "../../../../bootstrap/abi/ootp_offsets.h"
#include "../../../../core/dates/tick/current_date_tick_capture.h"
#include "../../../../core/logging/core_log.h"
#include "../../../../foreign/common/dates/foreign_waiver_date.h"
#include "../../../../foreign/common/player_eval/foreign_waiver_player_eval.h"
#include "../../../../foreign/retention_guard/foreign_retention_guard.h"
#include "../../../../foreign/rights/query/foreign_waiver_rights_query.h"
#include "../../../../runtime_memory/runtime_memory.h"
#include "../../../assignment/assignment/team_assignment.h"
#include "../../../assignment/roster_arrays/team_roster_arrays.h"
#include "../../../lookup/team_lookup.h"
#include "team_add_player_guard_foreign_purchase_restore.h"

static int kbo_team_add_restore_same_league_active_contract_after_blocked_add(
    uint8_t* player,
    uint8_t* source_team,
    uint8_t* blocked_team,
    uint32_t source_team_id,
    uint32_t source_league_id,
    uint32_t blocked_team_id,
    uint32_t blocked_league_id,
    uint32_t caller_rva)
{
    if (player == NULL
            || source_team == NULL
            || blocked_team == NULL
            || source_team_id == 0u
            || blocked_team_id == 0u
            || source_team_id == blocked_team_id
            || source_league_id == 0u
            || source_league_id != blocked_league_id
            || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)
            || !memory_range_readable(source_team, OOTP27_KBO_TEAM_READABLE_BYTES)
            || !memory_range_readable(blocked_team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return 0;
    }

    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    uint32_t before_current = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t before_active = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    uint32_t before_original = *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET);
    uint32_t before_default = memory_range_readable(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET, sizeof(uint32_t))
        ? *(uint32_t*)(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET)
        : 0u;
    uint32_t before_league = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
    uint32_t before_draft = *(uint32_t*)(player + OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET);
    uint8_t before_contract_level = player[OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET];
    uint8_t before_restricted = player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET];
    uint8_t before_secondary = player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET];
    uint8_t before_dfa = player[OOTP27_PLAYER_DFA_FLAG_OFFSET];

    if (player_id == 0u
            || before_current != 0u
            || before_active != 0u
            || before_original != source_team_id
            || before_contract_level != 1u) {
        return 0;
    }

    int removed_blocked = kbo_remove_player_id_from_known_team_roster_arrays(blocked_team, player_id);
    int called_pre_change = 0;
    int called_register = 0;
    int called_attach = 0;
    kbo_assign_player_to_team_internal(
        player,
        source_team,
        source_league_id,
        1,
        &called_pre_change,
        &called_register,
        &called_attach);

    player[OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET] = 1u;
    player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET] = 0u;
    player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET] = 0u;
    player[OOTP27_PLAYER_DFA_FLAG_OFFSET] = 0u;
    if (memory_range_readable(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET, sizeof(uint32_t))
            && *(uint32_t*)(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET) == 0u) {
        *(uint32_t*)(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET) = source_team_id;
    }
    int added_source = kbo_add_player_id_to_team_assignment_arrays(source_team, player_id);

    uint32_t after_current = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t after_active = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    uint32_t after_original = *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET);
    if (after_current != source_team_id || after_active != source_team_id) {
        kbo_log_runtimef(
            "custom foreign policy blocked same-league active-contract source restore failed player=%u blocked_team=%u source_team=%u source_league=%u caller_rva=0x%x before_current=%u before_active=%u before_original=%u before_default=%u before_league=%u before_draft=%u before_level=%u before_restricted=%u before_secondary=%u before_dfa=%u after_current=%u after_active=%u after_original=%u removed_blocked=%d added_source=%d pre=%d register=%d attach=%d",
            player_id,
            blocked_team_id,
            source_team_id,
            source_league_id,
            caller_rva,
            before_current,
            before_active,
            before_original,
            before_default,
            before_league,
            before_draft,
            (uint32_t)before_contract_level,
            (uint32_t)before_restricted,
            (uint32_t)before_secondary,
            (uint32_t)before_dfa,
            after_current,
            after_active,
            after_original,
            removed_blocked,
            added_source,
            called_pre_change,
            called_register,
            called_attach);
        return 0;
    }

    kbo_log_runtimef(
        "custom foreign policy blocked same-league active-contract source restored player=%u blocked_team=%u source_team=%u source_league=%u caller_rva=0x%x before_current=%u before_active=%u before_original=%u before_default=%u before_league=%u before_draft=%u before_level=%u before_restricted=%u before_secondary=%u before_dfa=%u after_current=%u after_active=%u after_original=%u after_default=%u after_level=%u after_restricted=%u after_secondary=%u after_dfa=%u removed_blocked=%d added_source=%d pre=%d register=%d attach=%d",
        player_id,
        blocked_team_id,
        source_team_id,
        source_league_id,
        caller_rva,
        before_current,
        before_active,
        before_original,
        before_default,
        before_league,
        before_draft,
        (uint32_t)before_contract_level,
        (uint32_t)before_restricted,
        (uint32_t)before_secondary,
        (uint32_t)before_dfa,
        after_current,
        after_active,
        after_original,
        memory_range_readable(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET, sizeof(uint32_t))
            ? *(uint32_t*)(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET)
            : 0u,
        (uint32_t)player[OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET],
        (uint32_t)player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET],
        (uint32_t)player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET],
        (uint32_t)player[OOTP27_PLAYER_DFA_FLAG_OFFSET],
        removed_blocked,
        added_source,
        called_pre_change,
        called_register,
        called_attach);
    return 1;
}

static int kbo_team_add_restore_active_foreign_right_after_blocked_purchase(
    uint8_t* player,
    uint8_t* source_team,
    uint32_t source_team_id,
    uint32_t source_league_id,
    uint32_t blocked_team_id,
    uint32_t caller_rva,
    uint32_t today)
{
    if (player == NULL || source_team == NULL || today == 0u
            || source_team_id == 0u
            || source_league_id == 0u
            || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)
            || !memory_range_readable(source_team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return 0;
    }

    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    uint32_t holder_team_id = 0u;
    if (player_id == 0u
            || !kbo_find_active_foreign_waiver_holder(player_id, today, &holder_team_id)
            || holder_team_id != source_team_id) {
        return 0;
    }

    uint32_t before_current = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t before_active = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    uint32_t before_original = *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET);
    uint32_t before_default = memory_range_readable(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET, sizeof(uint32_t))
        ? *(uint32_t*)(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET)
        : 0u;
    uint32_t before_league = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
    uint32_t before_draft = *(uint32_t*)(player + OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET);
    uint8_t before_restricted = player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET];
    uint8_t before_secondary = player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET];
    uint8_t before_contract_level = player[OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET];
    uint8_t before_dfa = player[OOTP27_PLAYER_DFA_FLAG_OFFSET];

    int removed_roster = kbo_remove_player_id_from_known_team_roster_arrays(source_team, player_id);

    *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET) = 0u;
    *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET) = 0u;
    if (*(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET) == source_team_id) {
        *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET) = 0u;
    }
    if (*(uint32_t*)(player + OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET) == source_league_id) {
        *(uint32_t*)(player + OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET) = 0u;
    }
    if (*(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET) == 0u) {
        *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET) = source_team_id;
    }
    if (*(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_LEAGUE_ID_OFFSET) == 0u) {
        *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_LEAGUE_ID_OFFSET) = source_league_id;
    }
    if (memory_range_readable(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET, sizeof(uint32_t))
            && before_default != 0u
            && before_default != source_team_id) {
        *(uint32_t*)(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET) = 0u;
    }
    player[OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET] = 0u;
    player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET] = 1u;
    player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET] = 1u;
    player[OOTP27_PLAYER_DFA_FLAG_OFFSET] = 0u;
    int added_restricted = kbo_add_player_id_to_team_fixed_array(
        source_team,
        OOTP27_TEAM_RESTRICTED_PLAYER_IDS_OFFSET,
        player_id);

    kbo_log_runtimef(
        "custom foreign policy blocked reserve-right restored player=%u blocked_team=%u holder_team=%u holder_league=%u today=%u caller_rva=0x%x before_current=%u before_active=%u before_original=%u before_default=%u before_league=%u before_draft=%u before_level=%u before_restricted=%u before_secondary=%u before_dfa=%u after_current=%u after_active=%u after_original=%u after_default=%u after_league=%u after_draft=%u after_level=%u after_restricted=%u after_secondary=%u after_dfa=%u removed_roster=%d added_restricted=%d",
        player_id,
        blocked_team_id,
        source_team_id,
        source_league_id,
        today,
        caller_rva,
        before_current,
        before_active,
        before_original,
        before_default,
        before_league,
        before_draft,
        (uint32_t)before_contract_level,
        (uint32_t)before_restricted,
        (uint32_t)before_secondary,
        (uint32_t)before_dfa,
        *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET),
        *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET),
        *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET),
        memory_range_readable(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET, sizeof(uint32_t))
            ? *(uint32_t*)(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET)
            : 0u,
        *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET),
        *(uint32_t*)(player + OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET),
        (uint32_t)player[OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET],
        (uint32_t)player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET],
        (uint32_t)player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET],
        (uint32_t)player[OOTP27_PLAYER_DFA_FLAG_OFFSET],
        removed_roster,
        added_restricted);
    return 1;
}

int kbo_team_add_restore_source_team_after_blocked_foreign_purchase(
    uint8_t* player,
    uint32_t source_team_id,
    uint32_t blocked_team_id,
    uint32_t caller_rva)
{
    if (player == NULL
            || source_team_id == 0u
            || blocked_team_id == 0u
            || source_team_id == blocked_team_id
            || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    if (player_id == 0u
            || current_team_id != 0u
            || active_team_id != 0u
            || !kbo_player_is_foreign_for_kbo_rights(player)) {
        return 0;
    }

    uint8_t* source_team = find_kbo_team_by_numeric_id_any_league(source_team_id, 1);
    if (source_team == NULL
            || !memory_range_readable(source_team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return 0;
    }

    uint32_t source_league_id = *(uint32_t*)(source_team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
    uint8_t* blocked_team = find_kbo_team_by_numeric_id_any_league(blocked_team_id, 1);
    uint32_t blocked_league_id = blocked_team != NULL
            && memory_range_readable(blocked_team, OOTP27_KBO_TEAM_READABLE_BYTES)
        ? *(uint32_t*)(blocked_team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET)
        : 0u;
    if (source_league_id != 0u
            && blocked_league_id != 0u
            && source_league_id == blocked_league_id) {
        return kbo_team_add_restore_same_league_active_contract_after_blocked_add(
            player,
            source_team,
            blocked_team,
            source_team_id,
            source_league_id,
            blocked_team_id,
            blocked_league_id,
            caller_rva);
    }

    uint32_t today = 0u;
    if (kbo_current_date_tick_latest_published_date(&today) && today != 0u) {
        if (kbo_foreign_retention_guard_restore_recorded_holder_signing(
                "blocked_foreign_purchase",
                player,
                source_team,
                today)) {
            return 1;
        }
        if (kbo_team_add_restore_active_foreign_right_after_blocked_purchase(
                player,
                source_team,
                source_team_id,
                source_league_id,
                blocked_team_id,
                caller_rva,
                today)) {
            return 1;
        }
    }

    kbo_log_runtimef(
        "custom foreign policy blocked purchase source restore skipped player=%u blocked_team=%u source_team=%u source_league=%u caller_rva=0x%x reason=no_recorded_holder_or_active_right current=%u active=%u original=%u",
        player_id,
        blocked_team_id,
        source_team_id,
        source_league_id,
        caller_rva,
        *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET),
        *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET),
        *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET));
    return 0;
}
