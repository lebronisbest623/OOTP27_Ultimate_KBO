#ifndef KBOFIX_SRC_COMPETITIVE_BALANCE_TAX_EXCEPTIONS_SQL_CBT_EXCEPTIONS_SQL_STORE_H_
#define KBOFIX_SRC_COMPETITIVE_BALANCE_TAX_EXCEPTIONS_SQL_CBT_EXCEPTIONS_SQL_STORE_H_

#include "../cbt_exceptions.h"

int kbo_cbt_exceptions_sql_load_designations(KboCbtExceptionDesignation* rows, int max, int* out_count);
int kbo_cbt_exceptions_sql_replace_designations(const KboCbtExceptionDesignation* rows, int count);

#endif
