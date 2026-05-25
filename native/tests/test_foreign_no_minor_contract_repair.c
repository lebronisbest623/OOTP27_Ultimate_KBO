#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../src/bootstrap/abi/ootp_offsets.h"
#include "../src/foreign/no_minor_contracts/repair/foreign_no_minor_contract_repair.h"
#include "../src/team/assignment/roster_arrays/team_roster_arrays.h"

static const uint32_t TEST_KBO_LEAGUE_ID = 100u;
static const uint32_t TEST_MINOR_LEAGUE_ID = 101u;
static const uint32_t TEST_PARENT_TEAM_ID = 6u;
static const uint32_t TEST_AFFILIATE_TEAM_ID = 13u;
static const uint32_t TEST_PLAYER_ID = 31246u;

int memory_range_readable(const void* ptr, size_t size)
{
    (void)size;
    return ptr != NULL;
}

static void write_u32(uint8_t* bytes, uint32_t offset, uint32_t value)
{
    *(uint32_t*)(bytes + offset) = value;
}

static uint32_t read_u32(uint8_t* bytes, uint32_t offset)
{
    return *(uint32_t*)(bytes + offset);
}

static void write_team(uint8_t* team, uint32_t team_id, uint32_t league_id, uint32_t parent_team_id)
{
    memset(team, 0, OOTP27_KBO_TEAM_READABLE_BYTES);
    write_u32(team, OOTP27_KBO_TEAM_ID_OFFSET, team_id);
    write_u32(team, OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET, league_id);
    write_u32(team, OOTP27_KBO_TEAM_PARENT_TEAM_ID_OFFSET, parent_team_id);
}

static void write_affiliate_assignment_player(uint8_t* player)
{
    memset(player, 0, OOTP27_PLAYER_SCAN_BYTES);
    write_u32(player, OOTP27_PLAYER_ID_OFFSET, TEST_PLAYER_ID);
    write_u32(player, OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET, TEST_AFFILIATE_TEAM_ID);
    write_u32(player, OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET, TEST_PARENT_TEAM_ID);
    write_u32(player, OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET, TEST_AFFILIATE_TEAM_ID);
    write_u32(player, OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET, TEST_AFFILIATE_TEAM_ID);
    write_u32(player, OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET, TEST_MINOR_LEAGUE_ID);
    write_u32(player, OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET, TEST_MINOR_LEAGUE_ID);
    write_u32(player, OOTP27_PLAYER_ORIGINAL_LEAGUE_ID_OFFSET, TEST_MINOR_LEAGUE_ID);
    player[OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET] = 0u;
    player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET] = 1u;
    player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET] = 1u;
    player[OOTP27_PLAYER_DFA_FLAG_OFFSET] = 1u;
}

