#include <assert.h>
#include <stdio.h>

#include "../src/team/add_player_guard/foreign_policy/team_add_player_guard_former_org_policy.h"

static void test_released_market_player_does_not_block_when_rights_lookup_ready(void)
{
    assert(!kbo_team_add_former_org_market_block_applies(
        0u,
        0u,
        4u,
        100u,
        100u,
        4u,
        5u,
        1,
        0));
    printf("test_released_market_player_does_not_block_when_rights_lookup_ready: PASS\n");
}

static void test_pre_rights_lookup_keeps_former_org_block(void)
{
    assert(kbo_team_add_former_org_market_block_applies(
        0u,
        0u,
        4u,
        100u,
        100u,
        4u,
        5u,
        0,
        0));
    printf("test_pre_rights_lookup_keeps_former_org_block: PASS\n");
}

static void test_same_org_does_not_block(void)
{
    assert(!kbo_team_add_former_org_market_block_applies(
        0u,
        0u,
        4u,
        100u,
        100u,
        4u,
        4u,
        0,
        0));
    printf("test_same_org_does_not_block: PASS\n");
}

static void test_assigned_player_does_not_block(void)
{
    assert(!kbo_team_add_former_org_market_block_applies(
        4u,
        4u,
        4u,
        100u,
        100u,
        4u,
        5u,
        0,
        0));
    printf("test_assigned_player_does_not_block: PASS\n");
}

static void test_disable_flag_does_not_block(void)
{
    assert(!kbo_team_add_former_org_market_block_applies(
        0u,
        0u,
        4u,
        100u,
        100u,
        4u,
        5u,
        0,
        1));
    printf("test_disable_flag_does_not_block: PASS\n");
}

int main(void)
{
    test_released_market_player_does_not_block_when_rights_lookup_ready();
    test_pre_rights_lookup_keeps_former_org_block();
    test_same_org_does_not_block();
    test_assigned_player_does_not_block();
    test_disable_flag_does_not_block();
    printf("All team-add former-org policy tests passed.\n");
    return 0;
}
