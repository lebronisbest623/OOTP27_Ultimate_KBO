#ifndef KBOFIX_SRC_INDEPENDENT_ACQUISITION_SELLER_TRANSFER_H_
#define KBOFIX_SRC_INDEPENDENT_ACQUISITION_SELLER_TRANSFER_H_

#include <stdint.h>

#include "../../../independent_acquisition_ai_internal.h"

int kbo_independent_acquisition_seller_apply_transfer(
    uint32_t today,
    KboIndependentAcquisitionQueuedRequest* selected,
    uint8_t* player,
    int seller_limit_reached,
    int pacing_blocked,
    const char* source,
    int* out_cash_charged,
    int32_t* out_old_cash,
    int32_t* out_new_cash,
    int32_t* out_cash_cost);

#endif
