#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "fa_market_policy.h"

#include "../../core/core_flags/localappdata/localappdata_reader.h"
#include "../../core/league_roles/kbo_league_roles.h"
#include "../../core/policy/core_policy.h"

#define KBO_FA_MARKET_POLICY_FILE "fa_market_policy.json"
#define KBO_FA_MARKET_SERVICE_TIME_DAYS_PER_SEASON_KEY "service_time_days_per_season"
#define KBO_FA_MARKET_FA_DECLARATION_SERVICE_SEASONS_MIN_KEY "fa_declaration_service_seasons_min"

static INIT_ONCE g_kbo_fa_market_policy_once = INIT_ONCE_STATIC_INIT;
static KboFaMarketPolicy g_kbo_fa_market_policy;

static int32_t kbo_fa_market_policy_int(const char* key, int32_t fallback, int32_t min_value, int32_t max_value)
{
    return kbo_read_clamped_policy_int(KBO_FA_MARKET_POLICY_FILE, key, fallback, min_value, max_value);
}

static BOOL CALLBACK kbo_fa_market_policy_init_once(PINIT_ONCE init_once, PVOID parameter, PVOID* context)
{
    (void)init_once;
    (void)parameter;
    (void)context;

    KboFaMarketPolicy* p = &g_kbo_fa_market_policy;
    p->undrafted_college_league_id = kbo_fa_market_policy_int(
        "undrafted_college_league_id",
        (int32_t)kbo_league_role_college_league_id(),
        0,
        1000000);
    p->undrafted_college_draft_subtype = kbo_fa_market_policy_int("undrafted_college_draft_subtype", 1, 0, 255);
    p->undrafted_college_age_max = kbo_fa_market_policy_int("undrafted_college_age_max", 25, 0, 80);
    p->undrafted_high_school_league_id = kbo_fa_market_policy_int(
        "undrafted_high_school_league_id",
        (int32_t)kbo_league_role_high_school_league_id(),
        0,
        1000000);
    p->undrafted_high_school_age_max = kbo_fa_market_policy_int("undrafted_high_school_age_max", 20, 0, 80);
    p->independent_league_id = kbo_fa_market_policy_int(
        "independent_league_id",
        (int32_t)kbo_league_role_independent_league_id(),
        0,
        1000000);
    p->player_age_min = kbo_fa_market_policy_int("player_age_min", 16, 0, 80);
    p->player_age_max = kbo_fa_market_policy_int("player_age_max", 60, 0, 100);
    p->service_time_days_per_season = kbo_fa_market_policy_int(KBO_FA_MARKET_SERVICE_TIME_DAYS_PER_SEASON_KEY, 145, 1, 366);
    p->fa_declaration_service_seasons_min = kbo_fa_market_policy_int(KBO_FA_MARKET_FA_DECLARATION_SERVICE_SEASONS_MIN_KEY, 8, 1, 40);
    p->salary_grade_a_overall_rank_max = kbo_fa_market_policy_int("salary_grade_a_overall_rank_max", 30, 0, 10000);
    p->salary_grade_a_team_rank_max = kbo_fa_market_policy_int("salary_grade_a_team_rank_max", 3, 0, 10000);
    p->salary_grade_b_overall_rank_max = kbo_fa_market_policy_int("salary_grade_b_overall_rank_max", 60, 0, 10000);
    p->salary_grade_b_team_rank_max = kbo_fa_market_policy_int("salary_grade_b_team_rank_max", 10, 0, 10000);
    p->investigation_quality_salary_min = kbo_fa_market_policy_int("investigation_quality_salary_min", 100000000, 0, 2000000000);
    p->investigation_quality_demand_min = kbo_fa_market_policy_int("investigation_quality_demand_min", 100000000, 0, 2000000000);
    p->investigation_quality_value_score_min = kbo_fa_market_policy_int("investigation_quality_value_score_min", 55000, 0, 10000000);
    p->investigation_high_demand_min = kbo_fa_market_policy_int("investigation_high_demand_min", 300000000, 0, 2000000000);
    p->investigation_very_high_demand_min = kbo_fa_market_policy_int("investigation_very_high_demand_min", 700000000, 0, 2000000000);
    p->investigation_age_veteran_min = kbo_fa_market_policy_int("investigation_age_veteran_min", 34, 0, 80);
    p->investigation_age_old_min = kbo_fa_market_policy_int("investigation_age_old_min", 36, 0, 80);
    p->investigation_market_days_long_min = kbo_fa_market_policy_int("investigation_market_days_long_min", 45, 0, 1000);
    p->investigation_market_days_very_long_min = kbo_fa_market_policy_int("investigation_market_days_very_long_min", 90, 0, 1000);
    p->orphan_rescue_market_days_min = kbo_fa_market_policy_int("orphan_rescue_market_days_min", p->investigation_market_days_long_min, 0, 1000);
    p->investigation_unexplained_value_score_min = kbo_fa_market_policy_int("investigation_unexplained_value_score_min", 85000, 0, 10000000);
    p->investigation_thread_sleep_ms = kbo_fa_market_policy_int("investigation_thread_sleep_ms", 5000, 100, 600000);
    p->investigation_top_log_count = kbo_fa_market_policy_int("investigation_top_log_count", 20, 0, 1000);
    if (p->investigation_very_high_demand_min < p->investigation_high_demand_min) {
        p->investigation_very_high_demand_min = p->investigation_high_demand_min;
    }
    if (p->investigation_age_old_min < p->investigation_age_veteran_min) {
        p->investigation_age_old_min = p->investigation_age_veteran_min;
    }
    if (p->investigation_market_days_very_long_min < p->investigation_market_days_long_min) {
        p->investigation_market_days_very_long_min = p->investigation_market_days_long_min;
    }
    if (p->orphan_rescue_market_days_min <= 0) {
        p->orphan_rescue_market_days_min = p->investigation_market_days_long_min;
    }
    if (p->player_age_max < p->player_age_min) {
        p->player_age_max = p->player_age_min;
    }
    return TRUE;
}

