#include <stdint.h>

#include "team_org_assignment_query.h"
#include "../../lookup/team_lookup.h"
#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../runtime_memory/runtime_memory.h"
uint32_t kbo_org_team_id_for_team_id(uint32_t team_id)
{
    if (team_id == 0u) {
        return 0u;
    }

    uint8_t* team = find_kbo_team_by_numeric_id_any_league(team_id, 1);
    if (team != NULL && memory_range_readable(team + OOTP27_KBO_TEAM_PARENT_TEAM_ID_OFFSET, sizeof(uint32_t))) {
        uint32_t parent_team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_PARENT_TEAM_ID_OFFSET);
        if (parent_team_id != 0u) {
            return parent_team_id;
        }
    }
    return team_id;
}

static int kbo_team_id_matches_org_or_affiliate(uint32_t team_id, uint32_t org_team_id)
{
    if (team_id == 0u || org_team_id == 0u) {
        return 0;
    }
    if (team_id == org_team_id) {
        return 1;
    }
    return kbo_org_team_id_for_team_id(team_id) == org_team_id;
}

int kbo_team_ids_share_org(uint32_t left_team_id, uint32_t right_team_id)
{
    if (left_team_id == 0u || right_team_id == 0u) {
        return 0;
    }
    return kbo_org_team_id_for_team_id(left_team_id) == kbo_org_team_id_for_team_id(right_team_id);
}

int kbo_player_current_assignment_matches_team_or_affiliate(uint8_t* player, uint32_t team_id)
{
    if (player == NULL
            || team_id == 0u
            || !memory_range_readable(
                player,
                OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET + sizeof(uint32_t))) {
        return 0;
    }

    uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    if (current_team_id != 0u && kbo_team_id_matches_org_or_affiliate(current_team_id, team_id)) {
        return 1;
    }

    uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    if (active_team_id != 0u && kbo_team_id_matches_org_or_affiliate(active_team_id, team_id)) {
        return 1;
    }

    uint32_t loan_team_id = *(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET);
    if (loan_team_id != 0u && kbo_team_id_matches_org_or_affiliate(loan_team_id, team_id)) {
        return 1;
    }

    return 0;
}

