#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../src/bootstrap/abi/ootp_offsets.h"
#include "../src/foreign/signability/foreign_policy/wrappers/offer_attach/contract_type/foreign_ai_offer_contract_type.h"
#include "../src/foreign/signability/foreign_policy/wrappers/offer_attach/foreign_signability_offer_attach_probe_utils.h"

static uint8_t g_team_9[OOTP27_KBO_TEAM_READABLE_BYTES];
static uint8_t g_team_18[OOTP27_KBO_TEAM_READABLE_BYTES];
static uint8_t g_team_28[OOTP27_KBO_TEAM_READABLE_BYTES];

int memory_range_readable(const void* address, SIZE_T size)
{
    return address != NULL && size > 0u;
}

uint32_t kbo_resolve_kbo_league_id(void)
{
    return 100u;
}

uint8_t* find_kbo_team_by_numeric_id_any_league(uint32_t team_id, int allow_deleted)
{
    (void)allow_deleted;
    if (team_id == 9u) {
        return g_team_9;
    }
    if (team_id == 18u) {
        return g_team_18;
    }
    if (team_id == 28u) {
        return g_team_28;
    }
    return NULL;
}

int kbo_player_is_foreign_for_kbo_rights(uint8_t* player)
{
    return player != NULL && player[0] == 1u;
}

void kbo_log_runtimef_at(const char* file, int line, const char* format, ...)
{
    (void)file;
    (void)line;
    (void)format;
}

static void configure_teams(void)
{
    memset(g_team_9, 0, sizeof(g_team_9));
    memset(g_team_18, 0, sizeof(g_team_18));
    memset(g_team_28, 0, sizeof(g_team_28));
    *(uint32_t*)(g_team_9 + OOTP27_KBO_TEAM_ID_OFFSET) = 9u;
    *(uint32_t*)(g_team_9 + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET) = 100u;
    *(uint32_t*)(g_team_18 + OOTP27_KBO_TEAM_ID_OFFSET) = 18u;
    *(uint32_t*)(g_team_18 + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET) = 101u;
    *(uint32_t*)(g_team_18 + OOTP27_KBO_TEAM_PARENT_TEAM_ID_OFFSET) = 9u;
    *(uint32_t*)(g_team_28 + OOTP27_KBO_TEAM_ID_OFFSET) = 28u;
    *(uint32_t*)(g_team_28 + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET) = 200u;
}

static void test_apply_bytes_forces_major_and_clears_minor(void)
{
    uint8_t offer[KBO_OFFER_READABLE_BYTES] = {0};
    offer[KBO_OFFER_MAJOR_FLAG_OFFSET] = 0u;
    offer[KBO_OFFER_MINOR_FLAG_OFFSET] = 1u;

    KboForeignAiOfferContractTypeResult result;
    assert(kbo_foreign_ai_offer_contract_type_apply_bytes(offer, sizeof(offer), &result));
    assert(result.eligible);
    assert(result.changed);
    assert(result.before_major == 0u);
    assert(result.before_minor == 1u);
    assert(offer[KBO_OFFER_MAJOR_FLAG_OFFSET] == 1u);
    assert(offer[KBO_OFFER_MINOR_FLAG_OFFSET] == 0u);
}

static void test_kbo_foreign_offer_is_forced_before_acceptance(void)
{
    uint8_t player[OOTP27_PLAYER_SCAN_BYTES] = {0};
    uint8_t offer[KBO_OFFER_READABLE_BYTES] = {0};
    player[0] = 1u;
    *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET) = 31199u;
    offer[KBO_OFFER_MAJOR_FLAG_OFFSET] = 0u;
    offer[KBO_OFFER_MINOR_FLAG_OFFSET] = 1u;

    assert(kbo_foreign_ai_offer_force_major_contract(
        (uintptr_t)player,
        (uintptr_t)offer,
        9,
        "test"));
    assert(offer[KBO_OFFER_MAJOR_FLAG_OFFSET] == 1u);
    assert(offer[KBO_OFFER_MINOR_FLAG_OFFSET] == 0u);
}

static void test_kbo_affiliate_offer_is_forced_to_parent_major_terms(void)
{
    uint8_t player[OOTP27_PLAYER_SCAN_BYTES] = {0};
    uint8_t offer[KBO_OFFER_READABLE_BYTES] = {0};
    player[0] = 1u;
    *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET) = 31199u;
    *(int32_t*)(offer + KBO_OFFER_TEAM_ID_OFFSET) = 18;
    offer[KBO_OFFER_MAJOR_FLAG_OFFSET] = 0u;
    offer[KBO_OFFER_MINOR_FLAG_OFFSET] = 1u;

    assert(kbo_foreign_ai_offer_force_major_contract(
        (uintptr_t)player,
        (uintptr_t)offer,
        0,
        "test"));
    assert(offer[KBO_OFFER_MAJOR_FLAG_OFFSET] == 1u);
    assert(offer[KBO_OFFER_MINOR_FLAG_OFFSET] == 0u);
}

static void test_non_foreign_or_non_kbo_offer_is_not_touched(void)
{
    uint8_t player[OOTP27_PLAYER_SCAN_BYTES] = {0};
    uint8_t offer[KBO_OFFER_READABLE_BYTES] = {0};
    offer[KBO_OFFER_MAJOR_FLAG_OFFSET] = 0u;
    offer[KBO_OFFER_MINOR_FLAG_OFFSET] = 1u;

    assert(!kbo_foreign_ai_offer_force_major_contract(
        (uintptr_t)player,
        (uintptr_t)offer,
        9,
        "test"));
    assert(offer[KBO_OFFER_MAJOR_FLAG_OFFSET] == 0u);
    assert(offer[KBO_OFFER_MINOR_FLAG_OFFSET] == 1u);

    player[0] = 1u;
    assert(!kbo_foreign_ai_offer_force_major_contract(
        (uintptr_t)player,
        (uintptr_t)offer,
        28,
        "test"));
    assert(offer[KBO_OFFER_MAJOR_FLAG_OFFSET] == 0u);
    assert(offer[KBO_OFFER_MINOR_FLAG_OFFSET] == 1u);
}

int main(void)
{
    configure_teams();
    test_apply_bytes_forces_major_and_clears_minor();
    test_kbo_foreign_offer_is_forced_before_acceptance();
    test_kbo_affiliate_offer_is_forced_to_parent_major_terms();
    test_non_foreign_or_non_kbo_offer_is_not_touched();
    printf("All foreign AI offer contract type tests passed.\n");
    return 0;
}