const KboFaMarketPolicy* kbo_fa_market_policy(void)
{
    InitOnceExecuteOnce(
        &g_kbo_fa_market_policy_once,
        kbo_fa_market_policy_init_once,
        NULL,
        NULL);
    return &g_kbo_fa_market_policy;
}

int kbo_fa_market_policy_service_time_days_per_season(void)
{
    return kbo_fa_market_policy()->service_time_days_per_season;
}

int kbo_fa_market_policy_fa_declaration_service_seasons_min(void)
{
    return kbo_fa_market_policy()->fa_declaration_service_seasons_min;
}

int kbo_set_fa_market_policy_service_time_days_per_season(int value)
{
    int clamped = value < 1 ? 1 : (value > 366 ? 366 : value);
    if (!kbo_write_localappdata_named_json_int_value(
            KBO_FA_MARKET_POLICY_FILE,
            KBO_FA_MARKET_SERVICE_TIME_DAYS_PER_SEASON_KEY,
            clamped)) {
        return 0;
    }
    (void)kbo_fa_market_policy();
    g_kbo_fa_market_policy.service_time_days_per_season = clamped;
    return 1;
}

int kbo_set_fa_market_policy_fa_declaration_service_seasons_min(int value)
{
    int clamped = value < 1 ? 1 : (value > 40 ? 40 : value);
    if (!kbo_write_localappdata_named_json_int_value(
            KBO_FA_MARKET_POLICY_FILE,
            KBO_FA_MARKET_FA_DECLARATION_SERVICE_SEASONS_MIN_KEY,
            clamped)) {
        return 0;
    }
    (void)kbo_fa_market_policy();
    g_kbo_fa_market_policy.fa_declaration_service_seasons_min = clamped;
    return 1;
}
