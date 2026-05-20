#ifndef KBOFIX_SRC_FOREIGN_WAIVER_WINDOW_EVENTS_FOREIGN_WAIVER_WINDOW_EVENTS_H_
#define KBOFIX_SRC_FOREIGN_WAIVER_WINDOW_EVENTS_FOREIGN_WAIVER_WINDOW_EVENTS_H_

#include <stdint.h>

void kbo_queue_foreign_priority_league_event(
    uint32_t event_yyyymmdd,
    const char* title,
    const char* source);
uint32_t kbo_detect_offseason_starts_event(uint32_t today_yyyymmdd, uint32_t league_id);
int kbo_write_foreign_waiver_window(uint32_t start_yyyymmdd, uint32_t end_yyyymmdd, const char* reason);

#endif
