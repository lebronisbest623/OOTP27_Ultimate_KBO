#include <stddef.h>
#include <stdint.h>

#include "core_current_date.h"
#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../runtime_memory/runtime_memory.h"
#include "core_text_date.h"
#include "constants/kbo_date_constants.h"
/* Core current-date readers. */

static int kbo_current_date_components_valid(uint32_t year, uint32_t month, uint32_t day)
{
    static const uint8_t month_days_common[12] = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };
    if (year < KBO_HISTORY_YEAR_MIN || year > KBO_SIM_YEAR_MAX || month < 1u || month > 12u || day < 1u) {
        return 0;
    }
    uint32_t max_day = month_days_common[month - 1u];
    if (month == 2u && kbo_is_leap_year(year)) {
        max_day = 29u;
    }
    return day <= max_day;
}



int kbo_current_date_is_valid(uint32_t* out_year, uint32_t* out_month, uint32_t* out_day)
{
    if (out_year != NULL) {
        *out_year = 0;
    }
    if (out_month != NULL) {
        *out_month = 0;
    }
    if (out_day != NULL) {
        *out_day = 0;
    }

    uintptr_t global = get_ootp_global_database();
    if (global == 0 || !memory_range_readable((void*)(global + OOTP27_GLOBAL_CURRENT_DATE_OFFSET), sizeof(uintptr_t))) {
        return 0;
    }

    uintptr_t current_date = *(uintptr_t*)(global + OOTP27_GLOBAL_CURRENT_DATE_OFFSET);
    if (current_date == 0 || !memory_range_readable((void*)current_date, OOTP27_CURRENT_DATE_DAY_OFFSET + sizeof(uint8_t))) {
        return 0;
    }

    uint32_t year  = *(uint16_t*)(current_date + OOTP27_CURRENT_DATE_YEAR_OFFSET);
    uint32_t month = *(uint8_t*)(current_date  + OOTP27_CURRENT_DATE_MONTH_OFFSET);
    uint32_t day   = *(uint8_t*)(current_date  + OOTP27_CURRENT_DATE_DAY_OFFSET);

    if (!kbo_current_date_components_valid(year, month, day)) {
        return 0;
    }

    if (out_year  != NULL) { *out_year  = year;  }
    if (out_month != NULL) { *out_month = month; }
    if (out_day   != NULL) { *out_day   = day;   }
    return 1;
}

int kbo_get_current_yyyymmdd(uint32_t* out_date)
{
    if (out_date == NULL) {
        return 0;
    }

    uint32_t year = 0;
    uint32_t month = 0;
    uint32_t day = 0;
    if (!kbo_current_date_is_valid(&year, &month, &day)) {
        return 0;
    }
    *out_date = year * 10000u + month * 100u + day;
    return 1;
}

int kbo_current_year_relaxed(uint32_t* out_year)
{
    if (out_year != NULL) {
        *out_year = 0;
    }

    uint32_t year = 0;
    uint32_t month = 0;
    uint32_t day = 0;
    if (kbo_current_date_is_valid(&year, &month, &day)) {
        if (out_year != NULL) {
            *out_year = year;
        }
        return 1;
    }

    uintptr_t global = get_ootp_global_database();
    if (global == 0 || !memory_range_readable((void*)(global + OOTP27_GLOBAL_CURRENT_DATE_OFFSET), sizeof(uintptr_t))) {
        return 0;
    }

    uintptr_t current_date = *(uintptr_t*)(global + OOTP27_GLOBAL_CURRENT_DATE_OFFSET);
    if (current_date == 0 || !memory_range_readable((void*)current_date, OOTP27_CURRENT_DATE_YEAR_OFFSET + sizeof(uint16_t))) {
        return 0;
    }

    year = *(uint16_t*)(current_date + OOTP27_CURRENT_DATE_YEAR_OFFSET);
    if (year < (int)KBO_HISTORY_YEAR_MIN || year > (int)KBO_RECORD_YEAR_MAX) {
        return 0;
    }

    if (out_year != NULL) {
        *out_year = year;
    }
    return 1;
}

int kbo_current_history_date(char* out, size_t out_size, uint32_t fallback_year, const char* event_type)
{
    uintptr_t global = get_ootp_global_database();
    if (global != 0 && memory_range_readable((void*)(global + OOTP27_GLOBAL_CURRENT_DATE_OFFSET), sizeof(uintptr_t))) {
        uintptr_t current_date = *(uintptr_t*)(global + OOTP27_GLOBAL_CURRENT_DATE_OFFSET);
        if (current_date != 0
                && memory_range_readable((void*)current_date, OOTP27_CURRENT_DATE_DAY_OFFSET + sizeof(uint8_t))) {
            uint32_t year  = *(uint16_t*)(current_date + OOTP27_CURRENT_DATE_YEAR_OFFSET);
            uint32_t month = *(uint8_t*)(current_date  + OOTP27_CURRENT_DATE_MONTH_OFFSET);
            uint32_t day   = *(uint8_t*)(current_date  + OOTP27_CURRENT_DATE_DAY_OFFSET);
            if (kbo_format_history_date(out, out_size, year, month, day)) {
                return 1;
            }
        }
    }

    (void)event_type;
    return kbo_format_history_date(out, out_size, fallback_year, 1, 1);
}

