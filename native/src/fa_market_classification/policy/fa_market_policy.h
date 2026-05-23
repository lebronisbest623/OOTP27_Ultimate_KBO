#ifndef KBOFIX_SRC_FA_MARKET_POLICY_H_
#define KBOFIX_SRC_FA_MARKET_POLICY_H_

#include <stdint.h>

typedef struct KboFaMarketPolicy {
    int32_t undrafted_college_league_id;
    int32_t undrafted_college_draft_subtype;
    int32_t undrafted_college_age_max;
    int32_t undrafted_high_school_league_id;
    int32_t undrafted_high_school_age_max;
    int32_t independent_league_id;
    int32_t player_age_min;
    int32_t player_age_max;
    int32_t service_time_days_per_season;
    int32_t fa_declaration_service_seasons_min;
    int32_t salary_grade_a_overall_rank_max;
    int32_t salary_grade_a_team_rank_max;
    int32_t salary_grade_b_overall_rank_max;
    int32_t salary_grade_b_team_rank_max;
    int32_t investigation_quality_salary_min;
    int32_t investigation_quality_demand_min;
    int32_t investigation_quality_value_score_min;
    int32_t investigation_high_demand_min;
    int32_t investigation_very_high_demand_min;
    int32_t investigation_age_veteran_min;
    int32_t investigation_age_old_min;
    int32_t investigation_market_days_long_min;
    int32_t investigation_market_days_very_long_min;
    int32_t investigation_unexplained_value_score_min;
    int32_t investigation_thread_sleep_ms;
    int32_t investigation_top_log_count;
} KboFaMarketPolicy;

const KboFaMarketPolicy* kbo_fa_market_policy(void);
int kbo_fa_market_policy_service_time_days_per_season(void);
int kbo_fa_market_policy_fa_declaration_service_seasons_min(void);
int kbo_set_fa_market_policy_service_time_days_per_season(int value);
int kbo_set_fa_market_policy_fa_declaration_service_seasons_min(int value);

#endif
