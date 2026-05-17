#ifndef KBOFIX_SRC_CORE_CORE_CURRENT_DATE_H_
#define KBOFIX_SRC_CORE_CORE_CURRENT_DATE_H_

#include <stdint.h>
#include <stddef.h>

int kbo_current_date_is_valid(uint32_t* out_year, uint32_t* out_month, uint32_t* out_day);
int kbo_get_current_yyyymmdd(uint32_t* out_date);
int kbo_current_year_relaxed(uint32_t* out_year);
int kbo_current_history_date(char* out, size_t out_size, uint32_t fallback_year, const char* event_type);

#endif
