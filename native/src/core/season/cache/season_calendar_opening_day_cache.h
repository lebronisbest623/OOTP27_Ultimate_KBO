#ifndef KBOFIX_SRC_CORE_SEASON_CACHE_SEASON_CALENDAR_OPENING_DAY_CACHE_H_
#define KBOFIX_SRC_CORE_SEASON_CACHE_SEASON_CALENDAR_OPENING_DAY_CACHE_H_

#include "../season_calendar.h"
#include "../sql/season_calendar_sql_store.h"

#include <stddef.h>
#include <stdint.h>

int kbo_season_calendar_path(char* out, size_t out_size);
int kbo_season_calendar_opening_day_valid(uint32_t season, uint32_t opening_day);
void kbo_season_calendar_opening_day_cache_store(
    uint32_t league_id,
    uint32_t season,
    uint32_t opening_day,
    uint32_t observed_date,
    const char* source);
void kbo_season_calendar_sanitize_source(const char* source, char* out, size_t out_size);
int kbo_season_calendar_load_opening_day_details(
    uint32_t league_id,
    uint32_t season,
    uint32_t* out_opening_day,
    uint32_t* out_observed_date,
    char* out_source,
    size_t out_source_size);

#endif


