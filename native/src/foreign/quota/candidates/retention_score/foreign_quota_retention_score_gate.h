#ifndef KBOFIX_SRC_FOREIGN_QUOTA_CANDIDATES_RETENTION_SCORE_GATE_H_
#define KBOFIX_SRC_FOREIGN_QUOTA_CANDIDATES_RETENTION_SCORE_GATE_H_

#include <stdint.h>

int kbo_retention_candidate_score_clears_best(int32_t candidate_score, int32_t best_score);
int kbo_retention_candidate_consumes_last_effective_slot(
    uint32_t effective_after,
    uint32_t effective_limit);
int kbo_retention_candidate_slot_reservation_active(
    uint32_t retained_on_yyyymmdd,
    uint32_t today_yyyymmdd,
    uint32_t reserve_days);
int kbo_retention_open_market_candidate_replacement_allowed(
    uint32_t effective_after,
    uint32_t effective_limit,
    int reserve_active,
    int candidate_clears_retained_best);

#endif
