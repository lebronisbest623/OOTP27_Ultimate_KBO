#ifndef KBOFIX_SRC_CORE_SEASON_SEASON_CALENDAR_H_
#define KBOFIX_SRC_CORE_SEASON_SEASON_CALENDAR_H_

#include <stddef.h>
#include <stdint.h>

int kbo_season_calendar_read_league_opening_day(uintptr_t league_ptr, uint32_t* out_date);
int kbo_season_calendar_load_schedule_opening_day(uint32_t season, uint32_t* out_opening_day);
int kbo_season_calendar_store_opening_day(
    uint32_t league_id,
    uint32_t season,
    uint32_t opening_day,
    uint32_t observed_date,
    const char* source);
int kbo_season_calendar_load_opening_day(
    uint32_t league_id,
    uint32_t season,
    uint32_t* out_opening_day);
int kbo_season_calendar_resolve_opening_day(
    uint32_t league_id,
    uint32_t season,
    uint32_t today,
    uint32_t* out_opening_day);
int kbo_season_calendar_resolve_opening_day_with_league_ptr(
    uint32_t league_id,
    uint32_t season,
    uint32_t today,
    uintptr_t league_ptr,
    const char* source,
    uint32_t* out_opening_day);

#endif
