#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../src/foreign/signability/no_minor_demand/submit_offer_probe_no_minor_demand_internal.h"

static const uint32_t TEST_LEAGUE_ID = 100u;
static const uint32_t TEST_PLAYER_ID = 31246u;

int kbo_foreign_policy_player_id_plausible(uint32_t player_id)
{
    return player_id > 0u && player_id < 1000000u;
}

int kbo_foreign_policy_market_age_allowed(uint16_t age)
{
    return age >= 16u && age <= 60u;
}

int kbo_foreign_policy_demand_salary_plausible(int32_t demand)
{
    return demand > 0 && demand <= 2000000;
}

static void write_u16(uint8_t* player, uint32_t offset, uint16_t value)
{
    *(uint16_t*)(player + offset) = value;
}

static void write_u32(uint8_t* player, uint32_t offset, uint32_t value)
{
    *(uint32_t*)(player + offset) = value;
}

static void write_i32(uint8_t* player, uint32_t offset, int32_t value)
{
    *(int32_t*)(player + offset) = value;
}

static void write_teamless_fa(uint8_t* player, uint32_t nation_id)
{
    memset(player, 0, OOTP27_PLAYER_SCAN_BYTES);
    write_u32(player, OOTP27_PLAYER_ID_OFFSET, TEST_PLAYER_ID);
    write_u16(player, OOTP27_PLAYER_AGE_OFFSET, 28u);
    write_u32(player, OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET, TEST_LEAGUE_ID);
    write_u32(player, OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET, TEST_LEAGUE_ID);
    write_u32(player, OOTP27_PLAYER_NATION_ID_OFFSET, nation_id);
    write_i32(player, OOTP27_PLAYER_FA_DEMAND_SALARY_OFFSET, 100000);
}

static void test_foreign_teamless_demand_is_baseline_only(void)
{
    uint8_t player[OOTP27_PLAYER_SCAN_BYTES];
    write_teamless_fa(player, 1u);

    assert(kbo_no_minor_scan_is_teamless_demand_floor_candidate(player, TEST_LEAGUE_ID));
    assert(kbo_no_minor_scan_is_foreign_fa_candidate(player));
    assert(!kbo_no_minor_scan_should_floor_teamless_demand(player, TEST_LEAGUE_ID));

    printf("test_foreign_teamless_demand_is_baseline_only: PASS\n");
}

static void test_domestic_teamless_demand_can_use_no_minor_floor(void)
{
    uint8_t player[OOTP27_PLAYER_SCAN_BYTES];
    write_teamless_fa(player, OOTP27_KBO_KOREA_NATION_ID);

    assert(kbo_no_minor_scan_is_teamless_demand_floor_candidate(player, TEST_LEAGUE_ID));
    assert(!kbo_no_minor_scan_is_foreign_fa_candidate(player));
    assert(kbo_no_minor_scan_should_floor_teamless_demand(player, TEST_LEAGUE_ID));

    printf("test_domestic_teamless_demand_can_use_no_minor_floor: PASS\n");
}

int main(void)
{
    test_foreign_teamless_demand_is_baseline_only();
    test_domestic_teamless_demand_can_use_no_minor_floor();
    printf("All no-minor demand classification tests passed.\n");
    return 0;
}
