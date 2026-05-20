#ifndef KBOFIX_SRC_INDEPENDENT_ACQUISITION_SELLER_QUEUE_H_
#define KBOFIX_SRC_INDEPENDENT_ACQUISITION_SELLER_QUEUE_H_

#include <stdint.h>

#include "../../../independent_acquisition_ai_internal.h"

void kbo_independent_acquisition_seller_clear_later_requests(
    KboIndependentAcquisitionQueuedRequest* queue,
    int request_count,
    int start_index,
    uint32_t player_id,
    uint32_t seller_team_id);
int kbo_independent_acquisition_seller_daily_index(
    uint32_t seller_team_id,
    uint32_t* seller_ids,
    int* seller_transfer_counts,
    int* seller_count,
    int max_sellers);

#endif
