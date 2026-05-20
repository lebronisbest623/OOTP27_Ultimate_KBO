#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>
#include <string.h>

#include "../../../../core/dates/core_text_date.h"
#include "../../../../core/dates/tick/current_date_tick_capture.h"
#include "ui_date_format.h"

int kbo_hub_days_until_yyyymmdd(uint32_t yyyymmdd)
{
    if (yyyymmdd == 0u) {
        return -1;
    }
    uint32_t year = 0;
    uint32_t month = 0;
    uint32_t day = 0;
    if (!kbo_current_date_tick_latest_components(&year, &month, &day)) {
        return -1;
    }
    uint32_t today_serial = kbo_date_serial(year, month, day);
    uint32_t end_serial = kbo_date_serial(yyyymmdd / 10000u, (yyyymmdd / 100u) % 100u, yyyymmdd % 100u);
    if (today_serial == 0u || end_serial == 0u) {
        return -1;
    }
    return end_serial > today_serial ? (int)(end_serial - today_serial) : 0;
}

const char* kbo_hub_weekday_abbrev(int weekday)
{
    static const char* names[] = { "SUN.", "MON.", "TUE.", "WED.", "THU.", "FRI.", "SAT." };
    if (weekday < 0 || weekday > 6) {
        return "";
    }
    return names[weekday];
}

static const char* kbo_hub_month_abbrev(uint32_t month)
{
    static const char* names[] = {
        "", "JAN.", "FEB.", "MAR.", "APR.", "MAY", "JUN.",
        "JUL.", "AUG.", "SEP.", "OCT.", "NOV.", "DEC."
    };
    if (month < 1u || month > 12u) {
        return "";
    }
    return names[month];
}

static const char* kbo_hub_day_suffix(uint32_t day)
{
    if ((day % 100u) >= 11u && (day % 100u) <= 13u) {
        return "TH";
    }
    switch (day % 10u) {
    case 1u: return "ST";
    case 2u: return "ND";
    case 3u: return "RD";
    default: return "TH";
    }
}

void kbo_hub_format_ootp_date(uint32_t year, uint32_t month, uint32_t day, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';
    if (year < 1900u || month < 1u || month > 12u || day < 1u || day > 31u) {
        snprintf(out, out_size, "DATE UNKNOWN");
        return;
    }

    SYSTEMTIME st;
    memset(&st, 0, sizeof(st));
    st.wYear = (WORD)year;
    st.wMonth = (WORD)month;
    st.wDay = (WORD)day;
    FILETIME ft;
    if (!SystemTimeToFileTime(&st, &ft)) {
        snprintf(out, out_size, "DATE UNKNOWN");
        return;
    }

    ULARGE_INTEGER value;
    value.LowPart = ft.dwLowDateTime;
    value.HighPart = ft.dwHighDateTime;
    uint64_t days_since_1601 = value.QuadPart / 10000000ull / 86400ull;
    int weekday = (int)((days_since_1601 + 1ull) % 7ull);
    snprintf(out, out_size, "%s %s %u%s, %u",
        kbo_hub_weekday_abbrev(weekday),
        kbo_hub_month_abbrev(month),
        day,
        kbo_hub_day_suffix(day),
        year);
}
