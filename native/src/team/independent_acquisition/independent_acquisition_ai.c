#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "independent_acquisition_ai.h"
#include "ai/independent_acquisition_ai_internal.h"
#include "ai/lifecycle/independent_acquisition_ai_lifecycle.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../core/core_flags/api/flags_api.h"
#include "../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../core/dates/core_text_date.h"
#include "../../core/dates/tick/current_date_tick_capture.h"
#include "../../core/files/save_paths/core_save_paths.h"
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
static volatile LONG g_kbo_independent_acquisition_ai_last_processed_date = 0;
static volatile LONG g_kbo_independent_acquisition_ai_running = 0;
static char g_kbo_independent_acquisition_ai_cursor_save_path[MAX_PATH] = {0};

#define KBO_INDEPENDENT_ACQUISITION_AI_CURSOR_FILE "independent_acquisition_ai_cursor.txt"
#define KBO_INDEPENDENT_ACQUISITION_AI_MAX_CATCHUP_DAYS 220

typedef struct KboIndependentAcquisitionSellerAvailability {
    int seed_rows;
    int unresolved_rows;
    int seller_count;
    int available_seller_count;
    int capped_sellers;
    int32_t seller_transfer_limit;
} KboIndependentAcquisitionSellerAvailability;

static int kbo_independent_acquisition_window_active_silent(uint32_t today)
{
    return kbo_independent_team_acquisition_window_active(today, NULL, NULL);
}

static uint32_t kbo_independent_acquisition_next_date(uint32_t today)
{
    return kbo_add_days_yyyymmdd(today, 1u);
}

static int kbo_independent_acquisition_ai_cursor_path(char* out, size_t out_size)
{
    return kbo_get_save_scoped_data_file(
        KBO_INDEPENDENT_ACQUISITION_AI_CURSOR_FILE,
        out,
        out_size);
}

static uint32_t kbo_independent_acquisition_load_processed_date(void)
{
    char path[MAX_PATH] = {0};
    if (!kbo_independent_acquisition_ai_cursor_path(path, sizeof(path))) {
        return 0u;
    }

    HANDLE file = CreateFileA(
        path,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return 0u;
    }

    char text[32] = {0};
    DWORD read = 0u;
    int ok = ReadFile(file, text, sizeof(text) - 1u, &read, NULL) && read > 0u;
    CloseHandle(file);
    if (!ok) {
        return 0u;
    }

    unsigned long value = strtoul(text, NULL, 10);
    if (value < 19820101ul || value > 22001231ul) {
        return 0u;
    }
    return (uint32_t)value;
}

static void kbo_independent_acquisition_persist_processed_date(uint32_t today, const char* source)
{
    char path[MAX_PATH] = {0};
    if (today == 0u || !kbo_independent_acquisition_ai_cursor_path(path, sizeof(path))) {
        return;
    }

    char text[32] = {0};
    snprintf(text, sizeof(text), "%u\r\n", today);
    HANDLE file = CreateFileA(
        path,
        GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == INVALID_HANDLE_VALUE) {
        kbo_log_runtimef(
            "independent acquisition AI cursor persist skipped source=%s date=%u gle=%lu path=%s",
            source != NULL ? source : "",
            today,
            (unsigned long)GetLastError(),
            path);
        return;
    }

    DWORD written = 0u;
    DWORD len = (DWORD)strlen(text);
    if (!WriteFile(file, text, len, &written, NULL) || written != len) {
        kbo_log_runtimef(
            "independent acquisition AI cursor persist failed source=%s date=%u gle=%lu path=%s",
            source != NULL ? source : "",
            today,
            (unsigned long)GetLastError(),
            path);
    }
    CloseHandle(file);
}

static uint32_t kbo_independent_acquisition_observed_processed_date(void)
{
    return (uint32_t)InterlockedCompareExchange(
        &g_kbo_independent_acquisition_ai_last_processed_date,
        0,
        0);
}

static void kbo_independent_acquisition_sync_cursor_save_scope(void)
{
    char save_path[MAX_PATH] = {0};
    if (!kbo_get_current_save_path(save_path, sizeof(save_path))) {
        return;
    }
    if (g_kbo_independent_acquisition_ai_cursor_save_path[0] != '\0'
            && strcmp(g_kbo_independent_acquisition_ai_cursor_save_path, save_path) == 0) {
        return;
    }

    snprintf(
        g_kbo_independent_acquisition_ai_cursor_save_path,
        sizeof(g_kbo_independent_acquisition_ai_cursor_save_path),
        "%s",
        save_path);
    InterlockedExchange(&g_kbo_independent_acquisition_ai_last_processed_date, 0);
}

