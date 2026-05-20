#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../independent_acquisition_ui.h"
#include "../../ai/independent_acquisition_ai_internal.h"

#include <stdint.h>
#include <string.h>

#include "../../../../bootstrap/abi/ootp_offsets.h"
#include "../../../../core/logging/core_log.h"
#include "../../../../foreign/common/player_eval/foreign_waiver_player_eval.h"
#include "../../../../foreign/common/policy/foreign_player_policy.h"
#include "../../../../foreign/common/policy/foreign_waiver_policy.h"
#include "../../../../runtime_memory/runtime_memory.h"
#include "../../../assignment/org_query/team_org_assignment_query.h"
#include "../../../lookup/team_lookup.h"

int kbo_independent_acquisition_ui_submit_offer(
    uint32_t buyer_team_id,
    uint32_t seller_team_id,
    uint32_t player_id,
    const char* source)
{
    KboIndependentAcquisitionUiContext context;
    if (!kbo_independent_acquisition_ui_context(buyer_team_id, &context)
            || !context.window_open
            || !context.buyer_valid
            || seller_team_id == 0u
            || player_id == 0u) {
        return KBO_INDEPENDENT_ACQUISITION_UI_SUBMIT_CLOSED;
    }

    KboIndependentFuturesTeamLeague sellers[KBO_INDEPENDENT_ACQUISITION_MAX_SELLERS];
    int seller_count = kbo_collect_independent_futures_team_leagues(
        sellers,
        KBO_INDEPENDENT_ACQUISITION_MAX_SELLERS,
        NULL,
        NULL);
    const KboIndependentFuturesTeamLeague* seller = NULL;
    for (int i = 0; i < seller_count; i++) {
        if (sellers[i].team_id == seller_team_id) {
            seller = &sellers[i];
            break;
        }
    }
    if (seller == NULL) {
        return KBO_INDEPENDENT_ACQUISITION_UI_SUBMIT_INVALID;
    }

    if (kbo_independent_acquisition_decision_exists(context.season, seller_team_id, player_id)) {
        return KBO_INDEPENDENT_ACQUISITION_UI_SUBMIT_DECIDED;
    }
    if (kbo_independent_acquisition_transferred_count(context.season, seller_team_id)
            >= kbo_foreign_player_policy()->independent_acquisition_seller_transfer_limit) {
        return KBO_INDEPENDENT_ACQUISITION_UI_SUBMIT_BLOCKED;
    }
    if (kbo_independent_acquisition_request_exists(
            context.season,
            buyer_team_id,
            seller_team_id,
            player_id)) {
        uint32_t current_team_id = 0u;
        uint32_t current_league_id = 0u;
        uint8_t* existing_player = kbo_find_player_by_id(player_id, &current_team_id, &current_league_id);
        (void)current_team_id;
        (void)current_league_id;
        if (existing_player != NULL) {
            kbo_record_custom_foreign_pending_offer(buyer_team_id, existing_player, context.today);
        }
        kbo_independent_acquisition_ui_invalidate_offer_cache();
        return KBO_INDEPENDENT_ACQUISITION_UI_SUBMIT_DUPLICATE;
    }

    uint8_t* buyer_team = find_kbo_team_by_numeric_id_any_league(buyer_team_id, 1);
    if (buyer_team == NULL || !memory_range_readable(buyer_team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return KBO_INDEPENDENT_ACQUISITION_UI_SUBMIT_INVALID;
    }

    KboIndependentAcquisitionBuyerState buyer;
    kbo_independent_acquisition_read_buyer_state(buyer_team, &buyer);
    if (buyer.team_id == 0u) {
        return KBO_INDEPENDENT_ACQUISITION_UI_SUBMIT_INVALID;
    }

    uint32_t current_team_id = 0u;
    uint32_t current_league_id = 0u;
    uint8_t* player = kbo_find_player_by_id(player_id, &current_team_id, &current_league_id);
    (void)current_team_id;
    (void)current_league_id;
    if (player == NULL
            || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)
            || !kbo_independent_acquisition_player_status_ok(player)
            || !kbo_player_current_assignment_matches_team_or_affiliate(player, seller_team_id)
            || kbo_player_current_assignment_matches_team_or_affiliate(player, buyer_team_id)) {
        return KBO_INDEPENDENT_ACQUISITION_UI_SUBMIT_INVALID;
    }

    int32_t cash_cost = kbo_independent_acquisition_cash_cost_for_player(player);
    if (cash_cost <= 0 || buyer.cash_available < cash_cost) {
        return KBO_INDEPENDENT_ACQUISITION_UI_SUBMIT_NO_CASH;
    }

    KboIndependentAcquisitionCandidate candidate;
    memset(&candidate, 0, sizeof(candidate));
    candidate.player_ptr = (uintptr_t)player;
    candidate.player_id = player_id;
    candidate.seller_team_id = seller_team_id;
    candidate.seller_league_id = seller->league_id;
    candidate.nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
    candidate.pitcher = *(uint8_t*)(player + OOTP27_PLAYER_POSITION_GROUP_OFFSET) == 1u ? 1u : 0u;
    candidate.asian_quota = kbo_player_is_asian_quota_slot_candidate(player) ? 1u : 0u;
    candidate.value_score = kbo_foreign_waiver_value_score(player);
    candidate.effective_before = buyer.effective_foreign_count;
    candidate.effective_after = buyer.effective_foreign_count;
    candidate.effective_limit = KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT;
    if (kbo_player_is_foreign_for_kbo_rights(player)) {
        if (!kbo_custom_foreign_policy_team_allows_candidate(
                buyer.team_id,
                player,
                &candidate.effective_before,
                &candidate.effective_after,
                &candidate.effective_limit,
                &candidate.slot_type,
                &candidate.injured_player_id)) {
            return KBO_INDEPENDENT_ACQUISITION_UI_SUBMIT_BLOCKED;
        }
    }
    candidate.request_score = kbo_independent_acquisition_score_candidate_for_buyer(
        &buyer,
        player,
        candidate.effective_before,
        candidate.effective_limit);

    if (!kbo_independent_acquisition_append_request(
            context.today,
            &candidate,
            &buyer,
            seller,
            source != NULL ? source : "hub_independent_offer")) {
        return KBO_INDEPENDENT_ACQUISITION_UI_SUBMIT_FAILED;
    }
    kbo_independent_acquisition_ui_invalidate_offer_cache();
    kbo_record_custom_foreign_pending_offer(buyer.team_id, player, context.today);
    kbo_log_runtimef(
        "independent acquisition UI offer source=%s buyer=%u seller=%u player=%u score=%lld cash_cost=%d",
        source != NULL ? source : "",
        buyer.team_id,
        seller_team_id,
        player_id,
        (long long)candidate.request_score,
        cash_cost);
    return KBO_INDEPENDENT_ACQUISITION_UI_SUBMIT_OK;
}

