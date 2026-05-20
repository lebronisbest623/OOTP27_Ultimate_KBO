#ifndef KBOFIX_SRC_CORE_DATES_TICK_CAPTURE_CURRENT_DATE_TICK_CAPTURE_INTERNAL_H_
#define KBOFIX_SRC_CORE_DATES_TICK_CAPTURE_CURRENT_DATE_TICK_CAPTURE_INTERNAL_H_

#include <stdint.h>
#include <windows.h>

#include "../current_date_tick_capture.h"

LONG kbo_current_date_tick_latest_sequence(void);
const char* kbo_current_date_tick_log_label(const char* label);
int kbo_current_date_tick_log_allowed(LONG* log_count);
void kbo_current_date_tick_reset_sync_consumer_dates(void);
int kbo_current_date_tick_dispatch_sync_consumers(uint32_t date, uint32_t site_rva);
int kbo_current_date_tick_publish_save_enter_current(
    const char* save_path,
    const char* label);

#endif
