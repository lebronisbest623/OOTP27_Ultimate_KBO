#ifndef KBOFIX_SRC_FA_MARKET_INVESTIGATION_RESCUE_DOMESTIC_FA_ORPHAN_RESCUE_H_
#define KBOFIX_SRC_FA_MARKET_INVESTIGATION_RESCUE_DOMESTIC_FA_ORPHAN_RESCUE_H_

#include <stdint.h>

#include "domestic_fa_orphan_rescue_policy.h"

int kbo_domestic_fa_orphan_rescue_enabled(void);
int kbo_domestic_fa_orphan_rescue_dry_run(void);
void kbo_domestic_fa_orphan_rescue_update_cache(
    uint32_t today,
    const KboDomesticFaInvestigationCandidate* candidates,
    int candidate_count);
int kbo_domestic_fa_orphan_rescue_collect_cached(
    uint32_t today,
    KboDomesticFaOrphanRescueCachedCandidate* out_candidates,
    int max_candidates);
int32_t kbo_domestic_fa_orphan_rescue_force_market_candidates(
    uintptr_t frame_ptr,
    uint32_t requester_team_id,
    uintptr_t candidate_array,
    int32_t insert_index,
    uint32_t today);
int kbo_domestic_fa_orphan_rescue_player_can_enter_market(
    uint8_t* player,
    uint32_t expected_player_id);
void kbo_domestic_fa_orphan_rescue_record_candidate_evidence(
    const KboDomesticFaOrphanRescueCachedCandidate* candidate,
    uint32_t requester_team_id,
    int32_t before_index,
    int32_t after_index,
    uint32_t today,
    int dry_run);
uintptr_t kbo_domestic_fa_orphan_rescue_offer_candidate_replacement(
    uintptr_t original_candidate_ptr,
    uint32_t requester_team_id,
    uint32_t requester_league_id,
    uint32_t today);

#endif
