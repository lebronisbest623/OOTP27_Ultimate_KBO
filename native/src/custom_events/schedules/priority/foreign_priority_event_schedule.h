#ifndef KBOFIX_SRC_CUSTOM_EVENTS_FOREIGN_PRIORITY_EVENT_SCHEDULE_H_
#define KBOFIX_SRC_CUSTOM_EVENTS_FOREIGN_PRIORITY_EVENT_SCHEDULE_H_

#include <stdint.h>

uint32_t kbo_recent_phase_transition_offseason_anchor(uint32_t league_id, uint32_t today_yyyymmdd);
int kbo_schedule_foreign_priority_custom_events_at_anchor(
    const char* source,
    uint32_t today,
    uint32_t league_id,
    uint32_t offseason_starts_yyyymmdd);
int kbo_schedule_foreign_priority_custom_events_for_anchor(
    const char* source,
    uint32_t offseason_starts_yyyymmdd);
int kbo_schedule_foreign_priority_custom_events_for_anchor_on_date(
    const char* source,
    uint32_t today,
    uint32_t offseason_starts_yyyymmdd);
int kbo_schedule_foreign_priority_custom_events_for_date(
    const char* source,
    uint32_t today);
int kbo_schedule_foreign_priority_custom_events(const char* source);

#endif
