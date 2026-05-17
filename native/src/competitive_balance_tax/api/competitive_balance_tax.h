#ifndef KBOFIX_SRC_COMPETITIVE_BALANCE_TAX_API_H_
#define KBOFIX_SRC_COMPETITIVE_BALANCE_TAX_API_H_

#include <stdint.h>

int kbo_process_competitive_balance_tax(uint32_t season, const char* source);
int kbo_process_competitive_balance_tax_for_date(uint32_t season, uint32_t news_yyyymmdd, const char* source);

#endif
