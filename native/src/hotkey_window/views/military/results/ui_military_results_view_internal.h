#ifndef KBOFIX_SRC_HOTKEY_WINDOW_VIEWS_MILITARY_RESULTS_INTERNAL_H_
#define KBOFIX_SRC_HOTKEY_WINDOW_VIEWS_MILITARY_RESULTS_INTERNAL_H_

#include "../internal/ui_military_view_internal.h"
#include "../../../../core/dates/tick/current_date_tick_capture.h"

LONG kbo_military_results_candidate_count(void);
void kbo_military_results_add_year(uint16_t* years, int* year_count, uint32_t year);
void kbo_military_results_sort_years(uint16_t* years, int year_count);

#endif
