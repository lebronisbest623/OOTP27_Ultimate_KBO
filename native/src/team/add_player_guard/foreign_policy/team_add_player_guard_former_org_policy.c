#include "team_add_player_guard_former_org_policy.h"

int kbo_team_add_former_org_market_block_applies(
    uint32_t before_current_team_id,
    uint32_t before_active_team_id,
    uint32_t before_original_team_id,
    uint32_t team_league_id,
    uint32_t kbo_league_id,
    uint32_t source_org_team_id,
    uint32_t target_org_team_id,
    int rights_lookup_ready,
    int block_disabled)
{
    /* Once rights are queryable, active reserve-right ownership is enforced upstream. */
    if (block_disabled || rights_lookup_ready) {
        return 0;
    }
    if (before_current_team_id != 0u
            || before_active_team_id != 0u
            || before_original_team_id == 0u) {
        return 0;
    }
    if (team_league_id == 0u || kbo_league_id == 0u || team_league_id != kbo_league_id) {
        return 0;
    }
    if (source_org_team_id == 0u
            || target_org_team_id == 0u
            || source_org_team_id == target_org_team_id) {
        return 0;
    }
    return 1;
}
