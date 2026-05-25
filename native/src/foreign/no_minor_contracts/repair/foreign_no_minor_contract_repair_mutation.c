#include "foreign_no_minor_contract_repair.h"

#include <string.h>

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../team/assignment/roster_arrays/team_roster_arrays.h"

static int kbo_foreign_no_minor_team_id_belongs_to_parent_org(
    uint32_t team_id,
    uint32_t parent_team_id,
    uint32_t affiliate_team_id)
{
    return team_id == 0u || team_id == parent_team_id || team_id == affiliate_team_id;
}

static uint32_t kbo_foreign_no_minor_read_player_default_team(uint8_t* player)
{
    return memory_range_readable(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET, sizeof(uint32_t))
        ? *(uint32_t*)(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET)
        : 0u;
}

static void kbo_foreign_no_minor_fill_after_result(
    uint8_t* player,
    KboForeignNoMinorContractRepairResult* result)
{
    if (player == NULL || result == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return;
    }

    result->after_current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    result->after_active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    result->after_original_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET);
    result->after_default_team_id = kbo_foreign_no_minor_read_player_default_team(player);
    result->after_current_league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
    result->after_draft_league_id = *(uint32_t*)(player + OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET);
    result->after_original_league_id = *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_LEAGUE_ID_OFFSET);
    result->after_contract_level = player[OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET];
    result->after_restricted = player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET];
    result->after_secondary_restricted = player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET];
    result->after_dfa = player[OOTP27_PLAYER_DFA_FLAG_OFFSET];
}

