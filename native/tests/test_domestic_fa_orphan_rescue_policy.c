#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/fa_market_investigation/rescue/domestic_fa_orphan_rescue_policy.h"
#include "../src/bootstrap/abi/ootp_offsets.h"

static KboFaMarketPolicy test_policy(void)
{
    KboFaMarketPolicy policy;
    memset(&policy, 0, sizeof(policy));
    policy.investigation_quality_value_score_min = 55000;
    policy.investigation_very_high_demand_min = 700000000;
    policy.investigation_market_days_long_min = 45;
    policy.investigation_market_days_very_long_min = 90;
    policy.orphan_rescue_market_days_min = 45;
    policy.investigation_unexplained_value_score_min = 85000;
    return policy;
}

static KboDomesticFaInvestigationCandidate test_candidate(
    uint32_t player_id,
    const char* grade,
    int32_t value_score,
    int32_t fa_demand,
    uint32_t market_days)
{
    KboDomesticFaInvestigationCandidate candidate;
    memset(&candidate, 0, sizeof(candidate));
    candidate.row.player_id = player_id;
    candidate.row.nation_id = OOTP27_KBO_KOREA_NATION_ID;
    candidate.row.current_team_id = 0u;
    candidate.row.active_team_id = 0u;
    candidate.row.draft_league_id = 0u;
    candidate.row.retired_flag = 0u;
    candidate.row.fa_demand = fa_demand;
    snprintf(candidate.row.case_label, sizeof(candidate.row.case_label), "KBO_FA_CARRYOVER_UNSIGNED");
    snprintf(candidate.row.grade, sizeof(candidate.row.grade), "%s", grade);
    candidate.value_score = value_score;
    candidate.market_days = market_days;
    return candidate;
}

static KboDomesticFaOrphanRescueCachedCandidate cached_candidate(
    uint32_t player_id,
    const char* grade,
    int32_t value_score)
{
    KboDomesticFaOrphanRescueCachedCandidate candidate;
    memset(&candidate, 0, sizeof(candidate));
    candidate.player_id = player_id;
    candidate.value_score = value_score;
    candidate.fa_demand = 300000;
    candidate.market_days = 149u;
    candidate.original_team_id = 5u;
    candidate.age = 30u;
    snprintf(candidate.grade, sizeof(candidate.grade), "%s", grade);
    return candidate;
}

static void test_c_grade_quality_carryovers_are_rescue_eligible(void)
{
    KboFaMarketPolicy policy = test_policy();
    KboDomesticFaInvestigationCandidate shin_min_jae =
        test_candidate(633u, "C", 81045, 300000, 149u);
    KboDomesticFaInvestigationCandidate kim_ho_ryeong =
        test_candidate(1293u, "C", 80725, 300000, 149u);
    KboDomesticFaInvestigationCandidate low_ungraded =
        test_candidate(900001u, "", 30000, 300000, 149u);
    KboDomesticFaInvestigationCandidate high_demand =
        test_candidate(900002u, "C", 120000, 700000000, 149u);

    assert(kbo_domestic_fa_orphan_rescue_candidate_eligible(&shin_min_jae, &policy));
    assert(kbo_domestic_fa_orphan_rescue_candidate_eligible(&kim_ho_ryeong, &policy));
    shin_min_jae.row.draft_league_id = 100u;
    assert(kbo_domestic_fa_orphan_rescue_candidate_eligible(&shin_min_jae, &policy));
    assert(!kbo_domestic_fa_orphan_rescue_candidate_eligible(&low_ungraded, &policy));
    assert(!kbo_domestic_fa_orphan_rescue_candidate_eligible(&high_demand, &policy));
    printf("test_c_grade_quality_carryovers_are_rescue_eligible: PASS\n");
}

static void test_rescue_window_opens_before_very_long_market(void)
{
    KboFaMarketPolicy policy = test_policy();
    KboDomesticFaInvestigationCandidate day_44 =
        test_candidate(900101u, "C", 81045, 300000, 44u);
    KboDomesticFaInvestigationCandidate day_45 =
        test_candidate(900102u, "C", 81045, 300000, 45u);

    assert(!kbo_domestic_fa_orphan_rescue_candidate_eligible(&day_44, &policy));
    assert(kbo_domestic_fa_orphan_rescue_candidate_eligible(&day_45, &policy));
    printf("test_rescue_window_opens_before_very_long_market: PASS\n");
}

static void test_rescue_priority_prefers_c_grade_efficiency(void)
{
    KboDomesticFaOrphanRescueCachedCandidate candidates[4];
    candidates[0] = cached_candidate(715u, "A", 52730);
    candidates[1] = cached_candidate(398u, "B", 82345);
    candidates[2] = cached_candidate(963u, "C", 120530);
    candidates[3] = cached_candidate(633u, "C", 81045);

    qsort(
        candidates,
        sizeof(candidates) / sizeof(candidates[0]),
        sizeof(candidates[0]),
        kbo_domestic_fa_orphan_rescue_compare_cached_desc);

    assert(candidates[0].player_id == 963u);
    assert(candidates[1].player_id == 633u);
    assert(candidates[2].player_id == 398u);
    assert(candidates[3].player_id == 715u);
    printf("test_rescue_priority_prefers_c_grade_efficiency: PASS\n");
}

static void test_team_start_index_spreads_requesters(void)
{
    assert(kbo_domestic_fa_orphan_rescue_team_start_index(0u, 17) == 0);
    assert(kbo_domestic_fa_orphan_rescue_team_start_index(1u, 17) == 0);
    assert(kbo_domestic_fa_orphan_rescue_team_start_index(2u, 17) == 4);
    assert(kbo_domestic_fa_orphan_rescue_team_start_index(3u, 17) == 8);
    assert(kbo_domestic_fa_orphan_rescue_team_start_index(5u, 17) == 16);
    assert(kbo_domestic_fa_orphan_rescue_team_start_index(6u, 17) == 3);
    printf("test_team_start_index_spreads_requesters: PASS\n");
}

static void test_original_team_gets_rescue_before_cross_team(void)
{
    KboFaMarketPolicy policy = test_policy();
    KboDomesticFaOrphanRescueCachedCandidate candidate =
        cached_candidate(633u, "C", 81045);
    candidate.original_team_id = 5u;
    candidate.market_days = 45u;

    assert(kbo_domestic_fa_orphan_rescue_candidate_original_team_fit(&candidate, 5u));
    assert(!kbo_domestic_fa_orphan_rescue_candidate_original_team_fit(&candidate, 7u));
    assert(kbo_domestic_fa_orphan_rescue_candidate_team_allowed(&candidate, 5u, &policy));
    assert(!kbo_domestic_fa_orphan_rescue_candidate_team_allowed(&candidate, 7u, &policy));

    candidate.market_days = 90u;
    assert(kbo_domestic_fa_orphan_rescue_candidate_team_allowed(&candidate, 7u, &policy));
    printf("test_original_team_gets_rescue_before_cross_team: PASS\n");
}

int main(void)
{
    test_c_grade_quality_carryovers_are_rescue_eligible();
    test_rescue_window_opens_before_very_long_market();
    test_rescue_priority_prefers_c_grade_efficiency();
    test_team_start_index_spreads_requesters();
    test_original_team_gets_rescue_before_cross_team();
    printf("All domestic FA orphan rescue policy tests passed.\n");
    return 0;
}
