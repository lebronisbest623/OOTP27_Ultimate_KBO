#ifndef KBOFIX_SRC_COMPETITIVE_BALANCE_TAX_EVENTS_CBT_EVENTS_H_
#define KBOFIX_SRC_COMPETITIVE_BALANCE_TAX_EVENTS_CBT_EVENTS_H_

#include <stdint.h>

int kbo_schedule_cbt_custom_events_for_date(uint32_t today, const char* source);
int kbo_schedule_cbt_custom_events(const char* source);
void start_kbo_cbt_event_scheduler_thread(void);
int kbo_handle_cbt_deadline_event(uint32_t event_yyyymmdd, const char* source);
int kbo_handle_cbt_announcement_event(uint32_t event_yyyymmdd, const char* source);

#endif
