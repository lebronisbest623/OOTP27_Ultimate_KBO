#include "../internal/amateur_reputation_internal.h"

static int32_t kbo_round_div_i32(int32_t numerator, int32_t denominator)
{
    if (denominator <= 0) {
        return 0;
    }
    if (numerator >= 0) {
        return (numerator + denominator / 2) / denominator;
    }
    return -((-numerator + denominator / 2) / denominator);
}

uint8_t kbo_amateur_reputation_clamp_for_league(uint32_t league_id, int32_t value)
{
    const KboAmateurPlayerQualityPolicy* policy = kbo_amateur_player_quality_policy();
    int32_t min_value = league_id == KBO_COLLEGE_LEAGUE_ID
        ? policy->college_reputation_min
        : policy->high_school_reputation_min;
    int32_t max_value = league_id == KBO_COLLEGE_LEAGUE_ID
        ? policy->college_reputation_max
        : policy->high_school_reputation_max;
    if (value < min_value) {
        value = min_value;
    } else if (value > max_value) {
        value = max_value;
    }
    return (uint8_t)value;
}

static int32_t kbo_amateur_reputation_raw_delta_for_rank(
    const KboAmateurPlayerQualityPolicy* policy,
    const KboAmateurReputationUpdateRow* row,
    int rank_index,
    int row_count)
{
    int32_t delta = 0;
    if (row->score >= policy->reputation_elite_score_min) {
        delta += policy->reputation_elite_delta;
    } else if (row->score >= policy->reputation_strong_score_min) {
        delta += policy->reputation_strong_delta;
    } else if (row->score >= policy->reputation_positive_score_min) {
        delta += policy->reputation_positive_delta;
    } else if (row->score <= policy->reputation_poor_score_max) {
        delta += policy->reputation_poor_delta;
    } else if (row->score <= policy->reputation_weak_score_max) {
        delta += policy->reputation_weak_delta;
    } else if (row->score <= policy->reputation_negative_score_max) {
        delta += policy->reputation_negative_delta;
    }

    if (rank_index < policy->reputation_top_major_rank_count) {
        delta += policy->reputation_top_major_bonus;
    } else if (rank_index < policy->reputation_top_minor_rank_count) {
        delta += policy->reputation_top_minor_bonus;
    }
    if (rank_index >= row_count - policy->reputation_bottom_major_rank_count) {
        delta -= policy->reputation_bottom_major_penalty;
    } else if (rank_index >= row_count - policy->reputation_bottom_minor_rank_count) {
        delta -= policy->reputation_bottom_minor_penalty;
    }
    return delta;
}

void kbo_apply_amateur_reputation_balanced_deltas(
    uint32_t league_id,
    KboAmateurReputationUpdateRow* rows,
    int row_count,
    int32_t* out_raw_delta_sum,
    int32_t* out_balance_adjustment,
    int32_t* out_final_delta_sum)
{
    if (out_raw_delta_sum != NULL) { *out_raw_delta_sum = 0; }
    if (out_balance_adjustment != NULL) { *out_balance_adjustment = 0; }
    if (out_final_delta_sum != NULL) { *out_final_delta_sum = 0; }
    if (rows == NULL || row_count <= 0 || row_count > KBO_AMATEUR_ASSIGNMENT_TEAM_MAX) {
        return;
    }

    const KboAmateurPlayerQualityPolicy* policy = kbo_amateur_player_quality_policy();
    int32_t raw_deltas[KBO_AMATEUR_ASSIGNMENT_TEAM_MAX] = {0};
    int32_t raw_delta_sum = 0;
    for (int i = 0; i < row_count; i++) {
        raw_deltas[i] = kbo_amateur_reputation_raw_delta_for_rank(
            policy,
            &rows[i],
            i,
            row_count);
        raw_delta_sum += raw_deltas[i];
    }

    int32_t balance_adjustment = kbo_round_div_i32(raw_delta_sum, row_count);
    int32_t final_delta_sum = 0;
    for (int i = 0; i < row_count; i++) {
        int32_t balanced_delta = raw_deltas[i] - balance_adjustment;
        rows[i].new_reputation = kbo_amateur_reputation_clamp_for_league(
            league_id,
            (int32_t)rows[i].old_reputation + balanced_delta);
        final_delta_sum += (int32_t)rows[i].new_reputation - (int32_t)rows[i].old_reputation;
    }

    if (out_raw_delta_sum != NULL) { *out_raw_delta_sum = raw_delta_sum; }
    if (out_balance_adjustment != NULL) { *out_balance_adjustment = balance_adjustment; }
    if (out_final_delta_sum != NULL) { *out_final_delta_sum = final_delta_sum; }
}
