#ifndef KBOFIX_SRC_FA_COMPENSATION_PROTECTION_SQL_FA_COMPENSATION_PROTECTED_LISTS_SQL_STORE_H_
#define KBOFIX_SRC_FA_COMPENSATION_PROTECTION_SQL_FA_COMPENSATION_PROTECTED_LISTS_SQL_STORE_H_

#include <stddef.h>

#include "../fa_compensation_protected_lists.h"

int kbo_fa_compensation_protected_lists_sql_path(char* out, size_t out_size);
int kbo_fa_compensation_protected_lists_sql_append(
    const KboFaCompensationRecord* rec,
    uint32_t due_yyyymmdd,
    uint32_t generated_yyyymmdd,
    const KboFaProtectedCandidate* candidates,
    int candidate_count,
    const char* source);

#endif
