#ifndef KBOFIX_SRC_OFFER_CANDIDATE_REPLACEMENT_OFFER_CANDIDATE_REPLACEMENT_DISPATCHER_H_
#define KBOFIX_SRC_OFFER_CANDIDATE_REPLACEMENT_OFFER_CANDIDATE_REPLACEMENT_DISPATCHER_H_

#include <stdint.h>

typedef enum KboOfferCandidateReplacementSource {
    KBO_OFFER_CANDIDATE_REPLACEMENT_NONE = 0,
    KBO_OFFER_CANDIDATE_REPLACEMENT_DOMESTIC_FA_RESCUE = 1
} KboOfferCandidateReplacementSource;

typedef struct KboOfferCandidateReplacementResult {
    uintptr_t player_ptr;
    KboOfferCandidateReplacementSource source;
} KboOfferCandidateReplacementResult;

int kbo_offer_candidate_replacement_dispatcher_needs_hook(void);
KboOfferCandidateReplacementResult kbo_offer_candidate_replacement_dispatch(
    uintptr_t original_candidate_ptr,
    uint32_t requester_team_id,
    uint32_t requester_league_id,
    uint32_t today);

#endif