static uint32_t kbo_independent_acquisition_processed_date(void)
{
    kbo_independent_acquisition_sync_cursor_save_scope();

    uint32_t cached = kbo_independent_acquisition_observed_processed_date();
    if (cached != 0u) {
        return cached;
    }

    uint32_t loaded = kbo_independent_acquisition_load_processed_date();
    if (loaded != 0u) {
        InterlockedCompareExchange(
            &g_kbo_independent_acquisition_ai_last_processed_date,
            (LONG)loaded,
            0);
    }
    return loaded;
}

static void kbo_independent_acquisition_mark_processed_date(uint32_t today, const char* source)
{
    if (today != 0u) {
        kbo_independent_acquisition_sync_cursor_save_scope();
        InterlockedExchange(
            &g_kbo_independent_acquisition_ai_last_processed_date,
            (LONG)today);
        kbo_independent_acquisition_persist_processed_date(today, source);
    }
}

static KboIndependentAcquisitionSellerAvailability
kbo_independent_acquisition_collect_available_sellers_for_date(
    uint32_t today,
    KboIndependentFuturesTeamLeague* out_available_sellers,
    int max_available_sellers)
{
    KboIndependentAcquisitionSellerAvailability availability;
    memset(&availability, 0, sizeof(availability));
    availability.seller_transfer_limit =
        kbo_foreign_player_policy()->independent_acquisition_seller_transfer_limit;

    KboIndependentFuturesTeamLeague sellers[KBO_INDEPENDENT_ACQUISITION_MAX_SELLERS];
    memset(sellers, 0, sizeof(sellers));
    availability.seller_count = kbo_collect_independent_futures_team_leagues(
        sellers,
        KBO_INDEPENDENT_ACQUISITION_MAX_SELLERS,
        &availability.seed_rows,
        &availability.unresolved_rows);
    if (availability.seller_count <= 0) {
        return availability;
    }

    uint32_t season = kbo_independent_acquisition_effective_season(today);
    for (int i = 0; i < availability.seller_count; i++) {
        int transfers = kbo_independent_acquisition_transferred_count(season, sellers[i].team_id);
        if (transfers >= availability.seller_transfer_limit) {
            availability.capped_sellers++;
            continue;
        }
        if (out_available_sellers != NULL
                && availability.available_seller_count < max_available_sellers) {
            out_available_sellers[availability.available_seller_count] = sellers[i];
        }
        availability.available_seller_count++;
    }

    return availability;
}

static int kbo_independent_acquisition_has_pending_requests(uint32_t today)
{
    uint32_t season = kbo_independent_acquisition_effective_season(today);
    KboIndependentAcquisitionQueuedRequest pending;
    memset(&pending, 0, sizeof(pending));
    return kbo_independent_acquisition_load_requests(season, &pending, 1) > 0;
}

