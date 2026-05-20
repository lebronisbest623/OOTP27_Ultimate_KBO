#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../independent_acquisition_ui.h"
#include "../../ai/independent_acquisition_ai_internal.h"
#include "parts/assignments/independent_acquisition_ui_offer_assignments.h"
#include "parts/cache/independent_acquisition_ui_offer_cache.h"
#include "parts/rows/independent_acquisition_ui_offer_rows.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../../../bootstrap/abi/ootp_offsets.h"
#include "../../../../core/core_flags/api/flags_api.h"
#include "../../../../foreign/common/player_eval/foreign_waiver_player_eval.h"
#include "../../../../foreign/common/policy/foreign_player_policy.h"
#include "../../../../foreign/common/policy/foreign_waiver_policy.h"
#include "../../../../runtime_memory/runtime_memory.h"
#include "../../../lookup/team_lookup.h"

int kbo_independent_acquisition_ui_collect_offer_rows(
    uint32_t buyer_team_id,
    KboIndependentAcquisitionUiOfferRow* out_rows,
    int max_rows,
    KboIndependentAcquisitionUiContext* out_context)
{
    if (out_rows == NULL || max_rows <= 0) {
        return 0;
    }
    memset(out_rows, 0, sizeof(out_rows[0]) * (size_t)max_rows);

    KboIndependentAcquisitionUiContext context;
    if (!kbo_independent_acquisition_ui_context(buyer_team_id, &context)) {
        if (out_context != NULL) { *out_context = context; }
        return 0;
    }
    if (out_context != NULL) {
        *out_context = context;
    }
    if (!context.policy_enabled || !context.buyer_valid || context.seller_count <= 0) {
        return 0;
    }

    int32_t foreign_cash_cost = kbo_get_independent_acquisition_foreign_cash_cost();
    int32_t domestic_cash_cost = kbo_get_independent_acquisition_domestic_cash_cost();
    int32_t seller_transfer_limit =
        kbo_foreign_player_policy()->independent_acquisition_seller_transfer_limit;
    int cached_count = 0;
    if (kbo_independent_acquisition_ui_offer_cache_try_copy(
            buyer_team_id,
            &context,
            foreign_cash_cost,
            domestic_cash_cost,
            seller_transfer_limit,
            out_rows,
            max_rows,
            out_context,
            &cached_count)) {
        return cached_count;
    }

    KboIndependentFuturesTeamLeague sellers[KBO_INDEPENDENT_ACQUISITION_MAX_SELLERS];
    int seller_count = kbo_collect_independent_futures_team_leagues(
        sellers,
        KBO_INDEPENDENT_ACQUISITION_MAX_SELLERS,
        NULL,
        NULL);
    if (seller_count <= 0) {
        return 0;
    }

    KboIndependentAcquisitionUiTeamOrgCache team_org_cache;
    memset(&team_org_cache, 0, sizeof(team_org_cache));
    KboIndependentAcquisitionUiSellerMatch seller_matches[KBO_INDEPENDENT_ACQUISITION_MAX_SELLERS];
    memset(seller_matches, 0, sizeof(seller_matches));

    int available_seller_count = 0;
    for (int i = 0; i < seller_count; i++) {
        int transfers = kbo_independent_acquisition_transferred_count(context.season, sellers[i].team_id);
        seller_matches[available_seller_count].seller = sellers[i];
        seller_matches[available_seller_count].match =
            kbo_independent_acquisition_ui_team_match(sellers[i].team_id, &team_org_cache);
        seller_matches[available_seller_count].transfer_blocked = transfers >= seller_transfer_limit;
        available_seller_count++;
    }
    seller_count = available_seller_count;
    if (seller_count <= 0) {
        return 0;
    }

    uint8_t* buyer_team = find_kbo_team_by_numeric_id_any_league(buyer_team_id, 1);
    if (buyer_team == NULL || !memory_range_readable(buyer_team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return 0;
    }
    KboIndependentAcquisitionBuyerState buyer;
    kbo_independent_acquisition_read_buyer_state(buyer_team, &buyer);
    if (buyer.team_id == 0u) {
        return 0;
    }

    KboIndependentAcquisitionUiTeamMatch buyer_match =
        kbo_independent_acquisition_ui_team_match(buyer.team_id, &team_org_cache);
    KboIndependentAcquisitionQueuedRequest requests[KBO_INDEPENDENT_ACQUISITION_MAX_QUEUE];
    memset(requests, 0, sizeof(requests));
    int request_count = kbo_independent_acquisition_load_requests(
        context.season,
        requests,
        KBO_INDEPENDENT_ACQUISITION_MAX_QUEUE);

    KboIndependentAcquisitionDecisionKey decision_keys[
        KBO_INDEPENDENT_ACQUISITION_UI_DECISION_KEY_MAX];
    memset(decision_keys, 0, sizeof(decision_keys));
    int decision_key_count = kbo_independent_acquisition_load_decision_keys(
        context.season,
        decision_keys,
        KBO_INDEPENDENT_ACQUISITION_UI_DECISION_KEY_MAX);
    int decision_lookup_fallback = decision_key_count < 0
        || decision_key_count >= KBO_INDEPENDENT_ACQUISITION_UI_DECISION_KEY_MAX;
    if (decision_key_count < 0) {
        decision_key_count = 0;
    }

    uintptr_t player_vector = 0u;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)
            || player_vector == 0u
            || player_count <= 0
            || player_count > 200000
            || !memory_range_readable((void*)player_vector, (SIZE_T)player_count * sizeof(uintptr_t))) {
        return 0;
    }

    int count = 0;
    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }
        uint8_t* player = (uint8_t*)player_ptr;
        if (!kbo_independent_acquisition_player_status_ok(player)) {
            continue;
        }

        KboIndependentAcquisitionUiPlayerAssignmentSnapshot assignment;
        if (!kbo_independent_acquisition_ui_build_player_assignment_snapshot(
                player,
                &team_org_cache,
                &assignment)
                || kbo_independent_acquisition_ui_assignment_snapshot_matches_team(
                    &assignment,
                    &buyer_match)) {
            continue;
        }

        const KboIndependentAcquisitionUiSellerMatch* seller =
            kbo_independent_acquisition_ui_seller_for_player(
                &assignment,
                seller_matches,
                seller_count);
        if (seller == NULL) {
            continue;
        }

        uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        if (player_id == 0u) {
            continue;
        }
        int32_t cash_cost = kbo_independent_acquisition_cash_cost_for_player(player);
        if (cash_cost <= 0) {
            continue;
        }
        int no_cash = buyer.cash_available < cash_cost;
        int seller_transfer_blocked = seller->transfer_blocked;

        uint32_t effective_before = buyer.effective_foreign_count;
        uint32_t effective_after = buyer.effective_foreign_count;
        uint32_t effective_limit = KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT;
        uint8_t slot_type = 0u;
        uint32_t injured_player_id = 0u;
        int foreign_player = kbo_player_is_foreign_for_kbo_rights(player);
        int asian_quota = foreign_player && kbo_player_is_asian_quota_slot_candidate(player);
        int policy_blocked = 0;
        if (foreign_player) {
            int allowed = kbo_custom_foreign_policy_team_allows_candidate(
                    buyer.team_id,
                    player,
                    &effective_before,
                    &effective_after,
                    &effective_limit,
                    &slot_type,
                    &injured_player_id);
            policy_blocked = !allowed;
        }

        KboIndependentAcquisitionUiOfferRow row;
        memset(&row, 0, sizeof(row));
        row.player_ptr = player_ptr;
        row.player_id = player_id;
        row.seller_team_id = seller->seller.team_id;
        row.seller_league_id = seller->seller.league_id;
        row.nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
        row.effective_before = effective_before;
        row.effective_after = effective_after;
        row.effective_limit = effective_limit;
        row.injured_player_id = injured_player_id;
        row.age = memory_range_readable(player + OOTP27_PLAYER_AGE_OFFSET, sizeof(uint16_t))
            ? *(uint16_t*)(player + OOTP27_PLAYER_AGE_OFFSET)
            : 0u;
        row.pitcher = *(uint8_t*)(player + OOTP27_PLAYER_POSITION_GROUP_OFFSET) == 1u ? 1u : 0u;
        row.foreign_player = foreign_player ? 1u : 0u;
        row.asian_quota = asian_quota ? 1u : 0u;
        row.slot_type = slot_type;
        row.offer_blocked = (no_cash || policy_blocked || seller_transfer_blocked) ? 1u : 0u;
        row.already_requested = kbo_independent_acquisition_ui_request_row_exists(
            requests,
            request_count,
            buyer.team_id,
            seller->seller.team_id,
            player_id) ? 1u : 0u;
        row.already_decided = kbo_independent_acquisition_ui_decision_key_exists(
            decision_keys,
            decision_key_count,
            decision_lookup_fallback,
            context.season,
            seller->seller.team_id,
            player_id) ? 1u : 0u;
        row.value_score = kbo_foreign_waiver_value_score(player);
        row.cash_cost = cash_cost;
        row.request_score = kbo_independent_acquisition_score_candidate_for_buyer(
            &buyer,
            player,
            effective_before,
            effective_limit);
        kbo_independent_acquisition_ui_slot_label(
            row.slot_type,
            row.foreign_player,
            row.asian_quota,
            row.slot_label,
            sizeof(row.slot_label));
        if (seller_transfer_blocked) {
            snprintf(row.status_label, sizeof(row.status_label), "한도 도달");
        } else if (no_cash) {
            snprintf(row.status_label, sizeof(row.status_label), "자금 부족");
        } else if (policy_blocked) {
            snprintf(row.status_label, sizeof(row.status_label), "영입 불가");
        } else {
            snprintf(row.status_label, sizeof(row.status_label), "가능");
        }
        kbo_independent_acquisition_ui_insert_offer_row(out_rows, &count, max_rows, &row);
    }

    if (count > 1) {
        qsort(out_rows, (size_t)count, sizeof(out_rows[0]), kbo_independent_acquisition_ui_offer_row_cmp);
    }
    kbo_independent_acquisition_ui_offer_cache_store(
        buyer_team_id,
        &context,
        foreign_cash_cost,
        domestic_cash_cost,
        seller_transfer_limit,
        out_rows,
        count,
        max_rows);
    return count;
}
