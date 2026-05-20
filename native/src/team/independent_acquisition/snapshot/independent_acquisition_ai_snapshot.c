#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../independent_acquisition_ai_module.h"
#include "../ai/lifecycle/independent_acquisition_ai_lifecycle.h"
#include "../window/independent_acquisition_window.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../../core/logging/core_log.h"
#include "../../../core/teams/core_team_collect.h"
#include "../../../foreign/common/policy/foreign_waiver_policy.h"
#include "../../../foreign/injury/api/foreign_injury_labels.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../control/team_human_control.h"
#include "../../lookup/team_lookup.h"

int kbo_run_independent_team_acquisition_ai_with_snapshot_for_date(
    uint32_t today,
    const uintptr_t* snapshot,
    int32_t player_count,
    const char* source,
    int* out_abort_for_save)
{
    int result = 0;
    int abort_for_save = 0;
    int claimed_today = 0;
    int completed_daily_run = 0;
    int candidate_pool_attempted = 0;
    KboIndependentAcquisitionCandidatePool candidate_pool;
    memset(&candidate_pool, 0, sizeof(candidate_pool));

    if (out_abort_for_save != NULL) {
        *out_abort_for_save = 0;
    }
    if (today == 0u || snapshot == NULL || player_count <= 0) {
        return 0;
    }

    uint32_t season = kbo_independent_acquisition_effective_season(today);
    int window_active = kbo_independent_acquisition_window_active_silent(today);
    if (!window_active) {
        return 0;
    }
    if (kbo_independent_acquisition_abort_if_save(source, "after_date", today)) {
        abort_for_save = 1;
        goto cleanup;
    }

    KboIndependentFuturesTeamLeague available_sellers[KBO_INDEPENDENT_ACQUISITION_MAX_SELLERS];
    memset(available_sellers, 0, sizeof(available_sellers));
    KboIndependentAcquisitionSellerAvailability availability =
        kbo_independent_acquisition_collect_available_sellers_for_date(
            today,
            available_sellers,
            KBO_INDEPENDENT_ACQUISITION_MAX_SELLERS);
    int seller_count = availability.seller_count;
    int available_seller_count = availability.available_seller_count;
    int capped_sellers = availability.capped_sellers;
    int32_t seller_transfer_limit = availability.seller_transfer_limit;
    if (seller_count <= 0) {
        kbo_log_runtimef(
            "independent acquisition AI skipped source=%s reason=no_resolved_seller today=%u seed_rows=%d unresolved=%d",
            source != NULL ? source : "",
            today,
            availability.seed_rows,
            availability.unresolved_rows);
        goto cleanup;
    }

    if (capped_sellers > 0) {
        kbo_log_runtimef(
            "independent acquisition AI seller transfer limit source=%s today=%u limit=%d sellers=%d available=%d capped=%d",
            source != NULL ? source : "",
            today,
            seller_transfer_limit,
            seller_count,
            available_seller_count,
            capped_sellers);
    }
    if (available_seller_count <= 0) {
        if (kbo_independent_acquisition_abort_if_save(source, "before_daily_claim", today)) {
            abort_for_save = 1;
            goto cleanup;
        }
        if (!kbo_independent_acquisition_claim_daily_run(today)) {
            goto cleanup;
        }
        claimed_today = 1;
        kbo_log_runtimef(
            "independent acquisition AI summary source=%s today=%u requested=0 shortlist_requests=0 refreshed_pending=0 buyers=0 skipped_human=0 sellers=%d available_sellers=0 capped_sellers=%d seller_transfer_limit=%d buyer_pending_limit=%d player_count=%d team_scanned=0 team_unreadable=0",
            source != NULL ? source : "",
            today,
            seller_count,
            capped_sellers,
            seller_transfer_limit,
            KBO_INDEPENDENT_ACQUISITION_BUYER_PENDING_LIMIT,
            player_count);
        if (kbo_independent_acquisition_abort_if_save(source, "before_seller_ai", today)) {
            abort_for_save = 1;
            goto cleanup;
        }
        int transferred = kbo_run_independent_team_acquisition_seller_ai(
            today,
            snapshot,
            player_count,
            source);
        result += transferred;
        completed_daily_run = 1;
        goto cleanup;
    }
    if (kbo_independent_acquisition_abort_if_save(source, "before_buyer_scan", today)) {
        abort_for_save = 1;
        goto cleanup;
    }

    uint32_t kbo_league_id = kbo_resolve_kbo_league_id();
    uint32_t buyer_team_ids[KBO_INDEPENDENT_ACQUISITION_MAX_BUYERS] = {0};
    int scanned = 0;
    int unreadable = 0;
    int buyer_count = collect_kbo_league_team_ids(
        kbo_league_id,
        buyer_team_ids,
        KBO_INDEPENDENT_ACQUISITION_MAX_BUYERS,
        &scanned,
        &unreadable);
    if (kbo_independent_acquisition_abort_if_save(source, "before_daily_claim", today)) {
        abort_for_save = 1;
        goto cleanup;
    }
    if (!kbo_independent_acquisition_claim_daily_run(today)) {
        goto cleanup;
    }
    claimed_today = 1;

    int requested = 0;
    int refreshed_pending = 0;
    int shortlist_requests = 0;
    int considered_buyers = 0;
    int skipped_human = 0;
    KboIndependentAcquisitionQueuedRequest pending_requests[KBO_INDEPENDENT_ACQUISITION_MAX_QUEUE];
    int pending_request_count = kbo_independent_acquisition_load_requests(
        season,
        pending_requests,
        KBO_INDEPENDENT_ACQUISITION_MAX_QUEUE);
    for (int i = 0; i < buyer_count; i++) {
        if ((i & 3) == 0
                && kbo_independent_acquisition_abort_if_save(source, "buyer_loop", today)) {
            abort_for_save = 1;
            goto cleanup;
        }
        uint32_t buyer_team_id = buyer_team_ids[i];
        if (kbo_team_is_human_controlled(buyer_team_id, "independent_acquisition_ai")) {
            skipped_human++;
            continue;
        }
        int buyer_pending_count = kbo_independent_acquisition_buyer_pending_request_count(
                pending_requests,
                pending_request_count,
                buyer_team_id);
        if (buyer_pending_count >= KBO_INDEPENDENT_ACQUISITION_BUYER_PENDING_LIMIT) {
            refreshed_pending++;
            continue;
        }

        uint8_t* buyer_team = find_kbo_team_by_numeric_id_any_league(buyer_team_id, 1);
        if (buyer_team == NULL || !memory_range_readable(buyer_team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            continue;
        }

        KboIndependentAcquisitionBuyerState buyer;
        kbo_independent_acquisition_read_buyer_state(buyer_team, &buyer);
        if (buyer.team_id == 0u) {
            continue;
        }
        considered_buyers++;

        while (window_active
                && available_seller_count > 0
                && buyer_pending_count < KBO_INDEPENDENT_ACQUISITION_BUYER_PENDING_LIMIT) {
            if (!candidate_pool_attempted) {
                candidate_pool_attempted = 1;
                kbo_independent_acquisition_build_candidate_pool(
                    snapshot,
                    player_count,
                    available_sellers,
                    available_seller_count,
                    &candidate_pool);
            }
            if (candidate_pool.count <= 0) {
                break;
            }
            KboIndependentAcquisitionCandidate candidate;
            if (!kbo_independent_acquisition_choose_candidate_from_pool(
                    &candidate_pool,
                    pending_requests,
                    pending_request_count,
                    &buyer,
                    &candidate)) {
                break;
            }

            const KboIndependentFuturesTeamLeague* seller = NULL;
            for (int s = 0; s < available_seller_count; s++) {
                if (available_sellers[s].team_id == candidate.seller_team_id) {
                    seller = &available_sellers[s];
                    break;
                }
            }
            if (seller == NULL) {
                break;
            }
            if (kbo_independent_acquisition_abort_if_save(source, "before_append_request", today)) {
                abort_for_save = 1;
                goto cleanup;
            }

            int request_available = kbo_independent_acquisition_append_request(
                today,
                &candidate,
                &buyer,
                seller,
                source);
            if (!request_available) {
                break;
            }

            char request_score_text[32] = {0};
            snprintf(request_score_text, sizeof(request_score_text), "%" PRId64, (int64_t)candidate.request_score);
            if (kbo_independent_acquisition_abort_if_save(source, "before_pending_offer_record", today)) {
                abort_for_save = 1;
                goto cleanup;
            }
            kbo_record_custom_foreign_pending_offer(
                buyer.team_id,
                (uint8_t*)candidate.player_ptr,
                today);
            if (pending_request_count < KBO_INDEPENDENT_ACQUISITION_MAX_QUEUE) {
                KboIndependentAcquisitionQueuedRequest* pending = &pending_requests[pending_request_count++];
                pending->date = today;
                pending->season = season;
                pending->buyer_team_id = buyer.team_id;
                pending->seller_team_id = candidate.seller_team_id;
                pending->player_id = candidate.player_id;
                pending->request_score = candidate.request_score;
                pending->value_score = candidate.value_score;
                pending->cash_cost = kbo_independent_acquisition_cash_cost_for_player(
                    (uint8_t*)candidate.player_ptr);
            }
            buyer_pending_count++;
            shortlist_requests++;
            requested++;
            kbo_log_runtimef(
                "independent acquisition AI request source=%s action=%s buyer=%u seller=%u seller_csv=%s player=%u score=%s value=%d cash_cost=%d cash_available=%d effective=%u->%u limit=%u slot=%s shortlist_slot=%d",
                source != NULL ? source : "",
                "new",
                buyer.team_id,
                candidate.seller_team_id,
                seller->team_csv_id,
                candidate.player_id,
                request_score_text,
                candidate.value_score,
                kbo_independent_acquisition_cash_cost_for_player((uint8_t*)candidate.player_ptr),
                buyer.cash_available,
                candidate.effective_before,
                candidate.effective_after,
                candidate.effective_limit,
                candidate.slot_type != 0u ? kbo_foreign_injury_slot_label(candidate.slot_type) : "none",
                buyer_pending_count);
        }
    }

    kbo_log_runtimef(
        "independent acquisition AI summary source=%s today=%u requested=%d shortlist_requests=%d refreshed_pending=%d buyers=%d skipped_human=%d sellers=%d available_sellers=%d capped_sellers=%d seller_transfer_limit=%d buyer_pending_limit=%d player_count=%d team_scanned=%d team_unreadable=%d",
        source != NULL ? source : "",
        today,
        requested,
        shortlist_requests,
        refreshed_pending,
        considered_buyers,
        skipped_human,
        seller_count,
        available_seller_count,
        capped_sellers,
        seller_transfer_limit,
        KBO_INDEPENDENT_ACQUISITION_BUYER_PENDING_LIMIT,
        player_count,
        scanned,
        unreadable);
    if (kbo_independent_acquisition_abort_if_save(source, "before_seller_ai", today)) {
        abort_for_save = 1;
        goto cleanup;
    }
    int transferred = kbo_run_independent_team_acquisition_seller_ai(
        today,
        snapshot,
        player_count,
        source);
    result += requested + transferred;
    completed_daily_run = 1;

cleanup:
    kbo_independent_acquisition_free_candidate_pool(&candidate_pool);
    if (abort_for_save && claimed_today && !completed_daily_run) {
        kbo_independent_acquisition_release_daily_run(today);
    }
    if (abort_for_save && out_abort_for_save != NULL) {
        *out_abort_for_save = 1;
    }
    return result;
}
