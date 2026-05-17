#ifndef KBO_CORE_TEXT_DATE_H
#define KBO_CORE_TEXT_DATE_H

#include <stddef.h>
#include <stdint.h>

int ascii_equals_ignore_case(const char* a, const char* b);
int kbo_is_leap_year(uint32_t year);
uint32_t kbo_date_serial(uint32_t year, uint32_t month, uint32_t day);
int kbo_yyyymmdd_valid(uint32_t yyyymmdd);
uint32_t kbo_yyyymmdd_add_days(uint32_t yyyymmdd, uint32_t add_days);
int kbo_format_history_date(char* out, size_t out_size, uint32_t year, uint32_t month, uint32_t day);

#endif