int kbo_independent_acquisition_ui_cancel_offer(
    uint32_t buyer_team_id,
    uint32_t seller_team_id,
    uint32_t player_id,
    const char* source)
{
    KboIndependentAcquisitionUiContext context;
    if (!kbo_independent_acquisition_ui_context(buyer_team_id, &context)
            || !context.buyer_valid
            || seller_team_id == 0u
            || player_id == 0u) {
        return KBO_INDEPENDENT_ACQUISITION_UI_CANCEL_INVALID;
    }

    if (kbo_independent_acquisition_decision_exists(context.season, seller_team_id, player_id)) {
        return KBO_INDEPENDENT_ACQUISITION_UI_CANCEL_DECIDED;
    }

    int removed = kbo_independent_acquisition_cancel_request(
        context.season,
        buyer_team_id,
        seller_team_id,
        player_id,
        source != NULL ? source : "hub_independent_offer_cancel");
    if (removed <= 0) {
        return KBO_INDEPENDENT_ACQUISITION_UI_CANCEL_NOT_FOUND;
    }

    kbo_cancel_custom_foreign_pending_offer(buyer_team_id, player_id);
    kbo_independent_acquisition_ui_invalidate_offer_cache();
    kbo_log_runtimef(
        "independent acquisition UI offer cancel source=%s buyer=%u seller=%u player=%u removed=%d",
        source != NULL ? source : "",
        buyer_team_id,
        seller_team_id,
        player_id,
        removed);
    return KBO_INDEPENDENT_ACQUISITION_UI_CANCEL_OK;
}
