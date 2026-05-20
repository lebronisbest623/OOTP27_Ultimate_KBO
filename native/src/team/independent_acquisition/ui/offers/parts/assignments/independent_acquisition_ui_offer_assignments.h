#ifndef KBOFIX_SRC_TEAM_INDEPENDENT_ACQUISITION_UI_OFFERS_OFFER_ASSIGNMENTS_H_
#define KBOFIX_SRC_TEAM_INDEPENDENT_ACQUISITION_UI_OFFERS_OFFER_ASSIGNMENTS_H_

#include <stdint.h>

#include "../../../../ai/independent_acquisition_ai_internal.h"

#define KBO_INDEPENDENT_ACQUISITION_UI_TEAM_ORG_CACHE_MAX 1024

typedef struct KboIndependentAcquisitionUiTeamOrgCacheEntry {
    uint32_t team_id;
    uint32_t org_team_id;
} KboIndependentAcquisitionUiTeamOrgCacheEntry;

typedef struct KboIndependentAcquisitionUiTeamOrgCache {
    KboIndependentAcquisitionUiTeamOrgCacheEntry entries[
        KBO_INDEPENDENT_ACQUISITION_UI_TEAM_ORG_CACHE_MAX];
    int count;
} KboIndependentAcquisitionUiTeamOrgCache;

typedef struct KboIndependentAcquisitionUiTeamMatch {
    uint32_t team_id;
    uint32_t org_team_id;
} KboIndependentAcquisitionUiTeamMatch;

typedef struct KboIndependentAcquisitionUiSellerMatch {
    KboIndependentFuturesTeamLeague seller;
    KboIndependentAcquisitionUiTeamMatch match;
    int transfer_blocked;
} KboIndependentAcquisitionUiSellerMatch;

typedef struct KboIndependentAcquisitionUiPlayerAssignmentSnapshot {
    uint32_t team_ids[3];
    uint32_t org_team_ids[3];
    int count;
} KboIndependentAcquisitionUiPlayerAssignmentSnapshot;

KboIndependentAcquisitionUiTeamMatch kbo_independent_acquisition_ui_team_match(
    uint32_t team_id,
    KboIndependentAcquisitionUiTeamOrgCache* cache);
int kbo_independent_acquisition_ui_build_player_assignment_snapshot(
    uint8_t* player,
    KboIndependentAcquisitionUiTeamOrgCache* cache,
    KboIndependentAcquisitionUiPlayerAssignmentSnapshot* out_snapshot);
int kbo_independent_acquisition_ui_assignment_snapshot_matches_team(
    const KboIndependentAcquisitionUiPlayerAssignmentSnapshot* snapshot,
    const KboIndependentAcquisitionUiTeamMatch* match);
const KboIndependentAcquisitionUiSellerMatch* kbo_independent_acquisition_ui_seller_for_player(
    const KboIndependentAcquisitionUiPlayerAssignmentSnapshot* snapshot,
    const KboIndependentAcquisitionUiSellerMatch* sellers,
    int seller_count);

#endif
