#ifndef KBOFIX_SRC_INDEPENDENT_ACQUISITION_AI_LIFECYCLE_H_
#define KBOFIX_SRC_INDEPENDENT_ACQUISITION_AI_LIFECYCLE_H_

#include "../independent_acquisition_ai_internal.h"

int kbo_independent_acquisition_claim_daily_run(uint32_t today);
void kbo_independent_acquisition_release_daily_run(uint32_t today);
int kbo_independent_acquisition_abort_if_save(
    const char* source,
    const char* stage,
    uint32_t today);
int kbo_independent_acquisition_window_active(uint32_t today);
int kbo_independent_acquisition_buyer_pending_request_count(
    const KboIndependentAcquisitionQueuedRequest* requests,
    int request_count,
    uint32_t buyer_team_id);
uint32_t kbo_independent_acquisition_effective_season(uint32_t today);

#endif