int kbo_foreign_no_minor_contract_repair_affiliate_assignment(
    uint8_t* player,
    uint8_t* affiliate_team,
    uint8_t* parent_team,
    KboForeignNoMinorContractRepairResult* out_result)
{
    if (out_result != NULL) {
        memset(out_result, 0, sizeof(*out_result));
    }
    if (player == NULL || affiliate_team == NULL || parent_team == NULL
            || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)
            || !memory_range_readable(affiliate_team, OOTP27_KBO_TEAM_READABLE_BYTES)
            || !memory_range_readable(parent_team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return 0;
    }

    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    uint32_t affiliate_team_id = *(uint32_t*)(affiliate_team + OOTP27_KBO_TEAM_ID_OFFSET);
    uint32_t affiliate_league_id = *(uint32_t*)(affiliate_team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
    uint32_t affiliate_parent_team_id = *(uint32_t*)(affiliate_team + OOTP27_KBO_TEAM_PARENT_TEAM_ID_OFFSET);
    uint32_t parent_team_id = *(uint32_t*)(parent_team + OOTP27_KBO_TEAM_ID_OFFSET);
    uint32_t parent_league_id = *(uint32_t*)(parent_team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);

    uint32_t before_current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t before_active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    uint32_t before_original_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET);
    uint32_t before_default_team_id = kbo_foreign_no_minor_read_player_default_team(player);
    uint32_t before_current_league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
    uint32_t before_draft_league_id = *(uint32_t*)(player + OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET);
    uint32_t before_original_league_id = *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_LEAGUE_ID_OFFSET);

    if (out_result != NULL) {
        out_result->player_id = player_id;
        out_result->affiliate_team_id = affiliate_team_id;
        out_result->parent_team_id = parent_team_id;
        out_result->before_current_team_id = before_current_team_id;
        out_result->before_active_team_id = before_active_team_id;
        out_result->before_original_team_id = before_original_team_id;
        out_result->before_default_team_id = before_default_team_id;
        out_result->before_current_league_id = before_current_league_id;
        out_result->before_draft_league_id = before_draft_league_id;
        out_result->before_original_league_id = before_original_league_id;
        out_result->before_contract_level = player[OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET];
        out_result->before_restricted = player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET];
        out_result->before_secondary_restricted = player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET];
        out_result->before_dfa = player[OOTP27_PLAYER_DFA_FLAG_OFFSET];
    }

    if (player_id == 0u
            || affiliate_team_id == 0u
            || parent_team_id == 0u
            || affiliate_league_id == 0u
            || parent_league_id == 0u
            || affiliate_league_id == parent_league_id
            || affiliate_parent_team_id != parent_team_id
            || before_current_team_id != affiliate_team_id
            || before_current_league_id != affiliate_league_id) {
        kbo_foreign_no_minor_fill_after_result(player, out_result);
        return 0;
    }

    if (player[OOTP27_PLAYER_RETIRED_FLAG_OFFSET] != 0u
            || player[OOTP27_PLAYER_MILITARY_ACTIVE_OFFSET] != 0u
            || player[OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET] != 0u
            || *(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET) != 0u) {
        kbo_foreign_no_minor_fill_after_result(player, out_result);
        return 0;
    }

    if (!kbo_foreign_no_minor_team_id_belongs_to_parent_org(before_active_team_id, parent_team_id, affiliate_team_id)
            || !kbo_foreign_no_minor_team_id_belongs_to_parent_org(before_original_team_id, parent_team_id, affiliate_team_id)
            || !kbo_foreign_no_minor_team_id_belongs_to_parent_org(before_default_team_id, parent_team_id, affiliate_team_id)) {
        kbo_foreign_no_minor_fill_after_result(player, out_result);
        return 0;
    }

    int changed = 0;
    int parent_had_assignment =
        kbo_team_fixed_array_contains_player(parent_team, OOTP27_TEAM_PLAYER_IDS_2760_OFFSET, player_id)
        && kbo_team_fixed_array_contains_player(parent_team, OOTP27_TEAM_PLAYER_IDS_2A80_OFFSET, player_id);

    int removed_affiliate = kbo_remove_player_id_from_known_team_roster_arrays(affiliate_team, player_id);
    int removed_parent_restricted = kbo_remove_player_id_from_team_fixed_array(
        parent_team,
        OOTP27_TEAM_RESTRICTED_PLAYER_IDS_OFFSET,
        player_id);
    if (removed_affiliate > 0 || removed_parent_restricted > 0) {
        changed = 1;
    }

    if (before_current_team_id != parent_team_id) {
        *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET) = parent_team_id;
        changed = 1;
    }
    if (before_active_team_id != parent_team_id) {
        *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET) = parent_team_id;
        changed = 1;
    }
    if (before_current_league_id != parent_league_id) {
        *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET) = parent_league_id;
        changed = 1;
    }
    if (before_draft_league_id != parent_league_id) {
        *(uint32_t*)(player + OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET) = parent_league_id;
        changed = 1;
    }
    if (before_original_league_id == 0u || before_original_league_id == affiliate_league_id) {
        if (before_original_league_id != parent_league_id) {
            *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_LEAGUE_ID_OFFSET) = parent_league_id;
            changed = 1;
        }
    }
    if (before_original_team_id == 0u || before_original_team_id == affiliate_team_id) {
        if (before_original_team_id != parent_team_id) {
            *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET) = parent_team_id;
            changed = 1;
        }
    }
    if ((before_default_team_id == 0u || before_default_team_id == affiliate_team_id)
            && memory_range_readable(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET, sizeof(uint32_t))) {
        if (before_default_team_id != parent_team_id) {
            *(uint32_t*)(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET) = parent_team_id;
            changed = 1;
        }
    }
    if (player[OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET] != 1u) {
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

    int added_parent_assignment = kbo_add_player_id_to_team_assignment_arrays(parent_team, player_id);
    int parent_has_assignment_after =
        kbo_team_fixed_array_contains_player(parent_team, OOTP27_TEAM_PLAYER_IDS_2760_OFFSET, player_id)
        && kbo_team_fixed_array_contains_player(parent_team, OOTP27_TEAM_PLAYER_IDS_2A80_OFFSET, player_id);
    if (!parent_had_assignment && parent_has_assignment_after) {
        changed = 1;
    }

    if (out_result != NULL) {
        out_result->removed_affiliate_arrays = removed_affiliate;
        out_result->removed_parent_restricted = removed_parent_restricted;
        out_result->added_parent_assignment_arrays = added_parent_assignment;
        out_result->changed = changed;
    }
    kbo_foreign_no_minor_fill_after_result(player, out_result);
    return changed;
}
