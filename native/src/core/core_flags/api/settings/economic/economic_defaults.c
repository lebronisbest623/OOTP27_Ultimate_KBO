#include "economic_defaults.h"

#include "../../../../policy/core_policy.h"

#define KBO_ECONOMIC_DEFAULTS_FILE "economic_defaults.json"

static const char* KBO_ECONOMIC_FOREIGN_DEMAND_KEYS[9] = {
    "foreign_fa_demand_minimum_salary",
    "foreign_fa_demand_poor_salary",
    "foreign_fa_demand_fair_salary",
    "foreign_fa_demand_below_average_salary",
    "foreign_fa_demand_average_salary",
    "foreign_fa_demand_above_average_salary",
    "foreign_fa_demand_good_salary",
    "foreign_fa_demand_star_salary",
    "foreign_fa_demand_superstar_salary"
};

static const char* KBO_ECONOMIC_ASIAN_QUOTA_DEMAND_KEYS[9] = {
    "asian_quota_fa_demand_minimum_salary",
    "asian_quota_fa_demand_poor_salary",
    "asian_quota_fa_demand_fair_salary",
    "asian_quota_fa_demand_below_average_salary",
    "asian_quota_fa_demand_average_salary",
    "asian_quota_fa_demand_above_average_salary",
    "asian_quota_fa_demand_good_salary",
    "asian_quota_fa_demand_star_salary",
    "asian_quota_fa_demand_superstar_salary"
};

static const char* KBO_ECONOMIC_NON_ASIAN_QUALITY_CAP_KEYS[5] = {
    "foreign_fa_non_asian_starter_quality_cap",
    "foreign_fa_non_asian_bullpen_quality_cap",
    "foreign_fa_non_asian_pitcher_quality_cap",
    "foreign_fa_non_asian_hitter_quality_cap",
    "foreign_fa_non_asian_catcher_quality_cap"
};

static const char* KBO_ECONOMIC_ASIAN_QUALITY_CAP_KEYS[5] = {
    "asian_quota_starter_quality_cap",
    "asian_quota_bullpen_quality_cap",
    "asian_quota_pitcher_quality_cap",
    "asian_quota_hitter_quality_cap",
    "asian_quota_catcher_quality_cap"
};

static int32_t kbo_economic_default_int(const char* key, int32_t fallback)
{
    int value = (int)fallback;
    if (kbo_read_policy_int_value(KBO_ECONOMIC_DEFAULTS_FILE, key, &value)) {
        return (int32_t)value;
    }
    return fallback;
}

static int32_t kbo_economic_default_positive_int(const char* key, int32_t fallback)
{
    int32_t value = kbo_economic_default_int(key, fallback);
    return value > 0 ? value : fallback;
}

static int32_t kbo_economic_default_nonnegative_int(const char* key, int32_t fallback)
{
    int32_t value = kbo_economic_default_int(key, fallback);
    return value >= 0 ? value : fallback;
}

static int32_t kbo_economic_default_indexed(
    int index,
    const char* const* keys,
    int count)
{
    if (index < 0 || index >= count) {
        return 0;
    }
    return kbo_economic_default_int(keys[index], 0);
}

int32_t kbo_economic_default_foreign_fa_demand_baseline(int index)
{
    return kbo_economic_default_indexed(
        index,
        KBO_ECONOMIC_FOREIGN_DEMAND_KEYS,
        9);
}

int32_t kbo_economic_default_asian_quota_fa_demand_baseline(int index)
{
    return kbo_economic_default_indexed(
        index,
        KBO_ECONOMIC_ASIAN_QUOTA_DEMAND_KEYS,
        9);
}

int32_t kbo_economic_default_asian_quota_salary_limit(void)
{
    return kbo_economic_default_int("asian_quota_salary_limit", 0);
}

int32_t kbo_economic_default_non_asian_quality_cap(int index)
{
    return kbo_economic_default_indexed(
        index,
        KBO_ECONOMIC_NON_ASIAN_QUALITY_CAP_KEYS,
        5);
}

int32_t kbo_economic_default_asian_quality_cap(int index)
{
    return kbo_economic_default_indexed(
        index,
        KBO_ECONOMIC_ASIAN_QUALITY_CAP_KEYS,
        5);
}

int kbo_economic_default_foreign_fa_quality_cap_enabled(void)
{
    int value = 0;
    if (kbo_read_policy_flag_value(
            KBO_ECONOMIC_DEFAULTS_FILE,
            "foreign_fa_quality_cap_enabled",
            &value)) {
        return value ? 1 : 0;
    }
    return 0;
}

int kbo_economic_default_intl_established_fa_multiplier(void)
{
    return (int)kbo_economic_default_int("intl_established_fa_multiplier", 0);
}

int kbo_economic_default_asian_games_no_gold_odds_denominator(void)
{
    return (int)kbo_economic_default_int("asian_games_no_gold_odds_denominator", 0);
}

int32_t kbo_economic_default_independent_acquisition_foreign_cash_cost(void)
{
    return kbo_economic_default_positive_int(
        "independent_acquisition_foreign_cash_cost",
        0);
}

int32_t kbo_economic_default_independent_acquisition_domestic_cash_cost(void)
{
    return kbo_economic_default_positive_int(
        "independent_acquisition_domestic_cash_cost",
        0);
}

int32_t kbo_economic_default_independent_acquisition_foreign_seller_transfer_fee(void)
{
    return kbo_economic_default_nonnegative_int(
        "independent_acquisition_foreign_seller_transfer_fee",
        0);
}

int32_t kbo_economic_default_independent_acquisition_domestic_seller_transfer_fee(void)
{
    return kbo_economic_default_nonnegative_int(
        "independent_acquisition_domestic_seller_transfer_fee",
        0);
}
