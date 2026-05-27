#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../src/custom_events/secondary_draft/secondary_draft.h"

static KboSecondaryDraftEligibilityInput eligible_input(void)
{
    KboSecondaryDraftEligibilityInput input = {0};
    input.player_id = 1234u;
    input.owner_team_id = 100u;
    input.age = 26u;
    input.service_days = (uint16_t)(KBO_SECONDARY_DRAFT_SERVICE_DAYS_PER_SEASON * 4u);
    input.has_evaluation = 1;
    return input;
}

static KboSecondaryDraftEligibilityDecision decide(KboSecondaryDraftEligibilityInput input)
{
    KboSecondaryDraftEligibilityDecision decision = {0};
    (void)kbo_secondary_draft_evaluate_eligibility(&input, &decision);
    return decision;
}

static void test_odd_year_only(void)
{
    assert(kbo_secondary_draft_is_odd_season(2025u));
    assert(kbo_secondary_draft_is_odd_season(20251130u));
    assert(!kbo_secondary_draft_is_odd_season(2026u));
    assert(!kbo_secondary_draft_is_odd_season(0u));
    printf("test_odd_year_only: PASS\n");
}

static void test_round_shape_and_cash(void)
{
    assert(kbo_secondary_draft_round_team_limit(10, 1u) == 10);
    assert(kbo_secondary_draft_round_team_limit(10, 3u) == 10);
    assert(kbo_secondary_draft_round_team_limit(10, 4u) == 3);
    assert(kbo_secondary_draft_round_team_limit(10, 5u) == 3);
    assert(kbo_secondary_draft_round_team_limit(2, 4u) == 2);
    assert(kbo_secondary_draft_expected_pick_count(10) == 36);

    assert(kbo_secondary_draft_cash_for_round(1u) == 400000000u);
    assert(kbo_secondary_draft_cash_for_round(2u) == 300000000u);
    assert(kbo_secondary_draft_cash_for_round(3u) == 200000000u);
    assert(kbo_secondary_draft_cash_for_round(4u) == 100000000u);
    assert(kbo_secondary_draft_cash_for_round(5u) == 100000000u);
    assert(kbo_secondary_draft_cash_for_round(0u) == 0u);
    printf("test_round_shape_and_cash: PASS\n");
}

static void test_source_loss_limit(void)
{
    assert(kbo_secondary_draft_source_loss_allows(0));
    assert(kbo_secondary_draft_source_loss_allows(3));
    assert(!kbo_secondary_draft_source_loss_allows(4));
    printf("test_source_loss_limit: PASS\n");
}

static void test_eligibility_exclusions(void)
{
    KboSecondaryDraftEligibilityInput input = eligible_input();
    KboSecondaryDraftEligibilityDecision decision = decide(input);
    assert(decision.eligible);
    assert(!decision.auto_excluded);

    input = eligible_input();
    input.foreign_player = 1;
    decision = decide(input);
    assert(!decision.eligible);
    assert(decision.auto_excluded);
    assert(strcmp(decision.reason, "foreign_player") == 0);

    input = eligible_input();
    input.current_year_fa = 1;
    decision = decide(input);
    assert(!decision.eligible);
    assert(strcmp(decision.reason, "current_year_fa") == 0);

    input = eligible_input();
    input.service_days = (uint16_t)(KBO_SECONDARY_DRAFT_SERVICE_DAYS_PER_SEASON * 3u);
    decision = decide(input);
    assert(!decision.eligible);
    assert(strcmp(decision.reason, "entry_year_1_to_3") == 0);

    input = eligible_input();
    input.total_seasons_known = 1;
    input.total_seasons = 4;
    input.military_history = 1;
    decision = decide(input);
    assert(!decision.eligible);
    assert(strcmp(decision.reason, "fourth_year_military_history") == 0);

    input = eligible_input();
    input.total_seasons_known = 1;
    input.total_seasons = 5;
    input.military_reserved = 1;
    input.military_history = 1;
    decision = decide(input);
    assert(decision.eligible);

    input = eligible_input();
    input.non_military_loan = 1;
    decision = decide(input);
    assert(!decision.eligible);
    assert(strcmp(decision.reason, "non_military_loan") == 0);
    printf("test_eligibility_exclusions: PASS\n");
}

int main(void)
{
    test_odd_year_only();
    test_round_shape_and_cash();
    test_source_loss_limit();
    test_eligibility_exclusions();
    printf("All secondary draft rule tests passed.\n");
    return 0;
}
