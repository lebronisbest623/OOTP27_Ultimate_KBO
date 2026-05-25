#define WIN32_LEAN_AND_MEAN
#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

#include "../src/bootstrap/abi/ootp_offsets.h"
#include "../src/fa_market_investigation/rescue/domestic_fa_orphan_rescue.h"

static int g_collect_cached_calls = 0;
static uint8_t g_rescue_player[OOTP27_PLAYER_SCAN_BYTES];

int memory_range_readable(const void* address, SIZE_T size)
{
    return address != NULL && size > 0u;
}

int kbo_domestic_fa_orphan_rescue_enabled(void)
{
    return 1;
}

int kbo_domestic_fa_orphan_rescue_dry_run(void)
{
    return 0;
}

uint32_t kbo_league_role_main_league_id(void)
{
    return 100u;
}

int kbo_get_foreign_waiver_current_yyyymmdd(uint32_t* out_date)
{
    if (out_date != NULL) {
        *out_date = 20261218u;
    }
    return 1;
}

const KboFaMarketPolicy* kbo_fa_market_policy(void)
{
    static KboFaMarketPolicy policy;
    return &policy;
}

int32_t kbo_foreign_waiver_value_score(uint8_t* player)
{
    if (player == NULL) {
        return 0;
    }
    return *(int16_t*)(player + OOTP27_PLAYER_TALENT_VALUE_OFFSET);
}

int kbo_player_is_foreign_for_kbo_rights(uint8_t* player)
{
    if (player == NULL) {
        return 0;
    }
    uint32_t nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
    return nation_id != 0u && nation_id != OOTP27_KBO_KOREA_NATION_ID;
}

int kbo_domestic_fa_orphan_rescue_collect_cached(
    uint32_t today,
    KboDomesticFaOrphanRescueCachedCandidate* out_candidates,
    int max_candidates)
{
    g_collect_cached_calls++;
    assert(today == 20261218u);
    if (out_candidates == NULL || max_candidates <= 0) {
        return 0;
    }

    memset(out_candidates, 0, sizeof(out_candidates[0]));
    out_candidates[0].player_id = 9001u;
    out_candidates[0].value_score = 200;
    out_candidates[0].original_team_id = 7u;
    out_candidates[0].market_days = 90u;
    snprintf(out_candidates[0].grade, sizeof(out_candidates[0].grade), "C");
    snprintf(out_candidates[0].case_label, sizeof(out_candidates[0].case_label), "test");
    return 1;
}

int kbo_domestic_fa_orphan_rescue_candidate_original_team_fit(
    const KboDomesticFaOrphanRescueCachedCandidate* candidate,
    uint32_t requester_team_id)
{
    return candidate != NULL && candidate->original_team_id == requester_team_id;
}

int kbo_domestic_fa_orphan_rescue_candidate_team_allowed(
    const KboDomesticFaOrphanRescueCachedCandidate* candidate,
    uint32_t requester_team_id,
    const KboFaMarketPolicy* policy)
{
    (void)policy;
    return candidate != NULL && candidate->original_team_id == requester_team_id;
}

int kbo_domestic_fa_orphan_rescue_team_start_index(
    uint32_t requester_team_id,
    int candidate_count)
{
    (void)requester_team_id;
    (void)candidate_count;
    return 0;
}

uint8_t* kbo_find_player_by_id(
    uint32_t player_id,
    uint32_t* out_current_team_id,
    uint32_t* out_current_league_id)
{
    (void)out_current_team_id;
    (void)out_current_league_id;
    return player_id == 9001u ? g_rescue_player : NULL;
}

int kbo_domestic_fa_orphan_rescue_player_can_enter_market(
    uint8_t* player,
    uint32_t expected_player_id)
{
    return player != NULL
        && *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET) == expected_player_id;
}

void kbo_domestic_fa_orphan_rescue_record_candidate_evidence(
    const KboDomesticFaOrphanRescueCachedCandidate* candidate,
    uint32_t requester_team_id,
    int32_t before_index,
    int32_t after_index,
    uint32_t today,
    int dry_run)
{
    (void)candidate;
    (void)requester_team_id;
    (void)before_index;
    (void)after_index;
    (void)today;
    (void)dry_run;
}

void kbo_log_runtimef_at(const char* file, int line, const char* format, ...)
{
    (void)file;
    (void)line;
    (void)format;
}

static void make_player(uint8_t* player, uint32_t player_id, uint32_t nation_id, int16_t score)
{
    memset(player, 0, OOTP27_PLAYER_SCAN_BYTES);
    *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET) = player_id;
    *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET) = nation_id;
    *(int16_t*)(player + OOTP27_PLAYER_TALENT_VALUE_OFFSET) = score;
}

static void test_foreign_original_candidate_is_not_replaced_by_domestic_rescue(void)
{
    uint8_t original[OOTP27_PLAYER_SCAN_BYTES];
    make_player(original, 31050u, 98u, 100);
    make_player(g_rescue_player, 9001u, OOTP27_KBO_KOREA_NATION_ID, 200);
    g_collect_cached_calls = 0;

    uintptr_t result = kbo_domestic_fa_orphan_rescue_offer_candidate_replacement(
        (uintptr_t)original,
        7u,
        100u,
        20261218u);

    assert(result == (uintptr_t)original);
    assert(g_collect_cached_calls == 0);
    printf("test_foreign_original_candidate_is_not_replaced_by_domestic_rescue: PASS\n");
}

static void test_domestic_original_candidate_can_still_use_rescue(void)
{
    uint8_t original[OOTP27_PLAYER_SCAN_BYTES];
    make_player(original, 633u, OOTP27_KBO_KOREA_NATION_ID, 100);
    make_player(g_rescue_player, 9001u, OOTP27_KBO_KOREA_NATION_ID, 200);
    g_collect_cached_calls = 0;

    uintptr_t result = kbo_domestic_fa_orphan_rescue_offer_candidate_replacement(
        (uintptr_t)original,
        7u,
        100u,
        20261218u);

    assert(result == (uintptr_t)g_rescue_player);
    assert(g_collect_cached_calls == 1);
    printf("test_domestic_original_candidate_can_still_use_rescue: PASS\n");
}

int main(void)
{
    test_foreign_original_candidate_is_not_replaced_by_domestic_rescue();
    test_domestic_original_candidate_can_still_use_rescue();
    printf("All domestic FA orphan rescue offer replacement tests passed.\n");
    return 0;
}
