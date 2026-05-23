#ifndef KBOFIX_SRC_FA_COMPENSATION_RECORDS_SQL_FA_COMPENSATION_RECORDS_SQL_STORE_H_
#define KBOFIX_SRC_FA_COMPENSATION_RECORDS_SQL_FA_COMPENSATION_RECORDS_SQL_STORE_H_

#include <stdint.h>

#include "../fa_compensation_records.h"

typedef struct KboFaCompensationDueSummary {
    int candidate_rows;
    int actionable_rows;
    int future_protected_rows;
    uint32_t next_protected_due_yyyymmdd;
} KboFaCompensationDueSummary;

int kbo_fa_compensation_records_sql_path(char* out, size_t out_size);
int kbo_fa_compensation_records_sql_load(KboFaCompensationRecord* records, int max_records, int* out_count);
int kbo_fa_compensation_records_sql_due_summary(
    uint32_t today,
    uint32_t protected_list_due_days,
    KboFaCompensationDueSummary* out);
int kbo_fa_compensation_records_sql_replace_all(const KboFaCompensationRecord* records, int record_count);
int kbo_fa_compensation_records_sql_append(const KboFaCompensationRecord* rec);

#endif
