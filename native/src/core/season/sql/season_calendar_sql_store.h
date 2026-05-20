#ifndef KBOFIX_SRC_CORE_SEASON_SQL_SEASON_CALENDAR_SQL_STORE_H_
#define KBOFIX_SRC_CORE_SEASON_SQL_SEASON_CALENDAR_SQL_STORE_H_

#include <stdint.h>

typedef struct KboSeasonCalendarOpeningDayRow {
    uint32_t league_id;
    uint32_t season;
    uint32_t opening_day;
    uint32_t observed_date;
    char source[64];
} KboSeasonCalendarOpeningDayRow;

int kbo_season_calendar_sql_load_opening_day(
    uint32_t league_id,
    uint32_t season,
    KboSeasonCalendarOpeningDayRow* out_row);
int kbo_season_calendar_sql_store_opening_day(
    uint32_t league_id,
    uint32_t season,
    uint32_t opening_day,
    uint32_t observed_date,
    const char* source);

#endif
