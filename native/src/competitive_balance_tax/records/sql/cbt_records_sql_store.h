#ifndef KBOFIX_SRC_COMPETITIVE_BALANCE_TAX_RECORDS_SQL_CBT_RECORDS_SQL_STORE_H_
#define KBOFIX_SRC_COMPETITIVE_BALANCE_TAX_RECORDS_SQL_CBT_RECORDS_SQL_STORE_H_

#include "../cbt_records.h"

int kbo_cbt_records_sql_path(char* out, size_t out_size);
int kbo_cbt_records_sql_load(KboCbtRecord* records, int max, int* out_count);
int kbo_cbt_records_sql_replace_all(const KboCbtRecord* records, int count);

#endif
