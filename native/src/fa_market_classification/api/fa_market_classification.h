#ifndef KBOFIX_SRC_FA_MARKET_CLASSIFICATION_FA_MARKET_CLASSIFICATION_H_
#define KBOFIX_SRC_FA_MARKET_CLASSIFICATION_FA_MARKET_CLASSIFICATION_H_

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stddef.h>

#include "../../fa_rules/fa_rules.h"
#include "../../fa_salary_snapshot/state/salary_snapshot_state.h"

#define KBO_FA_MARKET_CLASSIFICATION_MAX 8192
#define KBO_FA_MARKET_SEED_MAX 1024
#define KBO_FA_MARKET_INDEPENDENT_LEAGUE_ID 200u
#define KBO_FA_MARKET_HISTORY_TEXT_MAX 192

typedef struct KboFaMarketSeedCase {
    uint32_t player_id;
    char case_label[48];
    char grade[12];
    char note[128];
} KboFaMarketSeedCase;

typedef struct KboFaMarketClassification {
    uint32_t player_id;
    uint32_t nation_id;
    uint32_t current_team_id;
    uint32_t active_team_id;
    uint32_t original_team_id;
    uint32_t original_league_id;
    uint32_t current_league_id;
    uint32_t draft_league_id;
    uint32_t rights_team_id;
    uint16_t age;
    uint8_t contract_level;
    uint8_t dfa;
    uint8_t foreign_player;
    uint8_t retired_flag;
    uint8_t position_group;
    uint8_t position_role;
    uint8_t draft_class;
    uint8_t draft_subtype;
    uint8_t draft_eligible;
    uint8_t draft_extra;
    uint8_t generation_flags;
    uint8_t generation_context;
    uint8_t generation_grade;
    uint8_t generation_special;
    int32_t fa_demand;
    int32_t fa_grade_salary;
    uint32_t fa_grade_overall_rank;
    uint32_t fa_grade_team_rank;
    uint32_t fa_grade_snapshot_team_id;
    uint32_t fa_grade_snapshot_date;
    uint32_t fa_grade_opening_day;
    uint32_t fa_filing_date;
    uint32_t fa_filing_season;
    uint8_t fa_grade_auto;
    uint8_t fa_grade_team_changed_review;
    char player_name[96];
    char case_label[48];
    char grade[12];
    char fa_grade_flag[48];
    char reason[224];
} KboFaMarketClassification;

typedef struct KboFaMarketScanSummary {
    uint32_t league_id;
    uint32_t today_yyyymmdd;
    uint32_t current_year;
    int scanned;
    int candidates;
    int rows;
    int truncated;
    int seed_count;
    int salary_snapshot_count;
    int csv_written;
    char csv_path[MAX_PATH];
    char seed_path[MAX_PATH];
    char salary_snapshot_path[MAX_PATH];
} KboFaMarketScanSummary;

typedef struct KboFaMarketHistoryCase {
    uint32_t player_id;
    int found;
    int became_free_agent;
    int undrafted_free_agent;
    int released;
    char history_date[16];
    char history_text[KBO_FA_MARKET_HISTORY_TEXT_MAX];
} KboFaMarketHistoryCase;

#include "../seeds/fa_market_seed_cases.h"

int kbo_load_fa_market_history_cases(
    KboFaMarketClassification* rows,
    int row_count,
    KboFaMarketHistoryCase* histories,
    int max_histories);
int kbo_fa_market_overlay_filing_history_cases(
    KboFaMarketClassification* rows,
    int row_count,
    KboFaMarketHistoryCase* histories,
    int max_histories);
void kbo_classify_fa_market_row(
    KboFaMarketClassification* row,
    const KboFaMarketSeedCase* seeds,
    int seed_count,
    const KboFaMarketHistoryCase* history_case,
    uint32_t today);
void kbo_fa_market_apply_salary_snapshot_grade(
    KboFaMarketClassification* row,
    const KboFaSalarySnapshotGrade* salary_grades,
    int salary_grade_count,
    const KboFaRules* rules);
void kbo_fa_market_format_salary(int32_t salary, char* out, size_t out_size);
const char* kbo_fa_market_display_grade(const char* grade);
uint32_t kbo_fa_market_display_grade_sort_rank(const char* grade);
uint32_t kbo_fa_market_display_team_id(const KboFaMarketClassification* row);
const char* kbo_fa_market_display_case_label(const char* case_label);
int kbo_collect_fa_market_classifications(
    uint32_t requested_league_id,
    KboFaMarketClassification* rows,
    int max_rows,
    KboFaMarketScanSummary* summary,
    int write_csv,
    const char* source);
int kbo_collect_fa_market_classifications_page(
    uint32_t requested_league_id,
    KboFaMarketClassification* rows,
    int max_rows,
    int row_offset,
    KboFaMarketScanSummary* summary,
    int write_csv,
    const char* source);

#endif
