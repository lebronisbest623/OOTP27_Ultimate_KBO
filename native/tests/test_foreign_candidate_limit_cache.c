#include <assert.h>
#include <stdint.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../src/foreign/quota/candidates/cache/foreign_quota_candidate_limit_cache.h"

volatile LONG g_kbo_foreign_waiver_rights_generation = 1;
static uint64_t g_test_injury_replacement_fingerprint = 17ull;

uint64_t kbo_foreign_injury_replacement_fingerprint(void)
{
    return g_test_injury_replacement_fingerprint;
}

LONG kbo_custom_foreign_pending_offer_generation_for_team(uint32_t team_id)
{
    return (LONG)(team_id + 11u);
}

uint32_t kbo_foreign_org_count_cache_generation_for_team(uint32_t team_id)
{
    return team_id + 101u;
}

int main(void)
{
    int has_slot = 1;
    assert(!kbo_custom_foreign_extra_slot_team_cache_hit(
        7u,
        20270403u,
        100u,
        0u,
        &has_slot));

    kbo_custom_foreign_extra_slot_team_cache_store(
        7u,
        20270403u,
        100u,
        0u,
        0);
    has_slot = 1;
    assert(kbo_custom_foreign_extra_slot_team_cache_hit(
        7u,
        20270403u,
        100u,
        0u,
        &has_slot));
    assert(has_slot == 0);

    assert(!kbo_custom_foreign_extra_slot_team_cache_hit(
        7u,
        20270403u,
        100u,
        1u,
        &has_slot));

    kbo_custom_foreign_extra_slot_team_cache_store(
        7u,
        20270403u,
        100u,
        1u,
        1);
    has_slot = 0;
    assert(kbo_custom_foreign_extra_slot_team_cache_hit(
        7u,
        20270403u,
        100u,
        1u,
        &has_slot));
    assert(has_slot == 1);

    g_test_injury_replacement_fingerprint++;
    assert(!kbo_custom_foreign_extra_slot_team_cache_hit(
        7u,
        20270403u,
        100u,
        1u,
        &has_slot));

    return 0;
}
