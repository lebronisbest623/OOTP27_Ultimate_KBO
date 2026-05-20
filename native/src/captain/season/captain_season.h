#ifndef KBOFIX_SRC_CAPTAIN_SEASON_CAPTAIN_SEASON_H_
#define KBOFIX_SRC_CAPTAIN_SEASON_CAPTAIN_SEASON_H_

#include <stdint.h>

uint32_t kbo_captain_effective_season(uint32_t date_yyyymmdd, uint32_t league_season);
int kbo_captain_calendar_season_recovery_active(
    uint32_t date_yyyymmdd,
    uint32_t league_season,
    uint8_t phase);
int kbo_captain_calendar_preseason_window_active(
    uint32_t date_yyyymmdd,
    uint32_t league_season,
    uint8_t phase);
int kbo_captain_preseason_first_day_active(
    uint32_t date_yyyymmdd,
    uint32_t league_season,
    uint8_t phase);
int kbo_captain_seed_startup_window_active(uint32_t date_yyyymmdd, uint32_t season);

#endif
