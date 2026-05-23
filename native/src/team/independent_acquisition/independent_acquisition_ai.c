#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "independent_acquisition_ai.h"
#include "independent_acquisition_ai_module.h"
#include "ai/lifecycle/independent_acquisition_ai_lifecycle.h"

#include <stdint.h>

#include "../../core/core_flags/api/flags_api.h"
#include "../../core/dates/tick/current_date_tick_capture.h"
#include "../../core/logging/core_log.h"
#include "../../foreign/common/policy/foreign_waiver_policy.h"
#include "../../runtime_memory/runtime_memory.h"
#include "../lookup/team_lookup.h"
#include "window/independent_acquisition_window.h"
#include "../../core/core_flags/keys/runtime_flag_keys.generated.h"

volatile LONG g_kbo_independent_acquisition_ai_running = 0;

static int kbo_run_independent_team_acquisition_ai_for_date_core(
    uint32_t today,
    const char* source)
{
    if (!kbo_fix_enabled()
            || !kbo_custom_foreign_policy_enabled()
            || read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_DISABLE_INDEPENDENT_ACQUISITION_AI_FILE)) {
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
            || player_count > KBO_RUNTIME_MAX_PLAYER_VECTOR_COUNT) {
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

int kbo_run_independent_team_acquisition_ai_for_date(uint32_t today, const char* source)
{
    return kbo_run_independent_team_acquisition_ai_for_date_core(today, source);
}

int kbo_run_independent_team_acquisition_ai(const char* source)
{
    if (!kbo_fix_enabled()
            || !kbo_custom_foreign_policy_enabled()
            || read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_DISABLE_INDEPENDENT_ACQUISITION_AI_FILE)) {
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
            || player_count > KBO_RUNTIME_MAX_PLAYER_VECTOR_COUNT) {
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
