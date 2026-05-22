#include "../internal/intl_established_fa_postscan_internal.h"
#include "../../../core/runtime_tuning/runtime_tuning_policy.h"

void kbo_intl_established_fa_postscan_try_run(void)
{
    if (!kbo_runtime_pause_for_save_if_needed("intl_established_fa_postscan")) {
        return;
    }
    if (InterlockedCompareExchange(
            &g_kbo_intl_established_fa_postscan.pending,
            KBO_INTL_FA_POSTSCAN_RUNNING,
            KBO_INTL_FA_POSTSCAN_PENDING) != KBO_INTL_FA_POSTSCAN_PENDING) {
        return;
    }

    ULONGLONG now = GetTickCount64();
    if (now < g_kbo_intl_established_fa_postscan.due_tick) {
        InterlockedExchange(&g_kbo_intl_established_fa_postscan.pending, KBO_INTL_FA_POSTSCAN_PENDING);
        return;
    }

    KboIntlEstablishedFaPostscanState batch = g_kbo_intl_established_fa_postscan;
    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    int vector_ready = find_kbo_global_player_vector(&player_vector, &player_count, NULL);
    int observed_count = kbo_intl_established_fa_postscan_observed_player_count();
    if (!vector_ready && batch.attempts < KBO_INTL_ESTABLISHED_FA_POSTSCAN_MAX_RETRIES) {
        g_kbo_intl_established_fa_postscan.attempts = batch.attempts + 1;
        g_kbo_intl_established_fa_postscan.due_tick = now + KBO_INTL_ESTABLISHED_FA_POSTSCAN_RETRY_MS;
        InterlockedExchange(&g_kbo_intl_established_fa_postscan.pending, KBO_INTL_FA_POSTSCAN_PENDING);
        kbo_log_runtimef(
            "international established FA postscan retry batch=%ld attempt=%d reason=no_player_vector",
            batch.batch_id,
            batch.attempts + 1);
        return;
    }

    if (vector_ready
            && batch.before_count > 0
            && batch.expected_count > 0
            && player_count < batch.before_count + batch.expected_count
            && observed_count <= 0
            && batch.attempts < KBO_INTL_ESTABLISHED_FA_POSTSCAN_MAX_RETRIES) {
        g_kbo_intl_established_fa_postscan.attempts = batch.attempts + 1;
        g_kbo_intl_established_fa_postscan.due_tick = now + KBO_INTL_ESTABLISHED_FA_POSTSCAN_RETRY_MS;
        InterlockedExchange(&g_kbo_intl_established_fa_postscan.pending, KBO_INTL_FA_POSTSCAN_PENDING);
        kbo_log_runtimef(
            "international established FA postscan retry batch=%ld attempt=%d reason=waiting_for_players before=%d after=%d expected=%d observed=%d",
            batch.batch_id,
            batch.attempts + 1,
            batch.before_count,
            player_count,
            batch.expected_count,
            observed_count);
        return;
    }

    kbo_intl_established_fa_postscan_run(&batch);
    InterlockedExchange(&g_kbo_intl_established_fa_postscan.pending, KBO_INTL_FA_POSTSCAN_IDLE);
}

