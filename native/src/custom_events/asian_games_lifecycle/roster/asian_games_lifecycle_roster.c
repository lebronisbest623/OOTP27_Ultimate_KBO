#include "../../runtime/common/custom_events_common.h"
#include "asian_games_lifecycle_roster.h"
#include <stdio.h>
#include <string.h>
#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/logging/core_log.h"
#include "../../../core/dates/core_current_date.h"
#include "../../../core/files/save_paths/core_save_paths.h"
#include "../../../core/dates/core_text_date.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../team/lookup/team_lookup.h"
#include "../../asian_games/player_eval/asian_games_player_eligibility.h"

/* Asian Games roster lifecycle query helpers. */

int kbo_asian_games_roster_contains_player(uint32_t player_id)
{
    if (player_id == 0u) {
        return 0;
    }
    LONG roster_count = g_kbo_asian_games_roster_count;
    if (roster_count < 0 || roster_count > KBO_ASIAN_GAMES_ROSTER_SIZE) {
        roster_count = 0;
    }
    for (LONG i = 0; i < roster_count; i++) {
        if (g_kbo_asian_games_roster[i].player_id == player_id) {
            return 1;
        }
    }
    return 0;
}

int kbo_asian_games_roster_wildcard_count_except(LONG except_index)
{
    int count = 0;
    LONG roster_count = g_kbo_asian_games_roster_count;
    if (roster_count < 0 || roster_count > KBO_ASIAN_GAMES_ROSTER_SIZE) {
        roster_count = 0;
    }
    for (LONG i = 0; i < roster_count; i++) {
        if (i != except_index && g_kbo_asian_games_roster[i].wildcard != 0u) {
            count++;
        }
    }
    return count;
}

int kbo_asian_games_replacement_allowed_for_org(uint32_t new_org_id, uint32_t old_org_id, LONG old_index)
{
    if (new_org_id == 0u || new_org_id == old_org_id) {
        return 1;
    }

    int count = 0;
    LONG roster_count = g_kbo_asian_games_roster_count;
    if (roster_count < 0 || roster_count > KBO_ASIAN_GAMES_ROSTER_SIZE) {
        roster_count = 0;
    }
    for (LONG i = 0; i < roster_count; i++) {
        if (i == old_index) {
            continue;
        }
        uint32_t roster_org_id = kbo_asian_games_org_team_id_for_team(g_kbo_asian_games_roster[i].original_team_id);
        if (roster_org_id == new_org_id) {
            count++;
        }
    }
    return count < kbo_asian_games_roster_policy()->team_max_players;
}

int kbo_asian_games_player_unavailable_for_departure(uint8_t* player)
{
    if (player == NULL || !kbo_player_pointer_plausible((uintptr_t)player)) {
        return 1;
    }
    return !kbo_asian_games_player_status_allows_selection(player);
}
