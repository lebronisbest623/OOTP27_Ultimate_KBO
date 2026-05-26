#include "../../flags_api.h"

#include "../../../localappdata/localappdata_reader.h"
#include "../economic/economic_defaults.h"

#include <stdint.h>

static const char* KBO_FOREIGN_FA_DEMAND_BASELINE_KEYS[9] = {
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

static const char* KBO_ASIAN_QUOTA_FA_DEMAND_BASELINE_KEYS[9] = {
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

#define KBO_ASIAN_QUOTA_SALARY_LIMIT_KEY "asian_quota_salary_limit"

int32_t kbo_clamp_foreign_fa_demand_baseline_value(int32_t value)
{
    if (value < 0) {
        return 0;
    }
    if (value > 20000000) {
        return 20000000;
    }
    return value;
}

int32_t kbo_get_foreign_fa_demand_baseline_value(int index)
{
    if (index < 0 || index >= 9) {
        return 0;
    }
    int value = kbo_economic_default_foreign_fa_demand_baseline(index);
    if (!kbo_read_localappdata_setting_int_value(KBO_FOREIGN_FA_DEMAND_BASELINE_KEYS[index], &value)) {
        value = kbo_economic_default_foreign_fa_demand_baseline(index);
    }
    return kbo_clamp_foreign_fa_demand_baseline_value(value);
}

int32_t kbo_get_asian_quota_fa_demand_baseline_value(int index)
{
    if (index < 0 || index >= 9) {
        return 0;
    }
    int value = kbo_economic_default_asian_quota_fa_demand_baseline(index);
    if (!kbo_read_localappdata_setting_int_value(KBO_ASIAN_QUOTA_FA_DEMAND_BASELINE_KEYS[index], &value)) {
        value = kbo_economic_default_asian_quota_fa_demand_baseline(index);
    }
    return kbo_clamp_foreign_fa_demand_baseline_value(value);
}

int32_t kbo_get_foreign_fa_demand_baseline_value_for_player(int index, int asian_quota)
{
    return asian_quota
        ? kbo_get_asian_quota_fa_demand_baseline_value(index)
        : kbo_get_foreign_fa_demand_baseline_value(index);
}

int kbo_set_foreign_fa_demand_baseline_value(int index, int32_t value)
{
    if (index < 0 || index >= 9) {
        return 0;
    }
    return kbo_write_localappdata_setting_int_value(
        KBO_FOREIGN_FA_DEMAND_BASELINE_KEYS[index],
        kbo_clamp_foreign_fa_demand_baseline_value(value));
}

int kbo_set_asian_quota_fa_demand_baseline_value(int index, int32_t value)
{
    if (index < 0 || index >= 9) {
        return 0;
    }
    return kbo_write_localappdata_setting_int_value(
        KBO_ASIAN_QUOTA_FA_DEMAND_BASELINE_KEYS[index],
        kbo_clamp_foreign_fa_demand_baseline_value(value));
}

int32_t kbo_clamp_asian_quota_salary_limit_value(int32_t value)
{
    if (value < 0) {
        return 0;
    }
    if (value > 20000000) {
        return 20000000;
    }
    return value;
}

int32_t kbo_get_asian_quota_salary_limit(void)
{
    int value = kbo_economic_default_asian_quota_salary_limit();
    if (!kbo_read_localappdata_setting_int_value(KBO_ASIAN_QUOTA_SALARY_LIMIT_KEY, &value)) {
        value = kbo_economic_default_asian_quota_salary_limit();
    }
    return kbo_clamp_asian_quota_salary_limit_value(value);
}

int kbo_set_asian_quota_salary_limit(int32_t value)
{
    return kbo_write_localappdata_setting_int_value(
        KBO_ASIAN_QUOTA_SALARY_LIMIT_KEY,
        kbo_clamp_asian_quota_salary_limit_value(value));
}
