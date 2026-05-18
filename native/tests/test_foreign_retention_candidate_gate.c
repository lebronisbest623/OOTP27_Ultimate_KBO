#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../src/foreign/signability/foreign_policy/wrappers/retained_candidates/foreign_signability_foreign_ai_fa_retained_candidates.h"

int kbo_foreign_policy_demand_salary_plausible(int32_t demand)
{
    return demand > 0 && demand < 1000000000;
}

static void test_zero_demand_retained_candidate_is_not_forced(void)
{
    KboAiFaStatusRetainedCandidate candidate;
    const char* reason = NULL;
    memset(&candidate, 0, sizeof(candidate));

    candidate.player_id = 1705u;
    candidate.market_free_agent = 1;
    candidate.fa_demand = 0;

    assert(!kbo_ai_fa_status_retained_candidate_force_gate(&candidate, &reason));
    assert(reason != NULL && strcmp(reason, "demand_not_ready") == 0);
    printf("test_zero_demand_retained_candidate_is_not_forced: PASS\n");
}

static void test_market_retained_candidate_with_demand_is_forced(void)
{
    KboAiFaStatusRetainedCandidate candidate;
    const char* reason = "unchanged";
    memset(&candidate, 0, sizeof(candidate));

    candidate.player_id = 1705u;
    candidate.market_free_agent = 1;
    candidate.fa_demand = 1275000;

    assert(kbo_ai_fa_status_retained_candidate_force_gate(&candidate, &reason));
    assert(reason == NULL);
    printf("test_market_retained_candidate_with_demand_is_forced: PASS\n");
}

static void test_already_in_org_retained_candidate_is_not_forced(void)
{
    KboAiFaStatusRetainedCandidate candidate;
    const char* reason = NULL;
    memset(&candidate, 0, sizeof(candidate));

    candidate.player_id = 1705u;
    candidate.market_free_agent = 1;
    candidate.already_in_org = 1;
    candidate.fa_demand = 1275000;

    assert(!kbo_ai_fa_status_retained_candidate_force_gate(&candidate, &reason));
    assert(reason != NULL && strcmp(reason, "already_in_org") == 0);
    printf("test_already_in_org_retained_candidate_is_not_forced: PASS\n");
}

static void test_holder_org_retained_candidate_is_not_forced(void)
{
    KboAiFaStatusRetainedCandidate candidate;
    const char* reason = NULL;
    memset(&candidate, 0, sizeof(candidate));

    candidate.player_id = 1705u;
    candidate.holder_org_candidate = 1;
    candidate.fa_demand = 1275000;

    assert(!kbo_ai_fa_status_retained_candidate_force_gate(&candidate, &reason));
    assert(reason != NULL && strcmp(reason, "not_market_free_agent") == 0);
    printf("test_holder_org_retained_candidate_is_not_forced: PASS\n");
}

int main(void)
{
    test_zero_demand_retained_candidate_is_not_forced();
    test_market_retained_candidate_with_demand_is_forced();
    test_already_in_org_retained_candidate_is_not_forced();
    test_holder_org_retained_candidate_is_not_forced();
    printf("All foreign retention candidate gate tests passed.\n");
    return 0;
}
