#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../src/bootstrap/abi/ootp_offsets.h"
#include "../src/team/independent_acquisition/ai/independent_acquisition_ai_internal.h"

static int g_test_player_foreign = 0;

int memory_range_readable(const void* address, SIZE_T size)
{
    return address != NULL && size <= OOTP27_KBO_TEAM_READABLE_BYTES;
}

int kbo_player_is_foreign_for_kbo_rights(uint8_t* player)
{
    (void)player;
    return g_test_player_foreign;
}

int32_t kbo_get_independent_acquisition_foreign_cash_cost(void)
{
    return 100000;
}

int32_t kbo_get_independent_acquisition_domestic_cash_cost(void)
{
    return 30000;
}

int32_t kbo_get_independent_acquisition_foreign_seller_transfer_fee(void)
{
    return 90000;
}

int32_t kbo_get_independent_acquisition_domestic_seller_transfer_fee(void)
{
    return 25000;
}

void kbo_count_active_foreign_for_asian_quota(
    uintptr_t team_ptr,
    uint32_t* out_asian_hitters,
    uint32_t* out_asian_pitchers,
    uint32_t* out_non_asian_hitters,
    uint32_t* out_non_asian_pitchers)
{
    (void)team_ptr;
    if (out_asian_hitters != NULL) { *out_asian_hitters = 0u; }
    if (out_asian_pitchers != NULL) { *out_asian_pitchers = 0u; }
    if (out_non_asian_hitters != NULL) { *out_non_asian_hitters = 0u; }
    if (out_non_asian_pitchers != NULL) { *out_non_asian_pitchers = 0u; }
}

uint32_t kbo_effective_foreign_count_with_asian_quota(
    uint32_t asian_count,
    uint32_t non_asian_foreign_count)
{
    return asian_count + non_asian_foreign_count;
}

static int32_t* test_team_cash_ptr(uint8_t* team)
{
    return (int32_t*)(
        team
        + KBO_INDEPENDENT_ACQUISITION_TEAM_FINANCIALS_BLOCK_OFFSET
        + KBO_INDEPENDENT_ACQUISITION_TEAM_FINANCIALS_CASH_OFFSET);
}

static void test_seller_transfer_fee_follows_player_class(void)
{
    uint8_t player[OOTP27_PLAYER_SCAN_BYTES];
    memset(player, 0, sizeof(player));

    g_test_player_foreign = 1;
    assert(kbo_independent_acquisition_seller_transfer_fee_for_player(player) == 90000);

    g_test_player_foreign = 0;
    assert(kbo_independent_acquisition_seller_transfer_fee_for_player(player) == 25000);

    printf("test_seller_transfer_fee_follows_player_class: PASS\n");
}

static void test_seller_cash_credit_adds_transfer_fee(void)
{
    uint8_t team[OOTP27_KBO_TEAM_READABLE_BYTES];
    memset(team, 0, sizeof(team));
    *test_team_cash_ptr(team) = 125000;

    int32_t old_cash = 0;
    int32_t new_cash = 0;
    assert(kbo_independent_acquisition_credit_team_cash(team, 25000, &old_cash, &new_cash));
    assert(old_cash == 125000);
    assert(new_cash == 150000);
    assert(*test_team_cash_ptr(team) == 150000);

    printf("test_seller_cash_credit_adds_transfer_fee: PASS\n");
}

static void test_seller_cash_credit_rejects_overflow(void)
{
    uint8_t team[OOTP27_KBO_TEAM_READABLE_BYTES];
    memset(team, 0, sizeof(team));
    *test_team_cash_ptr(team) = KBO_INDEPENDENT_ACQUISITION_FINANCIAL_FIELD_ABS_LIMIT - 10;

    int32_t old_cash = 0;
    int32_t new_cash = 0;
    assert(!kbo_independent_acquisition_credit_team_cash(team, 25, &old_cash, &new_cash));
    assert(old_cash == 0);
    assert(new_cash == 0);
    assert(*test_team_cash_ptr(team) == KBO_INDEPENDENT_ACQUISITION_FINANCIAL_FIELD_ABS_LIMIT - 10);

    printf("test_seller_cash_credit_rejects_overflow: PASS\n");
}

int main(void)
{
    test_seller_transfer_fee_follows_player_class();
    test_seller_cash_credit_adds_transfer_fee();
    test_seller_cash_credit_rejects_overflow();
    printf("All independent acquisition cash flow tests passed.\n");
    return 0;
}
