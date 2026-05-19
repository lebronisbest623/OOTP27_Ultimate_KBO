#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>

#include "../internal/team_add_player_guard_internal.h"
#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../core/logging/core_log.h"
#include "../../../foreign/common/policy/foreign_waiver_policy.h"
#include "../../../foreign/common/player_eval/foreign_waiver_player_eval.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../assignment/roster_arrays/team_roster_arrays.h"
#include "../../lookup/team_lookup.h"

void kbo_team_add_normalize_foreign_retention_contract_success(
    uint32_t caller_rva,
    uintptr_t team_ptr,
    uintptr_t player_ptr,
    uint32_t before_current_team_id,
    uint32_t before_active_team_id,
    uint32_t before_original_team_id)
{
    if (!kbo_fix_enabled()
            || !kbo_custom_foreign_policy_enabled()
            || !kbo_team_add_known_foreign_market_minor_caller(caller_rva)
            || before_current_team_id != 0u
            || before_active_team_id != 0u
            || team_ptr == 0
            || player_ptr == 0
            || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)
            || !kbo_player_pointer_plausible(player_ptr)
            || !memory_range_readable((void*)player_ptr, OOTP27_PLAYER_SCAN_BYTES)) {
        return;
    }

    uint8_t* team = (uint8_t*)team_ptr;
    uint8_t* player = (uint8_t*)player_ptr;
    if (!kbo_player_is_foreign_for_kbo_rights(player)) {
        return;
    }

    uint32_t kbo_league_id = kbo_resolve_kbo_league_id();
    uint32_t team_league_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
    uint32_t team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET);
    if (kbo_league_id == 0u || team_league_id != kbo_league_id || team_id == 0u) {
        return;
    }

    uint32_t after_current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t after_active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    if (after_current_team_id != team_id || after_active_team_id != team_id) {
        return;
    }

    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    uint32_t old_original_team_id = memory_range_readable(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET, sizeof(uint32_t))
        ? *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET)
        : 0u;
    uint32_t old_default_team_id = memory_range_readable(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET, sizeof(uint32_t))
        ? *(uint32_t*)(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET)
        : 0u;
    uint8_t old_contract_level = player[OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET];
    uint8_t old_restricted = player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET];
    uint8_t old_secondary = player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET];
    uint8_t old_dfa = player[OOTP27_PLAYER_DFA_FLAG_OFFSET];
    int changed = 0;

    if (old_contract_level != 1u) {
        player[OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET] = 1u;
        changed = 1;
    }
    if (player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET] != 0u) {
        player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET] = 0u;
        changed = 1;
    }
    if (player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET] != 0u) {
        player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET] = 0u;
        changed = 1;
    }
    if (player[OOTP27_PLAYER_DFA_FLAG_OFFSET] != 0u) {
        player[OOTP27_PLAYER_DFA_FLAG_OFFSET] = 0u;
        changed = 1;
    }
    int removed_restricted = kbo_remove_player_id_from_team_fixed_array(
        team,
        OOTP27_TEAM_RESTRICTED_PLAYER_IDS_OFFSET,
        player_id);
    if (removed_restricted > 0) {
        changed = 1;
    }
    if (*(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET) != team_league_id) {
        *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET) = team_league_id;
        changed = 1;
    }
    if (*(uint32_t*)(player + OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET) != team_league_id) {
        *(uint32_t*)(player + OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET) = team_league_id;
        changed = 1;
    }
    if (old_original_team_id == 0u
            && memory_range_readable(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET, sizeof(uint32_t))) {
        *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET) = team_id;
        changed = 1;
    }
    if (old_default_team_id == 0u
            && memory_range_readable(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET, sizeof(uint32_t))) {
        *(uint32_t*)(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET) = team_id;
        changed = 1;
    }
    kbo_add_player_id_to_team_assignment_arrays(team, player_id);

    if (!changed) {
        return;
    }

    static volatile LONG normalize_log_count = 0;
    LONG slot = InterlockedIncrement(&normalize_log_count);
    if (slot <= 200) {
        kbo_log_runtimef(
            "foreign no-minor team-add cleanup applied player=%u team=%u league=%u caller_rva=0x%x before_current=%u before_active=%u before_original=%u old_original=%u old_default=%u after_current=%u after_active=%u after_original=%u after_default=%u old_contract_level=%u after_contract_level=%u old_restricted=%u old_secondary=%u old_dfa=%u after_restricted=%u after_secondary=%u after_dfa=%u removed_restricted=%d",
            player_id,
            team_id,
            team_league_id,
            caller_rva,
            before_current_team_id,
            before_active_team_id,
            before_original_team_id,
            old_original_team_id,
            old_default_team_id,
            after_current_team_id,
            after_active_team_id,
            memory_range_readable(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET, sizeof(uint32_t))
                ? *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET)
                : 0u,
            memory_range_readable(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET, sizeof(uint32_t))
                ? *(uint32_t*)(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET)
                : 0u,
            (uint32_t)old_contract_level,
            (uint32_t)player[OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET],
            (uint32_t)old_restricted,
            (uint32_t)old_secondary,
            (uint32_t)old_dfa,
            (uint32_t)player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET],
            (uint32_t)player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET],
            (uint32_t)player[OOTP27_PLAYER_DFA_FLAG_OFFSET],
            removed_restricted);
    }
}
