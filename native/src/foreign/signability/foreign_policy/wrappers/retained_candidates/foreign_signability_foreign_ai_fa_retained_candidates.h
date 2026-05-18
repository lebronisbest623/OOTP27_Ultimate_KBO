#ifndef KBOFIX_SRC_FOREIGN_SIGNABILITY_FOREIGN_AI_FA_RETAINED_CANDIDATES_H_
#define KBOFIX_SRC_FOREIGN_SIGNABILITY_FOREIGN_AI_FA_RETAINED_CANDIDATES_H_

#include <stdint.h>

#define KBO_AI_FA_STATUS_FORCED_RETENTION_MAX 16

typedef struct KboAiFaStatusRetainedCandidate {
    uintptr_t player_ptr;
    uint32_t player_id, current_team_id, active_team_id, original_team_id;
    uint32_t default_team_id, loan_team_id, draft_league_id;
    int32_t score, threshold, fa_demand;
    uint8_t position_group, position_role, asian, contract_level;
    int already_in_org, market_free_agent, holder_org_candidate;
    const char* reject_reason;
} KboAiFaStatusRetainedCandidate;

int kbo_ai_fa_status_retention_recently_attempted(
    uintptr_t frame_ptr,
    uintptr_t candidate_array,
    uint32_t requester_team_id,
    uint32_t today);
int kbo_ai_fa_status_retention_priority_enabled(void);
void kbo_ai_fa_status_log_retained_candidate_eval(
    uint32_t requester_team_id,
    uint32_t today,
    const KboAiFaStatusRetainedCandidate* candidate,
    int can_enter);
int kbo_ai_fa_status_evaluate_retained_market_candidate(
    uint8_t* player,
    uint32_t expected_player_id,
    uint32_t requester_team_id,
    uint32_t today,
    KboAiFaStatusRetainedCandidate* out_candidate);
void kbo_ai_fa_status_sort_retained_candidates(
    KboAiFaStatusRetainedCandidate* candidates,
    int count);
int32_t kbo_ai_fa_status_force_retained_market_candidates(
    uintptr_t frame_ptr,
    uint32_t requester_team_id,
    uintptr_t candidate_array,
    int32_t insert_index);

#endif
