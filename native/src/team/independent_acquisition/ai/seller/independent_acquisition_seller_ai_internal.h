#ifndef KBO_TEAM_INDEPENDENT_ACQUISITION_SELLER_AI_INTERNAL_H
#define KBO_TEAM_INDEPENDENT_ACQUISITION_SELLER_AI_INTERNAL_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../independent_acquisition_ai_internal.h"

typedef struct KboIndependentAcquisitionSellerAiSelection {
    KboIndependentAcquisitionQueuedRequest selected;
    uint8_t* player;
    int64_t best_request_score;
    int64_t best_fit_score;
    int64_t second_best_request_score;
    int best_buyer_transfers;
    uint32_t best_tiebreaker;
    int market_offer_count;
    int abort_for_save;
} KboIndependentAcquisitionSellerAiSelection;

typedef struct KboIndependentAcquisitionSellerAiApplyResult {
    int moved;
    int appended_decision;
    int abort_for_save;
} KboIndependentAcquisitionSellerAiApplyResult;

int kbo_independent_acquisition_seller_transfer_count_for_ai(
    uint32_t season,
    uint32_t seller_team_id,
    KboIndependentAcquisitionTransferSummary* summaries,
    int summary_count);
int kbo_independent_acquisition_buyer_transfer_count_for_ai(
    uint32_t season,
    uint32_t buyer_team_id,
    KboIndependentAcquisitionTransferSummary* summaries,
    int summary_count);
uint32_t kbo_independent_acquisition_last_transfer_date_for_ai(
    uint32_t season,
    uint32_t seller_team_id,
    KboIndependentAcquisitionTransferSummary* summaries,
    int summary_count);
void kbo_independent_acquisition_record_transfer_summary_for_ai(
    KboIndependentAcquisitionTransferSummary* summaries,
    int* summary_count,
    int max_count,
    uint32_t team_id,
    uint32_t today,
    int update_last_transfer_date);
KboIndependentAcquisitionSellerAiSelection kbo_independent_acquisition_seller_select_best_request(
    uint32_t today,
    KboIndependentAcquisitionQueuedRequest* queue,
    int start_index,
    int request_count,
    const KboIndependentAcquisitionQueuedRequest* group,
    const uintptr_t* player_snapshot,
    int32_t player_count,
    KboIndependentAcquisitionTransferSummary* buyer_summaries,
    int buyer_summary_count,
    const char* source);
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
    int32_t seller_transfer_limit);

#endif
