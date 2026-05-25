#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "../src/offer_candidate/replacement/offer_candidate_replacement_dispatcher.h"

static int g_domestic_rescue_enabled = 0;
static int g_domestic_rescue_call_count = 0;
static uintptr_t g_domestic_rescue_replacement = 0u;
static uintptr_t g_last_original_candidate = 0u;
static uint32_t g_last_requester_team_id = 0u;
static uint32_t g_last_requester_league_id = 0u;
static uint32_t g_last_today = 0u;

static void reset_domestic_rescue_stub(void)
{
    g_domestic_rescue_enabled = 0;
    g_domestic_rescue_call_count = 0;
    g_domestic_rescue_replacement = 0u;
    g_last_original_candidate = 0u;
    g_last_requester_team_id = 0u;
    g_last_requester_league_id = 0u;
    g_last_today = 0u;
}

int kbo_domestic_fa_orphan_rescue_enabled(void)
{
    return g_domestic_rescue_enabled;
}

uintptr_t kbo_domestic_fa_orphan_rescue_offer_candidate_replacement(
    uintptr_t original_candidate_ptr,
    uint32_t requester_team_id,
    uint32_t requester_league_id,
    uint32_t today)
{
    g_domestic_rescue_call_count++;
    g_last_original_candidate = original_candidate_ptr;
    g_last_requester_team_id = requester_team_id;
    g_last_requester_league_id = requester_league_id;
    g_last_today = today;
    return g_domestic_rescue_replacement != 0u
        ? g_domestic_rescue_replacement
        : original_candidate_ptr;
}

static void test_needs_hook_follows_rescue_policy(void)
{
    reset_domestic_rescue_stub();
    assert(!kbo_offer_candidate_replacement_dispatcher_needs_hook());
    g_domestic_rescue_enabled = 1;
    assert(kbo_offer_candidate_replacement_dispatcher_needs_hook());
    printf("test_needs_hook_follows_rescue_policy: PASS\n");
}

static void test_invalid_dispatch_input_keeps_original_without_calling_rescue(void)
{
    reset_domestic_rescue_stub();

    KboOfferCandidateReplacementResult result =
        kbo_offer_candidate_replacement_dispatch(0u, 7u, 100u, 20260701u);
    assert(result.player_ptr == 0u);
    assert(result.source == KBO_OFFER_CANDIDATE_REPLACEMENT_NONE);

    result = kbo_offer_candidate_replacement_dispatch(0x1000u, 0u, 100u, 20260701u);
    assert(result.player_ptr == 0x1000u);
    assert(result.source == KBO_OFFER_CANDIDATE_REPLACEMENT_NONE);

    result = kbo_offer_candidate_replacement_dispatch(0x1000u, 7u, 100u, 0u);
    assert(result.player_ptr == 0x1000u);
    assert(result.source == KBO_OFFER_CANDIDATE_REPLACEMENT_NONE);
    assert(g_domestic_rescue_call_count == 0);
    printf("test_invalid_dispatch_input_keeps_original_without_calling_rescue: PASS\n");
}

static void test_domestic_rescue_miss_keeps_original_and_records_call(void)
{
    reset_domestic_rescue_stub();

    KboOfferCandidateReplacementResult result =
        kbo_offer_candidate_replacement_dispatch(0x1000u, 7u, 100u, 20260701u);

    assert(result.player_ptr == 0x1000u);
    assert(result.source == KBO_OFFER_CANDIDATE_REPLACEMENT_NONE);
    assert(g_domestic_rescue_call_count == 1);
    assert(g_last_original_candidate == 0x1000u);
    assert(g_last_requester_team_id == 7u);
    assert(g_last_requester_league_id == 100u);
    assert(g_last_today == 20260701u);
    printf("test_domestic_rescue_miss_keeps_original_and_records_call: PASS\n");
}

static void test_domestic_rescue_hit_tags_replacement_source(void)
{
    reset_domestic_rescue_stub();
    g_domestic_rescue_replacement = 0x2000u;

    KboOfferCandidateReplacementResult result =
        kbo_offer_candidate_replacement_dispatch(0x1000u, 7u, 100u, 20260701u);

    assert(result.player_ptr == 0x2000u);
    assert(result.source == KBO_OFFER_CANDIDATE_REPLACEMENT_DOMESTIC_FA_RESCUE);
    assert(g_domestic_rescue_call_count == 1);
    printf("test_domestic_rescue_hit_tags_replacement_source: PASS\n");
}

int main(void)
{
    test_needs_hook_follows_rescue_policy();
    test_invalid_dispatch_input_keeps_original_without_calling_rescue();
    test_domestic_rescue_miss_keeps_original_and_records_call();
    test_domestic_rescue_hit_tags_replacement_source();
    printf("All offer candidate replacement dispatcher tests passed.\n");
    return 0;
}
