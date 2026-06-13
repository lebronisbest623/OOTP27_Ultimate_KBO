#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../secondary_draft_internal.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../team/lookup/team_lookup.h"
#include "../../../team/names/team_string.h"

static int kbo_secondary_draft_team_name_placeholder(const char* text)
{
    return text == NULL
        || text[0] == '\0'
        || _stricmp(text, "Team") == 0
        || _stricmp(text, "Unknown") == 0;
}

void kbo_secondary_draft_copy_team_name(uint8_t* team, uint32_t team_id, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';

    if (team == NULL) {
        team = find_kbo_team_by_numeric_id_any_league(team_id, 1);
    }
    if (team != NULL && memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        char city[64] = {0};
        char nickname[64] = {0};
        char full_name[96] = {0};
        copy_ootp_string_object_text(team, OOTP27_KBO_TEAM_CITY_STRING_OFFSET, city, sizeof(city));
        copy_ootp_string_object_text(team, OOTP27_KBO_TEAM_NICKNAME_STRING_OFFSET, nickname, sizeof(nickname));
        copy_ootp_string_object_text(team, OOTP27_KBO_TEAM_FULL_NAME_STRING_OFFSET, full_name, sizeof(full_name));

        if (!kbo_secondary_draft_team_name_placeholder(full_name)
                && (strchr(full_name, ' ') != NULL || kbo_ootp_text_has_non_ascii(full_name))) {
            snprintf(out, out_size, "%s", full_name);
            return;
        }
        if (!kbo_secondary_draft_team_name_placeholder(city)
                && !kbo_secondary_draft_team_name_placeholder(nickname)
                && _stricmp(city, nickname) != 0) {
            snprintf(out, out_size, "%s %s", city, nickname);
            return;
        }
        if (!kbo_secondary_draft_team_name_placeholder(full_name)) {
            snprintf(out, out_size, "%s", full_name);
            return;
        }
        if (!kbo_secondary_draft_team_name_placeholder(nickname)) {
            snprintf(out, out_size, "%s", nickname);
            return;
        }
        if (!kbo_secondary_draft_team_name_placeholder(city)) {
            snprintf(out, out_size, "%s", city);
            return;
        }
    }

    snprintf(out, out_size, "Team #%u", team_id);
}

int kbo_secondary_draft_collect_main_teams(
    uint32_t league_id,
    KboSecondaryDraftTeam* teams,
    int max_teams)
{
    if (teams == NULL || max_teams <= 0 || league_id == 0u) {
        return 0;
    }
    memset(teams, 0, (size_t)max_teams * sizeof(teams[0]));

    uintptr_t global = get_ootp_global_database();
    if (global == 0u
            || !memory_range_readable((void*)(global + OOTP27_KBO_TEAM_COUNT_OFFSET), sizeof(int32_t))) {
        return 0;
    }

    uintptr_t team_vector = *(uintptr_t*)(global + OOTP27_KBO_TEAM_VECTOR_OFFSET);
    int32_t team_count = *(int32_t*)(global + OOTP27_KBO_TEAM_COUNT_OFFSET);
    if (team_vector == 0u || team_count <= 0 || team_count > KBO_RUNTIME_MAX_TEAM_VECTOR_COUNT
            || !memory_range_readable((void*)team_vector, (SIZE_T)team_count * sizeof(uintptr_t))) {
        return 0;
    }

    int count = 0;
    for (int32_t i = 0; i < team_count && count < max_teams; i++) {
        uintptr_t team_ptr = *(uintptr_t*)(team_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (team_ptr == 0u || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            continue;
        }
        uint8_t* team = (uint8_t*)team_ptr;
        if (team[OOTP27_KBO_TEAM_DELETED_OFFSET] != 0u) {
            continue;
        }
        uint32_t team_league_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
        uint32_t parent_team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_PARENT_TEAM_ID_OFFSET);
        uint32_t team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET);
        if (team_id == 0u || team_league_id != league_id || parent_team_id != 0u) {
            continue;
        }

        KboSecondaryDraftTeam* out = &teams[count++];
        out->team = team;
        out->team_id = team_id;
        out->org_team_id = team_id;
        out->league_id = team_league_id;
        out->wins = *(uint32_t*)(team + OOTP27_KBO_TEAM_REGULAR_WINS_OFFSET);
        out->losses = *(uint32_t*)(team + OOTP27_KBO_TEAM_REGULAR_LOSSES_OFFSET);
        out->ties = *(uint32_t*)(team + OOTP27_KBO_TEAM_REGULAR_TIES_OFFSET);
        out->games = out->wins + out->losses + out->ties;
        kbo_secondary_draft_copy_team_name(team, team_id, out->name, sizeof(out->name));
    }
    return count;
}

int kbo_secondary_draft_team_order_cmp(const void* left, const void* right)
{
    const KboSecondaryDraftTeam* a = (const KboSecondaryDraftTeam*)left;
    const KboSecondaryDraftTeam* b = (const KboSecondaryDraftTeam*)right;
    if (a->games != 0u && b->games != 0u) {
        int64_t a_points = ((int64_t)a->wins * 2ll) + (int64_t)a->ties;
        int64_t b_points = ((int64_t)b->wins * 2ll) + (int64_t)b->ties;
        int64_t left_value = a_points * (int64_t)b->games;
        int64_t right_value = b_points * (int64_t)a->games;
        if (left_value != right_value) {
            return left_value < right_value ? -1 : 1;
        }
        if (a->wins != b->wins) {
            return a->wins < b->wins ? -1 : 1;
        }
        if (a->losses != b->losses) {
            return a->losses > b->losses ? -1 : 1;
        }
    }
    if (a->team_id != b->team_id) {
        return a->team_id < b->team_id ? -1 : 1;
    }
    return 0;
}

int kbo_secondary_draft_team_index_by_id(
    const KboSecondaryDraftTeam* teams,
    int team_count,
    uint32_t team_id)
{
    if (teams == NULL || team_count <= 0 || team_id == 0u) {
        return -1;
    }
    for (int i = 0; i < team_count; i++) {
        if (teams[i].team_id == team_id || teams[i].org_team_id == team_id) {
            return i;
        }
    }
    return -1;
}
