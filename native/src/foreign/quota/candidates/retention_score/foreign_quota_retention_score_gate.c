#include "foreign_quota_retention_score_gate.h"

int kbo_retention_candidate_score_clears_best(int32_t candidate_score, int32_t best_score)
{
    return candidate_score >= best_score;
}

int kbo_retention_candidate_consumes_last_effective_slot(
    uint32_t effective_after,
    uint32_t effective_limit)
{
    return effective_limit > 0u && effective_after >= effective_limit;
}
