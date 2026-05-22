#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "military_selection_policy.h"

#include "../../../../core/policy/core_policy.h"

#define KBO_MILITARY_SELECTION_POLICY_FILE "military_service_policy.json"

static INIT_ONCE g_kbo_military_selection_policy_once = INIT_ONCE_STATIC_INIT;
static KboMilitarySelectionPolicy g_kbo_military_selection_policy;

static int32_t kbo_military_selection_policy_int(const char* key, int32_t fallback, int32_t min_value, int32_t max_value)
{
    return kbo_read_clamped_policy_int(KBO_MILITARY_SELECTION_POLICY_FILE, key, fallback, min_value, max_value);
}

static BOOL CALLBACK kbo_military_selection_policy_init_once(PINIT_ONCE init_once, PVOID parameter, PVOID* context)
{
    (void)init_once;
    (void)parameter;
    (void)context;

    KboMilitarySelectionPolicy* p = &g_kbo_military_selection_policy;
    p->service_days = kbo_military_selection_policy_int("service_days", 545, 1, 3660);
    p->sang_capacity = kbo_military_selection_policy_int("sang_capacity", 35, 0, 200);
    p->sang_annual_intake = kbo_military_selection_policy_int("sang_annual_intake", 18, 0, 200);
    p->return_preview_days = kbo_military_selection_policy_int("return_preview_days", 14, 0, 365);
    p->existing_loan_refresh_window_days = kbo_military_selection_policy_int("existing_loan_refresh_window_days", 90, 0, 3660);
    p->draft_score_talent_weight = kbo_military_selection_policy_int("draft_score_talent_weight", 55, 0, 10000);
    p->draft_score_overall_weight = kbo_military_selection_policy_int("draft_score_overall_weight", 25, 0, 10000);
    p->draft_score_ratings_weight = kbo_military_selection_policy_int("draft_score_ratings_weight", 10, 0, 10000);
    p->draft_score_career_weight = kbo_military_selection_policy_int("draft_score_career_weight", 10, 0, 10000);
    p->draft_score_ideal_age_min = kbo_military_selection_policy_int("draft_score_ideal_age_min", 21, 0, 80);
    p->draft_score_ideal_age_max = kbo_military_selection_policy_int("draft_score_ideal_age_max", 26, 0, 80);
    p->draft_score_ideal_age_base_bonus = kbo_military_selection_policy_int("draft_score_ideal_age_base_bonus", 300, -1000000, 1000000);
    p->draft_score_ideal_age_step_penalty = kbo_military_selection_policy_int("draft_score_ideal_age_step_penalty", 20, 0, 1000000);
    p->draft_score_young_base_bonus = kbo_military_selection_policy_int("draft_score_young_base_bonus", 120, -1000000, 1000000);
    p->draft_score_young_step_penalty = kbo_military_selection_policy_int("draft_score_young_step_penalty", 10, 0, 1000000);
    p->draft_score_old_base_bonus = kbo_military_selection_policy_int("draft_score_old_base_bonus", 120, -1000000, 1000000);
    p->draft_score_old_step_penalty = kbo_military_selection_policy_int("draft_score_old_step_penalty", 15, 0, 1000000);
    p->draft_score_player_id_mod = kbo_military_selection_policy_int("draft_score_player_id_mod", 97, 1, 1000000);
    p->fa_fast_block_log_max = kbo_military_selection_policy_int("fa_fast_block_log_max", 120, 0, 1000000);
    p->fa_offer_eligibility_log_max = kbo_military_selection_policy_int("fa_offer_eligibility_log_max", 120, 0, 1000000);
    p->fa_signability_log_max = kbo_military_selection_policy_int("fa_signability_log_max", 200, 0, 1000000);
    p->fa_submit_block_log_max = kbo_military_selection_policy_int("fa_submit_block_log_max", 200, 0, 1000000);
    p->fa_ai_block_log_max = kbo_military_selection_policy_int("fa_ai_block_log_max", 300, 0, 1000000);
    p->fa_player_action_log_max = kbo_military_selection_policy_int("fa_player_action_log_max", 160, 0, 1000000);
    p->fa_player_action_context_scan_bytes = kbo_military_selection_policy_int("fa_player_action_context_scan_bytes", 0x300, 0x40, 0x4000);
    p->fa_player_id_max = kbo_military_selection_policy_int("fa_player_id_max", 200000000, 1, 2000000000);
    p->fa_action_id_min = kbo_military_selection_policy_int("fa_action_id_min", 0x20, 0, 10000);
    p->fa_action_id_max = kbo_military_selection_policy_int("fa_action_id_max", 0x80, 0, 10000);
    if (p->draft_score_ideal_age_max < p->draft_score_ideal_age_min) {
        p->draft_score_ideal_age_max = p->draft_score_ideal_age_min;
    }
    if (p->sang_annual_intake > p->sang_capacity && p->sang_capacity > 0) {
        p->sang_annual_intake = p->sang_capacity;
    }
    if (p->fa_action_id_max < p->fa_action_id_min) {
        p->fa_action_id_max = p->fa_action_id_min;
    }
    return TRUE;
}

const KboMilitarySelectionPolicy* kbo_military_selection_policy(void)
{
    InitOnceExecuteOnce(
        &g_kbo_military_selection_policy_once,
        kbo_military_selection_policy_init_once,
        NULL,
        NULL);
    return &g_kbo_military_selection_policy;
}

int32_t kbo_military_service_days(void)
{
    const KboMilitarySelectionPolicy* policy = kbo_military_selection_policy();
    return policy->service_days > 0 ? policy->service_days : 545;
}
