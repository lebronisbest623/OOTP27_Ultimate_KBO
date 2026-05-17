#ifndef KBOFIX_SRC_FOREIGN_FOREIGN_WAIVER_DATE_H_
#define KBOFIX_SRC_FOREIGN_FOREIGN_WAIVER_DATE_H_

#include <stdint.h>

#include "../../../core/dates/core_current_date.h"

int kbo_days_in_month(uint32_t year, uint32_t month);
uint32_t kbo_add_one_month_yyyymmdd(uint32_t yyyymmdd);
uint32_t kbo_add_days_yyyymmdd(uint32_t yyyymmdd, uint32_t add_days);
uint32_t kbo_add_years_yyyymmdd(uint32_t yyyymmdd, uint32_t add_years);
int kbo_parse_yyyymmdd(const char* date_text, uint32_t* out_date);
int kbo_get_foreign_waiver_current_yyyymmdd(uint32_t* out_date);

#endif
