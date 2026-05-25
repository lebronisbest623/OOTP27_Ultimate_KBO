#include "offer_candidate_replacement_dispatcher.h"

#include "../../fa_market_investigation/rescue/domestic_fa_orphan_rescue.h"

static KboOfferCandidateReplacementResult kbo_offer_candidate_replacement_none(
    uintptr_t original_candidate_ptr)
{
    KboOfferCandidateReplacementResult result;
    result.player_ptr = original_candidate_ptr;
    result.source = KBO_OFFER_CANDIDATE_REPLACEMENT_NONE;
    return result;
}

static KboOfferCandidateReplacementResult kbo_offer_candidate_replacement_from_source(
    uintptr_t player_ptr,
    KboOfferCandidateReplacementSource source)
{
    KboOfferCandidateReplacementResult result;
    result.player_ptr = player_ptr;
    result.source = source;
    return result;
}

int kbo_offer_candidate_replacement_dispatcher_needs_hook(void)
{
    return kbo_domestic_fa_orphan_rescue_enabled();
}

KboOfferCandidateReplacementResult kbo_offer_candidate_replacement_dispatch(
    uintptr_t original_candidate_ptr,
    uint32_t requester_team_id,
    uint32_t requester_league_id,
    uint32_t today)
{
    if (original_candidate_ptr == 0u || requester_team_id == 0u || today == 0u) {
        return kbo_offer_candidate_replacement_none(original_candidate_ptr);
    }

    uintptr_t domestic_replacement =
        kbo_domestic_fa_orphan_rescue_offer_candidate_replacement(
            original_candidate_ptr,
            requester_team_id,
            requester_league_id,
            today);
    if (domestic_replacement != original_candidate_ptr) {
        return kbo_offer_candidate_replacement_from_source(
            domestic_replacement,
            KBO_OFFER_CANDIDATE_REPLACEMENT_DOMESTIC_FA_RESCUE);
    }

    return kbo_offer_candidate_replacement_none(original_candidate_ptr);
}