static int kbo_run_independent_team_acquisition_ai_with_snapshot_for_date(
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

int kbo_run_independent_team_acquisition_ai_for_date(uint32_t today, const char* source)
{
    if (!kbo_fix_enabled()
            || !kbo_custom_foreign_policy_enabled()
            || read_kbo_localappdata_flag_file("disable_independent_acquisition_ai.txt")) {
        return 0;
    }
    if (today == 0u) {
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
    uintptr_t* snapshot = NULL;
    if (kbo_independent_acquisition_abort_if_save(source, "after_lock", today)) {
        abort_for_save = 1;
        goto cleanup;
    }

    uint32_t previous_processed_date = kbo_independent_acquisition_processed_date();
    if (previous_processed_date >= today) {
        goto cleanup;
    }
    if (!kbo_independent_acquisition_window_active_silent(today)) {
        kbo_independent_acquisition_window_active(today);
        kbo_independent_acquisition_mark_processed_date(today, source);
        goto cleanup;
    }
    KboIndependentAcquisitionSellerAvailability availability =
        kbo_independent_acquisition_collect_available_sellers_for_date(today, NULL, 0);
    if ((availability.seller_count <= 0 || availability.available_seller_count <= 0)
            && !kbo_independent_acquisition_has_pending_requests(today)) {
        if (availability.seller_count <= 0) {
            kbo_log_runtimef(
                "independent acquisition AI skipped source=%s reason=no_resolved_seller today=%u seed_rows=%d unresolved=%d",
                source != NULL ? source : "",
                today,
                availability.seed_rows,
                availability.unresolved_rows);
        } else {
            if (availability.capped_sellers > 0) {
                kbo_log_runtimef(
                    "independent acquisition AI seller transfer limit source=%s today=%u limit=%d sellers=%d available=0 capped=%d",
                    source != NULL ? source : "",
                    today,
                    availability.seller_transfer_limit,
                    availability.seller_count,
                    availability.capped_sellers);
            }
            kbo_log_runtimef(
                "independent acquisition AI skipped source=%s reason=no_available_seller today=%u sellers=%d capped_sellers=%d seller_transfer_limit=%d pending_requests=0",
                source != NULL ? source : "",
                today,
                availability.seller_count,
                availability.capped_sellers,
                availability.seller_transfer_limit);
        }
        kbo_independent_acquisition_mark_processed_date(today, source);
        goto cleanup;
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

    result += kbo_run_independent_team_acquisition_ai_with_snapshot_for_date(
        today,
        snapshot,
        player_count,
        source,
        &abort_for_save);
    if (!abort_for_save) {
        kbo_independent_acquisition_mark_processed_date(today, source);
    }

cleanup:
    if (snapshot != NULL) {
        HeapFree(GetProcessHeap(), 0, snapshot);
    }
    InterlockedExchange(&g_kbo_independent_acquisition_ai_running, 0);
    return result;
}

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
    uintptr_t* snapshot = NULL;
    uint32_t today = 0u;
    if (kbo_independent_acquisition_abort_if_save(source, "after_lock", today)) {
        abort_for_save = 1;
        goto cleanup;
    }

    if (!kbo_current_date_tick_latest_published_date(&today)) {
        goto cleanup;
    }

    uint32_t previous_processed_date = kbo_independent_acquisition_processed_date();
    if (previous_processed_date == today) {
        goto cleanup;
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

    uint32_t run_date = today;
    if (previous_processed_date != 0u && previous_processed_date < today) {
        uint32_t next_date = kbo_independent_acquisition_next_date(previous_processed_date);
        if (next_date != 0u && next_date <= today) {
            run_date = next_date;
        }
    }

    int scanned_days = 0;
    int active_days = 0;
    int closed_days = 0;
    int truncated = 0;
    while (run_date != 0u && run_date <= today) {
        if (scanned_days >= KBO_INDEPENDENT_ACQUISITION_AI_MAX_CATCHUP_DAYS) {
            truncated = 1;
            break;
        }
        scanned_days++;
        if (kbo_independent_acquisition_abort_if_save(source, "date_loop", run_date)) {
            abort_for_save = 1;
            break;
        }

        if (kbo_independent_acquisition_window_active_silent(run_date)) {
            active_days++;
            result += kbo_run_independent_team_acquisition_ai_with_snapshot_for_date(
                run_date,
                snapshot,
                player_count,
                source,
                &abort_for_save);
            if (abort_for_save) {
                break;
            }
        } else {
            closed_days++;
            if (run_date == today) {
                kbo_independent_acquisition_window_active(run_date);
            }
        }

        kbo_independent_acquisition_mark_processed_date(run_date, source);
        if (run_date == today) {
            break;
        }
        uint32_t next_date = kbo_independent_acquisition_next_date(run_date);
        if (next_date == 0u || next_date <= run_date) {
            break;
        }
        run_date = next_date;
    }

    if (scanned_days > 1 || truncated) {
        kbo_log_runtimef(
            "independent acquisition AI date catchup source=%s previous=%u today=%u scanned_days=%d active_days=%d closed_days=%d result=%d truncated=%d",
            source != NULL ? source : "",
            previous_processed_date,
            today,
            scanned_days,
            active_days,
            closed_days,
            result,
            truncated);
    }

cleanup:
    if (snapshot != NULL) {
        HeapFree(GetProcessHeap(), 0, snapshot);
    }
    InterlockedExchange(&g_kbo_independent_acquisition_ai_running, 0);
    return result;
}
