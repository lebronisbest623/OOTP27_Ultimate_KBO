#include "foreign_quota_retention_score_gate.h"

int kbo_retention_candidate_score_clears_best(int32_t candidate_score, int32_t best_score)
{
    return candidate_score >= best_score;
}
