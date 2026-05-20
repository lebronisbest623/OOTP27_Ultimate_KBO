#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../core/logging/core_log.h"
#include "../../../core/files/save_paths/core_save_paths.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../team/lookup/team_lookup.h"
#include "../../../team/assignment/roster_arrays/team_roster_arrays.h"
#include "../../waiver_core/api/foreign_waiver_core.h"
#include "../../common/dates/foreign_waiver_date.h"
#include "../api/foreign_waiver_decisions.h"
#include "../../common/paths/foreign_waiver_paths.h"
#include "../../common/player_eval/foreign_waiver_player_eval.h"
#include "../../common/policy/foreign_waiver_policy.h"
#include "../../rights/query/foreign_waiver_rights_query.h"
#include "../internal/foreign_waiver_decisions_state_internal.h"
#include "../internal/foreign_waiver_decisions_team_internal.h"

KboLock g_kbo_foreign_waiver_decision_lock = KBO_LOCK_INIT;

uint32_t kbo_get_player_original_team_id(uint8_t* player)
{
    if (player == NULL || !memory_range_readable(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET, sizeof(uint32_t))) {
        return 0;
    }
    return *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET);
}

uint32_t kbo_get_foreign_waiver_decision_team_id(uint8_t* player)
{
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    /*
     * OOTP saves can carry an active_team_id for foreign players at league start.
     * That is only the club with priority to decide during the KBO reserve-rights
     * event, not an already exercised reserve right. Stored rights are handled by
     * kbo_state.sqlite3 foreign_reserve_rights table; candidate ownership should
     * appear only while the negotiation window is open.
     */
    if (!kbo_is_foreign_waiver_negotiation_window_open()) {
        return 0;
    }

    uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    if (active_team_id != 0u) {
        return active_team_id;
    }

    uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    if (current_team_id != 0u) {
        return current_team_id;
    }

    return 0u;
}

int kbo_original_club_priority_window_allows(uint8_t* player, uint32_t team_id, const char* action_name)
{
    if (!kbo_is_foreign_waiver_negotiation_window_open()) {
        return 1;
    }

    uint32_t priority_team_id = kbo_get_foreign_waiver_decision_team_id(player);
    if (priority_team_id == 0 || team_id == priority_team_id) {
        return 1;
    }

    uint32_t player_id = 0;
    if (player != NULL && memory_range_readable(player + OOTP27_PLAYER_ID_OFFSET, sizeof(uint32_t))) {
        player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    }
    kbo_log_runtimef(
        "foreign priority negotiation: blocked action=%s team=%u player=%u priority_team=%u",
        action_name == NULL ? "" : action_name,
        team_id,
        player_id,
        priority_team_id);
    return 0;
}

