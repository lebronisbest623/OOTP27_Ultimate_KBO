#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "independent_acquisition_ai.h"
#include "ai/independent_acquisition_ai_internal.h"
#include "ai/lifecycle/independent_acquisition_ai_lifecycle.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../core/core_flags/api/flags_api.h"
#include "../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../core/dates/core_text_date.h"
#include "../../core/logging/core_log.h"
#include "../../core/season/phase/season_phase.h"
#include "../../core/teams/core_team_collect.h"
#include "../../foreign/common/dates/foreign_waiver_date.h"
#include "../../foreign/common/policy/foreign_player_policy.h"
#include "../../foreign/common/policy/foreign_waiver_policy.h"
#include "../../foreign/injury/api/foreign_injury_labels.h"
#include "../../runtime_memory/runtime_memory.h"
#include "../classification/team_classification.h"
#include "../control/team_human_control.h"
#include "../lookup/team_lookup.h"
#include "window/independent_acquisition_window.h"

extern volatile LONG g_kbo_runtime_date_stable_ready;
static volatile LONG g_kbo_independent_acquisition_ai_last_run_date = 0;
static volatile LONG g_kbo_independent_acquisition_ai_running = 0;

int kbo_run_independent_team_acquisition_ai(const char* source)
{
    if (!kbo_fix_enabled()
            || !kbo_custom_foreign_policy_enabled()
            || read_kbo_localappdata_flag_file("disable_independent_acquisition_ai.txt")) {
        return 0;
    }
    if (!kbo_runtime_pause_for_save_if_needed(source != NULL ? source : "independent_acquisition_ai")) {
        return 0;
    }
    if (InterlockedCompareExchange(&g_kbo_runtime_date_stable_ready, 0, 0) == 0) {
        static volatile LONG skipped_unstable_log_count = 0;
        if (InterlockedIncrement(&skipped_unstable_log_count) <= 40) {
            kbo_log_runtimef(
                "independent acquisition AI skipped source=%s reason=date_not_stable",
                source != NULL ? source : "");
        }
        return 0;
    }

    if (InterlockedCompareExchange(&g_kbo_independent_acquisition_ai_running, 1, 0) != 0) {
        kbo_log_runtimef(
            "independent acquisition AI skipped source=%s reason=already_running",
            source != NULL ? source : "");
        return 0;
    }

    int result = 0;
    int abort_for_save = 0;
    int claimed_today = 0;
    int completed_daily_run = 0;
    uintptr_t* snapshot = NULL;
    uint32_t today = 0u;
    if (kbo_independent_acquisition_abort_if_save(source, "after_lock", today)) {
        abort_for_save = 1;
        goto cleanup;
    }

    if (!kbo_get_current_yyyymmdd(&today)) {
        goto cleanup;
    }
    uint32_t season = kbo_independent_acquisition_effective_season(today);
    int window_active = kbo_independent_acquisition_window_active(today);
    if (!window_active) {
        goto cleanup;
    }
    if (kbo_independent_acquisition_abort_if_save(source, "after_date", today)) {
        abort_for_save = 1;
        goto cleanup;
    }

    KboIndependentFuturesTeamLeague sellers[KBO_INDEPENDENT_ACQUISITION_MAX_SELLERS];
    int seed_rows = 0;
    int unresolved_rows = 0;
    int seller_count = kbo_collect_independent_futures_team_leagues(
        sellers,
        KBO_INDEPENDENT_ACQUISITION_MAX_SELLERS,
        &seed_rows,
        &unresolved_rows);
    if (seller_count <= 0) {
        kbo_log_runtimef(
            "independent acquisition AI skipped source=%s reason=no_resolved_seller today=%u seed_rows=%d unresolved=%d",
            source != NULL ? source : "",
            today,
            seed_rows,
            unresolved_rows);
        goto cleanup;
    }

    int32_t seller_transfer_limit =
        kbo_foreign_player_policy()->independent_acquisition_seller_transfer_limit;
    KboIndependentFuturesTeamLeague available_sellers[KBO_INDEPENDENT_ACQUISITION_MAX_SELLERS];
    memset(available_sellers, 0, sizeof(available_sellers));
    int available_seller_count = 0;
    int capped_sellers = 0;
    for (int i = 0; i < seller_count; i++) {
        if ((i & 3) == 0
                && kbo_independent_acquisition_abort_if_save(source, "seller_limit_scan", today)) {
            abort_for_save = 1;
            goto cleanup;
        }
        int transfers = kbo_independent_acquisition_transferred_count(season, sellers[i].team_id);
        if (transfers >= seller_transfer_limit) {
            capped_sellers++;
            continue;
        }
        if (available_seller_count < KBO_INDEPENDENT_ACQUISITION_MAX_SELLERS) {
            available_sellers[available_seller_count++] = sellers[i];
        }
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
    if (kbo_independent_acquisition_abort_if_save(source, "before_player_snapshot", today)) {
        abort_for_save = 1;
        goto cleanup;
    }

    uintptr_t player_vector = 0u;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)
            || player_vector == 0u
            || player_count <= 0
            || player_count > 200000) {
        goto cleanup;
    }
    SIZE_T player_vector_bytes = (SIZE_T)player_count * sizeof(uintptr_t);
    if (!memory_range_readable((void*)player_vector, player_vector_bytes)) {
        goto cleanup;
    }
    snapshot = (uintptr_t*)HeapAlloc(GetProcessHeap(), 0, player_vector_bytes);
    if (snapshot == NULL) {
        goto cleanup;
    }
    SIZE_T bytes_read = 0u;
    if (!ReadProcessMemory(
            GetCurrentProcess(),
            (LPCVOID)player_vector,
            snapshot,
            player_vector_bytes,
            &bytes_read)
            || bytes_read != player_vector_bytes) {
        goto cleanup;
    }
    if (kbo_independent_acquisition_abort_if_save(source, "after_player_snapshot", today)) {
        abort_for_save = 1;
        goto cleanup;
    }

    uint32_t kbo_league_id = kbo_get_foreign_waiver_league_id();
    if (kbo_league_id == 0u) {
        kbo_league_id = kbo_resolve_kbo_league_id();
    }
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
            KboIndependentAcquisitionCandidate candidate;
            if (!kbo_independent_acquisition_choose_candidate_for_buyer(
                    snapshot,
                    player_count,
                    available_sellers,
                    available_seller_count,
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
    result = requested + transferred;
    completed_daily_run = 1;

cleanup:
    if (abort_for_save && claimed_today && !completed_daily_run) {
        kbo_independent_acquisition_release_daily_run(today);
    }
    if (snapshot != NULL) {
        HeapFree(GetProcessHeap(), 0, snapshot);
    }
    InterlockedExchange(&g_kbo_independent_acquisition_ai_running, 0);
    return result;
}
