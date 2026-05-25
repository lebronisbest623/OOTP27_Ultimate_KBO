#ifndef KBOFIX_SRC_TEAM_ADD_PLAYER_GUARD_FOREIGN_POLICY_TEAM_ADD_PLAYER_GUARD_FORMER_ORG_POLICY_H_
#define KBOFIX_SRC_TEAM_ADD_PLAYER_GUARD_FOREIGN_POLICY_TEAM_ADD_PLAYER_GUARD_FORMER_ORG_POLICY_H_

#include <stdint.h>

int kbo_team_add_former_org_market_block_applies(
    uint32_t before_current_team_id,
    uint32_t before_active_team_id,
    uint32_t before_original_team_id,
    uint32_t team_league_id,
    uint32_t kbo_league_id,
    uint32_t source_org_team_id,
    uint32_t target_org_team_id,
    int rights_lookup_ready,
    int block_disabled);

#endif
