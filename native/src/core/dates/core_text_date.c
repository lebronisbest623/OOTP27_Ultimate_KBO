#include "core_text_date.h"

#include <stdio.h>

static int kbo_yyyymmdd_parts(
    uint32_t yyyymmdd,
    uint32_t* out_year,
    uint32_t* out_month,
    uint32_t* out_day)
{
    uint32_t year = yyyymmdd / 10000u;
    uint32_t month = (yyyymmdd / 100u) % 100u;
    uint32_t day = yyyymmdd % 100u;
    if (out_year != NULL) {
        *out_year = year;
    }
    if (out_month != NULL) {
        *out_month = month;
    }
    if (out_day != NULL) {
        *out_day = day;
    }
    return year >= 1980u && year <= 2200u;
}

int ascii_equals_ignore_case(const char* a, const char* b)
{
    if (a == NULL || b == NULL) {
        return 0;
    }

    while (*a != '\0' && *b != '\0') {
        char ca = *a;
        char cb = *b;
        if (ca >= 'A' && ca <= 'Z') {
            ca = (char)(ca - 'A' + 'a');
        }
        if (cb >= 'A' && cb <= 'Z') {
            cb = (char)(cb - 'A' + 'a');
        }
        if (ca != cb) {
            return 0;
        }
        a++;
        b++;
    }

    return *a == '\0' && *b == '\0';
}

int kbo_is_leap_year(uint32_t year)
{
    return (year % 4u == 0u && year % 100u != 0u) || (year % 400u == 0u);
}

uint32_t kbo_date_serial(uint32_t year, uint32_t month, uint32_t day)
{
    static const uint16_t days_before_month[12] = {
        0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
    };
    static const uint8_t days_by_month[12] = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };
    if (year < 1 || month < 1 || month > 12 || day < 1 || day > 31) {
        return 0;
    }
    uint32_t max_day = days_by_month[month - 1u];
    if (month == 2u && kbo_is_leap_year(year)) {
        max_day = 29u;
    }
    if (day > max_day) {
        return 0;
    }

    uint32_t y = year - 1u;
    uint32_t serial = y * 365u + y / 4u - y / 100u + y / 400u;
    serial += days_before_month[month - 1u];
    if (month > 2 && kbo_is_leap_year(year)) {
        serial++;
    }
    serial += day;
    return serial;
}

int kbo_yyyymmdd_valid(uint32_t yyyymmdd)
{
    uint32_t year = 0u;
    uint32_t month = 0u;
    uint32_t day = 0u;
    if (!kbo_yyyymmdd_parts(yyyymmdd, &year, &month, &day)) {
        return 0;
    }
    return kbo_date_serial(year, month, day) != 0u;
}

static uint32_t kbo_yyyymmdd_from_serial(uint32_t serial)
{
    uint32_t min_serial = kbo_date_serial(1980u, 1u, 1u);
    uint32_t max_serial = kbo_date_serial(2200u, 12u, 31u);
    if (serial < min_serial || serial > max_serial) {
        return 0u;
    }

    uint32_t low = 1980u;
    uint32_t high = 2200u;
    while (low < high) {
        uint32_t mid = low + (high - low + 1u) / 2u;
        if (kbo_date_serial(mid, 1u, 1u) <= serial) {
            low = mid;
        } else {
            high = mid - 1u;
        }
    }

    uint32_t year = low;
    uint32_t day_of_year = serial - kbo_date_serial(year, 1u, 1u) + 1u;
    static const uint8_t days_by_month[12] = {
        31u, 28u, 31u, 30u, 31u, 30u, 31u, 31u, 30u, 31u, 30u, 31u
    };
    for (uint32_t month = 1u; month <= 12u; month++) {
        uint32_t max_day = days_by_month[month - 1u];
        if (month == 2u && kbo_is_leap_year(year)) {
            max_day = 29u;
        }
        if (day_of_year <= max_day) {
            return year * 10000u + month * 100u + day_of_year;
        }
        day_of_year -= max_day;
    }

    return 0u;
}

uint32_t kbo_yyyymmdd_add_days(uint32_t yyyymmdd, uint32_t add_days)
{
    uint32_t year = 0u;
    uint32_t month = 0u;
    uint32_t day = 0u;
    if (!kbo_yyyymmdd_parts(yyyymmdd, &year, &month, &day)) {
        return 0u;
    }

    uint32_t serial = kbo_date_serial(year, month, day);
    if (serial == 0u || UINT32_MAX - serial < add_days) {
        return 0u;
    }
    return kbo_yyyymmdd_from_serial(serial + add_days);
}

int kbo_format_history_date(char* out, size_t out_size, uint32_t year, uint32_t month, uint32_t day)
{
    static const uint8_t month_days_common[12] = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };

    if (out == NULL || out_size < 9 || year < 1800 || year > 2200
            || month < 1 || month > 12 || day < 1) {
        return 0;
    }

    uint32_t max_day = month_days_common[month - 1u];
    if (month == 2 && kbo_is_leap_year(year)) {
        max_day = 29;
    }
    if (day > max_day) {
        return 0;
    }

    snprintf(out, out_size, "%04u%02u%02u", year, month, day);
    return 1;
}
