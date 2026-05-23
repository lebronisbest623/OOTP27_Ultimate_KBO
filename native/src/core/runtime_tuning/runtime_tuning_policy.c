#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "runtime_tuning_policy.h"
#include "../policy/core_policy.h"

#define KBO_RUNTIME_TUNING_POLICY_FILE "runtime_tuning_policy.json"

static INIT_ONCE g_kbo_runtime_tuning_policy_once = INIT_ONCE_STATIC_INIT;
static KboRuntimeTuningPolicy g_kbo_runtime_tuning_policy;

static int32_t kbo_runtime_tuning_policy_int(const char* key, int32_t fallback, int32_t min_value, int32_t max_value)
{
    return kbo_read_clamped_global_policy_int(KBO_RUNTIME_TUNING_POLICY_FILE, key, fallback, min_value, max_value);
}

static void kbo_runtime_tuning_policy_log_attempts(
    int32_t* out,
    int count,
    const char* key_prefix,
    const int32_t* defaults)
{
    for (int i = 0; i < count; i++) {
        char key[80] = {0};
        snprintf(key, sizeof(key), "%s_%d", key_prefix, i + 1);
        out[i] = kbo_runtime_tuning_policy_int(key, defaults[i], 1, 1000000);
    }
}

static BOOL CALLBACK kbo_runtime_tuning_policy_init_once(PINIT_ONCE init_once, PVOID parameter, PVOID* context)
{
    (void)init_once;
    (void)parameter;
    (void)context;

    KboRuntimeTuningPolicy* p = &g_kbo_runtime_tuning_policy;
    memset(p, 0, sizeof(*p));
    p->allstar_force_retry_attempts = kbo_runtime_tuning_policy_int("allstar_force_retry_attempts", 180, 1, 100000);
    p->allstar_force_retry_sleep_ms = kbo_runtime_tuning_policy_int("allstar_force_retry_sleep_ms", 1000, 0, 600000);
    p->sangmu_delayed_install_attempts = kbo_runtime_tuning_policy_int("sangmu_delayed_install_attempts", 180, 1, 100000);
    p->sangmu_delayed_install_stable_ticks = kbo_runtime_tuning_policy_int("sangmu_delayed_install_stable_ticks", 6, 1, 1000);
    p->sangmu_delayed_install_sleep_ms = kbo_runtime_tuning_policy_int("sangmu_delayed_install_sleep_ms", 1000, 0, 600000);
    static const int32_t sangmu_log_defaults[5] = { 1, 10, 30, 60, 120 };
    kbo_runtime_tuning_policy_log_attempts(p->sangmu_delayed_install_log_attempts, 5, "sangmu_delayed_install_log_attempt", sangmu_log_defaults);

    p->runtime_marker_wait_attempts = kbo_runtime_tuning_policy_int("runtime_marker_wait_attempts", 450, 1, 100000);
    p->runtime_marker_wait_stable_ticks = kbo_runtime_tuning_policy_int("runtime_marker_wait_stable_ticks", 8, 1, 1000);
    p->runtime_marker_wait_sleep_ms = kbo_runtime_tuning_policy_int("runtime_marker_wait_sleep_ms", 2000, 0, 600000);
    static const int32_t marker_log_defaults[3] = { 1, 5, 15 };
    kbo_runtime_tuning_policy_log_attempts(p->runtime_marker_log_attempts, 3, "runtime_marker_log_attempt", marker_log_defaults);
    p->runtime_marker_log_interval = kbo_runtime_tuning_policy_int("runtime_marker_log_interval", 30, 1, 100000);
    p->global_db_scan_interval_ms = kbo_runtime_tuning_policy_int("global_db_scan_interval_ms", 3000, 0, 600000);
    p->hub_foreign_slot_cache_ttl_ms = kbo_runtime_tuning_policy_int("hub_foreign_slot_cache_ttl_ms", 1000, 0, 600000);

    p->no_minor_delayed_install_attempts = kbo_runtime_tuning_policy_int("no_minor_delayed_install_attempts", 720, 1, 100000);
    p->no_minor_delayed_install_first_sleep_ms = kbo_runtime_tuning_policy_int("no_minor_delayed_install_first_sleep_ms", 1000, 0, 600000);
    p->no_minor_delayed_install_sleep_ms = kbo_runtime_tuning_policy_int("no_minor_delayed_install_sleep_ms", 5000, 0, 600000);
    p->no_minor_delayed_install_log_initial_attempts = kbo_runtime_tuning_policy_int("no_minor_delayed_install_log_initial_attempts", 8, 0, 100000);
    p->no_minor_delayed_install_log_interval = kbo_runtime_tuning_policy_int("no_minor_delayed_install_log_interval", 12, 1, 100000);

    p->season_phase_monitor_sleep_ms = kbo_runtime_tuning_policy_int("season_phase_monitor_sleep_ms", 2000, 100, 600000);
    p->captain_selection_thread_sleep_ms = kbo_runtime_tuning_policy_int("captain_selection_thread_sleep_ms", 5000, 100, 600000);
    p->custom_event_monitor_sleep_ms = kbo_runtime_tuning_policy_int("custom_event_monitor_sleep_ms", 1000, 100, 600000);
    p->amateur_reputation_update_thread_sleep_ms = kbo_runtime_tuning_policy_int("amateur_reputation_update_thread_sleep_ms", 5000, 100, 600000);
    p->amateur_assignment_ortools_batch_sleep_ms = kbo_runtime_tuning_policy_int("amateur_assignment_ortools_batch_sleep_ms", 250, 10, 600000);
    p->fa_salary_snapshot_thread_sleep_ms = kbo_runtime_tuning_policy_int("fa_salary_snapshot_thread_sleep_ms", 1000, 100, 600000);
    p->fa_salary_snapshot_phase_event_sleep_ms = kbo_runtime_tuning_policy_int("fa_salary_snapshot_phase_event_sleep_ms", 250, 10, 600000);
    p->fa_salary_snapshot_opening_window_days = kbo_runtime_tuning_policy_int("fa_salary_snapshot_opening_window_days", 3, 0, 60);
    p->foreign_waiver_scanner_sleep_ms = kbo_runtime_tuning_policy_int("foreign_waiver_scanner_sleep_ms", 5000, 100, 600000);
    p->foreign_roster_daily_audit_sleep_ms = kbo_runtime_tuning_policy_int("foreign_roster_daily_audit_sleep_ms", 5000, 100, 600000);
    p->intl_established_fa_postscan_sleep_ms = kbo_runtime_tuning_policy_int("intl_established_fa_postscan_sleep_ms", 1000, 100, 600000);
    p->intl_established_fa_postscan_delay_ms = kbo_runtime_tuning_policy_int("intl_established_fa_postscan_delay_ms", 4500, 0, 600000);
    p->intl_established_fa_postscan_retry_ms = kbo_runtime_tuning_policy_int("intl_established_fa_postscan_retry_ms", 2500, 0, 600000);
    p->intl_established_fa_postscan_max_retries = kbo_runtime_tuning_policy_int("intl_established_fa_postscan_max_retries", 8, 0, 100000);
    p->intl_established_fa_postscan_max_detail_logs = kbo_runtime_tuning_policy_int("intl_established_fa_postscan_max_detail_logs", 160, 0, 1000000);
    p->military_days_tick_sleep_ms = kbo_runtime_tuning_policy_int("military_days_tick_sleep_ms", 5000, 100, 600000);
    p->military_seed_bootstrap_attempts = kbo_runtime_tuning_policy_int("military_seed_bootstrap_attempts", 720, 1, 100000);
    p->military_seed_bootstrap_first_sleep_ms = kbo_runtime_tuning_policy_int("military_seed_bootstrap_first_sleep_ms", 2500, 0, 600000);
    p->military_seed_bootstrap_sleep_ms = kbo_runtime_tuning_policy_int("military_seed_bootstrap_sleep_ms", 5000, 0, 600000);
    p->military_seed_bootstrap_log_initial_attempts = kbo_runtime_tuning_policy_int("military_seed_bootstrap_log_initial_attempts", 8, 0, 100000);
    p->military_seed_bootstrap_log_interval = kbo_runtime_tuning_policy_int("military_seed_bootstrap_log_interval", 6, 1, 100000);
    return TRUE;
}

const KboRuntimeTuningPolicy* kbo_runtime_tuning_policy(void)
{
    InitOnceExecuteOnce(&g_kbo_runtime_tuning_policy_once, kbo_runtime_tuning_policy_init_once, NULL, NULL);
    return &g_kbo_runtime_tuning_policy;
}

int kbo_runtime_tuning_sangmu_delayed_install_log_attempt(int attempt)
{
    const KboRuntimeTuningPolicy* p = kbo_runtime_tuning_policy();
    for (int i = 0; i < 5; i++) {
        if (attempt == p->sangmu_delayed_install_log_attempts[i]) {
            return 1;
        }
    }
    return 0;
}

int kbo_runtime_tuning_runtime_marker_log_attempt(int attempt)
{
    const KboRuntimeTuningPolicy* p = kbo_runtime_tuning_policy();
    for (int i = 0; i < 3; i++) {
        if (attempt == p->runtime_marker_log_attempts[i]) {
            return 1;
        }
    }
    return p->runtime_marker_log_interval > 0 && (attempt % p->runtime_marker_log_interval) == 0;
}
