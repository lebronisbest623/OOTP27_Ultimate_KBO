#ifndef KBOFIX_SRC_FOREIGN_RIGHTS_SQL_FOREIGN_WAIVER_RIGHTS_SQL_STORE_H_
#define KBOFIX_SRC_FOREIGN_RIGHTS_SQL_FOREIGN_WAIVER_RIGHTS_SQL_STORE_H_

#include "../query/foreign_waiver_rights_query.h"

int kbo_foreign_waiver_rights_sql_load(
    KboForeignWaiverRetention* out_records,
    int capacity,
    int* out_count,
    int* out_deduped);
int kbo_foreign_waiver_rights_sql_replace_all(
    const KboForeignWaiverRetention* records,
    int count);

#endif
