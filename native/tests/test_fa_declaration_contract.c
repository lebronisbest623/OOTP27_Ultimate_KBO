#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../src/bootstrap/abi/ootp_offsets.h"
#include "../src/fa_declaration/fa_declaration_internal.h"

int memory_range_readable(const void* ptr, size_t size)
{
    return ptr != NULL && size > 0u;
}

int16_t kbo_read_player_i16(uint8_t* player, uint32_t offset)
{
    if (player == NULL || offset + sizeof(int16_t) > OOTP27_PLAYER_SCAN_BYTES) {
        return 0;
    }
    return *(int16_t*)(player + offset);
}

int32_t kbo_foreign_waiver_value_score(uint8_t* player)
{
    (void)player;
    return 0;
}

void kbo_copy_player_display_name(uint8_t* player, char* out, size_t out_size)
{
    (void)player;
    if (out != NULL && out_size > 0u) {
        snprintf(out, out_size, "Test Player");
    }
}

int kbo_fa_rules_load(KboFaRules* rules)
{
    if (rules != NULL) {
        memset(rules, 0, sizeof(*rules));
    }
    return 0;
}

const KboFaSalarySnapshotGrade* kbo_find_fa_salary_snapshot_grade(
    const KboFaSalarySnapshotGrade* rows,
    int row_count,
    uint32_t player_id)
{
    (void)rows;
    (void)row_count;
    (void)player_id;
    return NULL;
}

uint32_t kbo_fa_filing_team_league_id(uint32_t team_id)
{
    (void)team_id;
    return 0u;
}

static int32_t* player_salary_slot(uint8_t* player, uint32_t slot)
{
    return (int32_t*)(player + OOTP27_PLAYER_CONTRACT_SALARY_Y1_OFFSET + (slot * sizeof(int32_t)));
}

static void test_future_extension_is_not_current_season_salary(void)
{
    uint8_t player[OOTP27_PLAYER_SCAN_BYTES];
    memset(player, 0, sizeof(player));

    *(int32_t*)(player + OOTP27_PLAYER_CONTRACT_START_YEAR_OFFSET) = 2027;
    *player_salary_slot(player, 0u) = 800000000;

    int32_t next_salary = 0;
    int32_t salary = kbo_fa_declaration_contract_salary_for_season(player, 2026u, &next_salary);

    assert(salary == 0);
    assert(next_salary == 800000000);

    printf("test_future_extension_is_not_current_season_salary: PASS\n");
}

static void test_current_expiring_contract_returns_current_salary(void)
{
    uint8_t player[OOTP27_PLAYER_SCAN_BYTES];
    memset(player, 0, sizeof(player));

    *(int32_t*)(player + OOTP27_PLAYER_CONTRACT_START_YEAR_OFFSET) = 2026;
    *player_salary_slot(player, 0u) = 600000000;
    *player_salary_slot(player, 1u) = 0;

    int32_t next_salary = -1;
    int32_t salary = kbo_fa_declaration_contract_salary_for_season(player, 2026u, &next_salary);

    assert(salary == 600000000);
    assert(next_salary == 0);

    printf("test_current_expiring_contract_returns_current_salary: PASS\n");
}

static void test_current_multiyear_contract_reports_next_salary(void)
{
    uint8_t player[OOTP27_PLAYER_SCAN_BYTES];
    memset(player, 0, sizeof(player));

    *(int32_t*)(player + OOTP27_PLAYER_CONTRACT_START_YEAR_OFFSET) = 2026;
    *player_salary_slot(player, 0u) = 600000000;
    *player_salary_slot(player, 1u) = 650000000;

    int32_t next_salary = 0;
    int32_t salary = kbo_fa_declaration_contract_salary_for_season(player, 2026u, &next_salary);

    assert(salary == 600000000);
    assert(next_salary == 650000000);

    printf("test_current_multiyear_contract_reports_next_salary: PASS\n");
}

static void test_unknown_start_year_preserves_first_salary_fallback(void)
{
    uint8_t player[OOTP27_PLAYER_SCAN_BYTES];
    memset(player, 0, sizeof(player));

    *(int32_t*)(player + OOTP27_PLAYER_CONTRACT_START_YEAR_OFFSET) = 0;
    *player_salary_slot(player, 0u) = 500000000;

    int32_t next_salary = 0;
    int32_t salary = kbo_fa_declaration_contract_salary_for_season(player, 2026u, &next_salary);

    assert(salary == 500000000);
    assert(next_salary == 0);

    printf("test_unknown_start_year_preserves_first_salary_fallback: PASS\n");
}

int main(void)
{
    test_future_extension_is_not_current_season_salary();
    test_current_expiring_contract_returns_current_salary();
    test_current_multiyear_contract_reports_next_salary();
    test_unknown_start_year_preserves_first_salary_fallback();
    printf("All FA declaration contract tests passed.\n");
    return 0;
}
