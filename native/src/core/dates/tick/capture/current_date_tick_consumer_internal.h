#ifndef KBO_CORE_DATES_TICK_CAPTURE_CURRENT_DATE_TICK_CONSUMER_INTERNAL_H
#define KBO_CORE_DATES_TICK_CAPTURE_CURRENT_DATE_TICK_CONSUMER_INTERNAL_H

#include "current_date_tick_capture_internal.h"

int kbo_current_date_tick_peek_ex(
    KboCurrentDateTickCursor* cursor,
    const char* label,
    LONG* overflow_log_count,
    KboCurrentDateTickEvent* out_event,
    uint32_t* out_missed_events);

#endif
