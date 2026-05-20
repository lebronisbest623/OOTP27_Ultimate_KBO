#ifndef KBOFIX_SRC_TEAM_INDEPENDENT_ACQUISITION_UI_OFFERS_OFFER_ROWS_H_
#define KBOFIX_SRC_TEAM_INDEPENDENT_ACQUISITION_UI_OFFERS_OFFER_ROWS_H_

#include <stddef.h>
#include <stdint.h>

#include "../../../independent_acquisition_ui.h"
#include "../../../../ai/independent_acquisition_ai_internal.h"

#define KBO_INDEPENDENT_ACQUISITION_UI_DECISION_KEY_MAX 4096

int kbo_independent_acquisition_ui_decision_key_exists(
    const KboIndependentAcquisitionDecisionKey* keys,
    int key_count,
    int fallback_to_cached_file,
    uint32_t season,
    uint32_t seller_team_id,
    uint32_t player_id);
void kbo_independent_acquisition_ui_slot_label(
    uint8_t slot_type,
    int foreign_player,
    int asian_quota,
    char* out,
    size_t out_size);
void kbo_independent_acquisition_ui_insert_offer_row(
    KboIndependentAcquisitionUiOfferRow* rows,
    int* count,
    int max_rows,
    const KboIndependentAcquisitionUiOfferRow* row);
int kbo_independent_acquisition_ui_offer_row_cmp(const void* a, const void* b);
int kbo_independent_acquisition_ui_request_row_exists(
    const KboIndependentAcquisitionQueuedRequest* requests,
    int request_count,
    uint32_t buyer_team_id,
    uint32_t seller_team_id,
    uint32_t player_id);

#endif
