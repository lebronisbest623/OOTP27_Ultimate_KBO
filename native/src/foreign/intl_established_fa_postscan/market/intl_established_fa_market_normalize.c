#include "../internal/intl_established_fa_postscan_internal.h"

static int kbo_intl_established_fa_league_matches_batch(
    uint32_t league_id,
    uint32_t primary_league_id,
    uint32_t fallback_league_id)
{
    return league_id != 0u
        && ((primary_league_id != 0u && league_id == primary_league_id)
            || (fallback_league_id != 0u && league_id == fallback_league_id));
}

static int kbo_intl_established_fa_has_league_context(
    uint8_t* player,
    uint32_t primary_league_id,
    uint32_t fallback_league_id)
{
    if (player == NULL) {
        return 0;
    }

    uint32_t current_league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
    uint32_t original_league_id = *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_LEAGUE_ID_OFFSET);
    uint32_t draft_league_id = *(uint32_t*)(player + OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET);
    return kbo_intl_established_fa_league_matches_batch(current_league_id, primary_league_id, fallback_league_id)
        || kbo_intl_established_fa_league_matches_batch(original_league_id, primary_league_id, fallback_league_id)
        || kbo_intl_established_fa_league_matches_batch(draft_league_id, primary_league_id, fallback_league_id);
}

static uint32_t kbo_intl_established_fa_context_league_id(
    uint8_t* player,
    uint32_t primary_league_id,
    uint32_t fallback_league_id)
{
    if (player == NULL) {
        return 0u;
    }

    uint32_t current_league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
    uint32_t original_league_id = *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_LEAGUE_ID_OFFSET);
    uint32_t draft_league_id = *(uint32_t*)(player + OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET);
    if (kbo_intl_established_fa_league_matches_batch(original_league_id, primary_league_id, fallback_league_id)) {
        return original_league_id;
    }
    if (kbo_intl_established_fa_league_matches_batch(current_league_id, primary_league_id, fallback_league_id)) {
        return current_league_id;
    }
    if (kbo_intl_established_fa_league_matches_batch(draft_league_id, primary_league_id, fallback_league_id)) {
        return draft_league_id;
    }
    if (primary_league_id != 0u) {
        return primary_league_id;
    }
    return fallback_league_id;
}

static int kbo_intl_established_fa_demand_index_for_score(int32_t score)
{
    const KboForeignPlayerPolicy* policy = kbo_foreign_player_policy();
    for (int index = KBO_FOREIGN_POLICY_RESERVE_DEMAND_INDEX_COUNT - 1; index >= 1; index--) {
        if (score >= policy->reserve_demand_score_min[index]) {
            return index;
        }
    }
    return 0;
}

static int32_t kbo_intl_established_fa_initial_demand(int asian_quota, int32_t value_score)
{
    int index = kbo_intl_established_fa_demand_index_for_score(value_score);
    int32_t demand = kbo_get_foreign_fa_demand_baseline_value_for_player(index, asian_quota);
    if (demand <= 0 && index != 0) {
        demand = kbo_get_foreign_fa_demand_baseline_value_for_player(0, asian_quota);
    }
    if (demand <= 0) {
        return 0;
    }

    const KboForeignPlayerPolicy* policy = kbo_foreign_player_policy();
    if (demand > policy->demand_salary_max) {
        demand = policy->demand_salary_max;
    }
    return demand;
}

static void kbo_intl_established_fa_capture_market_state(
    uint8_t* player,
    KboIntlEstablishedFaMarketNormalization* out,
    int after)
{
    if (player == NULL || out == NULL) {
        return;
    }

    uint32_t original_league_id = *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_LEAGUE_ID_OFFSET);
    uint32_t draft_league_id = *(uint32_t*)(player + OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET);
    uint8_t draft_class = player[OOTP27_PLAYER_DRAFT_CLASS_OFFSET];
    uint8_t draft_subtype = player[OOTP27_PLAYER_DRAFT_SUBTYPE_OFFSET];
    uint8_t draft_eligible = player[OOTP27_PLAYER_DRAFT_ELIGIBLE_OFFSET];
    uint8_t draft_extra = player[OOTP27_PLAYER_DRAFT_EXTRA_FLAG_OFFSET];
    uint8_t contract_level = player[OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET];
    int32_t fa_demand = *(int32_t*)(player + OOTP27_PLAYER_FA_DEMAND_SALARY_OFFSET);

    if (after) {
        out->after_original_league_id = original_league_id;
        out->after_draft_league_id = draft_league_id;
        out->after_draft_class = draft_class;
        out->after_draft_subtype = draft_subtype;
        out->after_draft_eligible = draft_eligible;
        out->after_draft_extra = draft_extra;
        out->after_contract_level = contract_level;
        out->after_fa_demand = fa_demand;
    } else {
        out->before_original_league_id = original_league_id;
        out->before_draft_league_id = draft_league_id;
        out->before_draft_class = draft_class;
        out->before_draft_subtype = draft_subtype;
        out->before_draft_eligible = draft_eligible;
        out->before_draft_extra = draft_extra;
        out->before_contract_level = contract_level;
        out->before_fa_demand = fa_demand;
    }
}

