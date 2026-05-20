#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "independent_acquisition_ai_internal.h"
#include "independent_acquisition_score.h"

#include <stdint.h>
#include <string.h>

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../bootstrap/profiling/profiler.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../foreign/common/player_eval/foreign_waiver_player_eval.h"
#include "../../../foreign/common/policy/foreign_waiver_policy.h"
#include "../../assignment/org_query/team_org_assignment_query.h"
#include "../../lookup/team_lookup.h"
#include "../../../runtime_memory/runtime_memory.h"

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
    int asian = foreign && kbo_player_is_asian_quota_slot_candidate(player);

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

static const KboIndependentFuturesTeamLeague*
kbo_independent_acquisition_seller_for_player(
    uint8_t* player,
    const KboIndependentFuturesTeamLeague* sellers,
    int seller_count)
{
    if (player == NULL || sellers == NULL || seller_count <= 0) {
        return NULL;
    }

    for (int s = 0; s < seller_count; s++) {
        if (sellers[s].team_id != 0u
                && kbo_player_current_assignment_matches_team_or_affiliate(
                    player,
                    sellers[s].team_id)) {
            return &sellers[s];
        }
    }
    return NULL;
}

int kbo_independent_acquisition_build_candidate_pool(
    const uintptr_t* player_snapshot,
    int32_t player_count,
    const KboIndependentFuturesTeamLeague* sellers,
    int seller_count,
    KboIndependentAcquisitionCandidatePool* out_pool)
{
    if (out_pool != NULL) {
        memset(out_pool, 0, sizeof(*out_pool));
    }
    if (player_snapshot == NULL
            || player_count <= 0
            || sellers == NULL
            || seller_count <= 0
            || out_pool == NULL) {
        return 0;
    }

    KBO_PROFILE_BEGIN(profile_independent_candidate_pool_build);

    SIZE_T bytes = (SIZE_T)player_count * sizeof(KboIndependentAcquisitionCandidatePoolEntry);
    KboIndependentAcquisitionCandidatePoolEntry* entries =
        (KboIndependentAcquisitionCandidatePoolEntry*)HeapAlloc(
            GetProcessHeap(),
            HEAP_ZERO_MEMORY,
            bytes);
    if (entries == NULL) {
        KBO_PROFILE_END(profile_independent_candidate_pool_build, "independent_acquisition.candidate_pool.alloc_failed");
        return 0;
    }

    int count = 0;
    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t player_ptr = player_snapshot[i];
        if (!kbo_player_pointer_plausible(player_ptr)
                || !memory_range_readable((void*)player_ptr, OOTP27_PLAYER_SCAN_BYTES)) {
            continue;
        }
        uint8_t* player = (uint8_t*)player_ptr;
        if (!kbo_independent_acquisition_player_status_ok(player)) {
            continue;
        }

        const KboIndependentFuturesTeamLeague* seller =
            kbo_independent_acquisition_seller_for_player(player, sellers, seller_count);
        if (seller == NULL) {
            continue;
        }

        uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        if (player_id == 0u) {
            continue;
        }

        int foreign = kbo_player_is_foreign_for_kbo_rights(player);
        KboIndependentAcquisitionCandidatePoolEntry* entry = &entries[count++];
        entry->player_ptr = player_ptr;
        entry->player_id = player_id;
        entry->seller_team_id = seller->team_id;
        entry->seller_league_id = seller->league_id;
        entry->nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
        entry->pitcher = *(uint8_t*)(player + OOTP27_PLAYER_POSITION_GROUP_OFFSET) == 1u ? 1u : 0u;
        entry->foreign = foreign ? 1u : 0u;
        entry->asian_quota = foreign && kbo_player_is_asian_quota_slot_candidate(player) ? 1u : 0u;
        entry->cash_cost = foreign
            ? kbo_get_independent_acquisition_foreign_cash_cost()
            : kbo_get_independent_acquisition_domestic_cash_cost();
        entry->value_score = kbo_foreign_waiver_value_score(player);
    }

    if (count <= 0) {
        HeapFree(GetProcessHeap(), 0, entries);
        KBO_PROFILE_END(profile_independent_candidate_pool_build, "independent_acquisition.candidate_pool.empty");
        return 0;
    }

    out_pool->entries = entries;
    out_pool->count = count;
    KBO_PROFILE_END(profile_independent_candidate_pool_build, "independent_acquisition.candidate_pool.build");
    return count;
}

