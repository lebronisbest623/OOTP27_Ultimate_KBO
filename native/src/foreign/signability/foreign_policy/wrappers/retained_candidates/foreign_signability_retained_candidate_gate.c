#include "foreign_signability_foreign_ai_fa_retained_candidates.h"

#include "../../../../common/policy/foreign_player_policy.h"

int kbo_ai_fa_status_retained_candidate_force_gate(
    const KboAiFaStatusRetainedCandidate* candidate,
    const char** out_reject_reason)
{
    if (out_reject_reason != NULL) {
        *out_reject_reason = "ok";
    }
    if (candidate == NULL) {
        if (out_reject_reason != NULL) { *out_reject_reason = "bad_candidate"; }
        return 0;
    }
    if (candidate->already_in_org) {
        if (out_reject_reason != NULL) { *out_reject_reason = "already_in_org"; }
        return 0;
    }
    if (!candidate->market_free_agent) {
        if (out_reject_reason != NULL) { *out_reject_reason = "not_market_free_agent"; }
        return 0;
    }
    if (!kbo_foreign_policy_demand_salary_plausible(candidate->fa_demand)) {
        if (out_reject_reason != NULL) { *out_reject_reason = "demand_not_ready"; }
        return 0;
    }
    if (out_reject_reason != NULL) {
        *out_reject_reason = NULL;
    }
    return 1;
}
