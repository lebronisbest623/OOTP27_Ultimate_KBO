#ifndef KBOFIX_SRC_FA_MARKET_CLASSIFICATION_INTERNAL_FA_MARKET_POLICY_INTERNAL_H_
#define KBOFIX_SRC_FA_MARKET_CLASSIFICATION_INTERNAL_FA_MARKET_POLICY_INTERNAL_H_

#include "fa_market_data_internal.h"

int kbo_fa_market_row_is_undrafted_domestic(const KboFaMarketClassification* row);
int kbo_fa_market_row_independent_source_kind(const KboFaMarketClassification* row);
int kbo_fa_market_row_is_independent_league_fa(const KboFaMarketClassification* row);
void kbo_fa_market_set_history_reason(
    KboFaMarketClassification* row,
    const KboFaMarketHistoryCase* history,
    const char* prefix);
uint32_t kbo_fa_market_history_date_u32(const KboFaMarketHistoryCase* history);
void kbo_fa_market_apply_history_filing_metadata(
    KboFaMarketClassification* row,
    const KboFaMarketHistoryCase* history);
int kbo_fa_market_history_is_carryover_unsigned(
    const KboFaMarketClassification* row,
    const KboFaMarketHistoryCase* history,
    uint32_t today_yyyymmdd);
int kbo_fa_market_apply_history_case(
    KboFaMarketClassification* row,
    const KboFaMarketHistoryCase* history);
int kbo_fa_market_grade_is_unknown(const char* grade);
uint32_t kbo_fa_market_display_team_id(const KboFaMarketClassification* row);
void kbo_fa_market_format_salary(int32_t salary, char* out, size_t out_size);
int kbo_fa_market_apply_age_grade_override(KboFaMarketClassification* row, const KboFaRules* rules);
void kbo_fa_market_apply_salary_snapshot_grade(
    KboFaMarketClassification* row,
    const KboFaSalarySnapshotGrade* salary_grades,
    int salary_grade_count,
    const KboFaRules* rules);
void kbo_fa_market_mark_history_case(KboFaMarketHistoryCase* history);
int kbo_load_fa_market_history_cases(
    KboFaMarketClassification* rows,
    int row_count,
    KboFaMarketHistoryCase* histories,
    int max_histories);
int kbo_fa_market_overlay_filing_history_cases(
    KboFaMarketClassification* rows,
    int row_count,
    KboFaMarketHistoryCase* histories,
    int history_count);
void kbo_classify_fa_market_row(
    KboFaMarketClassification* row,
    const KboFaMarketSeedCase* seeds,
    int seed_count,
    const KboFaMarketHistoryCase* history_case,
    uint32_t today_yyyymmdd);
int kbo_fa_market_case_rank(const char* case_label);
int kbo_compare_fa_market_classification_rows(const void* lhs, const void* rhs);
void kbo_fa_market_write_raw(HANDLE file, const char* text);
void kbo_fa_market_write_csv_text(HANDLE file, const char* text);
void kbo_write_fa_market_classification_csv(
    const KboFaMarketClassification* rows,
    int row_count,
    const KboFaMarketScanSummary* summary,
    const char* source);
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
