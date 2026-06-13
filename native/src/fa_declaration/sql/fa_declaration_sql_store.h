#ifndef KBOFIX_SRC_FA_DECLARATION_SQL_FA_DECLARATION_SQL_STORE_H_
#define KBOFIX_SRC_FA_DECLARATION_SQL_FA_DECLARATION_SQL_STORE_H_

#include "../fa_declaration_internal.h"

int kbo_fa_declaration_sql_path(char* out, size_t out_size);
int kbo_fa_declaration_sql_append_candidates(
    const KboFaDeclarationCandidate* candidates,
    int candidate_count,
    const char* source);
int kbo_fa_declaration_sql_find_latest_decision(
    uint32_t player_id,
    uint32_t season,
    KboFaDeclarationDecision* out_decision);
int kbo_fa_declaration_sql_load_season_decisions(
    uint32_t season,
    KboFaDeclarationDecision* decisions,
    int max_decisions,
    int* out_count);
int kbo_fa_declaration_sql_load_report_rows(
    KboFaDeclarationReportRow* rows,
    int max_rows,
    int* out_count);

#endif
