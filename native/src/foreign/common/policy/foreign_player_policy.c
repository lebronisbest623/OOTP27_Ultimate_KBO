#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "foreign_player_policy.h"

#include "../../../core/policy/core_policy.h"

#define KBO_FOREIGN_PLAYER_POLICY_FILE "foreign_player_policy.json"

static INIT_ONCE g_kbo_foreign_player_policy_once = INIT_ONCE_STATIC_INIT;
static KboForeignPlayerPolicy g_kbo_foreign_player_policy;

static int32_t kbo_foreign_player_policy_int(const char* key, int32_t fallback, int32_t min_value, int32_t max_value)
{
    return kbo_read_clamped_policy_int(KBO_FOREIGN_PLAYER_POLICY_FILE, key, fallback, min_value, max_value);
}

static void kbo_foreign_policy_add_asian_quota_nation(KboForeignPlayerPolicy* p, uint32_t nation_id)
{
    if (p == NULL || nation_id == 0u || p->asian_quota_nation_count >= KBO_FOREIGN_POLICY_ASIAN_QUOTA_NATION_MAX) {
        return;
    }
    for (int i = 0; i < p->asian_quota_nation_count; i++) {
        if (p->asian_quota_nation_ids[i] == nation_id) {
            return;
        }
    }
    p->asian_quota_nation_ids[p->asian_quota_nation_count++] = nation_id;
}

static BOOL CALLBACK kbo_foreign_player_policy_init_once(PINIT_ONCE init_once, PVOID parameter, PVOID* context)
{
    (void)init_once;
    (void)parameter;
    (void)context;

    KboForeignPlayerPolicy* p = &g_kbo_foreign_player_policy;
    memset(p, 0, sizeof(*p));
    p->value_score_talent_weight = kbo_foreign_player_policy_int("value_score_talent_weight", 55, 0, 10000);
    p->value_score_overall_weight = kbo_foreign_player_policy_int("value_score_overall_weight", 25, 0, 10000);
    p->value_score_ratings_weight = kbo_foreign_player_policy_int("value_score_ratings_weight", 10, 0, 10000);
    p->value_score_career_weight = kbo_foreign_player_policy_int("value_score_career_weight", 10, 0, 10000);
    p->regular_value_threshold = kbo_foreign_player_policy_int("regular_value_threshold", 85000, 0, 250000);
    p->asian_value_threshold = kbo_foreign_player_policy_int("asian_value_threshold", 60000, 0, 250000);
    p->custom_foreign_base_effective_limit = kbo_foreign_player_policy_int("custom_foreign_base_effective_limit", 3, 0, 10);
    p->waiver_window_days = kbo_foreign_player_policy_int("waiver_window_days", 20, 1, 3660);
    p->foreign_priority_marker_max_age_days = kbo_foreign_player_policy_int("foreign_priority_marker_max_age_days", 45, 0, 3660);
    p->phase_transition_anchor_max_age_days = kbo_foreign_player_policy_int("phase_transition_anchor_max_age_days", 70, 0, 3660);
    p->waiver_retention_years = kbo_foreign_player_policy_int("waiver_retention_years", 5, 0, 20);
    p->retention_guard_days = kbo_foreign_player_policy_int("retention_guard_days", 180, 0, 3660);
    p->market_age_min = kbo_foreign_player_policy_int("market_age_min", 16, 0, 80);
    p->market_age_max = kbo_foreign_player_policy_int("market_age_max", 60, 0, 80);
    p->player_id_max = kbo_foreign_player_policy_int("player_id_max", 200000000, 1, 2000000000);
    p->demand_salary_max = kbo_foreign_player_policy_int("demand_salary_max", 1000000000, 1, 2000000000);
    p->no_minor_copy_age_max = kbo_foreign_player_policy_int("no_minor_copy_age_max", 80, 1, 120);
    p->injury_replacement_min_days = kbo_foreign_player_policy_int("injury_replacement_min_days", 42, 0, 3660);
    p->injury_replacement_decision_margin_min = kbo_foreign_player_policy_int("injury_replacement_decision_margin_min", 12000, 0, 10000000);
    p->pending_offer_ttl_days = kbo_foreign_player_policy_int("pending_offer_ttl_days", 45, 0, 3660);
    p->independent_acquisition_seller_transfer_limit = kbo_foreign_player_policy_int("independent_acquisition_seller_transfer_limit", 5, 0, 100);
    p->ai_roster_daily_callup_max_attempts = kbo_foreign_player_policy_int("ai_roster_daily_callup_max_attempts", 24, 0, 10000);
    p->no_minor_scan_initial_delay_ms = kbo_foreign_player_policy_int("no_minor_scan_initial_delay_ms", 1000, 0, 600000);
    p->no_minor_scan_warmup_interval_ms = kbo_foreign_player_policy_int("no_minor_scan_warmup_interval_ms", 2000, 0, 600000);
    p->no_minor_scan_warmup_attempts = kbo_foreign_player_policy_int("no_minor_scan_warmup_attempts", 5, 0, 10000);
    p->no_minor_scan_interval_ms = kbo_foreign_player_policy_int("no_minor_scan_interval_ms", 60000, 100, 3600000);
    p->no_minor_scan_max_detail_logs = kbo_foreign_player_policy_int("no_minor_scan_max_detail_logs", 80, 0, 1000000);
    p->no_minor_demand_restore_timer_delay_ms = kbo_foreign_player_policy_int("no_minor_demand_restore_timer_delay_ms", 750, 0, 600000);
    p->injury_replacement_scan_sleep_ms = kbo_foreign_player_policy_int("injury_replacement_scan_sleep_ms", 7000, 100, 600000);
    p->retention_margin_divisor = kbo_foreign_player_policy_int("retention_margin_divisor", 10, 1, 1000);
    p->retention_margin_min = kbo_foreign_player_policy_int("retention_margin_min", 10000, 0, 10000000);
    p->retention_margin_max = kbo_foreign_player_policy_int("retention_margin_max", 25000, 0, 10000000);
    p->retention_slot_reserve_days = kbo_foreign_player_policy_int("retention_slot_reserve_days", 60, 0, 3660);
    p->reserve_demand_discount_percent = kbo_foreign_player_policy_int("reserve_demand_discount_percent", 85, 0, 1000);

    static const int32_t default_thresholds[KBO_FOREIGN_POLICY_RESERVE_DEMAND_INDEX_COUNT] = {
        0, 25000, 40000, 55000, 70000, 85000, 100000, 120000, 140000
    };
    p->reserve_demand_score_min[0] = 0;
    for (int i = 1; i < KBO_FOREIGN_POLICY_RESERVE_DEMAND_INDEX_COUNT; i++) {
        char key[64] = {0};
        snprintf(key, sizeof(key), "reserve_demand_score_index_%d", i);
        p->reserve_demand_score_min[i] = kbo_foreign_player_policy_int(key, default_thresholds[i], 0, 10000000);
    }
    for (int i = 2; i < KBO_FOREIGN_POLICY_RESERVE_DEMAND_INDEX_COUNT; i++) {
        if (p->reserve_demand_score_min[i] < p->reserve_demand_score_min[i - 1]) {
            p->reserve_demand_score_min[i] = p->reserve_demand_score_min[i - 1];
        }
    }

    if (p->market_age_max < p->market_age_min) {
        p->market_age_max = p->market_age_min;
    }
    if (p->retention_margin_max < p->retention_margin_min) {
        p->retention_margin_max = p->retention_margin_min;
    }

    static const uint32_t default_nations[] = { 98u, 12u, 43u };
    for (int i = 0; i < (int)(sizeof(default_nations) / sizeof(default_nations[0])); i++) {
        char key[64] = {0};
        snprintf(key, sizeof(key), "asian_quota_nation_id_%d", i + 1);
        int32_t value = kbo_read_clamped_policy_int(
            KBO_FOREIGN_PLAYER_POLICY_FILE,
            key,
            (int32_t)default_nations[i],
            1,
            1000000);
        kbo_foreign_policy_add_asian_quota_nation(p, (uint32_t)value);
    }
    for (int i = 4; i <= KBO_FOREIGN_POLICY_ASIAN_QUOTA_NATION_MAX; i++) {
        char key[64] = {0};
        snprintf(key, sizeof(key), "asian_quota_nation_id_%d", i);
        int32_t value = kbo_read_clamped_policy_int(KBO_FOREIGN_PLAYER_POLICY_FILE, key, 0, 1, 1000000);
        if (value > 0) {
            kbo_foreign_policy_add_asian_quota_nation(p, (uint32_t)value);
        }
    }
    return TRUE;
}

