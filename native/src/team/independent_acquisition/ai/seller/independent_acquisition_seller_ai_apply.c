#include "independent_acquisition_seller_ai_internal.h"
#include "helpers/decision/independent_acquisition_seller_decision.h"
#include "helpers/fit/independent_acquisition_seller_ai_helpers.h"
#include "helpers/transfer/independent_acquisition_seller_transfer.h"

#include "../../../../bootstrap/profiling/profiler.h"
#include "../../../../core/logging/core_log.h"

KboIndependentAcquisitionSellerAiApplyResult kbo_independent_acquisition_seller_apply_selected(
    uint32_t today,
    KboIndependentAcquisitionQueuedRequest* selected,
    uint8_t* player,
    int seller_limit_reached,
    int pacing_blocked,
    const char* source,
    int64_t best_request_score,
    int64_t best_fit_score,
    int64_t reservation_score,
    int64_t hold_value,
    int64_t second_best_request_score,
    int market_offer_count,
    int best_buyer_transfers,
    uint32_t best_tiebreaker,
    int seller_transfers,
    int32_t seller_transfer_limit)
{
    KboIndependentAcquisitionSellerAiApplyResult result = {0};
    if (selected == NULL) {
        return result;
    }
    int cash_charged = 0;
    int32_t old_cash = 0;
    int32_t new_cash = 0;
    int seller_cash_credited = 0;
    int32_t seller_old_cash = 0;
    int32_t seller_new_cash = 0;
    int32_t seller_transfer_fee = 0;
    int32_t cash_cost = 0;
    if (kbo_independent_acquisition_seller_abort_if_save(source, "before_assignment", today)) {
        result.abort_for_save = 1;
        return result;
    }
    KBO_PROFILE_BEGIN(profile_independent_seller_ai_apply_transfer);
    result.moved = kbo_independent_acquisition_seller_apply_transfer(
        today,
        selected,
        player,
        seller_limit_reached,
        pacing_blocked,
        source,
        &cash_charged,
        &old_cash,
        &new_cash,
        &seller_cash_credited,
        &seller_old_cash,
        &seller_new_cash,
        &seller_transfer_fee,
        &cash_cost);
    KBO_PROFILE_END(
        profile_independent_seller_ai_apply_transfer,
        result.moved
            ? "independent_acquisition.seller_ai.apply_transfer.moved"
            : "independent_acquisition.seller_ai.apply_transfer.not_moved");

    if (kbo_independent_acquisition_seller_abort_if_save(source, "before_append_decision", today)) {
        result.abort_for_save = 1;
        return result;
    }
    KBO_PROFILE_BEGIN(profile_independent_seller_ai_append_decision);
    result.appended_decision = kbo_independent_acquisition_seller_append_decision_and_log(
        today,
        selected,
        result.moved,
        old_cash,
        new_cash,
        cash_cost,
        seller_transfer_fee,
        seller_old_cash,
        seller_new_cash,
        seller_cash_credited,
        best_request_score,
        best_fit_score,
        reservation_score,
        hold_value,
        second_best_request_score,
        market_offer_count,
        best_buyer_transfers,
        best_tiebreaker,
        seller_transfers,
        seller_transfer_limit,
        source);
    KBO_PROFILE_END(
        profile_independent_seller_ai_append_decision,
        result.appended_decision
            ? "independent_acquisition.seller_ai.append_decision.ok"
            : "independent_acquisition.seller_ai.append_decision.failed");
    return result;
}
