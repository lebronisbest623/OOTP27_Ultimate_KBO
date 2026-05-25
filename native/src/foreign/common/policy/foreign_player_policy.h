#ifndef KBOFIX_SRC_FOREIGN_PLAYER_POLICY_H_
#define KBOFIX_SRC_FOREIGN_PLAYER_POLICY_H_

#include <stdint.h>

#define KBO_FOREIGN_POLICY_ASIAN_QUOTA_NATION_MAX 16
#define KBO_FOREIGN_POLICY_RESERVE_DEMAND_INDEX_COUNT 9

typedef struct KboForeignPlayerPolicy {
    int32_t value_score_talent_weight;
    int32_t value_score_overall_weight;
    int32_t value_score_ratings_weight;
    int32_t value_score_career_weight;
    int32_t regular_value_threshold;
    int32_t asian_value_threshold;
    int32_t custom_foreign_base_effective_limit;
    int32_t waiver_window_days;
    int32_t foreign_priority_marker_max_age_days;
    int32_t phase_transition_anchor_max_age_days;
    int32_t waiver_retention_years;
    int32_t retention_guard_days;
    int32_t market_age_min;
    int32_t market_age_max;
    int32_t player_id_max;
    int32_t demand_salary_max;
    int32_t no_minor_copy_age_max;
    int32_t injury_replacement_min_days;
    int32_t injury_replacement_decision_margin_min;
    int32_t pending_offer_ttl_days;
    int32_t independent_acquisition_seller_transfer_limit;
    int32_t ai_roster_daily_callup_max_attempts;
    int32_t no_minor_scan_initial_delay_ms;
    int32_t no_minor_scan_warmup_interval_ms;
    int32_t no_minor_scan_warmup_attempts;
    int32_t no_minor_scan_interval_ms;
    int32_t no_minor_scan_max_detail_logs;
    int32_t no_minor_demand_restore_timer_delay_ms;
    int32_t injury_replacement_scan_sleep_ms;
    int32_t retention_margin_divisor;
    int32_t retention_margin_min;
    int32_t retention_margin_max;
    int32_t retention_slot_reserve_days;
    int32_t reserve_demand_discount_percent;
    int32_t reserve_demand_score_min[KBO_FOREIGN_POLICY_RESERVE_DEMAND_INDEX_COUNT];
    uint32_t asian_quota_nation_ids[KBO_FOREIGN_POLICY_ASIAN_QUOTA_NATION_MAX];
    int asian_quota_nation_count;
} KboForeignPlayerPolicy;

const KboForeignPlayerPolicy* kbo_foreign_player_policy(void);
uint32_t kbo_custom_foreign_base_effective_limit(void);
int kbo_foreign_policy_player_id_plausible(uint32_t player_id);
int kbo_foreign_policy_market_age_allowed(uint16_t age);
int kbo_foreign_policy_demand_salary_plausible(int32_t demand);
int kbo_foreign_policy_player_copy_age_plausible(uint16_t age);

#endif
