#ifndef KBOFIX_SRC_FA_SALARY_SNAPSHOT_SQL_FA_SALARY_SNAPSHOT_SQL_STORE_H_
#define KBOFIX_SRC_FA_SALARY_SNAPSHOT_SQL_FA_SALARY_SNAPSHOT_SQL_STORE_H_

#include "../state/salary_snapshot_state.h"

int kbo_fa_salary_snapshot_sql_path(char* out, size_t out_size);
int kbo_fa_salary_snapshot_sql_exists(uint32_t season);
int kbo_fa_salary_snapshot_sql_replace(
    const KboFaSalarySnapshotRow* rows,
    int row_count,
    uint32_t date,
    uint32_t season,
    uint32_t opening_day,
    uint32_t league_id,
    const char* source);
int kbo_fa_salary_snapshot_sql_load_grade_rows(
    uint32_t season,
    KboFaSalarySnapshotGrade* rows,
    int max_rows,
    int* out_count);

#endif
