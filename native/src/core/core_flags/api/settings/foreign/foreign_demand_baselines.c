#include "../../flags_api.h"

#include "../../../localappdata/localappdata_reader.h"
#include "../economic/economic_defaults.h"
#include "../../../../product/ootp_product.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

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

enum {
    KBO_FOREIGN_FA_DEMAND_BASELINE_COUNT = 9,
    KBO_FOREIGN_FA_DEMAND_BASELINE_KIND_FOREIGN = 0,
    KBO_FOREIGN_FA_DEMAND_BASELINE_KIND_ASIAN = 1,
    KBO_FOREIGN_FA_DEMAND_BASELINE_KIND_COUNT = 2
};

static SRWLOCK g_kbo_foreign_fa_demand_baseline_cache_lock = SRWLOCK_INIT;
static int32_t g_kbo_foreign_fa_demand_baseline_cache[KBO_FOREIGN_FA_DEMAND_BASELINE_KIND_COUNT][KBO_FOREIGN_FA_DEMAND_BASELINE_COUNT];
static volatile LONG g_kbo_foreign_fa_demand_baseline_cache_valid = 0;
static volatile LONG g_kbo_foreign_fa_demand_baseline_cache_dirty = 1;
static volatile LONG g_kbo_foreign_fa_demand_baseline_watcher_started = 0;

static int32_t kbo_foreign_fa_demand_baseline_default_value(int index, int asian_quota)
{
    return asian_quota
        ? kbo_economic_default_asian_quota_fa_demand_baseline(index)
        : kbo_economic_default_foreign_fa_demand_baseline(index);
}

void kbo_invalidate_foreign_fa_demand_baseline_cache(void)
{
    InterlockedExchange(&g_kbo_foreign_fa_demand_baseline_cache_dirty, 1);
}

static int kbo_foreign_fa_demand_baseline_localappdata_dir_w(WCHAR* out, DWORD out_count)
{
    if (out == NULL || out_count == 0u) {
        return 0;
    }
    out[0] = L'\0';

    WCHAR local_app_data[MAX_PATH] = {0};
    DWORD got = GetEnvironmentVariableW(L"LOCALAPPDATA", local_app_data, (DWORD)(sizeof(local_app_data) / sizeof(local_app_data[0])));
    if (got == 0u || got >= (DWORD)(sizeof(local_app_data) / sizeof(local_app_data[0]))) {
        return 0;
    }

    int written = _snwprintf(out, out_count, L"%ls\\" KBO_PRODUCT_LOCAL_DATA_DIR_W, local_app_data);
    return written > 0 && (DWORD)written < out_count;
}

static DWORD WINAPI kbo_foreign_fa_demand_baseline_settings_watch_thread(void* param)
{
    (void)param;
    WCHAR dir[MAX_PATH] = {0};
    if (!kbo_foreign_fa_demand_baseline_localappdata_dir_w(dir, (DWORD)(sizeof(dir) / sizeof(dir[0])))) {
        return 0;
    }

    HANDLE changes = FindFirstChangeNotificationW(
        dir,
        FALSE,
        FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE);
    if (changes == INVALID_HANDLE_VALUE) {
        return 0;
    }

    while (kbo_runtime_threads_should_continue()) {
        DWORD wait_result = WaitForSingleObject(changes, 1000u);
        if (wait_result == WAIT_OBJECT_0) {
            kbo_invalidate_foreign_fa_demand_baseline_cache();
            if (!FindNextChangeNotification(changes)) {
                break;
            }
        } else if (wait_result == WAIT_FAILED) {
            break;
        }
    }

    FindCloseChangeNotification(changes);
    return 0;
}

static void kbo_foreign_fa_demand_baseline_start_watcher_once(void)
{
    if (InterlockedCompareExchange(&g_kbo_foreign_fa_demand_baseline_watcher_started, 1, 0) == 0) {
        (void)kbo_start_runtime_thread(
            kbo_foreign_fa_demand_baseline_settings_watch_thread,
            NULL,
            "foreign FA demand baseline settings watcher");
    }
}

static int kbo_foreign_fa_demand_baseline_cache_needs_reload(void)
{
    return InterlockedCompareExchange(&g_kbo_foreign_fa_demand_baseline_cache_valid, 0, 0) == 0
        || InterlockedCompareExchange(&g_kbo_foreign_fa_demand_baseline_cache_dirty, 0, 0) != 0;
}

