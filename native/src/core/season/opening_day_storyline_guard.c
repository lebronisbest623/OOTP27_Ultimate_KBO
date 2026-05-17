#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "opening_day_storyline_guard.h"

#include "../core_league_context_parts/api/league_context_lookup.h"
#include "../dates/core_current_date.h"
#include "../dates/core_text_date.h"
#include "../dates/tick/current_date_tick_capture.h"
#include "../logging/core_log.h"
#include "../../fa_salary_snapshot/paths/salary_snapshot_paths_dates.h"

int kbo_opening_day_storyline_guard_active(
    const char* source,
    uint32_t* out_date_key,
    uint32_t* out_opening_day)
{
    if (out_date_key != NULL) {
        *out_date_key = 0u;
    }
    if (out_opening_day != NULL) {
        *out_opening_day = 0u;
    }

    uint32_t date_key = 0u;
    if (!kbo_current_date_tick_latest_published_date(&date_key)) {
        return 0;
    }
    uint32_t year = date_key / 10000u;
    uint32_t month = (date_key / 100u) % 100u;
    uint32_t day = date_key % 100u;
    if (out_date_key != NULL) {
        *out_date_key = date_key;
    }

    uint32_t league_id = kbo_resolve_kbo_league_id();
    uintptr_t league_ptr = league_id != 0u ? kbo_find_league_ptr_from_id(league_id) : 0u;
    uint32_t opening_day = 0u;
    if (league_ptr == 0u || !kbo_fa_salary_snapshot_read_opening_day(league_ptr, &opening_day)) {
        return 0;
    }
    if (out_opening_day != NULL) {
        *out_opening_day = opening_day;
    }

    uint32_t opening_year = opening_day / 10000u;
    uint32_t opening_month = (opening_day / 100u) % 100u;
    uint32_t opening_day_of_month = opening_day % 100u;
    uint32_t current_serial = kbo_date_serial(year, month, day);
    uint32_t opening_serial = kbo_date_serial(opening_year, opening_month, opening_day_of_month);
    if (current_serial == 0u || opening_serial == 0u || current_serial != opening_serial) {
        return 0;
    }

    static volatile LONG log_count = 0;
    LONG slot = InterlockedIncrement(&log_count);
    if (slot <= 80 || (slot % 100) == 0) {
        kbo_log_runtimef(
            "KBO opening-day storyline guard active source=%s date=%u opening_day=%u league=%u reason=defer_runtime_roster_mutation_until_stock_news_pass_finishes slot=%ld",
            source != NULL ? source : "",
            date_key,
            opening_day,
            league_id,
            slot);
    }
    return 1;
}