static void test_repairs_affiliate_assignment_to_parent_major_contract(void)
{
    uint8_t player[OOTP27_PLAYER_SCAN_BYTES];
    uint8_t affiliate_team[OOTP27_KBO_TEAM_READABLE_BYTES];
    uint8_t parent_team[OOTP27_KBO_TEAM_READABLE_BYTES];
    write_affiliate_assignment_player(player);
    write_team(affiliate_team, TEST_AFFILIATE_TEAM_ID, TEST_MINOR_LEAGUE_ID, TEST_PARENT_TEAM_ID);
    write_team(parent_team, TEST_PARENT_TEAM_ID, TEST_KBO_LEAGUE_ID, 0u);

    ((uint32_t*)(affiliate_team + OOTP27_TEAM_PLAYER_IDS_2760_OFFSET))[0] = TEST_PLAYER_ID;
    ((uint32_t*)(affiliate_team + OOTP27_TEAM_PLAYER_IDS_2A80_OFFSET))[0] = TEST_PLAYER_ID;
    ((uint32_t*)(affiliate_team + OOTP27_TEAM_RESTRICTED_PLAYER_IDS_OFFSET))[0] = TEST_PLAYER_ID;
    ((uint32_t*)(parent_team + OOTP27_TEAM_RESTRICTED_PLAYER_IDS_OFFSET))[0] = TEST_PLAYER_ID;

    KboForeignNoMinorContractRepairResult result;
    int changed = kbo_foreign_no_minor_contract_repair_affiliate_assignment(
        player,
        affiliate_team,
        parent_team,
        &result);

    assert(changed);
    assert(result.changed);
    assert(read_u32(player, OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET) == TEST_PARENT_TEAM_ID);
    assert(read_u32(player, OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET) == TEST_PARENT_TEAM_ID);
    assert(read_u32(player, OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET) == TEST_PARENT_TEAM_ID);
    assert(read_u32(player, OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET) == TEST_PARENT_TEAM_ID);
    assert(read_u32(player, OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET) == TEST_KBO_LEAGUE_ID);
    assert(read_u32(player, OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET) == TEST_KBO_LEAGUE_ID);
    assert(read_u32(player, OOTP27_PLAYER_ORIGINAL_LEAGUE_ID_OFFSET) == TEST_KBO_LEAGUE_ID);
    assert(player[OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET] == 1u);
    assert(player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET] == 0u);
    assert(player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET] == 0u);
    assert(player[OOTP27_PLAYER_DFA_FLAG_OFFSET] == 0u);
    assert(!kbo_team_roster_arrays_contain_player(affiliate_team, TEST_PLAYER_ID));
    assert(!kbo_team_fixed_array_contains_player(parent_team, OOTP27_TEAM_RESTRICTED_PLAYER_IDS_OFFSET, TEST_PLAYER_ID));
    assert(kbo_team_fixed_array_contains_player(parent_team, OOTP27_TEAM_PLAYER_IDS_2760_OFFSET, TEST_PLAYER_ID));
    assert(kbo_team_fixed_array_contains_player(parent_team, OOTP27_TEAM_PLAYER_IDS_2A80_OFFSET, TEST_PLAYER_ID));
    assert(result.removed_affiliate_arrays == 3);
    assert(result.removed_parent_restricted == 1);
    assert(result.added_parent_assignment_arrays == 2);

    printf("test_repairs_affiliate_assignment_to_parent_major_contract: PASS\n");
}

static void test_skips_cross_org_active_team_mismatch(void)
{
    uint8_t player[OOTP27_PLAYER_SCAN_BYTES];
    uint8_t affiliate_team[OOTP27_KBO_TEAM_READABLE_BYTES];
    uint8_t parent_team[OOTP27_KBO_TEAM_READABLE_BYTES];
    write_affiliate_assignment_player(player);
    write_u32(player, OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET, 7u);
    write_team(affiliate_team, TEST_AFFILIATE_TEAM_ID, TEST_MINOR_LEAGUE_ID, TEST_PARENT_TEAM_ID);
    write_team(parent_team, TEST_PARENT_TEAM_ID, TEST_KBO_LEAGUE_ID, 0u);

    int changed = kbo_foreign_no_minor_contract_repair_affiliate_assignment(
        player,
        affiliate_team,
        parent_team,
        NULL);

    assert(!changed);
    assert(read_u32(player, OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET) == TEST_AFFILIATE_TEAM_ID);
    assert(read_u32(player, OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET) == 7u);

    printf("test_skips_cross_org_active_team_mismatch: PASS\n");
}

static void test_skips_main_league_assignment(void)
{
    uint8_t player[OOTP27_PLAYER_SCAN_BYTES];
    uint8_t affiliate_team[OOTP27_KBO_TEAM_READABLE_BYTES];
    uint8_t parent_team[OOTP27_KBO_TEAM_READABLE_BYTES];
    write_affiliate_assignment_player(player);
    write_u32(player, OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET, TEST_KBO_LEAGUE_ID);
    write_team(affiliate_team, TEST_AFFILIATE_TEAM_ID, TEST_MINOR_LEAGUE_ID, TEST_PARENT_TEAM_ID);
    write_team(parent_team, TEST_PARENT_TEAM_ID, TEST_KBO_LEAGUE_ID, 0u);

    int changed = kbo_foreign_no_minor_contract_repair_affiliate_assignment(
        player,
        affiliate_team,
        parent_team,
        NULL);

    assert(!changed);
    assert(read_u32(player, OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET) == TEST_AFFILIATE_TEAM_ID);
    assert(read_u32(player, OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET) == TEST_KBO_LEAGUE_ID);

    printf("test_skips_main_league_assignment: PASS\n");
}

int main(void)
{
    test_repairs_affiliate_assignment_to_parent_major_contract();
    test_skips_cross_org_active_team_mismatch();
    test_skips_main_league_assignment();
    printf("All foreign no-minor contract repair tests passed.\n");
    return 0;
}
