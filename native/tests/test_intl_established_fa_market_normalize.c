#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../src/core/league_roles/kbo_league_roles.h"
#include "../src/foreign/intl_established_fa_postscan/internal/intl_established_fa_postscan_internal.h"

static const uint32_t TEST_MAIN_LEAGUE_ID = 100u;

int memory_range_readable(const void* ptr, size_t size)
{
    return ptr != NULL && size <= OOTP27_PLAYER_SCAN_BYTES;
}

const KboForeignPlayerPolicy* kbo_foreign_player_policy(void)
{
    static KboForeignPlayerPolicy policy = {
        .demand_salary_max = 2000000,
        .market_age_min = 16,
        .market_age_max = 60,
        .reserve_demand_score_min = {0, 20000, 40000, 60000, 80000, 100000, 120000, 140000, 160000}
    };
    return &policy;
}

int kbo_foreign_policy_market_age_allowed(uint16_t age)
{
    const KboForeignPlayerPolicy* policy = kbo_foreign_player_policy();
    return age >= (uint16_t)policy->market_age_min && age <= (uint16_t)policy->market_age_max;
}

int32_t kbo_get_foreign_fa_demand_baseline_value_for_player(int index, int asian_quota)
{
    (void)index;
    return asian_quota ? 120000 : 1050000;
}

static void write_u32(uint8_t* player, uint32_t offset, uint32_t value)
{
    *(uint32_t*)(player + offset) = value;
}

static void write_i16(uint8_t* player, uint32_t offset, int16_t value)
{
    *(int16_t*)(player + offset) = value;
}

static int32_t read_i32(uint8_t* player, uint32_t offset)
{
    return *(int32_t*)(player + offset);
}

static uint32_t read_u32(uint8_t* player, uint32_t offset)
{
    return *(uint32_t*)(player + offset);
}

static void test_preserves_ootp_market_identity_fields(void)
{
    uint8_t player[OOTP27_PLAYER_SCAN_BYTES];
    memset(player, 0, sizeof(player));

    write_u32(player, OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET, TEST_MAIN_LEAGUE_ID);
    write_i16(player, OOTP27_PLAYER_AGE_OFFSET, 27);
    player[OOTP27_PLAYER_GENERATION_CONTEXT_OFFSET] = 3u;
    player[OOTP27_PLAYER_DRAFT_CLASS_OFFSET] = 4u;
    player[OOTP27_PLAYER_DRAFT_SUBTYPE_OFFSET] = 2u;
    player[OOTP27_PLAYER_DRAFT_ELIGIBLE_OFFSET] = 1u;
    player[OOTP27_PLAYER_DRAFT_EXTRA_FLAG_OFFSET] = 7u;
    player[OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET] = 1u;

    KboIntlEstablishedFaMarketNormalization normalized;
    int changed = kbo_intl_established_fa_normalize_market_state(
        player,
        TEST_MAIN_LEAGUE_ID,
        0u,
        0,
        110000,
        &normalized);

    assert(changed);
    assert(normalized.changed);
    assert(normalized.market_ready);
    assert(normalized.draft_fields_cleared);
    assert(!normalized.draft_league_cleared);
    assert(!normalized.contract_level_cleared);
    assert(read_u32(player, OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET) == TEST_MAIN_LEAGUE_ID);
    assert(player[OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET] == 1u);
    assert(player[OOTP27_PLAYER_DRAFT_CLASS_OFFSET] == 4u);
    assert(player[OOTP27_PLAYER_DRAFT_SUBTYPE_OFFSET] == 2u);
    assert(player[OOTP27_PLAYER_DRAFT_ELIGIBLE_OFFSET] == 0u);
    assert(player[OOTP27_PLAYER_DRAFT_EXTRA_FLAG_OFFSET] == 7u);
    assert(read_u32(player, OOTP27_PLAYER_ORIGINAL_LEAGUE_ID_OFFSET) == TEST_MAIN_LEAGUE_ID);
    assert(read_i32(player, OOTP27_PLAYER_FA_DEMAND_SALARY_OFFSET) == 1050000);

    printf("test_preserves_ootp_market_identity_fields: PASS\n");
}

int main(void)
{
    test_preserves_ootp_market_identity_fields();
    printf("All international established FA market normalization tests passed.\n");
    return 0;
}
