#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../src/bootstrap/abi/ootp_offsets.h"
#include "../src/foreign/quota/team_policy/foreign_quota_team_policy.h"
#include "../src/team/classification/parse/team_classification_seed_parse.h"

static uint8_t g_sang_team[OOTP27_KBO_TEAM_READABLE_BYTES];
static uint8_t g_kpb_team[OOTP27_KBO_TEAM_READABLE_BYTES];
static uint32_t g_military_policy_team_id = 0u;
static uint32_t g_independent_futures_team_id = 0u;
static uint32_t g_independent_league_team_id = 0u;
static uint32_t g_base_limit = 3u;

int memory_range_readable(const void* address, SIZE_T size)
{
    if (address == NULL || size == 0u) {
        return 0;
    }

    uintptr_t start = (uintptr_t)address;
    uintptr_t end = start + (uintptr_t)size;
    if (end < start) {
        return 0;
    }

    const struct {
        uintptr_t start;
        uintptr_t end;
    } ranges[] = {
        { (uintptr_t)g_sang_team, (uintptr_t)g_sang_team + sizeof(g_sang_team) },
        { (uintptr_t)g_kpb_team, (uintptr_t)g_kpb_team + sizeof(g_kpb_team) }
    };

    for (size_t i = 0u; i < sizeof(ranges) / sizeof(ranges[0]); i++) {
        if (start >= ranges[i].start && end <= ranges[i].end) {
            return 1;
        }
    }
    return 0;
}

uint8_t* find_kbo_team_by_csv_id_any_league(const char* csv_id, int allow_inactive)
{
    (void)allow_inactive;
    if (csv_id != NULL && _stricmp(csv_id, "SANG") == 0) {
        return g_sang_team;
    }
    if (csv_id != NULL && _stricmp(csv_id, "KPB") == 0) {
        return g_kpb_team;
    }
    return NULL;
}

int kbo_team_id_is_military_service_team(uint32_t team_id)
{
    return team_id != 0u && team_id == g_military_policy_team_id;
}

int kbo_team_classification_independent_kind_for_team(uint32_t team_id)
{
    if (team_id != 0u && team_id == g_independent_futures_team_id) {
        return KBO_TEAM_CLASSIFICATION_INDEPENDENT_KIND_FUTURES;
    }
    if (team_id != 0u && team_id == g_independent_league_team_id) {
        return KBO_TEAM_CLASSIFICATION_INDEPENDENT_KIND_LEAGUE;
    }
    return KBO_TEAM_CLASSIFICATION_INDEPENDENT_KIND_NONE;
}

uint32_t kbo_custom_foreign_base_effective_limit(void)
{
    return g_base_limit;
}

static void seed_team(uint8_t* team, uint32_t team_id)
{
    memset(team, 0, OOTP27_KBO_TEAM_READABLE_BYTES);
    *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET) = team_id;
}

static void test_military_and_police_teams_block_foreign_ownership(void)
{
    g_military_policy_team_id = 10u;

    assert(kbo_foreign_quota_team_is_military_or_police(10u));
    assert(kbo_foreign_quota_team_blocks_foreign_ownership(10u));
    assert(kbo_foreign_quota_base_effective_limit_for_team(10u) == 0u);
    assert(!kbo_foreign_quota_team_allows_injury_extra_slots(10u));

    assert(kbo_foreign_quota_team_is_military_or_police(60u));
    assert(kbo_foreign_quota_team_blocks_foreign_ownership(60u));
    assert(kbo_foreign_quota_base_effective_limit_for_team(60u) == 0u);

    assert(kbo_foreign_quota_team_is_military_or_police(70u));
    assert(kbo_foreign_quota_team_blocks_foreign_ownership(70u));
    assert(kbo_foreign_quota_base_effective_limit_for_team(70u) == 0u);

    printf("test_military_and_police_teams_block_foreign_ownership: PASS\n");
}

static void test_futures_independent_team_uses_four_player_raw_limit(void)
{
    g_independent_futures_team_id = 28u;

    assert(!kbo_foreign_quota_team_blocks_foreign_ownership(28u));
    assert(kbo_foreign_quota_team_is_futures_independent(28u));
    assert(kbo_foreign_quota_base_effective_limit_for_team(28u)
        == KBO_FOREIGN_QUOTA_FUTURES_INDEPENDENT_FOREIGN_LIMIT);
    assert(!kbo_foreign_quota_team_allows_injury_extra_slots(28u));
    assert(kbo_foreign_quota_effective_count_for_team(28u, 1u, 3u) == 4u);
    assert(kbo_foreign_quota_effective_count_for_team(28u, 1u, 4u) == 5u);

    printf("test_futures_independent_team_uses_four_player_raw_limit: PASS\n");
}

static void test_other_teams_keep_standard_effective_limit(void)
{
    g_independent_league_team_id = 91u;
    g_base_limit = 3u;

    assert(!kbo_foreign_quota_team_is_futures_independent(91u));
    assert(kbo_foreign_quota_base_effective_limit_for_team(91u) == 3u);
    assert(kbo_foreign_quota_team_allows_injury_extra_slots(91u));
    assert(kbo_foreign_quota_effective_count_for_team(91u, 1u, 3u) == 3u);
    assert(kbo_foreign_quota_effective_count_for_team(99u, 2u, 2u) == 3u);

    printf("test_other_teams_keep_standard_effective_limit: PASS\n");
}

int main(void)
{
    seed_team(g_sang_team, 60u);
    seed_team(g_kpb_team, 70u);

    test_military_and_police_teams_block_foreign_ownership();
    test_futures_independent_team_uses_four_player_raw_limit();
    test_other_teams_keep_standard_effective_limit();
    printf("Foreign quota team-policy tests passed.\n");
    return 0;
}