int kbo_intl_established_fa_postscan_run_pending_now(
    uint32_t event_yyyymmdd,
    const char* source)
{
    if (event_yyyymmdd == 0u
            || !kbo_runtime_pause_for_save_if_needed("intl_established_fa_postscan_event")) {
        return 0;
    }

    if (InterlockedCompareExchange(
            &g_kbo_intl_established_fa_postscan.pending,
            KBO_INTL_FA_POSTSCAN_RUNNING,
            KBO_INTL_FA_POSTSCAN_PENDING) != KBO_INTL_FA_POSTSCAN_PENDING) {
        return 0;
    }

    KboIntlEstablishedFaPostscanState batch = g_kbo_intl_established_fa_postscan;
    if (!kbo_intl_established_fa_postscan_batch_matches_event_date(&batch, event_yyyymmdd)) {
        InterlockedExchange(&g_kbo_intl_established_fa_postscan.pending, KBO_INTL_FA_POSTSCAN_PENDING);
        kbo_log_runtimef(
            "international established FA postscan event force deferred source=%s event_date=%u scheduled=%u reason=date_mismatch",
            source != NULL ? source : "",
            event_yyyymmdd,
            batch.scheduled_date);
        return 0;
    }

    ULONGLONG now = GetTickCount64();
    int32_t player_count = 0;
    int vector_ready = find_kbo_global_player_vector(NULL, &player_count, NULL);
    int observed_count = kbo_intl_established_fa_postscan_observed_player_count();
    if (!vector_ready) {
        g_kbo_intl_established_fa_postscan.attempts = batch.attempts + 1;
        g_kbo_intl_established_fa_postscan.due_tick = now + KBO_INTL_ESTABLISHED_FA_POSTSCAN_RETRY_MS;
        InterlockedExchange(&g_kbo_intl_established_fa_postscan.pending, KBO_INTL_FA_POSTSCAN_PENDING);
        kbo_log_runtimef(
            "international established FA postscan event force deferred source=%s batch=%ld event_date=%u attempt=%d reason=no_player_vector",
            source != NULL ? source : "",
            batch.batch_id,
            event_yyyymmdd,
            batch.attempts + 1);
        return 0;
    }

    if (batch.before_count > 0
            && batch.expected_count > 0
            && player_count < batch.before_count + batch.expected_count
            && observed_count <= 0) {
        g_kbo_intl_established_fa_postscan.attempts = batch.attempts + 1;
        g_kbo_intl_established_fa_postscan.due_tick = now + KBO_INTL_ESTABLISHED_FA_POSTSCAN_RETRY_MS;
        InterlockedExchange(&g_kbo_intl_established_fa_postscan.pending, KBO_INTL_FA_POSTSCAN_PENDING);
        kbo_log_runtimef(
            "international established FA postscan event force deferred source=%s batch=%ld event_date=%u attempt=%d reason=waiting_for_players before=%d after=%d expected=%d observed=%d",
            source != NULL ? source : "",
            batch.batch_id,
            event_yyyymmdd,
            batch.attempts + 1,
            batch.before_count,
            player_count,
            batch.expected_count,
            observed_count);
        return 0;
    }

    g_kbo_intl_established_fa_postscan.due_tick = now;
    kbo_intl_established_fa_postscan_run(&batch);
    InterlockedExchange(&g_kbo_intl_established_fa_postscan.pending, KBO_INTL_FA_POSTSCAN_IDLE);
    kbo_log_runtimef(
        "international established FA postscan event force ran source=%s batch=%ld event_date=%u after_count=%d expected=%d",
        source != NULL ? source : "",
        batch.batch_id,
        event_yyyymmdd,
        player_count,
        batch.expected_count);
    return 1;
}

DWORD WINAPI kbo_intl_established_fa_postscan_thread(LPVOID parameter)
{
    (void)parameter;
    kbo_log_runtime_line("international established FA postscan worker started");
    while (kbo_runtime_threads_should_continue()) {
        if (!kbo_runtime_sleep_should_continue((uint32_t)kbo_runtime_tuning_policy()->intl_established_fa_postscan_sleep_ms)) {
            break;
        }
        kbo_intl_established_fa_postscan_try_run();
    }
    InterlockedExchange(&g_kbo_intl_established_fa_postscan_worker_started, 0);
    kbo_log_runtime_line("international established FA postscan worker stopped");
    return 0;
}

void start_kbo_intl_established_fa_postscan_thread(void)
{
    if (InterlockedCompareExchange(&g_kbo_intl_established_fa_postscan_worker_started, 1, 0) != 0) {
        return;
    }

    if (!kbo_start_runtime_thread(
            kbo_intl_established_fa_postscan_thread,
            NULL,
            "intl established FA postscan")) {
        InterlockedExchange(&g_kbo_intl_established_fa_postscan_worker_started, 0);
    }
}

