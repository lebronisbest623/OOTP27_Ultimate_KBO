#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../independent_acquisition_ai_internal.h"

#include <stdint.h>
#include <string.h>

#include "../../../../bootstrap/abi/ootp_offsets.h"
#include "../../../../core/core_flags/api/flags_api.h"
#include "../../../../foreign/common/player_eval/foreign_waiver_player_eval.h"
#include "../../../../foreign/common/policy/foreign_waiver_policy.h"
#include "../../../../runtime_memory/runtime_memory.h"

static uint32_t kbo_independent_acquisition_active_count(uint8_t* team)
{
    if (team == NULL || !memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return 0u;
    }

    uint32_t count = 0u;
    uint32_t* active_ids = (uint32_t*)(team + OOTP27_TEAM_PLAYER_IDS_2A80_OFFSET);
    for (uint32_t i = 0u; i < OOTP27_TEAM_PLAYER_ID_ARRAY_COUNT; i++) {
        if (active_ids[i] != 0u) {
            count++;
        }
    }
    return count;
}

static int kbo_independent_acquisition_abs_i32_plausible(int32_t value)
{
    return value > -KBO_INDEPENDENT_ACQUISITION_FINANCIAL_FIELD_ABS_LIMIT
        && value < KBO_INDEPENDENT_ACQUISITION_FINANCIAL_FIELD_ABS_LIMIT;
}

int32_t* kbo_independent_acquisition_team_cash_ptr(uint8_t* team)
{
    if (team == NULL
            || !memory_range_readable(
                team + KBO_INDEPENDENT_ACQUISITION_TEAM_FINANCIALS_BLOCK_OFFSET,
                KBO_INDEPENDENT_ACQUISITION_TEAM_FINANCIALS_READABLE_BYTES)) {
        return NULL;
    }

    int32_t* cash = (int32_t*)(
        team
        + KBO_INDEPENDENT_ACQUISITION_TEAM_FINANCIALS_BLOCK_OFFSET
        + KBO_INDEPENDENT_ACQUISITION_TEAM_FINANCIALS_CASH_OFFSET);
    if (!kbo_independent_acquisition_abs_i32_plausible(*cash)) {
        return NULL;
    }
    return cash;
}

int32_t kbo_independent_acquisition_cash_cost_for_player(uint8_t* player)
{
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }
    return kbo_player_is_foreign_for_kbo_rights(player)
        ? kbo_get_independent_acquisition_foreign_cash_cost()
        : kbo_get_independent_acquisition_domestic_cash_cost();
}

int kbo_independent_acquisition_team_has_cash(uint8_t* team, int32_t cash_cost)
{
    if (cash_cost <= 0) {
        return 0;
    }
    int32_t* cash = kbo_independent_acquisition_team_cash_ptr(team);
    return cash != NULL && *cash >= cash_cost;
}

int kbo_independent_acquisition_charge_team_cash(
    uint8_t* team,
    int32_t cash_cost,
    int32_t* out_old_cash,
    int32_t* out_new_cash)
{
    if (out_old_cash != NULL) { *out_old_cash = 0; }
    if (out_new_cash != NULL) { *out_new_cash = 0; }
    if (cash_cost <= 0) {
        return 0;
    }

    int32_t* cash = kbo_independent_acquisition_team_cash_ptr(team);
    if (cash == NULL || *cash < cash_cost) {
        return 0;
    }

    int32_t old_cash = *cash;
    int32_t new_cash = old_cash - cash_cost;
    *cash = new_cash;
    if (out_old_cash != NULL) { *out_old_cash = old_cash; }
    if (out_new_cash != NULL) { *out_new_cash = new_cash; }
    return 1;
}

void kbo_independent_acquisition_read_buyer_state(
    uint8_t* team,
    KboIndependentAcquisitionBuyerState* out)
{
    if (out == NULL) {
        return;
    }
    memset(out, 0, sizeof(*out));
    if (team == NULL || !memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return;
    }

    out->team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET);
    out->league_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
    out->active_count = kbo_independent_acquisition_active_count(team);
    kbo_count_active_foreign_for_asian_quota(
        (uintptr_t)team,
        &out->asian_hitters,
        &out->asian_pitchers,
        &out->non_asian_hitters,
        &out->non_asian_pitchers);
    out->effective_foreign_count = kbo_effective_foreign_count_with_asian_quota(
        out->asian_hitters + out->asian_pitchers,
        out->non_asian_hitters + out->non_asian_pitchers);
    int32_t* cash = kbo_independent_acquisition_team_cash_ptr(team);
    out->cash_available = cash != NULL ? *cash : 0;
}
