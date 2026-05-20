#ifndef KBOFIX_SRC_INDEPENDENT_ACQUISITION_SELLER_DECISION_H_
#define KBOFIX_SRC_INDEPENDENT_ACQUISITION_SELLER_DECISION_H_

#include <stdint.h>

#include "../../../independent_acquisition_ai_internal.h"

int kbo_independent_acquisition_seller_append_decision_and_log(
    uint32_t today,
    const KboIndependentAcquisitionQueuedRequest* selected,
    int moved,
    int32_t old_cash,
    int32_t new_cash,
    int32_t cash_cost,
    int32_t seller_transfer_fee,
    int32_t seller_old_cash,
    int32_t seller_new_cash,
    int seller_cash_credited,
    int64_t request_score,
    int64_t best_fit_score,
    int64_t reservation_score,
    int64_t hold_value,
    int64_t second_best_request_score,
    int market_offer_count,
    int best_buyer_transfers,
    uint32_t best_tiebreaker,
    int seller_transfers,
    int seller_transfer_limit,
    const char* source);

#endif
