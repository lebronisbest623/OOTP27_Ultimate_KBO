#ifndef KBOFIX_SRC_TEAM_INDEPENDENT_ACQUISITION_UI_OFFERS_OFFER_CACHE_H_
#define KBOFIX_SRC_TEAM_INDEPENDENT_ACQUISITION_UI_OFFERS_OFFER_CACHE_H_

#include <stdint.h>

#include "../../../independent_acquisition_ui.h"

int kbo_independent_acquisition_ui_offer_cache_try_copy(
    uint32_t buyer_team_id,
    const KboIndependentAcquisitionUiContext* context,
    int32_t foreign_cash_cost,
    int32_t domestic_cash_cost,
    int32_t seller_transfer_limit,
    KboIndependentAcquisitionUiOfferRow* out_rows,
    int max_rows,
    KboIndependentAcquisitionUiContext* out_context,
    int* out_count);
void kbo_independent_acquisition_ui_offer_cache_store(
    uint32_t buyer_team_id,
    const KboIndependentAcquisitionUiContext* context,
    int32_t foreign_cash_cost,
    int32_t domestic_cash_cost,
    int32_t seller_transfer_limit,
    const KboIndependentAcquisitionUiOfferRow* rows,
    int count,
    int max_rows);

#endif
