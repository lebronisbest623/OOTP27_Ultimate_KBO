#ifndef KBOFIX_SRC_MILITARY_SERVICE_SELECTION_RESULTS_SQL_MILITARY_SELECTION_RESULTS_SQL_STORE_H_
#define KBOFIX_SRC_MILITARY_SERVICE_SELECTION_RESULTS_SQL_MILITARY_SELECTION_RESULTS_SQL_STORE_H_

#include "../military_selection_results_store.h"

int kbo_military_selection_results_sql_path(char* out, size_t out_size);
int kbo_military_selection_results_sql_load(
    KboMilitarySelectionResultEntry* out,
    int max_count,
    int* out_count);
int kbo_military_selection_results_sql_upsert_many(
    const KboMilitarySelectionResultEntry* entries,
    int entry_count);

#endif
