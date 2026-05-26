#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../src/bootstrap/abi/ootp_offsets.h"
#include "../src/foreign/signability/foreign_policy/wrappers/offer_attach/foreign_ai_offer_contract_type.h"
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

static void test_generated_foreign_offer_salary_matches_demand_baseline(void)
{
    uint8_t player[OOTP27_PLAYER_SCAN_BYTES] = {0};
    uint8_t offer[KBO_OFFER_READABLE_BYTES] = {0};
    player[0] = 1u;
    *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET) = 31055u;
    player[OOTP27_PLAYER_GENERATION_CONTEXT_OFFSET] = 3u;
    *(int32_t*)(player + OOTP27_PLAYER_FA_DEMAND_SALARY_OFFSET) = 1150000;
    *(int32_t*)(offer + KBO_OFFER_SALARY_PRIMARY_OFFSET) = 1014000;
    *(int32_t*)(offer + KBO_OFFER_SALARY_FIRST_YEAR_OFFSET) = 1030000;

    assert(kbo_foreign_ai_offer_match_demand_salary(
        (uintptr_t)player,
        (uintptr_t)offer,
        9,
        0,
        "test"));
    assert(*(int32_t*)(offer + KBO_OFFER_SALARY_PRIMARY_OFFSET) == 1150000);
    assert(*(int32_t*)(offer + KBO_OFFER_SALARY_FIRST_YEAR_OFFSET) == 1150000);
}

static void test_generated_foreign_final_gate_salary_arg_matches_demand_baseline(void)
{
    KboForeignAiOfferFinalGateSalaryResult result;
    assert(kbo_foreign_ai_offer_final_gate_salary_apply(
        700000,
        1150000,
        1,
        0,
        &result));
    assert(result.eligible);
    assert(result.changed);
    assert(result.before_salary_arg == 700000);
    assert(result.after_salary_arg == 1150000);
}

static void test_generated_foreign_final_gate_salary_arg_is_not_lowered(void)
{
    KboForeignAiOfferFinalGateSalaryResult result;
    assert(!kbo_foreign_ai_offer_final_gate_salary_apply(
        1300000,
        1150000,
        1,
        0,
        &result));
    assert(result.eligible);
    assert(!result.changed);
    assert(result.before_salary_arg == 1300000);
    assert(result.after_salary_arg == 1300000);
}

static void test_reserve_right_final_gate_salary_arg_is_not_matched(void)
{
    KboForeignAiOfferFinalGateSalaryResult result;
    assert(!kbo_foreign_ai_offer_final_gate_salary_apply(
        700000,
        1150000,
        1,
        1,
        &result));
    assert(!result.eligible);
    assert(result.reserve_right);
    assert(result.after_salary_arg == 700000);
}

static void test_runtime_final_gate_salary_arg_matches_generated_kbo_target(void)
{
    uint8_t player[OOTP27_PLAYER_SCAN_BYTES] = {0};
    uint8_t offer[KBO_OFFER_READABLE_BYTES] = {0};
    player[0] = 1u;
    *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET) = 31057u;
    player[OOTP27_PLAYER_GENERATION_CONTEXT_OFFSET] = 3u;
    *(int32_t*)(player + OOTP27_PLAYER_FA_DEMAND_SALARY_OFFSET) = 1150000;

    int32_t adjusted = kbo_foreign_ai_offer_adjust_final_gate_salary_arg(
        (uintptr_t)player,
        (uintptr_t)offer,
        9,
        0,
        700000,
        "test");
    assert(adjusted == 1150000);
}

static void test_runtime_final_gate_salary_arg_ignores_non_kbo_target(void)
{
    uint8_t player[OOTP27_PLAYER_SCAN_BYTES] = {0};
    uint8_t offer[KBO_OFFER_READABLE_BYTES] = {0};
    player[0] = 1u;
    *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET) = 31058u;
    player[OOTP27_PLAYER_GENERATION_CONTEXT_OFFSET] = 3u;
    *(int32_t*)(player + OOTP27_PLAYER_FA_DEMAND_SALARY_OFFSET) = 1150000;

    int32_t adjusted = kbo_foreign_ai_offer_adjust_final_gate_salary_arg(
        (uintptr_t)player,
        (uintptr_t)offer,
        28,
        0,
        700000,
        "test");
    assert(adjusted == 700000);
}

