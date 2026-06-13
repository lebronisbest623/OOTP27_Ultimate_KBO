#ifndef KBOFIX_SRC_FA_DECLARATION_FA_DECLARATION_H_
#define KBOFIX_SRC_FA_DECLARATION_FA_DECLARATION_H_

#include <stdint.h>
#include <stddef.h>

#define KBO_FA_DECLARATION_REPORT_MAX 4096

typedef struct KboFaDeclarationDecision {
    uint32_t player_id;
    uint32_t declaration_date;
    uint32_t season;
    uint32_t declared;
    uint32_t team_id;
    uint32_t league_id;
    uint8_t contract_level;
    int32_t salary;
    int32_t fa_demand;
    int32_t score;
} KboFaDeclarationDecision;

typedef struct KboFaDeclarationReportRow {
    uint32_t player_id;
    uint32_t declaration_date;
    uint32_t season;
    uint32_t declared;
    uint32_t team_id;
    uint32_t league_id;
    uint32_t nation_id;
    uint16_t age;
    uint8_t contract_level;
    int32_t salary;
    int32_t fa_demand;
    int32_t score;
    int32_t threshold;
    int16_t overall;
    int16_t talent;
    int16_t ratings;
    int16_t career;
    char player_name[96];
    char grade[12];
    char case_label[48];
    char source[48];
    char reason[192];
    char decision_reason[160];
} KboFaDeclarationReportRow;

int kbo_handle_fa_declaration_event(uint32_t event_yyyymmdd, const char* source);
int kbo_load_fa_declaration_report_rows(
    KboFaDeclarationReportRow* rows,
    int max_rows,
    char* out_path,
    size_t out_path_size);
int kbo_fa_declaration_find_latest_decision(
    uint32_t player_id,
    uint32_t season,
    KboFaDeclarationDecision* out_decision);
int kbo_fa_declaration_load_season_decisions(
    uint32_t season,
    KboFaDeclarationDecision* decisions,
    int max_decisions);
uint32_t kbo_fa_declaration_retained_contract_season(uint32_t declaration_season);
int kbo_fa_declaration_repair_retained_contract_salary(
    uint8_t* player,
    uint32_t season,
    const KboFaDeclarationDecision* decision,
    int32_t minimum_salary,
    const char* source);
int kbo_fa_declaration_repair_retained_contracts_for_season(
    uint32_t season,
    const char* source);

#endif
