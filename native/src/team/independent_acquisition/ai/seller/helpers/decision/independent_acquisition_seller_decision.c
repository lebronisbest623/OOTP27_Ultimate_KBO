#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "independent_acquisition_seller_decision.h"

#include <inttypes.h>
#include <stdio.h>

#include "../../../../../../core/logging/core_log.h"

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
    const char* source)
{
    if (selected == NULL) {
        return 0;
    }

    if (!kbo_independent_acquisition_append_decision(
            today,
            selected,
            moved,
            old_cash,
            new_cash,
            seller_transfer_fee,
            seller_old_cash,
            seller_new_cash,
            source)) {
        return 0;
    }

    char request_score_text[32] = {0};
    snprintf(request_score_text, sizeof(request_score_text), "%" PRId64, request_score);
    kbo_log_runtimef(
        "independent acquisition seller AI decision source=%s seller=%u player=%u buyer=%u score=%s adjusted_fit=%lld reservation=%lld hold_value=%lld second_best=%lld market_offers=%d buyer_transfers=%d tiebreaker=%u cash_cost=%d old_cash=%d new_cash=%d seller_transfer_fee=%d seller_old_cash=%d seller_new_cash=%d seller_cash_credited=%d transferred=%d seller_transfers=%d seller_transfer_limit=%d",
        source != NULL ? source : "",
        selected->seller_team_id,
        selected->player_id,
        selected->buyer_team_id,
        request_score_text,
        (long long)best_fit_score,
        (long long)reservation_score,
        (long long)hold_value,
        (long long)second_best_request_score,
        market_offer_count,
        best_buyer_transfers,
        best_tiebreaker,
        cash_cost,
        old_cash,
        new_cash,
        seller_transfer_fee,
        seller_old_cash,
        seller_new_cash,
        seller_cash_credited,
        moved,
        seller_transfers,
        seller_transfer_limit);
    return 1;
}
