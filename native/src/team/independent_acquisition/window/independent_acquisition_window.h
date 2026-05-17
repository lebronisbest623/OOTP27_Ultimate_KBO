#ifndef KBOFIX_SRC_TEAM_INDEPENDENT_ACQUISITION_WINDOW_H_
#define KBOFIX_SRC_TEAM_INDEPENDENT_ACQUISITION_WINDOW_H_

#include <stdint.h>

int kbo_handle_independent_team_acquisition_open_event(
    uint32_t event_yyyymmdd,
    const char* source);
int kbo_independent_team_acquisition_completion_valid(
    uint32_t league_id,
    uint32_t event_yyyymmdd);
uint32_t kbo_independent_team_acquisition_window_open_date(void);
int kbo_independent_team_acquisition_window_active(
    uint32_t today,
    uint32_t* out_open_date,
    uint8_t* out_effective_phase);
uint32_t kbo_independent_team_acquisition_window_elapsed_days(uint32_t today);
uint32_t kbo_independent_team_acquisition_window_planning_days(void);

#endif
