#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../../team/lookup/team_lookup.h"
#include "../../common/dates/foreign_waiver_date.h"
#include "../../common/player_eval/foreign_waiver_player_eval.h"
#include "../../common/policy/foreign_waiver_policy.h"
#include "../../rights/query/foreign_waiver_rights_query.h"
#include "../api/foreign_waiver_core.h"

int kbo_resolve_foreign_waiver_top_candidate_for_team(
    uint32_t team_id,
    uint32_t* out_player_id,
    uint32_t* out_current_team_id)
{
    if (team_id == 0u || out_player_id == NULL || out_current_team_id == NULL) {
        return 0;
    }
    *out_player_id = 0u;
    *out_current_team_id = 0u;
    {
        static LONG rights_loaded = 0;
        if (InterlockedCompareExchange(&rights_loaded, 1, 0) == 0) {
            kbo_load_foreign_waiver_rights();
        }
    }

    uint32_t today = 0u;
    if (!kbo_get_foreign_waiver_current_yyyymmdd(&today)) {
        return 0;
    }
    uint32_t configured_league_id = kbo_resolve_kbo_league_id();

    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        return 0;
    }

    int best_score = -1;
    uint32_t best_player_id = 0u;
    uint32_t best_current_team_id = 0u;
    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }
        uint8_t* player = (uint8_t*)player_ptr;
        uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        uint32_t decision_team_id = kbo_get_foreign_waiver_decision_team_id(player);
        uint32_t current_league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
        if (player_id == 0u || decision_team_id != team_id
                || (current_league_id != 0u && current_league_id != configured_league_id)
                || !kbo_player_is_foreign_for_kbo_rights(player)) {
            continue;
        }

        int score = kbo_foreign_waiver_value_score(player);
        if (kbo_has_active_foreign_waiver_right(team_id, player_id, today)) {
            continue;
        }

        if (score > best_score) {
            best_score = score;
            best_player_id = player_id;
            best_current_team_id = current_team_id;
        }
    }

    if (best_player_id == 0u) {
        return 0;
    }
    *out_player_id = best_player_id;
    *out_current_team_id = best_current_team_id;
    return 1;
}

