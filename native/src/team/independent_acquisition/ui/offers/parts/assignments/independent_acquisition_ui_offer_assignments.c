#include "independent_acquisition_ui_offer_assignments.h"

#include <string.h>

#include "../../../../../../bootstrap/abi/ootp_offsets.h"
#include "../../../../../../runtime_memory/runtime_memory.h"
#include "../../../../../lookup/team_lookup.h"

static uint32_t kbo_independent_acquisition_ui_org_team_id(
    uint32_t team_id,
    KboIndependentAcquisitionUiTeamOrgCache* cache)
{
    if (team_id == 0u) {
        return 0u;
    }
    if (cache != NULL) {
        for (int i = 0; i < cache->count; i++) {
            if (cache->entries[i].team_id == team_id) {
                return cache->entries[i].org_team_id;
            }
        }
    }

    uint32_t org_team_id = team_id;
    uint8_t* team = find_kbo_team_by_numeric_id_any_league(team_id, 1);
    if (team != NULL
            && memory_range_readable(
                team + OOTP27_KBO_TEAM_PARENT_TEAM_ID_OFFSET,
                sizeof(uint32_t))) {
        uint32_t parent_team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_PARENT_TEAM_ID_OFFSET);
        if (parent_team_id != 0u) {
            org_team_id = parent_team_id;
        }
    }

    if (cache != NULL && cache->count < KBO_INDEPENDENT_ACQUISITION_UI_TEAM_ORG_CACHE_MAX) {
        cache->entries[cache->count].team_id = team_id;
        cache->entries[cache->count].org_team_id = org_team_id;
        cache->count++;
    }
    return org_team_id;
}

KboIndependentAcquisitionUiTeamMatch kbo_independent_acquisition_ui_team_match(
    uint32_t team_id,
    KboIndependentAcquisitionUiTeamOrgCache* cache)
{
    KboIndependentAcquisitionUiTeamMatch match;
    memset(&match, 0, sizeof(match));
    match.team_id = team_id;
    match.org_team_id = kbo_independent_acquisition_ui_org_team_id(team_id, cache);
    return match;
}

static int kbo_independent_acquisition_ui_add_player_assignment(
    KboIndependentAcquisitionUiPlayerAssignmentSnapshot* snapshot,
    uint32_t team_id,
    KboIndependentAcquisitionUiTeamOrgCache* cache)
{
    if (snapshot == NULL || team_id == 0u) {
        return 0;
    }
    for (int i = 0; i < snapshot->count; i++) {
        if (snapshot->team_ids[i] == team_id) {
            return 1;
        }
    }
    if (snapshot->count >= 3) {
        return 0;
    }
    snapshot->team_ids[snapshot->count] = team_id;
    snapshot->org_team_ids[snapshot->count] =
        kbo_independent_acquisition_ui_org_team_id(team_id, cache);
    snapshot->count++;
    return 1;
}

int kbo_independent_acquisition_ui_build_player_assignment_snapshot(
    uint8_t* player,
    KboIndependentAcquisitionUiTeamOrgCache* cache,
    KboIndependentAcquisitionUiPlayerAssignmentSnapshot* out_snapshot)
{
    if (out_snapshot != NULL) {
        memset(out_snapshot, 0, sizeof(*out_snapshot));
    }
    if (player == NULL
            || out_snapshot == NULL
            || !memory_range_readable(
                player,
                OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET + sizeof(uint32_t))) {
        return 0;
    }

    kbo_independent_acquisition_ui_add_player_assignment(
        out_snapshot,
        *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET),
        cache);
    kbo_independent_acquisition_ui_add_player_assignment(
        out_snapshot,
        *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET),
        cache);
    kbo_independent_acquisition_ui_add_player_assignment(
        out_snapshot,
        *(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET),
        cache);
    return out_snapshot->count > 0;
}

int kbo_independent_acquisition_ui_assignment_snapshot_matches_team(
    const KboIndependentAcquisitionUiPlayerAssignmentSnapshot* snapshot,
    const KboIndependentAcquisitionUiTeamMatch* match)
{
    if (snapshot == NULL || match == NULL || match->team_id == 0u) {
        return 0;
    }
    for (int i = 0; i < snapshot->count; i++) {
        if (snapshot->team_ids[i] == match->team_id
                || snapshot->org_team_ids[i] == match->team_id) {
            return 1;
        }
    }
    return 0;
}

const KboIndependentAcquisitionUiSellerMatch* kbo_independent_acquisition_ui_seller_for_player(
    const KboIndependentAcquisitionUiPlayerAssignmentSnapshot* snapshot,
    const KboIndependentAcquisitionUiSellerMatch* sellers,
    int seller_count)
{
    if (snapshot == NULL || sellers == NULL || seller_count <= 0) {
        return NULL;
    }
    for (int i = 0; i < seller_count; i++) {
        if (sellers[i].seller.team_id != 0u
                && kbo_independent_acquisition_ui_assignment_snapshot_matches_team(
                    snapshot,
                    &sellers[i].match)) {
            return &sellers[i];
        }
    }
    return NULL;
}
