#include "captain_season.h"
#include "../../core/dates/constants/kbo_date_constants.h"

static int kbo_captain_year_plausible(uint32_t year)
{
    return year >= KBO_SEASON_YEAR_MIN && year <= KBO_SIM_YEAR_MAX;
}

static int kbo_captain_date_plausible(uint32_t date_yyyymmdd)
{
    uint32_t year = date_yyyymmdd / 10000u;
    uint32_t month = (date_yyyymmdd / 100u) % 100u;
    uint32_t day = date_yyyymmdd % 100u;
    if (!kbo_captain_year_plausible(year) || month == 0u || month > 12u || day == 0u || day > 31u) {
        return 0;
    }
    if ((month == 4u || month == 6u || month == 9u || month == 11u) && day > 30u) {
        return 0;
    }
    if (month == 2u) {
        int leap = (year % 4u) == 0u && ((year % 100u) != 0u || (year % 400u) == 0u);
        return day <= (leap ? 29u : 28u);
    }
    return 1;
}

uint32_t kbo_captain_effective_season(uint32_t date_yyyymmdd, uint32_t league_season)
{
    uint32_t date_year = date_yyyymmdd / 10000u;
    int date_valid = kbo_captain_date_plausible(date_yyyymmdd);
    int league_valid = kbo_captain_year_plausible(league_season);

    if (!date_valid) {
        return league_valid ? league_season : 0u;
    }
    if (!league_valid) {
        return date_year;
    }
    if (date_year > league_season) {
        return date_year;
    }
    return league_season;
}

int kbo_captain_calendar_season_recovery_active(
    uint32_t date_yyyymmdd,
    uint32_t league_season,
    uint8_t phase)
{
    if (!kbo_captain_date_plausible(date_yyyymmdd)
            || !kbo_captain_year_plausible(league_season)) {
        return 0;
    }
    if ((date_yyyymmdd / 10000u) <= league_season) {
        return 0;
    }
    return phase != 2u && phase != 3u;
}

int kbo_captain_calendar_preseason_window_active(
    uint32_t date_yyyymmdd,
    uint32_t league_season,
    uint8_t phase)
{
    if (!kbo_captain_date_plausible(date_yyyymmdd)
            || !kbo_captain_year_plausible(league_season)) {
        return 0;
    }
    if (phase == 2u || phase == 3u) {
        return 0;
    }

    uint32_t date_year = date_yyyymmdd / 10000u;
    uint32_t month_day = date_yyyymmdd % 10000u;
    uint32_t effective_season = kbo_captain_effective_season(date_yyyymmdd, league_season);
    if (date_year != effective_season) {
        return 0;
    }

    return month_day >= KBO_CAPTAIN_PRESEASON_START_MONTH_DAY
        && month_day <= KBO_CAPTAIN_PRESEASON_END_MONTH_DAY;
}

int kbo_captain_preseason_first_day_active(
    uint32_t date_yyyymmdd,
    uint32_t league_season,
    uint8_t phase)
{
    if (!kbo_captain_date_plausible(date_yyyymmdd) || phase != 2u) {
        return 0;
    }

    uint32_t effective_season = kbo_captain_effective_season(date_yyyymmdd, league_season);
    return (date_yyyymmdd / 10000u) == effective_season
        && (date_yyyymmdd % 10000u) == KBO_CAPTAIN_PRESEASON_START_MONTH_DAY;
}

int kbo_captain_seed_startup_window_active(uint32_t date_yyyymmdd, uint32_t season)
{
    if (!kbo_captain_date_plausible(date_yyyymmdd) || !kbo_captain_year_plausible(season)) {
        return 0;
    }

    uint32_t date_year = date_yyyymmdd / 10000u;
    uint32_t month_day = date_yyyymmdd % 10000u;
    return date_year == season
        && month_day >= KBO_CAPTAIN_PRESEASON_START_MONTH_DAY
        && month_day <= KBO_CAPTAIN_PRESEASON_END_MONTH_DAY;
}
