#ifndef KBO_CORE_FLAGS_FLAGS_API_H
#define KBO_CORE_FLAGS_FLAGS_API_H

#include <stdint.h>
#include <windows.h>

int read_kbo_localappdata_flag_file(const char* file_name);

int kbo_get_intl_established_fa_multiplier(void);
int kbo_set_intl_established_fa_multiplier(int value);
int kbo_clamp_intl_established_fa_multiplier(int value);

int32_t kbo_clamp_foreign_fa_demand_baseline_value(int32_t value);
int32_t kbo_get_foreign_fa_demand_baseline_value(int index);
int32_t kbo_get_asian_quota_fa_demand_baseline_value(int index);
int32_t kbo_get_foreign_fa_demand_baseline_value_for_player(int index, int asian_quota);
void kbo_invalidate_foreign_fa_demand_baseline_cache(void);
int kbo_set_foreign_fa_demand_baseline_value(int index, int32_t value);
int kbo_set_asian_quota_fa_demand_baseline_value(int index, int32_t value);
int32_t kbo_clamp_asian_quota_salary_limit_value(int32_t value);
int32_t kbo_get_asian_quota_salary_limit(void);
int kbo_set_asian_quota_salary_limit(int32_t value);

int kbo_get_foreign_fa_quality_cap_enabled_setting(void);
int kbo_set_foreign_fa_quality_cap_enabled_setting(int enabled);

#define KBO_ASIAN_GAMES_NO_GOLD_ODDS_DENOMINATOR_MIN 1
#define KBO_ASIAN_GAMES_NO_GOLD_ODDS_DENOMINATOR_MAX 100
int kbo_clamp_asian_games_no_gold_odds_denominator(int value);
int kbo_get_asian_games_no_gold_odds_denominator(void);
int kbo_set_asian_games_no_gold_odds_denominator(int value);

#define KBO_INDEPENDENT_ACQUISITION_CASH_COST_MIN 0
#define KBO_INDEPENDENT_ACQUISITION_CASH_COST_MAX 2000000000
int32_t kbo_clamp_independent_acquisition_cash_cost(int32_t value);
int32_t kbo_get_independent_acquisition_foreign_cash_cost(void);
int32_t kbo_get_independent_acquisition_domestic_cash_cost(void);
int32_t kbo_get_independent_acquisition_foreign_seller_transfer_fee(void);
int32_t kbo_get_independent_acquisition_domestic_seller_transfer_fee(void);
int kbo_set_independent_acquisition_foreign_cash_cost(int32_t value);
int kbo_set_independent_acquisition_domestic_cash_cost(int32_t value);
int kbo_set_independent_acquisition_foreign_seller_transfer_fee(int32_t value);
int kbo_set_independent_acquisition_domestic_seller_transfer_fee(int32_t value);

#define KBO_FOREIGN_FA_NON_ASIAN_QUALITY_CAP_COUNT 5
#define KBO_FOREIGN_FA_NON_ASIAN_QUALITY_CAP_STARTER 0
#define KBO_FOREIGN_FA_NON_ASIAN_QUALITY_CAP_BULLPEN 1
#define KBO_FOREIGN_FA_NON_ASIAN_QUALITY_CAP_PITCHER 2
#define KBO_FOREIGN_FA_NON_ASIAN_QUALITY_CAP_HITTER 3
#define KBO_FOREIGN_FA_NON_ASIAN_QUALITY_CAP_CATCHER 4
#define KBO_FOREIGN_FA_QUALITY_CAP_MAX 250000
int32_t kbo_clamp_foreign_fa_quality_cap_value(int32_t value);
int32_t kbo_get_foreign_fa_non_asian_quality_cap_value(int index);
int kbo_set_foreign_fa_non_asian_quality_cap_value(int index, int32_t value);

int kbo_get_profiler_enabled_setting(void);
int kbo_set_profiler_enabled_setting(int enabled);

int kbo_get_allow_all_ui_team_actions_setting(void);
int kbo_set_allow_all_ui_team_actions_setting(int enabled);

#define KBO_CUSTOM_NEWS_LANGUAGE_KO 0
#define KBO_CUSTOM_NEWS_LANGUAGE_EN 1
int kbo_get_custom_news_language_setting(void);
int kbo_set_custom_news_language_setting(int language);
const char* kbo_custom_news_language_dir(void);

int kbo_fix_enabled(void);
void kbo_request_runtime_threads_stop(void);
int kbo_start_runtime_thread(LPTHREAD_START_ROUTINE start, LPVOID parameter, const char* label);
void kbo_register_runtime_thread(HANDLE thread, const char* label);
void kbo_shutdown_runtime_threads(uint32_t timeout_ms);
int kbo_runtime_threads_should_continue(void);
int kbo_runtime_sleep_should_continue(uint32_t total_ms);
int kbo_runtime_save_in_progress(void);
int kbo_runtime_pause_for_save_if_needed(const char* label);

#endif
