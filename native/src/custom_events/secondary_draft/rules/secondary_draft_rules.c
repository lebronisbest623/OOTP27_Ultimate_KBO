#include "../secondary_draft.h"

#include <stdio.h>
#include <string.h>

int kbo_secondary_draft_is_odd_season(uint32_t season_or_yyyymmdd)
{
    uint32_t season = season_or_yyyymmdd >= 10000u
        ? season_or_yyyymmdd / 10000u
        : season_or_yyyymmdd;
    return season != 0u && (season % 2u) == 1u;
}

int kbo_secondary_draft_round_team_limit(int team_count, uint32_t round)
{
    if (team_count <= 0 || round == 0u || round > KBO_SECONDARY_DRAFT_ROUNDS) {
        return 0;
    }
    if (round <= KBO_SECONDARY_DRAFT_BASE_ROUNDS) {
        return team_count;
    }
    return team_count < KBO_SECONDARY_DRAFT_EXTRA_TEAMS
        ? team_count
        : KBO_SECONDARY_DRAFT_EXTRA_TEAMS;
}

int kbo_secondary_draft_expected_pick_count(int team_count)
{
    if (team_count <= 0) {
        return 0;
    }
    int picks = 0;
    for (uint32_t round = 1u; round <= KBO_SECONDARY_DRAFT_ROUNDS; round++) {
        picks += kbo_secondary_draft_round_team_limit(team_count, round);
    }
    return picks;
}

uint32_t kbo_secondary_draft_cash_for_round(uint32_t round)
{
    if (round == 1u) {
        return 400000000u;
    }
    if (round == 2u) {
        return 300000000u;
    }
    if (round == 3u) {
        return 200000000u;
    }
    return round != 0u ? 100000000u : 0u;
}

int kbo_secondary_draft_source_loss_allows(int loss_count)
{
    return loss_count >= 0 && loss_count < KBO_SECONDARY_DRAFT_SOURCE_LOSS_LIMIT;
}

static void kbo_secondary_draft_decision_set(
    KboSecondaryDraftEligibilityDecision* out,
    int eligible,
    int auto_excluded,
    int inferred_total_seasons,
    const char* reason)
{
    if (out == NULL) {
        return;
    }
    out->eligible = eligible;
    out->auto_excluded = auto_excluded;
    out->inferred_total_seasons = inferred_total_seasons;
    snprintf(out->reason, sizeof(out->reason), "%s", reason != NULL ? reason : "");
}

int kbo_secondary_draft_evaluate_eligibility(
    const KboSecondaryDraftEligibilityInput* input,
    KboSecondaryDraftEligibilityDecision* out)
{
    if (out != NULL) {
        memset(out, 0, sizeof(*out));
    }
    if (input == NULL || input->player_id == 0u || input->owner_team_id == 0u) {
        kbo_secondary_draft_decision_set(out, 0, 0, 0, "missing_identity");
        return 0;
    }

    int inferred_total_seasons = input->total_seasons_known
        ? input->total_seasons
        : (int)(input->service_days / KBO_SECONDARY_DRAFT_SERVICE_DAYS_PER_SEASON);
    if (!input->total_seasons_known
            && input->service_days == 0u
            && input->age >= 18u
            && input->age <= 23u) {
        inferred_total_seasons = 1;
    }

    if (input->foreign_player) {
        kbo_secondary_draft_decision_set(out, 0, 1, inferred_total_seasons, "foreign_player");
        return 0;
    }
    if (input->retired) {
        kbo_secondary_draft_decision_set(out, 0, 1, inferred_total_seasons, "retired");
        return 0;
    }
    if (input->dfa) {
        kbo_secondary_draft_decision_set(out, 0, 1, inferred_total_seasons, "dfa");
        return 0;
    }
    if (input->draft_pool) {
        kbo_secondary_draft_decision_set(out, 0, 1, inferred_total_seasons, "draft_pool");
        return 0;
    }
    if (!input->has_evaluation) {
        kbo_secondary_draft_decision_set(out, 0, 0, inferred_total_seasons, "no_evaluation");
        return 0;
    }
    if (input->current_year_fa) {
        kbo_secondary_draft_decision_set(out, 0, 1, inferred_total_seasons, "current_year_fa");
        return 0;
    }
    if (input->non_military_loan) {
        kbo_secondary_draft_decision_set(out, 0, 0, inferred_total_seasons, "non_military_loan");
        return 0;
    }
    if (inferred_total_seasons > 0 && inferred_total_seasons <= 3) {
        kbo_secondary_draft_decision_set(out, 0, 1, inferred_total_seasons, "entry_year_1_to_3");
        return 0;
    }
    if (inferred_total_seasons == 4 && input->military_history) {
        kbo_secondary_draft_decision_set(out, 0, 1, inferred_total_seasons, "fourth_year_military_history");
        return 0;
    }

    kbo_secondary_draft_decision_set(out, 1, 0, inferred_total_seasons, "eligible");
    return 1;
}
