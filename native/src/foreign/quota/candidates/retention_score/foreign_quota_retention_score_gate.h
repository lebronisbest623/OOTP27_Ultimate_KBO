#ifndef KBOFIX_SRC_FOREIGN_QUOTA_CANDIDATES_RETENTION_SCORE_GATE_H_
#define KBOFIX_SRC_FOREIGN_QUOTA_CANDIDATES_RETENTION_SCORE_GATE_H_

#include <stdint.h>

int kbo_retention_candidate_score_clears_best(int32_t candidate_score, int32_t best_score);

#endif
