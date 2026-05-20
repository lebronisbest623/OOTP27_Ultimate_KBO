#ifndef KBOFIX_SRC_FA_FILING_RECORDS_SQL_FA_FILING_SQL_STORE_H_
#define KBOFIX_SRC_FA_FILING_RECORDS_SQL_FA_FILING_SQL_STORE_H_

#include "../../fa_filing.h"

int kbo_fa_filing_sql_path(char* out, size_t out_size);
int kbo_fa_filing_sql_load(KboFaFilingRecord* rows, int max_rows, int* out_count);
int kbo_fa_filing_sql_replace_all(const KboFaFilingRecord* rows, int row_count);

#endif
