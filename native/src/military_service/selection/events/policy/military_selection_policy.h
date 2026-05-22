#ifndef KBOFIX_SRC_MILITARY_SELECTION_POLICY_H_
#define KBOFIX_SRC_MILITARY_SELECTION_POLICY_H_

#include <stdint.h>

typedef struct KboMilitarySelectionPolicy {
    int32_t service_days;
    int32_t sang_capacity;
    int32_t sang_annual_intake;
    int32_t return_preview_days;
    int32_t existing_loan_refresh_window_days;
    int32_t draft_score_talent_weight;
    int32_t draft_score_overall_weight;
    int32_t draft_score_ratings_weight;
    int32_t draft_score_career_weight;
    int32_t draft_score_ideal_age_min;
    int32_t draft_score_ideal_age_max;
    int32_t draft_score_ideal_age_base_bonus;
    int32_t draft_score_ideal_age_step_penalty;
    int32_t draft_score_young_base_bonus;
    int32_t draft_score_young_step_penalty;
    int32_t draft_score_old_base_bonus;
    int32_t draft_score_old_step_penalty;
    int32_t draft_score_player_id_mod;
    int32_t fa_fast_block_log_max;
    int32_t fa_offer_eligibility_log_max;
    int32_t fa_signability_log_max;
    int32_t fa_submit_block_log_max;
    int32_t fa_ai_block_log_max;
    int32_t fa_player_action_log_max;
    int32_t fa_player_action_context_scan_bytes;
    int32_t fa_player_id_max;
    int32_t fa_action_id_min;
    int32_t fa_action_id_max;
} KboMilitarySelectionPolicy;

const KboMilitarySelectionPolicy* kbo_military_selection_policy(void);
int32_t kbo_military_service_days(void);

#endif
