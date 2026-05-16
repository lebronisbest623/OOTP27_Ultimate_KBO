#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../independent_acquisition_ai_internal.h"
#include "independent_acquisition_seller_ai_helpers.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#include "../../../../bootstrap/abi/ootp_offsets.h"
#include "../../../../core/core_flags/api/flags_api.h"
#include "../../../../core/dates/core_text_date.h"
#include "../../../../core/logging/core_log.h"
#include "../../../../foreign/common/player_eval/foreign_waiver_player_eval.h"
#include "../../../../foreign/common/policy/foreign_player_policy.h"
#include "../../../../foreign/common/policy/foreign_waiver_policy.h"
#include "../../../../runtime_memory/runtime_memory.h"
#include "../../../assignment/assignment/team_assignment.h"
#include "../../../assignment/org_query/team_org_assignment_query.h"
#include "../../../lookup/team_lookup.h"

int kbo_run_independent_team_acquisition_seller_ai(
    uint32_t today,
    const uintptr_t* player_snapshot,
    int32_t player_count,
    const char* source)
{
    static volatile LONG seller_ai_running = 0;
    if (InterlockedCompareExchange(&seller_ai_running, 1, 0) != 0) {
        kbo_log_runtimef(
            "independent acquisition seller AI skipped source=%s today=%u reason=already_running",
            source != NULL ? source : "",
            today);
        return 0;
    }

    if (kbo_independent_acquisition_seller_abort_if_save(source, "after_lock", today)) {
        InterlockedExchange(&seller_ai_running, 0);
        return 0;
    }

    KboIndependentAcquisitionQueuedRequest queue[KBO_INDEPENDENT_ACQUISITION_MAX_QUEUE];
    uint32_t season = kbo_independent_acquisition_seller_effective_season(today);
    int request_count = kbo_independent_acquisition_load_requests(
        season,
        queue,
        KBO_INDEPENDENT_ACQUISITION_MAX_QUEUE);
    if (request_count <= 0) {
        InterlockedExchange(&seller_ai_running, 0);
        return 0;
    }

    int32_t seller_transfer_limit =
        kbo_foreign_player_policy()->independent_acquisition_seller_transfer_limit;
    int decided = 0;
    int transferred = 0;
    int limit_blocked = 0;
    int pacing_deferred = 0;
    int daily_cap_deferred = 0;
    int cooldown_deferred = 0;
    int strategy_deferred = 0;
    int abort_for_save = 0;
    uint32_t daily_seller_ids[KBO_INDEPENDENT_ACQUISITION_MAX_SELLERS] = {0};
    int daily_seller_transfer_counts[KBO_INDEPENDENT_ACQUISITION_MAX_SELLERS] = {0};
    int daily_seller_count = 0;
    for (int i = 0; i < request_count; i++) {
        if (kbo_independent_acquisition_seller_abort_if_save(source, "request_loop", today)) {
            abort_for_save = 1;
            break;
        }
        KboIndependentAcquisitionQueuedRequest* group = &queue[i];
        if (group->player_id == 0u) {
            continue;
        }
        int seller_transfers = kbo_independent_acquisition_transferred_count(
            group->season,
            group->seller_team_id);
        int seller_limit_reached = seller_transfers >= seller_transfer_limit;
        if (seller_limit_reached) {
            limit_blocked++;
        }
        uint32_t last_transfer_date = kbo_independent_acquisition_last_transfer_date(
            group->season,
            group->seller_team_id);
        uint32_t days_since_transfer = 0u;
        int cooldown_blocked = !seller_limit_reached
            && kbo_independent_acquisition_seller_cooldown_deferred(
                today,
                last_transfer_date,
                &days_since_transfer);
        if (cooldown_blocked) {
            cooldown_deferred++;
            kbo_log_runtimef(
                "independent acquisition seller AI deferred source=%s reason=seller_cooldown seller=%u player=%u buyer=%u last_transfer_date=%u days_since_transfer=%u cooldown_days=%d seller_transfers=%d seller_transfer_limit=%d",
                source != NULL ? source : "",
                group->seller_team_id,
                group->player_id,
                group->buyer_team_id,
                last_transfer_date,
                days_since_transfer,
                KBO_INDEPENDENT_ACQUISITION_SELLER_TRANSFER_COOLDOWN_DAYS,
                seller_transfers,
                seller_transfer_limit);
            for (int j = i + 1; j < request_count; j++) {
                if (queue[j].player_id == group->player_id
                        && queue[j].seller_team_id == group->seller_team_id) {
                    queue[j].player_id = 0u;
                }
            }
            continue;
        }
        int daily_seller_index = -1;
        for (int s = 0; s < daily_seller_count; s++) {
            if (daily_seller_ids[s] == group->seller_team_id) {
                daily_seller_index = s;
                break;
            }
        }
        if (daily_seller_index < 0 && daily_seller_count < KBO_INDEPENDENT_ACQUISITION_MAX_SELLERS) {
            daily_seller_index = daily_seller_count;
            daily_seller_ids[daily_seller_count] = group->seller_team_id;
            daily_seller_transfer_counts[daily_seller_count] = 0;
            daily_seller_count++;
        }
        int daily_cap_reached = daily_seller_index >= 0
            && daily_seller_transfer_counts[daily_seller_index]
                >= KBO_INDEPENDENT_ACQUISITION_SELLER_DAILY_TRANSFER_LIMIT;
        if (!seller_limit_reached && daily_cap_reached) {
            daily_cap_deferred++;
            kbo_log_runtimef(
                "independent acquisition seller AI deferred source=%s reason=daily_transfer_cap seller=%u player=%u buyer=%u transferred_today=%d daily_limit=%d seller_transfers=%d seller_transfer_limit=%d",
                source != NULL ? source : "",
                group->seller_team_id,
                group->player_id,
                group->buyer_team_id,
                daily_seller_transfer_counts[daily_seller_index],
                KBO_INDEPENDENT_ACQUISITION_SELLER_DAILY_TRANSFER_LIMIT,
                seller_transfers,
                seller_transfer_limit);
            for (int j = i + 1; j < request_count; j++) {
                if (queue[j].player_id == group->player_id
                        && queue[j].seller_team_id == group->seller_team_id) {
                    queue[j].player_id = 0u;
                }
            }
            continue;
        }
        uint32_t window_age_days = 0u;
        uint32_t target_day = 0u;
        uint32_t request_age_days = 0u;
        uint32_t days_remaining = 0u;
        int pacing_blocked = !seller_limit_reached
            && kbo_independent_acquisition_seller_pacing_deferred(
                today,
                group,
                seller_transfers,
                seller_transfer_limit,
                &window_age_days,
                &target_day,
                &request_age_days,
                &days_remaining);
        if (pacing_blocked) {
            pacing_deferred++;
            kbo_log_runtimef(
                "independent acquisition seller AI deferred source=%s reason=market_pacing seller=%u player=%u buyer=%u seller_transfers=%d seller_transfer_limit=%d window_age_days=%u target_day=%u request_age_days=%u days_remaining=%u",
                source != NULL ? source : "",
                group->seller_team_id,
                group->player_id,
                group->buyer_team_id,
                seller_transfers,
                seller_transfer_limit,
                window_age_days,
                target_day,
                request_age_days,
                days_remaining);
            for (int j = i + 1; j < request_count; j++) {
                if (queue[j].player_id == group->player_id
                        && queue[j].seller_team_id == group->seller_team_id) {
                    queue[j].player_id = 0u;
                }
            }
            continue;
        }
        uintptr_t player_ptr = kbo_independent_acquisition_find_player_snapshot(
            player_snapshot,
            player_count,
            group->player_id);
        uint8_t* player = (uint8_t*)player_ptr;
        KboIndependentAcquisitionQueuedRequest* best = group;
        int64_t best_fit_score = INT64_MIN;
        int64_t best_request_score = INT64_MIN;
        int64_t second_best_request_score = INT64_MIN;
        int best_buyer_transfers = 0;
        uint32_t best_tiebreaker = 0u;
        int market_offer_count = 0;
        for (int j = i; j < request_count; j++) {
            if ((j & 7) == 0
                    && kbo_independent_acquisition_seller_abort_if_save(source, "buyer_fit_loop", today)) {
                abort_for_save = 1;
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
            uint8_t* candidate_team = find_kbo_team_by_numeric_id_any_league(queue[j].buyer_team_id, 1);
            int32_t candidate_cash_cost = queue[j].cash_cost;
            if (candidate_cash_cost <= 0 && player != NULL) {
                candidate_cash_cost = kbo_independent_acquisition_cash_cost_for_player(player);
                queue[j].cash_cost = candidate_cash_cost;
            }
            int64_t fit_score = kbo_independent_acquisition_seller_fit_score(
                &queue[j],
                player,
                candidate_team,
                candidate_cash_cost);
            int buyer_transfers = kbo_independent_acquisition_buyer_transferred_count(
                queue[j].season,
                queue[j].buyer_team_id);
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
        if (abort_for_save) {
            break;
        }
        KboIndependentAcquisitionQueuedRequest selected = *best;
        int64_t reservation_score = 0ll;
        int64_t hold_value = 0ll;
        uint32_t strategic_window_age_days = 0u;
        uint32_t strategic_request_age_days = 0u;
        uint32_t strategic_days_remaining = 0u;
        int strategy_blocked = !seller_limit_reached
            && kbo_independent_acquisition_seller_strategy_deferred(
                today,
                &selected,
                seller_transfers,
                seller_transfer_limit,
                market_offer_count,
                selected.request_score,
                second_best_request_score,
                selected.value_score,
                &reservation_score,
                &hold_value,
                &strategic_window_age_days,
                &strategic_request_age_days,
                &strategic_days_remaining);
        if (strategy_blocked) {
            strategy_deferred++;
            kbo_log_runtimef(
                "independent acquisition seller AI deferred source=%s reason=strategic_hold seller=%u player=%u buyer=%u score=%lld reservation=%lld hold_value=%lld second_best=%lld value=%d market_offers=%d seller_transfers=%d seller_transfer_limit=%d window_age_days=%u request_age_days=%u days_remaining=%u",
                source != NULL ? source : "",
                selected.seller_team_id,
                selected.player_id,
                selected.buyer_team_id,
                (long long)selected.request_score,
                (long long)reservation_score,
                (long long)hold_value,
                (long long)second_best_request_score,
                selected.value_score,
                market_offer_count,
                seller_transfers,
                seller_transfer_limit,
                strategic_window_age_days,
                strategic_request_age_days,
                strategic_days_remaining);
            for (int j = i + 1; j < request_count; j++) {
                if (queue[j].player_id == group->player_id
                        && queue[j].seller_team_id == group->seller_team_id) {
                    queue[j].player_id = 0u;
                }
            }
            continue;
        }
        for (int j = i + 1; j < request_count; j++) {
            if (queue[j].player_id == group->player_id
                    && queue[j].seller_team_id == group->seller_team_id) {
                queue[j].player_id = 0u;
            }
        }

        uint8_t* buyer_team = find_kbo_team_by_numeric_id_any_league(selected.buyer_team_id, 1);
        uint8_t* seller_team = find_kbo_team_by_numeric_id_any_league(selected.seller_team_id, 1);
        int moved = 0;
        int cash_charged = 0;
        int32_t old_cash = 0;
        int32_t new_cash = 0;
        int32_t cash_cost = selected.cash_cost;
        if (cash_cost <= 0 && player != NULL) {
            cash_cost = kbo_independent_acquisition_cash_cost_for_player(player);
            selected.cash_cost = cash_cost;
        }
        if (kbo_independent_acquisition_seller_abort_if_save(source, "before_assignment", today)) {
            abort_for_save = 1;
            break;
        }
        if (!seller_limit_reached
                && !pacing_blocked
                && player != NULL
                && buyer_team != NULL
                && memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)
                && memory_range_readable(buyer_team, OOTP27_KBO_TEAM_READABLE_BYTES)
                && kbo_independent_acquisition_player_status_ok(player)
                && kbo_player_current_assignment_matches_team_or_affiliate(player, selected.seller_team_id)
                && !kbo_player_current_assignment_matches_team_or_affiliate(player, selected.buyer_team_id)
                && kbo_independent_acquisition_team_has_cash(buyer_team, cash_cost)) {
            int pre = 0;
            int reg = 0;
            int attach = 0;
            uint32_t buyer_league_id = *(uint32_t*)(buyer_team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
            kbo_assign_player_to_team_like_ootp(player, buyer_team, buyer_league_id, &pre, &reg, &attach);
            moved = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET) == selected.buyer_team_id;
            if (moved) {
                cash_charged = kbo_independent_acquisition_charge_team_cash(
                        buyer_team,
                        cash_cost,
                        &old_cash,
                        &new_cash);
                if (!cash_charged) {
                    kbo_log_runtimef(
                        "independent acquisition seller AI cash charge failed source=%s buyer=%u player=%u cost=%d",
                        source != NULL ? source : "",
                        selected.buyer_team_id,
                        selected.player_id,
                        cash_cost);
                }
            }
        }
        if (moved && cash_charged) {
            kbo_emit_independent_acquisition_transfer_news(
                today,
                player,
                buyer_team,
                seller_team,
                selected.player_id,
                selected.buyer_team_id,
                selected.seller_team_id,
                cash_cost,
                source);
        }

        if (kbo_independent_acquisition_seller_abort_if_save(source, "before_append_decision", today)) {
            abort_for_save = 1;
            break;
        }
        if (kbo_independent_acquisition_append_decision(today, &selected, moved, old_cash, new_cash, source)) {
            decided++;
            if (moved) {
                transferred++;
                if (daily_seller_index >= 0) {
                    daily_seller_transfer_counts[daily_seller_index]++;
                }
            }
            char request_score_text[32] = {0};
            snprintf(request_score_text, sizeof(request_score_text), "%" PRId64, (int64_t)best->request_score);
            kbo_log_runtimef(
                "independent acquisition seller AI decision source=%s seller=%u player=%u buyer=%u score=%s adjusted_fit=%lld reservation=%lld hold_value=%lld second_best=%lld market_offers=%d buyer_transfers=%d tiebreaker=%u cash_cost=%d old_cash=%d new_cash=%d transferred=%d seller_transfers=%d seller_transfer_limit=%d",
                source != NULL ? source : "",
                selected.seller_team_id,
                selected.player_id,
                selected.buyer_team_id,
                request_score_text,
                (long long)best_fit_score,
                (long long)reservation_score,
                (long long)hold_value,
                (long long)second_best_request_score,
                market_offer_count,
                best_buyer_transfers,
                best_tiebreaker,
                cash_cost,
                old_cash,
                new_cash,
                moved,
                seller_transfers,
                seller_transfer_limit);
        }
    }

    kbo_log_runtimef(
        "independent acquisition seller AI summary source=%s today=%u queued=%d decided=%d transferred=%d limit_blocked=%d pacing_deferred=%d strategy_deferred=%d cooldown_deferred=%d daily_cap_deferred=%d seller_transfer_limit=%d seller_daily_transfer_limit=%d seller_cooldown_days=%d aborted_for_save=%d",
        source != NULL ? source : "",
        today,
        request_count,
        decided,
        transferred,
        limit_blocked,
        pacing_deferred,
        strategy_deferred,
        cooldown_deferred,
        daily_cap_deferred,
        seller_transfer_limit,
        KBO_INDEPENDENT_ACQUISITION_SELLER_DAILY_TRANSFER_LIMIT,
        KBO_INDEPENDENT_ACQUISITION_SELLER_TRANSFER_COOLDOWN_DAYS,
        abort_for_save);
    InterlockedExchange(&seller_ai_running, 0);
    return transferred;
}
