#include "foreign_quota_retention_score_gate.h"
#include "../../../../core/dates/core_text_date.h"

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

int kbo_retention_candidate_slot_reservation_active(
    uint32_t retained_on_yyyymmdd,
    uint32_t today_yyyymmdd,
    uint32_t reserve_days)
{
    if (retained_on_yyyymmdd == 0u || today_yyyymmdd == 0u) {
        return 1;
    }
    uint32_t retained_serial = kbo_date_serial(
        retained_on_yyyymmdd / 10000u,
        (retained_on_yyyymmdd / 100u) % 100u,
        retained_on_yyyymmdd % 100u);
    uint32_t today_serial = kbo_date_serial(
        today_yyyymmdd / 10000u,
        (today_yyyymmdd / 100u) % 100u,
        today_yyyymmdd % 100u);
    if (retained_serial == 0u || today_serial == 0u) {
        return 1;
    }
    if (today_serial < retained_serial) {
        return 1;
    }
    return today_serial - retained_serial <= reserve_days;
}

int kbo_retention_open_market_candidate_replacement_allowed(
    uint32_t effective_after,
    uint32_t effective_limit,
    int reserve_active,
    int candidate_clears_retained_best)
{
    if (!reserve_active || candidate_clears_retained_best) {
        return 0;
    }
    return kbo_retention_candidate_consumes_last_effective_slot(
        effective_after,
        effective_limit);
}
