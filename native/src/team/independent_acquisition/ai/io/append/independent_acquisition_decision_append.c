#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../../independent_acquisition_ai_internal.h"
#include "../sql/independent_acquisition_sql_store.h"

int kbo_independent_acquisition_append_decision(
    uint32_t today,
    const KboIndependentAcquisitionQueuedRequest* request,
    int transferred,
    int32_t old_cash,
    int32_t new_cash,
    int32_t seller_transfer_fee,
    int32_t seller_old_cash,
    int32_t seller_new_cash,
    const char* source)
{
    if (today == 0u || request == NULL) {
        return 0;
    }

    return kbo_independent_acquisition_sql_append_decision(
        today,
        request,
        transferred,
        old_cash,
        new_cash,
        seller_transfer_fee,
        seller_old_cash,
        seller_new_cash,
        source);
}
