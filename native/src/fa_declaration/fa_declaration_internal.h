#ifndef KBOFIX_SRC_FA_DECLARATION_FA_DECLARATION_INTERNAL_H_
#define KBOFIX_SRC_FA_DECLARATION_FA_DECLARATION_INTERNAL_H_

#include <stddef.h>
#include <stdint.h>

#include "fa_declaration.h"
#include "../fa_market_classification/api/fa_market_classification.h"
#include "../fa_salary_snapshot/grading/salary_snapshot_grade_rows.h"

#define KBO_FA_DECLARATION_MAX 4096

typedef struct KboFaDeclarationCandidate {
    uintptr_t player_ptr;
    uint32_t player_id;
    uint32_t declaration_date;
    uint32_t season;
    uint32_t team_id;
    uint32_t league_id;
    uint32_t nation_id;
    uint16_t age;
    uint8_t contract_level;
    uint8_t dfa;
    uint8_t retired_flag;
    uint8_t declared;
    uint8_t from_market;
    int32_t salary;
    int32_t fa_demand;
    int32_t score;
    int32_t threshold;
    int16_t overall;
    int16_t talent;
    int16_t ratings;
    int16_t career;
    char player_name[96];
    char case_label[48];
    char grade[12];
    char reason[192];
    char decision_reason[160];
} KboFaDeclarationCandidate;

int kbo_fa_declaration_case_candidate(const char* case_label);
int kbo_fa_declaration_find_candidate(
    const KboFaDeclarationCandidate* candidates,
    int count,
    uint32_t player_id);
uint32_t kbo_fa_declaration_team_league(uint32_t team_id, uint32_t fallback_league_id);
int32_t kbo_fa_declaration_contract_salary_for_season(
    uint8_t* player,
    uint32_t season,
    int32_t* out_next_salary);
void kbo_fa_declaration_fill_from_player(
    KboFaDeclarationCandidate* candidate,
    uint8_t* player,
    uint32_t season);
void kbo_fa_declaration_apply_salary_grade(
    KboFaDeclarationCandidate* candidate,
    const KboFaSalarySnapshotGrade* grades,
    int grade_count);
void kbo_fa_declaration_decide(KboFaDeclarationCandidate* candidate);
int kbo_fa_declaration_parse_decision_fields(
    KboFaDeclarationDecision* decision,
    char fields[][128],
    int field_count);
int kbo_fa_declaration_add_market_candidate(
    const KboFaMarketClassification* row,
    uint32_t event_yyyymmdd,
    uint32_t season,
    uint32_t league_id,
    const KboFaSalarySnapshotGrade* grades,
    int grade_count,
    KboFaDeclarationCandidate* candidates,
    int* candidate_count);
int kbo_fa_declaration_append_csv(
    const KboFaDeclarationCandidate* candidates,
    int candidate_count,
    const char* source,
    char* out_path,
    size_t out_path_size);
int kbo_get_fa_declaration_csv_path(char* out, size_t out_size);

#endif