void kbo_independent_acquisition_free_candidate_pool(
    KboIndependentAcquisitionCandidatePool* pool)
{
    if (pool == NULL) {
        return;
    }
    if (pool->entries != NULL) {
        HeapFree(GetProcessHeap(), 0, pool->entries);
    }
    memset(pool, 0, sizeof(*pool));
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

int kbo_independent_acquisition_choose_candidate_from_pool(
    const KboIndependentAcquisitionCandidatePool* pool,
    const KboIndependentAcquisitionQueuedRequest* market_requests,
    int market_request_count,
    const KboIndependentAcquisitionBuyerState* buyer,
    KboIndependentAcquisitionCandidate* out_candidate)
{
    if (out_candidate != NULL) {
        memset(out_candidate, 0, sizeof(*out_candidate));
        out_candidate->request_score = INT64_MIN;
    }
    if (pool == NULL
            || pool->entries == NULL
            || pool->count <= 0
            || buyer == NULL
            || buyer->team_id == 0u
            || out_candidate == NULL) {
        return 0;
    }

    KBO_PROFILE_BEGIN(profile_independent_candidate_select);

    KboIndependentAcquisitionCandidate best = {0};
    best.request_score = INT64_MIN;
    for (int i = 0; i < pool->count; i++) {
        const KboIndependentAcquisitionCandidatePoolEntry* entry = &pool->entries[i];
        uintptr_t player_ptr = entry->player_ptr;
        if (!kbo_player_pointer_plausible(player_ptr)
                || !memory_range_readable((void*)player_ptr, OOTP27_PLAYER_SCAN_BYTES)) {
            continue;
        }
        uint8_t* player = (uint8_t*)player_ptr;
        if (kbo_player_current_assignment_matches_team_or_affiliate(player, buyer->team_id)) {
            continue;
        }

        uint32_t player_id = entry->player_id;
        if (player_id == 0u) {
            continue;
        }
        int buyer_already_requested = 0;
        if (market_requests != NULL && market_request_count > 0) {
            for (int r = 0; r < market_request_count; r++) {
                if (market_requests[r].buyer_team_id == buyer->team_id
                        && market_requests[r].player_id == player_id
                        && market_requests[r].seller_team_id == entry->seller_team_id) {
                    buyer_already_requested = 1;
                    break;
                }
            }
        }
        if (buyer_already_requested) {
            continue;
        }
        int32_t cash_cost = entry->cash_cost;
        if (cash_cost <= 0 || buyer->cash_available < cash_cost) {
            continue;
        }

        uint32_t effective_before = 0u;
        uint32_t effective_after = 0u;
        uint32_t effective_limit = KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT;
        uint8_t slot_type = 0u;
        uint32_t injured_player_id = 0u;
        if (entry->foreign) {
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
                        && market_requests[r].seller_team_id == entry->seller_team_id) {
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
        best.seller_team_id = entry->seller_team_id;
        best.seller_league_id = entry->seller_league_id;
        best.nation_id = entry->nation_id;
        best.pitcher = entry->pitcher;
        best.asian_quota = entry->asian_quota;
        best.value_score = entry->value_score;
        best.request_score = request_score;
        best.effective_before = effective_before;
        best.effective_after = effective_after;
        best.effective_limit = effective_limit;
        best.slot_type = slot_type;
        best.injured_player_id = injured_player_id;
    }

    if (best.player_id == 0u || best.request_score == INT64_MIN) {
        KBO_PROFILE_END(profile_independent_candidate_select, "independent_acquisition.candidate_pool.no_match");
        return 0;
    }
    *out_candidate = best;
    KBO_PROFILE_END(profile_independent_candidate_select, "independent_acquisition.candidate_pool.select");
    return 1;
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
    KboIndependentAcquisitionCandidatePool pool;
    memset(&pool, 0, sizeof(pool));
    int built = kbo_independent_acquisition_build_candidate_pool(
        player_snapshot,
        player_count,
        sellers,
        seller_count,
        &pool);
    if (built <= 0) {
        if (out_candidate != NULL) {
            memset(out_candidate, 0, sizeof(*out_candidate));
            out_candidate->request_score = INT64_MIN;
        }
        return 0;
    }
    int result = kbo_independent_acquisition_choose_candidate_from_pool(
        &pool,
        market_requests,
        market_request_count,
        buyer,
        out_candidate);
    kbo_independent_acquisition_free_candidate_pool(&pool);
    return result;
}
