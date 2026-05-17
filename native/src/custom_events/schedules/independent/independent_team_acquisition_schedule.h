#ifndef KBOFIX_SRC_CUSTOM_EVENTS_INDEPENDENT_TEAM_ACQUISITION_SCHEDULE_H_
#define KBOFIX_SRC_CUSTOM_EVENTS_INDEPENDENT_TEAM_ACQUISITION_SCHEDULE_H_

#include <stdint.h>

int kbo_schedule_independent_team_acquisition_custom_events_for_date(
    uint32_t today,
    const char* source);
int kbo_schedule_independent_team_acquisition_custom_events(const char* source);

#endif