int kbo_intl_established_fa_normalize_market_state(
    uint8_t* player,
    uint32_t primary_league_id,
    uint32_t fallback_league_id,
    int asian_quota,
    int32_t value_score,
    KboIntlEstablishedFaMarketNormalization* out)
{
    KboIntlEstablishedFaMarketNormalization local = {0};
    KboIntlEstablishedFaMarketNormalization* result = out != NULL ? out : &local;
    memset(result, 0, sizeof(*result));

    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    kbo_intl_established_fa_capture_market_state(player, result, 0);

    uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    uint32_t loan_team_id = *(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET);
    uint8_t retired_flag = player[OOTP27_PLAYER_RETIRED_FLAG_OFFSET];
    uint8_t generation_context = player[OOTP27_PLAYER_GENERATION_CONTEXT_OFFSET];
    int16_t age = *(int16_t*)(player + OOTP27_PLAYER_AGE_OFFSET);
    int has_context = kbo_intl_established_fa_has_league_context(
        player,
        primary_league_id,
        fallback_league_id);

    if (current_team_id != 0u
            || active_team_id != 0u
            || loan_team_id != 0u
            || retired_flag != 0u
            || generation_context != 3u
            || !has_context
            || !kbo_foreign_policy_market_age_allowed((uint16_t)age)) {
        kbo_intl_established_fa_capture_market_state(player, result, 1);
        return 0;
    }

    int changed = 0;
    uint32_t context_league_id = kbo_intl_established_fa_context_league_id(
        player,
        primary_league_id,
        fallback_league_id);

    if (*(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_LEAGUE_ID_OFFSET) == 0u && context_league_id != 0u) {
        *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_LEAGUE_ID_OFFSET) = context_league_id;
        result->original_league_seeded = 1;
        changed = 1;
    }

    /*
     * OOTP tags freshly generated established international FAs with the KBO
     * draft league and contract level before its own AI market pass sees them.
     * Preserve those OOTP-owned fields; only clear the explicit draft-eligible
     * bit that keeps them in the draft pool.
     */
    if (player[OOTP27_PLAYER_DRAFT_ELIGIBLE_OFFSET] != 0u) {
        player[OOTP27_PLAYER_DRAFT_ELIGIBLE_OFFSET] = 0u;
        result->draft_fields_cleared = 1;
        changed = 1;
    }

    int32_t fa_demand = *(int32_t*)(player + OOTP27_PLAYER_FA_DEMAND_SALARY_OFFSET);
    if (fa_demand <= 0) {
        int32_t demand = kbo_intl_established_fa_initial_demand(asian_quota, value_score);
        if (demand > 0) {
            *(int32_t*)(player + OOTP27_PLAYER_FA_DEMAND_SALARY_OFFSET) = demand;
            result->demand_initialized = 1;
            changed = 1;
        }
    }

    kbo_intl_established_fa_capture_market_state(player, result, 1);
    result->changed = changed;
    result->market_ready = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET) == 0u
        && *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET) == 0u
        && *(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET) == 0u
        && player[OOTP27_PLAYER_RETIRED_FLAG_OFFSET] == 0u
        && player[OOTP27_PLAYER_DRAFT_ELIGIBLE_OFFSET] == 0u
        && *(int32_t*)(player + OOTP27_PLAYER_FA_DEMAND_SALARY_OFFSET) > 0
        && kbo_intl_established_fa_has_league_context(player, primary_league_id, fallback_league_id);

    return changed;
}
