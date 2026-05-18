#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "fa_compensation_protection_policy.h"

#include "../../../core/policy/core_policy.h"

#define KBO_FA_COMPENSATION_POLICY_FILE "fa_compensation_policy.json"

static INIT_ONCE g_kbo_fa_compensation_policy_once = INIT_ONCE_STATIC_INIT;
static KboFaCompensationProtectionPolicy g_kbo_fa_compensation_policy;

static int32_t kbo_fa_compensation_policy_int(const char* key, int32_t fallback, int32_t min_value, int32_t max_value)
{
    return kbo_read_clamped_policy_int(KBO_FA_COMPENSATION_POLICY_FILE, key, fallback, min_value, max_value);
}

static BOOL CALLBACK kbo_fa_compensation_policy_init_once(PINIT_ONCE init_once, PVOID parameter, PVOID* context)
{
    (void)init_once;
    (void)parameter;
    (void)context;

    KboFaCompensationProtectionPolicy* p = &g_kbo_fa_compensation_policy;
    p->candidate_prospect_age_max = kbo_fa_compensation_policy_int("candidate_prospect_age_max", 22, 16, 50);
    p->candidate_prospect_bonus = kbo_fa_compensation_policy_int("candidate_prospect_bonus", 1200, 0, 1000000);
    p->candidate_young_age_max = kbo_fa_compensation_policy_int("candidate_young_age_max", 25, 16, 50);
    p->candidate_young_bonus = kbo_fa_compensation_policy_int("candidate_young_bonus", 850, 0, 1000000);
    p->candidate_prime_age_max = kbo_fa_compensation_policy_int("candidate_prime_age_max", 29, 16, 60);
    p->candidate_prime_bonus = kbo_fa_compensation_policy_int("candidate_prime_bonus", 350, 0, 1000000);
    p->candidate_old_low_overall_age_min = kbo_fa_compensation_policy_int("candidate_old_low_overall_age_min", 36, 16, 80);
    p->candidate_old_low_overall_max = kbo_fa_compensation_policy_int("candidate_old_low_overall_max", 44, 0, 100);
    p->candidate_old_low_overall_penalty = kbo_fa_compensation_policy_int("candidate_old_low_overall_penalty", 900, 0, 1000000);
    p->candidate_aging_age_min = kbo_fa_compensation_policy_int("candidate_aging_age_min", 34, 16, 80);
    p->candidate_aging_penalty = kbo_fa_compensation_policy_int("candidate_aging_penalty", 350, 0, 1000000);
    p->candidate_core_value_min = kbo_fa_compensation_policy_int("candidate_core_value_min", 60, 0, 100);
    p->candidate_core_bonus = kbo_fa_compensation_policy_int("candidate_core_bonus", 1000, 0, 1000000);
    p->candidate_regular_value_min = kbo_fa_compensation_policy_int("candidate_regular_value_min", 50, 0, 100);
    p->candidate_regular_bonus = kbo_fa_compensation_policy_int("candidate_regular_bonus", 450, 0, 1000000);
    p->candidate_upside_margin = kbo_fa_compensation_policy_int("candidate_upside_margin", 15, 0, 100);
    p->candidate_upside_bonus = kbo_fa_compensation_policy_int("candidate_upside_bonus", 650, 0, 1000000);
    p->candidate_cheap_salary_max = kbo_fa_compensation_policy_int("candidate_cheap_salary_max", 70000000, 0, 2147483647);
    p->candidate_cheap_age_max = kbo_fa_compensation_policy_int("candidate_cheap_age_max", 27, 16, 80);
    p->candidate_cheap_talent_margin = kbo_fa_compensation_policy_int("candidate_cheap_talent_margin", 10, 0, 100);
    p->candidate_cheap_bonus = kbo_fa_compensation_policy_int("candidate_cheap_bonus", 450, 0, 1000000);
    p->candidate_bad_contract_salary_min = kbo_fa_compensation_policy_int("candidate_bad_contract_salary_min", 800000000, 0, 2147483647);
    p->candidate_bad_contract_age_min = kbo_fa_compensation_policy_int("candidate_bad_contract_age_min", 33, 16, 80);
    p->candidate_bad_contract_overall_max = kbo_fa_compensation_policy_int("candidate_bad_contract_overall_max", 54, 0, 100);
    p->candidate_bad_contract_penalty = kbo_fa_compensation_policy_int("candidate_bad_contract_penalty", 1000, 0, 1000000);
    p->candidate_costly_vet_salary_min = kbo_fa_compensation_policy_int("candidate_costly_vet_salary_min", 500000000, 0, 2147483647);
    p->candidate_costly_vet_age_min = kbo_fa_compensation_policy_int("candidate_costly_vet_age_min", 32, 16, 80);
    p->candidate_costly_vet_penalty = kbo_fa_compensation_policy_int("candidate_costly_vet_penalty", 450, 0, 1000000);
    p->candidate_paid_core_salary_min = kbo_fa_compensation_policy_int("candidate_paid_core_salary_min", 300000000, 0, 2147483647);
    p->candidate_paid_core_overall_min = kbo_fa_compensation_policy_int("candidate_paid_core_overall_min", 60, 0, 100);
    p->candidate_paid_core_bonus = kbo_fa_compensation_policy_int("candidate_paid_core_bonus", 200, 0, 1000000);
    p->candidate_scarce_bonus = kbo_fa_compensation_policy_int("candidate_scarce_bonus", 650, 0, 1000000);
    p->candidate_deep_penalty = kbo_fa_compensation_policy_int("candidate_deep_penalty", 200, 0, 1000000);
    p->candidate_scarce_catcher_max = kbo_fa_compensation_policy_int("candidate_scarce_catcher_max", 2, 0, 200);
    p->candidate_scarce_infielder_max = kbo_fa_compensation_policy_int("candidate_scarce_infielder_max", 5, 0, 200);
    p->candidate_scarce_outfielder_max = kbo_fa_compensation_policy_int("candidate_scarce_outfielder_max", 6, 0, 200);
    p->candidate_scarce_pitcher_max = kbo_fa_compensation_policy_int("candidate_scarce_pitcher_max", 12, 0, 400);
    p->candidate_deep_catcher_min = kbo_fa_compensation_policy_int("candidate_deep_catcher_min", 5, 0, 200);
    p->candidate_deep_infielder_min = kbo_fa_compensation_policy_int("candidate_deep_infielder_min", 11, 0, 200);
    p->candidate_deep_outfielder_min = kbo_fa_compensation_policy_int("candidate_deep_outfielder_min", 11, 0, 200);
    p->candidate_deep_pitcher_min = kbo_fa_compensation_policy_int("candidate_deep_pitcher_min", 24, 0, 400);
    p->decision_missing_player_penalty = kbo_fa_compensation_policy_int("decision_missing_player_penalty", 100000, 0, 10000000);
    p->decision_prospect_age_max = kbo_fa_compensation_policy_int("decision_prospect_age_max", 22, 16, 50);
    p->decision_prospect_bonus = kbo_fa_compensation_policy_int("decision_prospect_bonus", 1000, 0, 1000000);
    p->decision_young_age_max = kbo_fa_compensation_policy_int("decision_young_age_max", 25, 16, 50);
    p->decision_young_bonus = kbo_fa_compensation_policy_int("decision_young_bonus", 700, 0, 1000000);
    p->decision_prime_age_max = kbo_fa_compensation_policy_int("decision_prime_age_max", 29, 16, 60);
    p->decision_prime_bonus = kbo_fa_compensation_policy_int("decision_prime_bonus", 250, 0, 1000000);
    p->decision_old_age_min = kbo_fa_compensation_policy_int("decision_old_age_min", 35, 16, 80);
    p->decision_old_penalty = kbo_fa_compensation_policy_int("decision_old_penalty", 900, 0, 1000000);
    p->decision_aging_age_min = kbo_fa_compensation_policy_int("decision_aging_age_min", 32, 16, 80);
    p->decision_aging_penalty = kbo_fa_compensation_policy_int("decision_aging_penalty", 350, 0, 1000000);
    p->decision_unknown_salary_bonus = kbo_fa_compensation_policy_int("decision_unknown_salary_bonus", 150, 0, 1000000);
    p->decision_cheap_salary_max = kbo_fa_compensation_policy_int("decision_cheap_salary_max", 70000000, 0, 2147483647);
    p->decision_cheap_bonus = kbo_fa_compensation_policy_int("decision_cheap_bonus", 500, 0, 1000000);
    p->decision_expensive_salary_min = kbo_fa_compensation_policy_int("decision_expensive_salary_min", 600000000, 0, 2147483647);
    p->decision_expensive_penalty = kbo_fa_compensation_policy_int("decision_expensive_penalty", 900, 0, 1000000);
    p->decision_costly_salary_min = kbo_fa_compensation_policy_int("decision_costly_salary_min", 300000000, 0, 2147483647);
    p->decision_costly_penalty = kbo_fa_compensation_policy_int("decision_costly_penalty", 350, 0, 1000000);
    p->decision_team_need_bonus = kbo_fa_compensation_policy_int("decision_team_need_bonus", 700, 0, 1000000);
    p->decision_surplus_penalty = kbo_fa_compensation_policy_int("decision_surplus_penalty", 250, 0, 1000000);
    p->decision_need_catcher_below = kbo_fa_compensation_policy_int("decision_need_catcher_below", 2, 0, 200);
    p->decision_need_infielder_below = kbo_fa_compensation_policy_int("decision_need_infielder_below", 5, 0, 200);
    p->decision_need_outfielder_below = kbo_fa_compensation_policy_int("decision_need_outfielder_below", 7, 0, 200);
    p->decision_need_pitcher_below = kbo_fa_compensation_policy_int("decision_need_pitcher_below", 13, 0, 400);
    p->decision_surplus_catcher_above = kbo_fa_compensation_policy_int("decision_surplus_catcher_above", 4, 0, 200);
    p->decision_surplus_infielder_above = kbo_fa_compensation_policy_int("decision_surplus_infielder_above", 10, 0, 200);
    p->decision_surplus_outfielder_above = kbo_fa_compensation_policy_int("decision_surplus_outfielder_above", 10, 0, 200);
    p->decision_surplus_pitcher_above = kbo_fa_compensation_policy_int("decision_surplus_pitcher_above", 22, 0, 400);
    p->decision_upside_margin = kbo_fa_compensation_policy_int("decision_upside_margin", 15, 0, 100);
    p->decision_upside_bonus = kbo_fa_compensation_policy_int("decision_upside_bonus", 450, 0, 1000000);
    p->cash_only_score_threshold = kbo_fa_compensation_policy_int("cash_only_score_threshold", 65000, 0, 10000000);
    p->cash_only_extra_cash_score_threshold = kbo_fa_compensation_policy_int("cash_only_extra_cash_score_threshold", 80000, 0, 10000000);
    p->cash_only_ab_grade_requires_no_player = kbo_fa_compensation_policy_int("cash_only_ab_grade_requires_no_player", 1, 0, 1);
    p->rookie_auto_protected_age_max = kbo_fa_compensation_policy_int("rookie_auto_protected_age_max", 23, 16, 50);
    if (p->cash_only_extra_cash_score_threshold < p->cash_only_score_threshold) {
        p->cash_only_extra_cash_score_threshold = p->cash_only_score_threshold;
    }
    return TRUE;
}

const KboFaCompensationProtectionPolicy* kbo_fa_compensation_protection_policy(void)
{
    InitOnceExecuteOnce(
        &g_kbo_fa_compensation_policy_once,
        kbo_fa_compensation_policy_init_once,
        NULL,
        NULL);
    return &g_kbo_fa_compensation_policy;
}
