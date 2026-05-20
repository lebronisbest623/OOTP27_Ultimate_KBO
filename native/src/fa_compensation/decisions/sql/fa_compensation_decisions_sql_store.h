#ifndef KBOFIX_SRC_FA_COMPENSATION_DECISIONS_SQL_FA_COMPENSATION_DECISIONS_SQL_STORE_H_
#define KBOFIX_SRC_FA_COMPENSATION_DECISIONS_SQL_FA_COMPENSATION_DECISIONS_SQL_STORE_H_

#include <stddef.h>

#include "../fa_compensation_decisions.h"

int kbo_fa_compensation_decisions_sql_path(char* out, size_t out_size);
int kbo_fa_compensation_decisions_sql_append_player_decision(
    const KboFaCompensationRecord* rec,
    uint32_t due_yyyymmdd,
    uint32_t decided_yyyymmdd,
    const KboFaProtectedCandidate* selected,
    int unprotected_candidate_count,
    const char* source);
int kbo_fa_compensation_decisions_sql_append_cash_only_decision(
    const KboFaCompensationRecord* rec,
    uint32_t due_yyyymmdd,
    uint32_t decided_yyyymmdd,
    const char* source);
int kbo_fa_compensation_decisions_sql_load_latest(
    uint32_t fa_player_id,
    KboFaCompensationDecisionRow* out);

#endif
