#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "independent_acquisition_ai_internal.h"
#include "independent_acquisition_score.h"

#include <stdint.h>
#include <string.h>

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../foreign/common/player_eval/foreign_waiver_player_eval.h"
#include "../../../foreign/common/policy/foreign_waiver_policy.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../assignment/org_query/team_org_assignment_query.h"
#include "../../lookup/team_lookup.h"

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

int kbo_independent_acquisition_player_status_ok(uint8_t* player)
{
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }
    return *(uint8_t*)(player + OOTP27_PLAYER_RETIRED_FLAG_OFFSET) == 0u
        && *(uint8_t*)(player + OOTP27_PLAYER_DFA_FLAG_OFFSET) == 0u
        && *(uint8_t*)(player + OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET) == 0u
        && *(uint8_t*)(player + OOTP27_PLAYER_INJURY_ACTIVE_OFFSET) == 0u;
}

static int64_t kbo_independent_acquisition_score_candidate(
    const KboIndependentAcquisitionBuyerState* buyer,
    uint8_t* player,
    uint32_t effective_before,
    uint32_t effective_limit,
    int market_interest_count)
{
    if (buyer == NULL || player == NULL) {
        return INT64_MIN;
    }

    int64_t score = (int64_t)kbo_foreign_waiver_value_score(player);
    int pitcher = *(uint8_t*)(player + OOTP27_PLAYER_POSITION_GROUP_OFFSET) == 1u;
    int foreign = kbo_player_is_foreign_for_kbo_rights(player);
    int asian = foreign && kbo_player_is_asian_quota_candidate(player);

    if (buyer->active_count < 28u) {
        score += (int64_t)(28u - buyer->active_count) * 4000;
    }
    if (effective_before < effective_limit) {
        score += (int64_t)(effective_limit - effective_before) * 6000;
    }
    if (!foreign) {
        score += 3000;
    } else if (asian) {
        if (buyer->asian_hitters + buyer->asian_pitchers == 0u) {
            score += 9000;
        }
        if (pitcher && buyer->asian_pitchers == 0u) {
            score += 4000;
        } else if (!pitcher && buyer->asian_hitters == 0u) {
            score += 4000;
        }
    } else if (pitcher) {
        if (buyer->non_asian_pitchers == 0u) {
            score += 9000;
        } else if (buyer->non_asian_pitchers >= 2u) {
            score -= 5000;
        }
    } else {
        if (buyer->non_asian_hitters == 0u) {
            score += 9000;
        } else if (buyer->non_asian_hitters >= 2u) {
            score -= 5000;
        }
    }

    score += (int64_t)kbo_read_player_i16(player, OOTP27_PLAYER_OVERALL_VALUE_OFFSET) * 120;
    score += (int64_t)kbo_read_player_i16(player, OOTP27_PLAYER_RATINGS_VALUE_OFFSET) * 80;

    score += kbo_independent_acquisition_market_interest_adjustment(market_interest_count);

    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    score += kbo_independent_acquisition_team_need_mix_adjustment(
        buyer->team_id,
        pitcher,
        foreign,
        asian);

    uint32_t scouting_mix = buyer->team_id * 1103515245u
        ^ player_id * 2654435761u
        ^ (pitcher ? 0x9e3779b9u : 0x7f4a7c15u)
        ^ (asian ? 0x85ebca6bu : 0xc2b2ae35u);
    scouting_mix ^= scouting_mix >> 16;
    scouting_mix *= 2246822519u;
    scouting_mix ^= scouting_mix >> 13;
    score += (int64_t)(scouting_mix % 7000u);
    return score;
}

int64_t kbo_independent_acquisition_score_candidate_for_buyer(
    const KboIndependentAcquisitionBuyerState* buyer,
    uint8_t* player,
    uint32_t effective_before,
    uint32_t effective_limit)
{
    return kbo_independent_acquisition_score_candidate(
        buyer,
        player,
        effective_before,
        effective_limit,
        0);
}

uintptr_t kbo_independent_acquisition_find_player_snapshot(
    const uintptr_t* player_snapshot,
    int32_t player_count,
    uint32_t player_id)
{
    if (player_snapshot == NULL || player_count <= 0 || player_id == 0u) {
        return 0u;
    }
    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t player_ptr = player_snapshot[i];
        if (!kbo_player_pointer_plausible(player_ptr)
                || !memory_range_readable((void*)player_ptr, OOTP27_PLAYER_SCAN_BYTES)) {
            continue;
        }
        uint8_t* player = (uint8_t*)player_ptr;
        if (*(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET) == player_id) {
            return player_ptr;
        }
    }
    return 0u;
}

