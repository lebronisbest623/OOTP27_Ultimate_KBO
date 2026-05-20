#include "team_add_player_guard_ai_roster_daily_internal.h"

static uint32_t kbo_ai_roster_daily_parent_team_id(uint32_t team_id)
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

static uint8_t* kbo_ai_roster_daily_resolve_active_team(uint8_t* player, uint32_t* out_team_id)
{
    if (out_team_id != NULL) {
        *out_team_id = 0u;
    }
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return NULL;
    }

    uint32_t team_ids[3] = {
        *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET),
        *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET),
        *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET),
    };
    uint32_t kbo_league_id = kbo_resolve_kbo_league_id();
    for (uint32_t i = 0u; i < 3u; i++) {
        uint32_t org_team_id = kbo_ai_roster_daily_parent_team_id(team_ids[i]);
        if (org_team_id == 0u) {
            continue;
        }

        uint8_t* team = find_kbo_team_by_numeric_id_any_league(org_team_id, 1);
        if (team == NULL || !memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            continue;
        }

        uint32_t team_league_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
        if (kbo_league_id != 0u && team_league_id != kbo_league_id) {
            continue;
        }

        if (out_team_id != NULL) {
            *out_team_id = org_team_id;
        }
        return team;
    }
    return NULL;
}

int kbo_ai_roster_daily_minor_callup_allows(int32_t team_arg, uint8_t* player, uint32_t* out_team_id)
{
    if (out_team_id != NULL) {
        *out_team_id = 0u;
    }
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    uint32_t kbo_league_id = kbo_resolve_kbo_league_id();
    uint32_t current_league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
    int minor_league_match = kbo_league_id != 0u
        && (current_league_id == kbo_league_id + 1u || (uint32_t)team_arg == kbo_league_id + 1u);
    if (!minor_league_match) {
        return 0;
    }

    uint32_t active_team_id = 0u;
    uint8_t* active_team = kbo_ai_roster_daily_resolve_active_team(player, &active_team_id);
    if (active_team == NULL) {
        return 0;
    }

    uint8_t allowed = kbo_custom_foreign_policy_callup_allows(
        (uintptr_t)active_team,
        (uintptr_t)player,
        0,
        (int32_t)KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT,
        3);
    if (out_team_id != NULL) {
        *out_team_id = active_team_id;
    }
    return allowed ? 1 : 0;
}
