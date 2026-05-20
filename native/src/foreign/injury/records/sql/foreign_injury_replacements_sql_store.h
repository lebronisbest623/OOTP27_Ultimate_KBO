#ifndef KBOFIX_SRC_FOREIGN_INJURY_RECORDS_SQL_FOREIGN_INJURY_REPLACEMENTS_SQL_STORE_H_
#define KBOFIX_SRC_FOREIGN_INJURY_RECORDS_SQL_FOREIGN_INJURY_REPLACEMENTS_SQL_STORE_H_

#include "../../internal/foreign_injury_internal.h"

int kbo_foreign_injury_replacements_sql_path(char* out, size_t out_size);
int kbo_foreign_injury_replacements_sql_load(
    KboForeignInjuryReplacement* out,
    int max_count,
    int* out_count);
int kbo_foreign_injury_replacements_sql_replace_all(
    const KboForeignInjuryReplacement* records,
    int record_count);

#endif