int kbo_independent_acquisition_choose_candidate_for_buyer(
    const uintptr_t* player_snapshot,
    int32_t player_count,
    const KboIndependentFuturesTeamLeague* sellers,
    int seller_count,
    const KboIndependentAcquisitionQueuedRequest* market_requests,
    int market_request_count,
    const KboIndependentAcquisitionBuyerState* buyer,
    KboIndependentAcquisitionCandidate* out_candidate)
{
    if (out_candidate != NULL) {
        memset(out_candidate, 0, sizeof(*out_candidate));
        out_candidate->request_score = INT64_MIN;
    }
    if (player_snapshot == NULL
            || player_count <= 0
            || sellers == NULL
            || seller_count <= 0
            || buyer == NULL
            || buyer->team_id == 0u
            || out_candidate == NULL) {
        return 0;
    }

    KboIndependentAcquisitionCandidate best = {0};
    best.request_score = INT64_MIN;
    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t player_ptr = player_snapshot[i];
        if (!kbo_player_pointer_plausible(player_ptr)
                || !memory_range_readable((void*)player_ptr, OOTP27_PLAYER_SCAN_BYTES)) {
            continue;
        }
        uint8_t* player = (uint8_t*)player_ptr;
        if (!kbo_independent_acquisition_player_status_ok(player)
                || kbo_player_current_assignment_matches_team_or_affiliate(player, buyer->team_id)) {
            continue;
        }

        const KboIndependentFuturesTeamLeague* seller = NULL;
        for (int s = 0; s < seller_count; s++) {
            if (sellers[s].team_id != 0u
                    && kbo_player_current_assignment_matches_team_or_affiliate(player, sellers[s].team_id)) {
                seller = &sellers[s];
                break;
            }
        }
        if (seller == NULL) {
            continue;
        }

        uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        if (player_id == 0u) {
            continue;
        }
        int buyer_already_requested = 0;
        if (market_requests != NULL && market_request_count > 0) {
            for (int r = 0; r < market_request_count; r++) {
                if (market_requests[r].buyer_team_id == buyer->team_id
                        && market_requests[r].player_id == player_id
                        && market_requests[r].seller_team_id == seller->team_id) {
                    buyer_already_requested = 1;
                    break;
                }
            }
        }
        if (buyer_already_requested) {
            continue;
        }
        int32_t cash_cost = kbo_independent_acquisition_cash_cost_for_player(player);
        if (cash_cost <= 0 || buyer->cash_available < cash_cost) {
            continue;
        }

        uint32_t effective_before = 0u;
        uint32_t effective_after = 0u;
        uint32_t effective_limit = KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT;
        uint8_t slot_type = 0u;
        uint32_t injured_player_id = 0u;
        if (kbo_player_is_foreign_for_kbo_rights(player)) {
            int allowed = kbo_custom_foreign_policy_team_allows_candidate(
                buyer->team_id,
                player,
                &effective_before,
                &effective_after,
                &effective_limit,
                &slot_type,
                &injured_player_id);
            if (!allowed) {
                continue;
            }
        }

        int market_interest_count = 0;
        if (market_requests != NULL && market_request_count > 0) {
            for (int r = 0; r < market_request_count; r++) {
                if (market_requests[r].player_id == player_id
                        && market_requests[r].seller_team_id == seller->team_id) {
                    market_interest_count++;
                }
            }
        }

        int64_t request_score = kbo_independent_acquisition_score_candidate(
            buyer,
            player,
            effective_before,
            effective_limit,
            market_interest_count);
        if (request_score <= best.request_score) {
            continue;
        }

        best.player_ptr = player_ptr;
        best.player_id = player_id;
        best.seller_team_id = seller->team_id;
        best.seller_league_id = seller->league_id;
        best.nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
        best.pitcher = *(uint8_t*)(player + OOTP27_PLAYER_POSITION_GROUP_OFFSET) == 1u ? 1u : 0u;
        best.asian_quota = kbo_player_is_asian_quota_candidate(player) ? 1u : 0u;
        best.value_score = kbo_foreign_waiver_value_score(player);
        best.request_score = request_score;
        best.effective_before = effective_before;
        best.effective_after = effective_after;
        best.effective_limit = effective_limit;
        best.slot_type = slot_type;
        best.injured_player_id = injured_player_id;
    }

    if (best.player_id == 0u || best.request_score == INT64_MIN) {
        return 0;
    }
    *out_candidate = best;
    return 1;
}
