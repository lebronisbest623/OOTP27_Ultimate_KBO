#include "../../current_date_tick_capture.h"

#include "../../../core_current_date.h"
#include "../../../core_text_date.h"

int kbo_current_date_tick_latest_published_date(uint32_t* out_date)
{
    if (out_date != NULL) {
        *out_date = 0u;
    }

    uint32_t date = (uint32_t)InterlockedCompareExchange(
        &g_kbo_current_date_tick_last_published_date,
        0,
        0);
    if (!kbo_yyyymmdd_valid(date)) {
        return 0;
    }
    if (out_date != NULL) {
        *out_date = date;
    }
    return 1;
}

int kbo_current_date_tick_latest_components(
    uint32_t* out_year,
    uint32_t* out_month,
    uint32_t* out_day)
{
    if (out_year != NULL) {
        *out_year = 0u;
    }
    if (out_month != NULL) {
        *out_month = 0u;
    }
    if (out_day != NULL) {
        *out_day = 0u;
    }

    uint32_t date = 0u;
    if (!kbo_current_date_tick_latest_published_date(&date)) {
        return 0;
    }

    uint32_t year = date / 10000u;
    uint32_t month = (date / 100u) % 100u;
    uint32_t day = date % 100u;
    if (out_year != NULL) {
        *out_year = year;
    }
    if (out_month != NULL) {
        *out_month = month;
    }
    if (out_day != NULL) {
        *out_day = day;
    }
    return 1;
}

int kbo_current_date_tick_latest_history_date(
    char* out,
    size_t out_size,
    uint32_t fallback_year)
{
    uint32_t year = 0u;
    uint32_t month = 0u;
    uint32_t day = 0u;
    if (kbo_current_date_tick_latest_components(&year, &month, &day)
            && kbo_format_history_date(out, out_size, year, month, day)) {
        return 1;
    }
    if (fallback_year == 0u) {
        return 0;
    }
    return kbo_format_history_date(out, out_size, fallback_year, 1u, 1u);
}

int kbo_current_date_tick_latest_boundary_context(KboDateBoundaryContext* out_context)
{
    return kbo_date_boundary_latest(out_context);
}
