#ifndef KBOFIX_SRC_INDEPENDENT_ACQUISITION_SELLER_AI_HELPERS_H_
#define KBOFIX_SRC_INDEPENDENT_ACQUISITION_SELLER_AI_HELPERS_H_

#include "../independent_acquisition_ai_internal.h"

int64_t kbo_independent_acquisition_seller_fit_score(
    const KboIndependentAcquisitionQueuedRequest* request,
    uint8_t* player,
    uint8_t* buyer_team,
    int32_t cash_cost);
uint32_t kbo_independent_acquisition_seller_tiebreaker(
    uint32_t today,
    const KboIndependentAcquisitionQueuedRequest* request);
int kbo_independent_acquisition_seller_pacing_deferred(
    uint32_t today,
    const KboIndependentAcquisitionQueuedRequest* request,
    int seller_transfers,
    int seller_transfer_limit,
    uint32_t* out_window_age_days,
    uint32_t* out_target_day,
    uint32_t* out_request_age_days,
    uint32_t* out_days_remaining);
int kbo_independent_acquisition_seller_cooldown_deferred(
    uint32_t today,
    uint32_t last_transfer_date,
    uint32_t* out_days_since_transfer);
int kbo_independent_acquisition_seller_strategy_deferred(
    uint32_t today,
    const KboIndependentAcquisitionQueuedRequest* request,
    int seller_transfers,
    int seller_transfer_limit,
    int market_offer_count,
    int64_t selected_score,
    int64_t second_best_score,
    int32_t player_value_score,
    int64_t* out_reservation_score,
    int64_t* out_hold_value,
    uint32_t* out_window_age_days,
    uint32_t* out_request_age_days,
    uint32_t* out_days_remaining);
int kbo_independent_acquisition_seller_abort_if_save(
    const char* source,
    const char* stage,
    uint32_t today);
uint32_t kbo_independent_acquisition_seller_effective_season(uint32_t today);

#endif