const KboForeignPlayerPolicy* kbo_foreign_player_policy(void)
{
    InitOnceExecuteOnce(
        &g_kbo_foreign_player_policy_once,
        kbo_foreign_player_policy_init_once,
        NULL,
        NULL);
    return &g_kbo_foreign_player_policy;
}

uint32_t kbo_custom_foreign_base_effective_limit(void)
{
    const KboForeignPlayerPolicy* policy = kbo_foreign_player_policy();
    return policy->custom_foreign_base_effective_limit > 0
        ? (uint32_t)policy->custom_foreign_base_effective_limit
        : 0u;
}

int kbo_foreign_policy_player_id_plausible(uint32_t player_id)
{
    const KboForeignPlayerPolicy* policy = kbo_foreign_player_policy();
    return player_id > 0u && player_id <= (uint32_t)policy->player_id_max;
}

int kbo_foreign_policy_market_age_allowed(uint16_t age)
{
    const KboForeignPlayerPolicy* policy = kbo_foreign_player_policy();
    return age >= (uint16_t)policy->market_age_min && age <= (uint16_t)policy->market_age_max;
}

int kbo_foreign_policy_demand_salary_plausible(int32_t demand)
{
    const KboForeignPlayerPolicy* policy = kbo_foreign_player_policy();
    return demand > 0 && demand < policy->demand_salary_max;
}

int kbo_foreign_policy_player_copy_age_plausible(uint16_t age)
{
    const KboForeignPlayerPolicy* policy = kbo_foreign_player_policy();
    return age < (uint16_t)policy->no_minor_copy_age_max;
}
