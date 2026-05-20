#include "ui_military_results_view_internal.h"
#include "../../../../core/dates/constants/kbo_date_constants.h"

LONG kbo_military_results_candidate_count(void)
{
    LONG count = g_kbo_military_draft_candidate_count;
    if (count < 0) { count = 0; }
    if (count > OOTP27_KBO_MAX_SPECIAL_HISTORY_KEYS) { count = OOTP27_KBO_MAX_SPECIAL_HISTORY_KEYS; }
    return count;
}

void kbo_military_results_add_year(uint16_t* years, int* year_count, uint32_t year)
{
    if (years == NULL || year_count == NULL || year < KBO_SEASON_YEAR_MIN || year > KBO_RECORD_YEAR_MAX) {
        return;
    }
    for (int i = 0; i < *year_count; i++) {
        if ((uint32_t)years[i] == year) {
            return;
        }
    }
    if (*year_count < OOTP27_KBO_MAX_SPECIAL_HISTORY_KEYS) {
        years[*year_count] = (uint16_t)year;
        (*year_count)++;
    }
}

void kbo_military_results_sort_years(uint16_t* years, int year_count)
{
    if (years == NULL || year_count <= 1) {
        return;
    }
    for (int left = 0; left < year_count; left++) {
        for (int right = left + 1; right < year_count; right++) {
            if (years[right] > years[left]) {
                uint16_t tmp = years[left];
                years[left] = years[right];
                years[right] = tmp;
            }
        }
    }
}
