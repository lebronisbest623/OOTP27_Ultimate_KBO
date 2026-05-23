#ifndef KBOFIX_SRC_CORE_RUNTIME_TUNING_POLICY_H_
#define KBOFIX_SRC_CORE_RUNTIME_TUNING_POLICY_H_

#include <stdint.h>

typedef struct KboRuntimeTuningPolicy {
    int32_t allstar_force_retry_attempts;
    int32_t allstar_force_retry_sleep_ms;
    int32_t sangmu_delayed_install_attempts;
    int32_t sangmu_delayed_install_stable_ticks;
    int32_t sangmu_delayed_install_sleep_ms;
    int32_t sangmu_delayed_install_log_attempts[5];
    int32_t runtime_marker_wait_attempts;
    int32_t runtime_marker_wait_stable_ticks;
    int32_t runtime_marker_wait_sleep_ms;
    int32_t runtime_marker_log_attempts[3];
    int32_t runtime_marker_log_interval;
    int32_t global_db_scan_interval_ms;
    int32_t hub_foreign_slot_cache_ttl_ms;
    int32_t no_minor_delayed_install_attempts;
    int32_t no_minor_delayed_install_first_sleep_ms;
    int32_t no_minor_delayed_install_sleep_ms;
    int32_t no_minor_delayed_install_log_initial_attempts;
    int32_t no_minor_delayed_install_log_interval;
    int32_t season_phase_monitor_sleep_ms;
    int32_t captain_selection_thread_sleep_ms;
    int32_t custom_event_monitor_sleep_ms;
    int32_t amateur_reputation_update_thread_sleep_ms;
    int32_t amateur_assignment_ortools_batch_sleep_ms;
    int32_t fa_salary_snapshot_thread_sleep_ms;
    int32_t fa_salary_snapshot_phase_event_sleep_ms;
    int32_t fa_salary_snapshot_opening_window_days;
    int32_t foreign_waiver_scanner_sleep_ms;
    int32_t foreign_roster_daily_audit_sleep_ms;
    int32_t intl_established_fa_postscan_sleep_ms;
    int32_t intl_established_fa_postscan_delay_ms;
    int32_t intl_established_fa_postscan_retry_ms;
    int32_t intl_established_fa_postscan_max_retries;
    int32_t intl_established_fa_postscan_max_detail_logs;
    int32_t military_days_tick_sleep_ms;
    int32_t military_seed_bootstrap_attempts;
    int32_t military_seed_bootstrap_first_sleep_ms;
    int32_t military_seed_bootstrap_sleep_ms;
    int32_t military_seed_bootstrap_log_initial_attempts;
    int32_t military_seed_bootstrap_log_interval;
} KboRuntimeTuningPolicy;

const KboRuntimeTuningPolicy* kbo_runtime_tuning_policy(void);
int kbo_runtime_tuning_sangmu_delayed_install_log_attempt(int attempt);
int kbo_runtime_tuning_runtime_marker_log_attempt(int attempt);

#endif
