#ifndef KBOFIX_SRC_MILITARY_SERVICE_SEED_REGISTRY_SQL_MILITARY_RESOLVED_SEED_SQL_STORE_H_
#define KBOFIX_SRC_MILITARY_SERVICE_SEED_REGISTRY_SQL_MILITARY_RESOLVED_SEED_SQL_STORE_H_

#include "../military_seed_registry.h"

int kbo_military_resolved_seed_sql_path(char* out, size_t out_size);
int kbo_military_resolved_seed_sql_load(
    KboMilitaryServiceSeed* out,
    int max_count,
    int* out_count);
int kbo_military_resolved_seed_sql_replace_all(
    const KboMilitaryServiceSeed* seeds,
    int seed_count);

#endif
