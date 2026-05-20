#ifndef KBOFIX_SRC_FA_COMPENSATION_RECORDS_SQL_FA_COMPENSATION_RECORDS_SQL_STORE_H_
#define KBOFIX_SRC_FA_COMPENSATION_RECORDS_SQL_FA_COMPENSATION_RECORDS_SQL_STORE_H_

#include "../fa_compensation_records.h"

int kbo_fa_compensation_records_sql_path(char* out, size_t out_size);
int kbo_fa_compensation_records_sql_load(KboFaCompensationRecord* records, int max_records, int* out_count);
int kbo_fa_compensation_records_sql_replace_all(const KboFaCompensationRecord* records, int record_count);
int kbo_fa_compensation_records_sql_append(const KboFaCompensationRecord* rec);

#endif
