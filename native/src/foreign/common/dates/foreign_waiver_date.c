#include <stdint.h>
#include <string.h>

#include "../../../core/dates/core_current_date.h"
#include "../../../core/dates/core_text_date.h"
#include "../../../core/dates/tick/current_date_tick_capture.h"
#include "foreign_waiver_date.h"

int kbo_days_in_month(uint32_t year, uint32_t month)
{
    static const int days[] = {0,31,28,31,30,31,30,31,31,30,31,30,31};
    if (month < 1u || month > 12u) {
        return 0;
    }
    if (month == 2u && kbo_is_leap_year(year)) {
        return 29;
    }
    return days[month];
}

uint32_t kbo_add_one_month_yyyymmdd(uint32_t yyyymmdd)
{
    uint32_t year = yyyymmdd / 10000u;
    uint32_t month = (yyyymmdd / 100u) % 100u;
    uint32_t day = yyyymmdd % 100u;
    if (year < 1980u || month < 1u || month > 12u || day < 1u || day > 31u) {
        return 0;
    }
    month++;
    if (month > 12u) {
        month = 1u;
        year++;
    }
    int dim = kbo_days_in_month(year, month);
    if (dim <= 0) {
        return 0;
    }
    if (day > (uint32_t)dim) {
        day = (uint32_t)dim;
    }
    return year * 10000u + month * 100u + day;
}

uint32_t kbo_add_days_yyyymmdd(uint32_t yyyymmdd, uint32_t add_days)
{
    uint32_t year = yyyymmdd / 10000u;
    uint32_t month = (yyyymmdd / 100u) % 100u;
    uint32_t day = yyyymmdd % 100u;
    if (year < 1980u || month < 1u || month > 12u || day < 1u || day > 31u) {
        return 0;
    }

    while (add_days > 0u) {
        int dim = kbo_days_in_month(year, month);
        if (dim <= 0) {
            return 0;
        }
        day++;
        if (day > (uint32_t)dim) {
            day = 1u;
            month++;
            if (month > 12u) {
                month = 1u;
                year++;
            }
        }
        add_days--;
    }
    return year * 10000u + month * 100u + day;
}

uint32_t kbo_add_years_yyyymmdd(uint32_t yyyymmdd, uint32_t add_years)
{
    uint32_t year = yyyymmdd / 10000u;
    uint32_t month = (yyyymmdd / 100u) % 100u;
    uint32_t day = yyyymmdd % 100u;
    if (year < 1800u || year > 2200u || month < 1u || month > 12u || day < 1u || day > 31u) {
        return 0u;
    }
    year += add_years;
    if (year > 2200u) {
        year = 2200u;
    }
    int dim = kbo_days_in_month(year, month);
    if (dim <= 0) {
        return 0u;
    }
    if (day > (uint32_t)dim) {
        day = (uint32_t)dim;
    }
    return year * 10000u + month * 100u + day;
}

int kbo_parse_yyyymmdd(const char* date_text, uint32_t* out_date)
{
    if (date_text == NULL || out_date == NULL) {
        return 0;
    }

    char ymd[9] = {0};
    size_t len = strlen(date_text);
    if (len >= 8
            && date_text[0] >= '0' && date_text[0] <= '9'
            && date_text[1] >= '0' && date_text[1] <= '9'
            && date_text[2] >= '0' && date_text[2] <= '9'
            && date_text[3] >= '0' && date_text[3] <= '9'
            && date_text[4] >= '0' && date_text[4] <= '9'
            && date_text[5] >= '0' && date_text[5] <= '9'
            && date_text[6] >= '0' && date_text[6] <= '9'
            && date_text[7] >= '0' && date_text[7] <= '9') {
        memcpy(ymd, date_text, 8);
    } else if (len >= 10 && date_text[4] == '-' && date_text[7] == '-') {
        ymd[0] = date_text[0]; ymd[1] = date_text[1]; ymd[2] = date_text[2]; ymd[3] = date_text[3];
        ymd[4] = date_text[5]; ymd[5] = date_text[6];
        ymd[6] = date_text[8]; ymd[7] = date_text[9];
    } else {
        return 0;
    }

    for (int i = 0; i < 8; i++) {
        if (ymd[i] < '0' || ymd[i] > '9') {
            return 0;
        }
    }

    *out_date = (uint32_t)(
        (ymd[0] - '0') * 10000000u +
        (ymd[1] - '0') * 1000000u +
        (ymd[2] - '0') * 100000u +
        (ymd[3] - '0') * 10000u +
        (ymd[4] - '0') * 1000u +
        (ymd[5] - '0') * 100u +
        (ymd[6] - '0') * 10u +
        (ymd[7] - '0'));
    return 1;
}

int kbo_get_foreign_waiver_current_yyyymmdd(uint32_t* out_date)
{
    return kbo_current_date_tick_latest_published_date(out_date);
}
