#include "team_roster_arrays.h"
#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/logging/core_log.h"
#include "../../../runtime_memory/runtime_memory.h"

/* Roster array helpers */

int kbo_remove_player_id_from_team_fixed_array(uint8_t* team, uint32_t array_offset, uint32_t player_id)
{
    if (team == NULL || player_id == 0 || !memory_range_readable(team + array_offset, OOTP27_TEAM_PLAYER_ID_ARRAY_COUNT * sizeof(uint32_t))) {
        return 0;
    }

    int removed = 0;
    uint32_t* ids = (uint32_t*)(team + array_offset);
    for (uint32_t i = 0; i < OOTP27_TEAM_PLAYER_ID_ARRAY_COUNT; i++) {
        if (ids[i] == player_id) {
            ids[i] = 0;
            removed++;
        }
    }
    return removed;
}

int kbo_team_fixed_array_contains_player(uint8_t* team, uint32_t array_offset, uint32_t player_id)
{
    if (team == NULL || player_id == 0 || !memory_range_readable(team + array_offset, OOTP27_TEAM_PLAYER_ID_ARRAY_COUNT * sizeof(uint32_t))) {
        return 0;
    }

    uint32_t* ids = (uint32_t*)(team + array_offset);
    for (uint32_t i = 0; i < OOTP27_TEAM_PLAYER_ID_ARRAY_COUNT; i++) {
        if (ids[i] == player_id) {
            return 1;
        }
    }
    return 0;
}

int kbo_add_player_id_to_team_fixed_array(uint8_t* team, uint32_t array_offset, uint32_t player_id)
{
    if (team == NULL || player_id == 0 || !memory_range_readable(team + array_offset, OOTP27_TEAM_PLAYER_ID_ARRAY_COUNT * sizeof(uint32_t))) {
        return 0;
    }

    uint32_t* ids = (uint32_t*)(team + array_offset);
    for (uint32_t i = 0; i < OOTP27_TEAM_PLAYER_ID_ARRAY_COUNT; i++) {
        if (ids[i] == player_id) {
            return 1;
        }
    }
    for (uint32_t i = 0; i < OOTP27_TEAM_PLAYER_ID_ARRAY_COUNT; i++) {
        if (ids[i] == 0) {
            ids[i] = player_id;
            return 1;
        }
    }
    return 0;
}

int kbo_team_roster_arrays_contain_player(uint8_t* team, uint32_t player_id)
{
    if (team == NULL || player_id == 0) {
        return 0;
    }

    return kbo_team_fixed_array_contains_player(team, OOTP27_TEAM_PLAYER_IDS_2760_OFFSET, player_id)
        || kbo_team_fixed_array_contains_player(team, OOTP27_TEAM_PLAYER_IDS_2A80_OFFSET, player_id)
        || kbo_team_fixed_array_contains_player(team, OOTP27_TEAM_PLAYER_IDS_2DA0_OFFSET, player_id)
        || kbo_team_fixed_array_contains_player(team, OOTP27_TEAM_RESTRICTED_PLAYER_IDS_OFFSET, player_id)
        || kbo_team_fixed_array_contains_player(team, OOTP27_TEAM_PLAYER_IDS_33E0_OFFSET, player_id)
        || kbo_team_fixed_array_contains_player(team, OOTP27_TEAM_PLAYER_IDS_3700_OFFSET, player_id);
}

int kbo_remove_player_id_from_team_active_roster_arrays(uint8_t* team, uint32_t player_id)
{
    if (team == NULL || player_id == 0) {
        return 0;
    }

    int removed = 0;
    removed += kbo_remove_player_id_from_team_fixed_array(team, OOTP27_TEAM_PLAYER_IDS_2760_OFFSET, player_id);
    removed += kbo_remove_player_id_from_team_fixed_array(team, OOTP27_TEAM_PLAYER_IDS_2A80_OFFSET, player_id);
    removed += kbo_remove_player_id_from_team_fixed_array(team, OOTP27_TEAM_PLAYER_IDS_2DA0_OFFSET, player_id);
    removed += kbo_remove_player_id_from_team_fixed_array(team, OOTP27_TEAM_PLAYER_IDS_33E0_OFFSET, player_id);
    removed += kbo_remove_player_id_from_team_fixed_array(team, OOTP27_TEAM_PLAYER_IDS_3700_OFFSET, player_id);
    return removed;
}

int kbo_remove_player_id_from_known_team_roster_arrays(uint8_t* team, uint32_t player_id)
{
    int removed = kbo_remove_player_id_from_team_active_roster_arrays(team, player_id);
    removed += kbo_remove_player_id_from_team_fixed_array(team, OOTP27_TEAM_RESTRICTED_PLAYER_IDS_OFFSET, player_id);
    return removed;
}

int kbo_add_player_id_to_team_assignment_arrays(uint8_t* team, uint32_t player_id)
{
    int added = 0;
    added += kbo_add_player_id_to_team_fixed_array(team, OOTP27_TEAM_PLAYER_IDS_2760_OFFSET, player_id);
    added += kbo_add_player_id_to_team_fixed_array(team, OOTP27_TEAM_PLAYER_IDS_2A80_OFFSET, player_id);
    return added;
}