static void kbo_foreign_fa_demand_baseline_reload_cache_locked(void)
{
    for (int i = 0; i < KBO_FOREIGN_FA_DEMAND_BASELINE_COUNT; i++) {
        int value = kbo_foreign_fa_demand_baseline_default_value(i, 0);
        if (!kbo_read_localappdata_setting_int_value(KBO_FOREIGN_FA_DEMAND_BASELINE_KEYS[i], &value)) {
            value = kbo_foreign_fa_demand_baseline_default_value(i, 0);
        }
        g_kbo_foreign_fa_demand_baseline_cache[KBO_FOREIGN_FA_DEMAND_BASELINE_KIND_FOREIGN][i] =
            kbo_clamp_foreign_fa_demand_baseline_value(value);

        value = kbo_foreign_fa_demand_baseline_default_value(i, 1);
        if (!kbo_read_localappdata_setting_int_value(KBO_ASIAN_QUOTA_FA_DEMAND_BASELINE_KEYS[i], &value)) {
            value = kbo_foreign_fa_demand_baseline_default_value(i, 1);
        }
        g_kbo_foreign_fa_demand_baseline_cache[KBO_FOREIGN_FA_DEMAND_BASELINE_KIND_ASIAN][i] =
            kbo_clamp_foreign_fa_demand_baseline_value(value);
    }
    InterlockedExchange(&g_kbo_foreign_fa_demand_baseline_cache_valid, 1);
    InterlockedExchange(&g_kbo_foreign_fa_demand_baseline_cache_dirty, 0);
}

static void kbo_foreign_fa_demand_baseline_ensure_cache(void)
{
    kbo_foreign_fa_demand_baseline_start_watcher_once();
    if (!kbo_foreign_fa_demand_baseline_cache_needs_reload()) {
        return;
    }

    AcquireSRWLockExclusive(&g_kbo_foreign_fa_demand_baseline_cache_lock);
    if (kbo_foreign_fa_demand_baseline_cache_needs_reload()) {
        kbo_foreign_fa_demand_baseline_reload_cache_locked();
    }
    ReleaseSRWLockExclusive(&g_kbo_foreign_fa_demand_baseline_cache_lock);
}

static int32_t kbo_get_foreign_fa_demand_baseline_cached_value(int index, int asian_quota)
{
    if (index < 0 || index >= KBO_FOREIGN_FA_DEMAND_BASELINE_COUNT) {
        return 0;
    }
    kbo_foreign_fa_demand_baseline_ensure_cache();

    int kind = asian_quota
        ? KBO_FOREIGN_FA_DEMAND_BASELINE_KIND_ASIAN
        : KBO_FOREIGN_FA_DEMAND_BASELINE_KIND_FOREIGN;
    AcquireSRWLockShared(&g_kbo_foreign_fa_demand_baseline_cache_lock);
    int32_t value = g_kbo_foreign_fa_demand_baseline_cache[kind][index];
    ReleaseSRWLockShared(&g_kbo_foreign_fa_demand_baseline_cache_lock);
    return value;
}

static void kbo_update_foreign_fa_demand_baseline_cached_value(int index, int asian_quota, int32_t value)
{
    if (index < 0 || index >= KBO_FOREIGN_FA_DEMAND_BASELINE_COUNT
            || InterlockedCompareExchange(&g_kbo_foreign_fa_demand_baseline_cache_valid, 0, 0) == 0) {
        return;
    }

    int kind = asian_quota
        ? KBO_FOREIGN_FA_DEMAND_BASELINE_KIND_ASIAN
        : KBO_FOREIGN_FA_DEMAND_BASELINE_KIND_FOREIGN;
    AcquireSRWLockExclusive(&g_kbo_foreign_fa_demand_baseline_cache_lock);
    g_kbo_foreign_fa_demand_baseline_cache[kind][index] =
        kbo_clamp_foreign_fa_demand_baseline_value(value);
    ReleaseSRWLockExclusive(&g_kbo_foreign_fa_demand_baseline_cache_lock);
}

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
    return kbo_get_foreign_fa_demand_baseline_cached_value(index, 0);
}

int32_t kbo_get_asian_quota_fa_demand_baseline_value(int index)
{
    return kbo_get_foreign_fa_demand_baseline_cached_value(index, 1);
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
    int32_t clamped = kbo_clamp_foreign_fa_demand_baseline_value(value);
    int ok = kbo_write_localappdata_setting_int_value(
        KBO_FOREIGN_FA_DEMAND_BASELINE_KEYS[index],
        clamped);
    if (ok) {
        kbo_update_foreign_fa_demand_baseline_cached_value(index, 0, clamped);
    }
    return ok;
}

int kbo_set_asian_quota_fa_demand_baseline_value(int index, int32_t value)
{
    if (index < 0 || index >= 9) {
        return 0;
    }
    int32_t clamped = kbo_clamp_foreign_fa_demand_baseline_value(value);
    int ok = kbo_write_localappdata_setting_int_value(
        KBO_ASIAN_QUOTA_FA_DEMAND_BASELINE_KEYS[index],
        clamped);
    if (ok) {
        kbo_update_foreign_fa_demand_baseline_cached_value(index, 1, clamped);
    }
    return ok;
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
