#include "../independent_acquisition_seller_ai_internal.h"
#include "../helpers/fit/independent_acquisition_seller_ai_helpers.h"

#include "../../../../../bootstrap/profiling/profiler.h"
#include "../../../../../core/dates/core_text_date.h"
#include "../../../../lookup/team_lookup.h"

#include <stdint.h>

KboIndependentAcquisitionSellerAiSelection kbo_independent_acquisition_seller_select_best_request(
    uint32_t today,
    KboIndependentAcquisitionQueuedRequest* queue,
    int start_index,
    int request_count,
    const KboIndependentAcquisitionQueuedRequest* group,
    const uintptr_t* player_snapshot,
    int32_t player_count,
    KboIndependentAcquisitionTransferSummary* buyer_summaries,
    int buyer_summary_count,
    const char* source)
{
    KboIndependentAcquisitionSellerAiSelection result = {0};
    if (queue == NULL || group == NULL) {
        return result;
    }

    KBO_PROFILE_BEGIN(profile_independent_seller_ai_find_player);
    uintptr_t player_ptr = kbo_independent_acquisition_find_player_snapshot(
        player_snapshot,
        player_count,
        group->player_id);
    KBO_PROFILE_END(
            profile_independent_seller_ai_find_player,
            player_ptr != 0u
                ? "independent_acquisition.seller_ai.find_player.hit"
                : "independent_acquisition.seller_ai.find_player.miss");
    uint8_t* player = (uint8_t*)player_ptr;
    KboIndependentAcquisitionQueuedRequest* best = group;
    int64_t best_fit_score = INT64_MIN;
    int64_t best_request_score = INT64_MIN;
    int64_t second_best_request_score = INT64_MIN;
    int best_buyer_transfers = 0;
    uint32_t best_tiebreaker = 0u;
    int market_offer_count = 0;
    KBO_PROFILE_BEGIN(profile_independent_seller_ai_buyer_fit_loop);
    for (int j = start_index; j < request_count; j++) {
            if ((j & 7) == 0
                    && kbo_independent_acquisition_seller_abort_if_save(source, "buyer_fit_loop", today)) {
                result.abort_for_save = 1;
                break;
            }
            if (queue[j].player_id != group->player_id
                    || queue[j].seller_team_id != group->seller_team_id) {
                continue;
            }
            market_offer_count++;
            if (queue[j].request_score > best_request_score) {
                second_best_request_score = best_request_score;
                best_request_score = queue[j].request_score;
            } else if (queue[j].request_score > second_best_request_score) {
                second_best_request_score = queue[j].request_score;
            }
            KBO_PROFILE_BEGIN(profile_independent_seller_ai_find_buyer_team);
            uint8_t* candidate_team = find_kbo_team_by_numeric_id_any_league(queue[j].buyer_team_id, 1);
        KBO_PROFILE_END(
                profile_independent_seller_ai_find_buyer_team,
                candidate_team != NULL
                    ? "independent_acquisition.seller_ai.find_buyer_team.hit"
                    : "independent_acquisition.seller_ai.find_buyer_team.miss");
            int32_t candidate_cash_cost = queue[j].cash_cost;
            if (candidate_cash_cost <= 0 && player != NULL) {
                candidate_cash_cost = kbo_independent_acquisition_cash_cost_for_player(player);
                queue[j].cash_cost = candidate_cash_cost;
            }
            KBO_PROFILE_BEGIN(profile_independent_seller_ai_fit_score);
            int64_t fit_score = kbo_independent_acquisition_seller_fit_score(
                &queue[j],
                player,
                candidate_team,
                candidate_cash_cost);
        KBO_PROFILE_END(
                profile_independent_seller_ai_fit_score,
                fit_score == INT64_MIN
                    ? "independent_acquisition.seller_ai.fit_score.rejected"
                    : "independent_acquisition.seller_ai.fit_score.scored");
            KBO_PROFILE_BEGIN(profile_independent_seller_ai_buyer_transfer_count);
            int buyer_transfers = kbo_independent_acquisition_buyer_transfer_count_for_ai(
                queue[j].season,
                queue[j].buyer_team_id,
                buyer_summaries,
                buyer_summary_count);
        KBO_PROFILE_END(
                profile_independent_seller_ai_buyer_transfer_count,
                "independent_acquisition.seller_ai.buyer_transferred_count");
            int64_t adjusted_fit_score = fit_score == INT64_MIN
                ? INT64_MIN
                : fit_score - ((int64_t)buyer_transfers * 1000000ll);
            if (adjusted_fit_score != INT64_MIN && today >= queue[j].date) {
                uint32_t today_serial = kbo_date_serial(
                    today / 10000u,
                    (today / 100u) % 100u,
                    today % 100u);
                uint32_t request_serial = kbo_date_serial(
                    queue[j].date / 10000u,
                    (queue[j].date / 100u) % 100u,
                    queue[j].date % 100u);
                if (today_serial != 0u && request_serial != 0u && today_serial >= request_serial) {
                    uint32_t request_age_days = today_serial - request_serial;
                    if (request_age_days > 30u) {
                        request_age_days = 30u;
                    }
                    adjusted_fit_score += (int64_t)request_age_days * 2500ll;
                }
            }
            uint32_t tiebreaker = kbo_independent_acquisition_seller_tiebreaker(today, &queue[j]);
            int best_penalty = best_buyer_transfers;
            if (adjusted_fit_score > best_fit_score
                    || (adjusted_fit_score == best_fit_score && buyer_transfers < best_penalty)
                    || (adjusted_fit_score == best_fit_score
                        && buyer_transfers == best_penalty
                        && queue[j].request_score > best->request_score)
                    || (adjusted_fit_score == best_fit_score
                        && buyer_transfers == best_penalty
                        && queue[j].request_score == best->request_score
                        && tiebreaker > best_tiebreaker)) {
                best = &queue[j];
                best_fit_score = adjusted_fit_score;
                best_buyer_transfers = buyer_transfers;
                best_tiebreaker = tiebreaker;
            }
        }
    KBO_PROFILE_END(
            profile_independent_seller_ai_buyer_fit_loop,
            result.abort_for_save
                ? "independent_acquisition.seller_ai.buyer_fit_loop.aborted"
                : "independent_acquisition.seller_ai.buyer_fit_loop.total");
    if (result.abort_for_save) {
        return result;
    }
    result.selected = *best;
    result.player = player;
    result.best_request_score = best_request_score;
    result.best_fit_score = best_fit_score;
    result.second_best_request_score = second_best_request_score;
    result.best_buyer_transfers = best_buyer_transfers;
    result.best_tiebreaker = best_tiebreaker;
    result.market_offer_count = market_offer_count;
    return result;
}
