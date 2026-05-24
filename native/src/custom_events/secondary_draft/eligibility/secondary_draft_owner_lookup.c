#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../secondary_draft_internal.h"

#include <stdint.h>
#include <string.h>

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../team/assignment/org_query/team_org_assignment_query.h"

static int kbo_secondary_draft_team_index_by_org(
    const KboSecondaryDraftTeam* teams,
    int team_count,
    uint32_t team_id)
{
    if (team_id == 0u) {
        return -1;
    }
    for (int i = 0; i < team_count; i++) {
        if (teams[i].team_id == team_id || teams[i].org_team_id == team_id) {
            return i;
        }
    }
    uint32_t org_team_id = kbo_org_team_id_for_team_id(team_id);
    if (org_team_id == 0u || org_team_id == team_id) {
        return -1;
    }
    for (int i = 0; i < team_count; i++) {
        if (teams[i].team_id == org_team_id || teams[i].org_team_id == org_team_id) {
            return i;
        }
    }
    return -1;
}

int kbo_secondary_draft_owner_index_for_player(
    uint8_t* player,
    const KboSecondaryDraftTeam* teams,
    int team_count)
{
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return -1;
    }
    uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    int index = kbo_secondary_draft_team_index_by_org(teams, team_count, active_team_id);
    if (index >= 0) {
        return index;
    }

    uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    index = kbo_secondary_draft_team_index_by_org(teams, team_count, current_team_id);
    if (index >= 0) {
        return index;
    }

    uint32_t default_team_id = *(uint32_t*)(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET);
    return kbo_secondary_draft_team_index_by_org(teams, team_count, default_team_id);
}

static int kbo_secondary_draft_owner_map_add(
    KboSecondaryDraftTeamOwnerMapEntry* entries,
    int max_entries,
    int count,
    uint32_t team_id,
    int owner_index)
{
    if (entries == NULL || team_id == 0u || owner_index < 0) {
        return count;
    }
    for (int i = 0; i < count; i++) {
        if (entries[i].team_id == team_id) {
            entries[i].owner_index = owner_index;
            return count;
        }
    }
    if (count >= max_entries) {
        return count;
    }
    entries[count].team_id = team_id;
    entries[count].owner_index = owner_index;
    return count + 1;
}

static int kbo_secondary_draft_owner_index_for_org_id(
    const KboSecondaryDraftTeam* teams,
    int team_count,
    uint32_t org_team_id)
{
    for (int i = 0; i < team_count; i++) {
        if (teams[i].team_id == org_team_id || teams[i].org_team_id == org_team_id) {
            return i;
        }
    }
    return -1;
}

int kbo_secondary_draft_build_team_owner_map(
    const KboSecondaryDraftTeam* teams,
    int team_count,
    KboSecondaryDraftTeamOwnerMapEntry* entries,
    int max_entries)
{
    if (teams == NULL || team_count <= 0 || entries == NULL || max_entries <= 0) {
        return 0;
    }
    memset(entries, 0, (size_t)max_entries * sizeof(entries[0]));
    int count = 0;
    for (int i = 0; i < team_count; i++) {
        count = kbo_secondary_draft_owner_map_add(entries, max_entries, count, teams[i].team_id, i);
        count = kbo_secondary_draft_owner_map_add(entries, max_entries, count, teams[i].org_team_id, i);
    }

    uintptr_t global = get_ootp_global_database();
    if (global == 0u
            || !memory_range_readable((void*)(global + OOTP27_KBO_TEAM_COUNT_OFFSET), sizeof(int32_t))) {
        return count;
    }
    uintptr_t team_vector = *(uintptr_t*)(global + OOTP27_KBO_TEAM_VECTOR_OFFSET);
    int32_t global_team_count = *(int32_t*)(global + OOTP27_KBO_TEAM_COUNT_OFFSET);
    if (team_vector == 0u || global_team_count <= 0 || global_team_count > KBO_RUNTIME_MAX_TEAM_VECTOR_COUNT
            || !memory_range_readable((void*)team_vector, (SIZE_T)global_team_count * sizeof(uintptr_t))) {
        return count;
    }
    for (int32_t i = 0; i < global_team_count && count < max_entries; i++) {
        uintptr_t team_ptr = *(uintptr_t*)(team_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (team_ptr == 0u || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            continue;
        }
        uint8_t* team = (uint8_t*)team_ptr;
        if (team[OOTP27_KBO_TEAM_DELETED_OFFSET] != 0u) {
            continue;
        }
        uint32_t team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET);
        uint32_t parent_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_PARENT_TEAM_ID_OFFSET);
        int owner_index = kbo_secondary_draft_owner_index_for_org_id(teams, team_count, parent_id);
        if (owner_index >= 0) {
            count = kbo_secondary_draft_owner_map_add(entries, max_entries, count, team_id, owner_index);
        }
    }
    return count;
}

static int kbo_secondary_draft_owner_map_lookup(
    const KboSecondaryDraftTeamOwnerMapEntry* entries,
    int entry_count,
    uint32_t team_id)
{
    if (entries == NULL || team_id == 0u) {
        return -1;
    }
    for (int i = 0; i < entry_count; i++) {
        if (entries[i].team_id == team_id) {
            return entries[i].owner_index;
        }
    }
    return -1;
}

int kbo_secondary_draft_owner_index_for_player_from_map(
    uint8_t* player,
    const KboSecondaryDraftTeamOwnerMapEntry* entries,
    int entry_count)
{
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return -1;
    }
    int index = kbo_secondary_draft_owner_map_lookup(
        entries,
        entry_count,
        *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET));
    if (index >= 0) {
        return index;
    }
    index = kbo_secondary_draft_owner_map_lookup(
        entries,
        entry_count,
        *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET));
    if (index >= 0) {
        return index;
    }
    return kbo_secondary_draft_owner_map_lookup(
        entries,
        entry_count,
        *(uint32_t*)(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET));
}
