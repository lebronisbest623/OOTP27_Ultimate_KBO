#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../src/bootstrap/abi/ootp_offsets.h"
#include "../src/core/csv/core_csv.h"
#include "../src/fa_declaration/fa_declaration.h"

int kbo_fix_enabled(void)
{
    return 1;
}

int memory_range_readable(const void* ptr, size_t size)
{
    return ptr != NULL && size > 0u;
}

void kbo_log_runtimef_at(const char* file, int line, const char* fmt, ...)
{
    (void)file;
    (void)line;
    (void)fmt;
}

int kbo_current_date_tick_latest_published_date(uint32_t* out_date)
{
    if (out_date != NULL) {
        *out_date = 0u;
    }
    return 0;
}

int kbo_get_fa_declaration_csv_path(char* out, size_t out_size)
{
    if (out != NULL && out_size > 0u) {
        out[0] = '\0';
    }
    return 0;
}

uint32_t kbo_fa_declaration_retained_contract_season(uint32_t declaration_season)
{
    return declaration_season + 1u;
}

KboCsvReader* kbo_csv_reader_open(const char* path)
{
    (void)path;
    return NULL;
}

void kbo_csv_reader_close(KboCsvReader* reader)
{
    (void)reader;
}

int kbo_csv_reader_next_row(KboCsvReader* reader)
{
    (void)reader;
    return 0;
}

int kbo_csv_reader_read_trimmed_fields(KboCsvReader* reader, char* fields, size_t field_size, int max_fields)
{
    (void)reader;
    (void)fields;
    (void)field_size;
    (void)max_fields;
    return 0;
}

uint8_t* kbo_find_player_by_id(uint32_t player_id, uint32_t* out_current_team_id, uint32_t* out_current_league_id)
{
    (void)player_id;
    if (out_current_team_id != NULL) {
        *out_current_team_id = 0u;
    }
    if (out_current_league_id != NULL) {
        *out_current_league_id = 0u;
    }
    return NULL;
}

uint8_t* find_kbo_team_by_numeric_id_any_league(uint32_t team_id, int allow_deleted)
{
    (void)team_id;
    (void)allow_deleted;
    return NULL;
}

void kbo_assign_player_to_team_like_ootp(
    uint8_t* player,
    uint8_t* team,
    uint32_t fallback_league_id,
    int* out_called_pre_change,
    int* out_called_register,
    int* out_called_attach)
{
    (void)player;
    (void)team;
    (void)fallback_league_id;
    if (out_called_pre_change != NULL) {
        *out_called_pre_change = 0;
    }
    if (out_called_register != NULL) {
        *out_called_register = 0;
    }
    if (out_called_attach != NULL) {
        *out_called_attach = 0;
    }
}

static int32_t* player_i32(uint8_t* player, uint32_t offset)
{
    return (int32_t*)(player + offset);
}

static uint32_t* player_u32(uint8_t* player, uint32_t offset)
{
    return (uint32_t*)(player + offset);
}

static void test_retained_fa_repair_normalizes_next_season_contract(void)
{
    uint8_t player[OOTP27_PLAYER_SCAN_BYTES];
    memset(player, 0, sizeof(player));

    *player_u32(player, OOTP27_PLAYER_ID_OFFSET) = 518u;
    *player_u32(player, OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET) = 7u;
    *player_i32(player, OOTP27_PLAYER_CONTRACT_START_YEAR_OFFSET) = 2026;
    *player_i32(player, OOTP27_PLAYER_CONTRACT_SALARY_Y1_OFFSET) = 600000;
    *player_i32(player, OOTP27_PLAYER_CONTRACT_SALARY_Y1_OFFSET + sizeof(int32_t)) = 0;
    player[OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET] = 1u;
    player[OOTP27_PLAYER_CONTRACT_TOTAL_YEARS_OFFSET] = 1u;
    player[OOTP27_PLAYER_CONTRACT_CURRENT_YEAR_OFFSET] = 0u;

    KboFaDeclarationDecision decision;
    memset(&decision, 0, sizeof(decision));
    decision.player_id = 518u;
    decision.declaration_date = 20261018u;
    decision.season = 2026u;
    decision.declared = 0u;
    decision.team_id = 7u;
    decision.league_id = 100u;
    decision.contract_level = 1u;
    decision.salary = 600000;

    int changed = kbo_fa_declaration_repair_retained_contract_salary(
        player,
        2027u,
        &decision,
        0,
        "test");

    assert(changed == 1);
    assert(*player_i32(player, OOTP27_PLAYER_CONTRACT_START_YEAR_OFFSET) == 2027);
    assert(*player_i32(player, OOTP27_PLAYER_CONTRACT_SALARY_Y1_OFFSET) == 600000);
    assert(*player_i32(player, OOTP27_PLAYER_CONTRACT_SALARY_Y1_OFFSET + sizeof(int32_t)) == 0);
    assert(player[OOTP27_PLAYER_CONTRACT_TOTAL_YEARS_OFFSET] == 1u);
    assert(player[OOTP27_PLAYER_CONTRACT_CURRENT_YEAR_OFFSET] == 0u);
    assert(*player_i32(player, OOTP27_PLAYER_ARBITRATION_OFFER_OFFSET) == 600000);
    assert(*player_i32(player, OOTP27_PLAYER_ARBITRATION_REQUEST_OFFSET) == 600000);
    assert(player[OOTP27_PLAYER_ARBITRATION_STATUS_OFFSET] == 1u);
    assert(player[OOTP27_PLAYER_ARBITRATION_TENDER_FLAG_OFFSET] == 1u);

    printf("test_retained_fa_repair_normalizes_next_season_contract: PASS\n");
}