static void test_generated_foreign_offer_salary_is_not_lowered(void)
{
    uint8_t player[OOTP27_PLAYER_SCAN_BYTES] = {0};
    uint8_t offer[KBO_OFFER_READABLE_BYTES] = {0};
    player[0] = 1u;
    *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET) = 31056u;
    player[OOTP27_PLAYER_GENERATION_CONTEXT_OFFSET] = 3u;
    *(int32_t*)(player + OOTP27_PLAYER_FA_DEMAND_SALARY_OFFSET) = 1000000;
    *(int32_t*)(offer + KBO_OFFER_SALARY_PRIMARY_OFFSET) = 1200000;
    *(int32_t*)(offer + KBO_OFFER_SALARY_FIRST_YEAR_OFFSET) = 1100000;

    assert(!kbo_foreign_ai_offer_match_demand_salary(
        (uintptr_t)player,
        (uintptr_t)offer,
        9,
        0,
        "test"));
    assert(*(int32_t*)(offer + KBO_OFFER_SALARY_PRIMARY_OFFSET) == 1200000);
    assert(*(int32_t*)(offer + KBO_OFFER_SALARY_FIRST_YEAR_OFFSET) == 1100000);
}

static void test_reserve_right_offer_salary_is_not_matched_to_open_market_demand(void)
{
    uint8_t player[OOTP27_PLAYER_SCAN_BYTES] = {0};
    uint8_t offer[KBO_OFFER_READABLE_BYTES] = {0};
    player[0] = 1u;
    *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET) = 5406u;
    player[OOTP27_PLAYER_GENERATION_CONTEXT_OFFSET] = 3u;
    *(int32_t*)(player + OOTP27_PLAYER_FA_DEMAND_SALARY_OFFSET) = 1150000;
    *(int32_t*)(offer + KBO_OFFER_SALARY_PRIMARY_OFFSET) = 900000;
    *(int32_t*)(offer + KBO_OFFER_SALARY_FIRST_YEAR_OFFSET) = 900000;

    assert(!kbo_foreign_ai_offer_match_demand_salary(
        (uintptr_t)player,
        (uintptr_t)offer,
        9,
        1,
        "test"));
    assert(*(int32_t*)(offer + KBO_OFFER_SALARY_PRIMARY_OFFSET) == 900000);
    assert(*(int32_t*)(offer + KBO_OFFER_SALARY_FIRST_YEAR_OFFSET) == 900000);
}

static void test_non_generated_foreign_offer_salary_is_not_matched(void)
{
    uint8_t player[OOTP27_PLAYER_SCAN_BYTES] = {0};
    uint8_t offer[KBO_OFFER_READABLE_BYTES] = {0};
    player[0] = 1u;
    *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET) = 220u;
    player[OOTP27_PLAYER_GENERATION_CONTEXT_OFFSET] = 0u;
    *(int32_t*)(player + OOTP27_PLAYER_FA_DEMAND_SALARY_OFFSET) = 1150000;
    *(int32_t*)(offer + KBO_OFFER_SALARY_PRIMARY_OFFSET) = 900000;
    *(int32_t*)(offer + KBO_OFFER_SALARY_FIRST_YEAR_OFFSET) = 900000;

    assert(!kbo_foreign_ai_offer_match_demand_salary(
        (uintptr_t)player,
        (uintptr_t)offer,
        9,
        0,
        "test"));
    assert(*(int32_t*)(offer + KBO_OFFER_SALARY_PRIMARY_OFFSET) == 900000);
    assert(*(int32_t*)(offer + KBO_OFFER_SALARY_FIRST_YEAR_OFFSET) == 900000);
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
    test_generated_foreign_offer_salary_matches_demand_baseline();
    test_generated_foreign_final_gate_salary_arg_matches_demand_baseline();
    test_generated_foreign_final_gate_salary_arg_is_not_lowered();
    test_reserve_right_final_gate_salary_arg_is_not_matched();
    test_runtime_final_gate_salary_arg_matches_generated_kbo_target();
    test_runtime_final_gate_salary_arg_ignores_non_kbo_target();
    test_generated_foreign_offer_salary_is_not_lowered();
    test_reserve_right_offer_salary_is_not_matched_to_open_market_demand();
    test_non_generated_foreign_offer_salary_is_not_matched();
    test_non_foreign_or_non_kbo_offer_is_not_touched();
    printf("All foreign AI offer contract type tests passed.\n");
    return 0;
}
