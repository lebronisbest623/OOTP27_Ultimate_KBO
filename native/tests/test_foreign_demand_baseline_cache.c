#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../src/core/core_flags/api/flags_api.h"

static const char* TEST_FOREIGN_FA_DEMAND_BASELINE_KEYS[9] = {
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

static const char* TEST_ASIAN_QUOTA_FA_DEMAND_BASELINE_KEYS[9] = {
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

static int g_test_values[2][9];
static int g_test_present[2][9];
static int g_read_count = 0;
static int g_write_count = 0;
static int g_watcher_start_count = 0;

static int test_find_setting_key(const char* key, int* out_kind, int* out_index)
{
    for (int i = 0; i < 9; i++) {
        if (strcmp(key, TEST_FOREIGN_FA_DEMAND_BASELINE_KEYS[i]) == 0) {
            if (out_kind != NULL) { *out_kind = 0; }
            if (out_index != NULL) { *out_index = i; }
            return 1;
        }
        if (strcmp(key, TEST_ASIAN_QUOTA_FA_DEMAND_BASELINE_KEYS[i]) == 0) {
            if (out_kind != NULL) { *out_kind = 1; }
            if (out_index != NULL) { *out_index = i; }
            return 1;
        }
    }
    return 0;
}

static void test_seed_settings(void)
{
    memset(g_test_values, 0, sizeof(g_test_values));
    memset(g_test_present, 0, sizeof(g_test_present));
    g_read_count = 0;
    g_write_count = 0;
    for (int i = 0; i < 9; i++) {
        g_test_values[0][i] = 1000000 + i;
        g_test_values[1][i] = 200000 + i;
        g_test_present[0][i] = 1;
        g_test_present[1][i] = 1;
    }
}

static void test_getters_load_once_and_reuse_cache(void)
{
    test_seed_settings();

    assert(kbo_get_foreign_fa_demand_baseline_value(3) == 1000003);
    assert(g_read_count == 18);
    assert(kbo_get_foreign_fa_demand_baseline_value(4) == 1000004);
    assert(kbo_get_asian_quota_fa_demand_baseline_value(7) == 200007);
    assert(g_read_count == 18);
    assert(g_watcher_start_count == 1);
    printf("test_getters_load_once_and_reuse_cache: PASS\n");
}

static void test_dirty_cache_reloads_external_setting_change(void)
{
    g_test_values[0][3] = 1234567;
    kbo_invalidate_foreign_fa_demand_baseline_cache();

    assert(kbo_get_foreign_fa_demand_baseline_value(3) == 1234567);
    assert(g_read_count == 36);
    assert(kbo_get_foreign_fa_demand_baseline_value(3) == 1234567);
    assert(g_read_count == 36);
    printf("test_dirty_cache_reloads_external_setting_change: PASS\n");
}

static void test_setter_updates_valid_cache_without_reload(void)
{
    int before_reads = g_read_count;

    assert(kbo_set_asian_quota_fa_demand_baseline_value(2, 777777));
    assert(g_write_count == 1);
    assert(kbo_get_asian_quota_fa_demand_baseline_value(2) == 777777);
    assert(g_read_count == before_reads);
    printf("test_setter_updates_valid_cache_without_reload: PASS\n");
}

int main(void)
{
    test_getters_load_once_and_reuse_cache();
    test_dirty_cache_reloads_external_setting_change();
    test_setter_updates_valid_cache_without_reload();
    printf("Foreign demand baseline cache tests passed.\n");
    return 0;
}

int32_t kbo_economic_default_foreign_fa_demand_baseline(int index)
{
    return 500000 + index;
}

int32_t kbo_economic_default_asian_quota_fa_demand_baseline(int index)
{
    return 100000 + index;
}

int32_t kbo_economic_default_asian_quota_salary_limit(void)
{
    return 150000;
}

int kbo_read_localappdata_setting_int_value(const char* key, int* out_value)
{
    int kind = 0;
    int index = 0;
    g_read_count++;
    if (out_value != NULL) {
        *out_value = 0;
    }
    if (!test_find_setting_key(key, &kind, &index) || !g_test_present[kind][index]) {
        return 0;
    }
    if (out_value != NULL) {
        *out_value = g_test_values[kind][index];
    }
    return 1;
}

int kbo_write_localappdata_setting_int_value(const char* key, int value)
{
    int kind = 0;
    int index = 0;
    if (!test_find_setting_key(key, &kind, &index)) {
        return 0;
    }
    g_test_values[kind][index] = value;
    g_test_present[kind][index] = 1;
    g_write_count++;
    return 1;
}

int kbo_start_runtime_thread(LPTHREAD_START_ROUTINE start, LPVOID parameter, const char* label)
{
    (void)start;
    (void)parameter;
    (void)label;
    g_watcher_start_count++;
    return 0;
}

int kbo_runtime_threads_should_continue(void)
{
    return 0;
}