static void test_retained_fa_repair_clears_stale_future_salary_slots(void)
{
    uint8_t player[OOTP27_PLAYER_SCAN_BYTES];
    memset(player, 0, sizeof(player));

    *player_u32(player, OOTP27_PLAYER_ID_OFFSET) = 922u;
    *player_i32(player, OOTP27_PLAYER_CONTRACT_START_YEAR_OFFSET) = 2026;
    *player_i32(player, OOTP27_PLAYER_CONTRACT_SALARY_Y1_OFFSET) = 600000;
    *player_i32(player, OOTP27_PLAYER_CONTRACT_SALARY_Y1_OFFSET + sizeof(int32_t)) = 600000;
    player[OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET] = 1u;
    player[OOTP27_PLAYER_CONTRACT_TOTAL_YEARS_OFFSET] = 1u;

    KboFaDeclarationDecision decision;
    memset(&decision, 0, sizeof(decision));
    decision.player_id = 922u;
    decision.season = 2026u;
    decision.salary = 600000;

    int changed = kbo_fa_declaration_repair_retained_contract_salary(
        player,
        2027u,
        &decision,
        0,
        "test");

    assert(changed == 1);
    assert(*player_i32(player, OOTP27_PLAYER_CONTRACT_START_YEAR_OFFSET) == 2027);
    assert(*player_i32(player, OOTP27_PLAYER_CONTRACT_SALARY_Y1_OFFSET) == 600000);
    assert(*player_i32(player, OOTP27_PLAYER_CONTRACT_SALARY_Y1_OFFSET + sizeof(int32_t)) == 0);

    printf("test_retained_fa_repair_clears_stale_future_salary_slots: PASS\n");
}

static void test_retained_fa_repair_does_not_overwrite_y1_when_season_slot_invalid(void)
{
    uint8_t player[OOTP27_PLAYER_SCAN_BYTES];
    memset(player, 0, sizeof(player));

    *player_u32(player, OOTP27_PLAYER_ID_OFFSET) = 398u;
    *player_i32(player, OOTP27_PLAYER_CONTRACT_START_YEAR_OFFSET) = 2026;
    *player_i32(player, OOTP27_PLAYER_CONTRACT_SALARY_Y1_OFFSET) = 400000;

    KboFaDeclarationDecision decision;
    memset(&decision, 0, sizeof(decision));
    decision.player_id = 398u;
    decision.season = 2026u;
    decision.salary = 700000;

    int changed = kbo_fa_declaration_repair_retained_contract_salary(
        player,
        2037u,
        &decision,
        0,
        "test");

    assert(changed == 1);
    assert(*player_i32(player, OOTP27_PLAYER_CONTRACT_SALARY_Y1_OFFSET) == 400000);
    assert(*player_i32(player, OOTP27_PLAYER_ARBITRATION_OFFER_OFFSET) == 700000);
    assert(*player_i32(player, OOTP27_PLAYER_ARBITRATION_REQUEST_OFFSET) == 700000);

    printf("test_retained_fa_repair_does_not_overwrite_y1_when_season_slot_invalid: PASS\n");
}

int main(void)
{
    test_retained_fa_repair_normalizes_next_season_contract();
    test_retained_fa_repair_clears_stale_future_salary_slots();
    test_retained_fa_repair_does_not_overwrite_y1_when_season_slot_invalid();
    printf("All FA declaration repair tests passed.\n");
    return 0;
}
