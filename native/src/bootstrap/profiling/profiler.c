#include "profiler.h"

#include <stdio.h>
#include <string.h>

#include "output/profiler_output.h"

#include "../../core/core_flags/localappdata/localappdata_reader.h"
#include "../../core/sync/lock.h"

/* Aggregated runtime profiler for DLL hot paths.
 *
 * Enable with LOCALAPPDATA\OOTP-KBO\kbo_flags.json:
 *   { "enable_kbo_profiler": true }
 *
 * Output:
 *   LOCALAPPDATA\OOTP-KBO\perf\kbo_perf_<pid>.csv
 */

#define KBO_PROFILER_MAX_ZONES 512
#define KBO_PROFILER_NAME_BYTES 96
#define KBO_PROFILER_FLUSH_INTERVAL_MS 1000u

#if defined(_MSC_VER)
#define KBO_PROFILER_THREAD_LOCAL __declspec(thread)
#elif defined(__GNUC__)
#define KBO_PROFILER_THREAD_LOCAL __thread
#else
#define KBO_PROFILER_THREAD_LOCAL
#endif

typedef struct KboProfilerZone {
    char name[KBO_PROFILER_NAME_BYTES];
    volatile LONG64 total_calls;
    volatile LONG64 total_us;
    volatile LONG64 total_slow_calls;
    volatile LONG64 last_calls;
    volatile LONG64 last_us;
    volatile LONG64 last_slow_calls;
    volatile LONG64 max_us;
    volatile LONG64 max_us_since_flush;
} KboProfilerZone;

static KboProfilerZone g_kbo_profiler_zones[KBO_PROFILER_MAX_ZONES];
static volatile LONG g_kbo_profiler_zone_count = 0;
static KboLock g_kbo_profiler_lock = KBO_LOCK_INIT;
static volatile LONG g_kbo_profiler_enabled_cache = -1;
static volatile LONG64 g_kbo_profiler_qpc_freq = 0;
static volatile LONG g_kbo_profiler_last_flush_tick = 0;
static volatile LONG g_kbo_profiler_dropped_zones = 0;
static KBO_PROFILER_THREAD_LOCAL int g_kbo_profiler_thread_depth = 0;

static LONG64 kbo_profiler_qpc_frequency(void)
{
    LONG64 freq = g_kbo_profiler_qpc_freq;
    if (freq > 0) {
        return freq;
    }

    LARGE_INTEGER li;
    if (!QueryPerformanceFrequency(&li) || li.QuadPart <= 0) {
        li.QuadPart = 1000000;
    }
    InterlockedCompareExchange64(&g_kbo_profiler_qpc_freq, li.QuadPart, 0);
    return g_kbo_profiler_qpc_freq;
}

int kbo_profiler_is_enabled(void)
{
    LONG cached = g_kbo_profiler_enabled_cache;
    if (cached != -1) {
        return cached == 1;
    }

    int enabled = 0;
    int configured = 0;
    if (kbo_read_localappdata_json_flag_value("enable_kbo_profiler", &configured)) {
        enabled = configured ? 1 : 0;
    }
    InterlockedCompareExchange(&g_kbo_profiler_enabled_cache, enabled ? 1 : 0, -1);
    return enabled;
}

void kbo_profiler_reset_enabled_cache(void)
{
    InterlockedExchange(&g_kbo_profiler_enabled_cache, -1);
}

static KboProfilerZone* kbo_profiler_get_zone(const char* name)
{
    if (name == NULL || name[0] == '\0') {
        name = "unnamed";
    }

    kbo_lock_enter(&g_kbo_profiler_lock);

    LONG count = g_kbo_profiler_zone_count;
    for (LONG i = 0; i < count; i++) {
        if (strncmp(g_kbo_profiler_zones[i].name, name, KBO_PROFILER_NAME_BYTES) == 0) {
            kbo_lock_leave(&g_kbo_profiler_lock);
            return &g_kbo_profiler_zones[i];
        }
    }

    if (count >= KBO_PROFILER_MAX_ZONES) {
        InterlockedIncrement(&g_kbo_profiler_dropped_zones);
        kbo_lock_leave(&g_kbo_profiler_lock);
        return NULL;
    }

    KboProfilerZone* zone = &g_kbo_profiler_zones[count];
    memset(zone, 0, sizeof(*zone));
    snprintf(zone->name, sizeof(zone->name), "%s", name);
    InterlockedExchange(&g_kbo_profiler_zone_count, count + 1);

    kbo_lock_leave(&g_kbo_profiler_lock);
    return zone;
}

static void kbo_profiler_update_max(volatile LONG64* target, LONG64 value)
{
    LONG64 old_value = *target;
    while (value > old_value
            && InterlockedCompareExchange64(target, value, old_value) != old_value) {
        old_value = *target;
    }
}

static unsigned long long kbo_profiler_elapsed_us_between(
    const LARGE_INTEGER* start,
    const LARGE_INTEGER* stop)
{
    if (start == NULL || stop == NULL) {
        return 0ULL;
    }

    LONG64 freq = kbo_profiler_qpc_frequency();
    LONG64 ticks = stop->QuadPart - start->QuadPart;
    if (ticks < 0) {
        ticks = 0;
    }
    return (unsigned long long)((ticks * 1000000LL) / freq);
}

static void kbo_profiler_flush_if_due(void)
{
    if (!kbo_profiler_is_enabled()) {
        return;
    }

    DWORD now = GetTickCount();
    LONG previous_tick = g_kbo_profiler_last_flush_tick;
    if (previous_tick == 0) {
        if (InterlockedCompareExchange(&g_kbo_profiler_last_flush_tick, (LONG)now, 0) == 0) {
            return;
        }
        previous_tick = g_kbo_profiler_last_flush_tick;
    }

    DWORD window_ms = now - (DWORD)previous_tick;
    if (window_ms < KBO_PROFILER_FLUSH_INTERVAL_MS) {
        return;
    }
    if (InterlockedCompareExchange(&g_kbo_profiler_last_flush_tick, (LONG)now, previous_tick) != previous_tick) {
        return;
    }

    kbo_profiler_write_header_if_needed();

    SYSTEMTIME ts;
    GetLocalTime(&ts);

    char timestamp[64] = {0};
    snprintf(
        timestamp,
        sizeof(timestamp),
        "%04u-%02u-%02u %02u:%02u:%02u.%03u",
        (unsigned)ts.wYear,
        (unsigned)ts.wMonth,
        (unsigned)ts.wDay,
        (unsigned)ts.wHour,
        (unsigned)ts.wMinute,
        (unsigned)ts.wSecond,
        (unsigned)ts.wMilliseconds);

    char buffer[16384] = {0};
    DWORD used = 0;
    LONG count = g_kbo_profiler_zone_count;
    if (count > KBO_PROFILER_MAX_ZONES) {
        count = KBO_PROFILER_MAX_ZONES;
    }

    for (LONG i = 0; i < count; i++) {
        KboProfilerZone* zone = &g_kbo_profiler_zones[i];
        LONG64 total_calls = zone->total_calls;
        LONG64 total_us = zone->total_us;
        LONG64 total_slow_calls = zone->total_slow_calls;
        LONG64 previous_calls = InterlockedExchange64(&zone->last_calls, total_calls);
        LONG64 previous_us = InterlockedExchange64(&zone->last_us, total_us);
        LONG64 delta_calls = total_calls - previous_calls;
        LONG64 delta_us = total_us - previous_us;
        LONG64 max_us = InterlockedExchange64(&zone->max_us_since_flush, 0);
        LONG64 previous_slow_calls = InterlockedExchange64(&zone->last_slow_calls, total_slow_calls);
        LONG64 delta_slow = total_slow_calls - previous_slow_calls;
        LONG64 avg_us = delta_calls > 0 ? delta_us / delta_calls : 0;

        if (delta_calls <= 0 && max_us <= 0) {
            continue;
        }

        char line[512] = {0};
        int len = snprintf(
            line,
            sizeof(line),
            "%s,%lu,%lu,%s,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lu\r\n",
            timestamp,
            GetCurrentProcessId(),
            GetCurrentThreadId(),
            zone->name,
            (long long)total_calls,
            (long long)delta_calls,
            (long long)total_us,
            (long long)delta_us,
            (long long)avg_us,
            (long long)max_us,
            (long long)total_slow_calls,
            (long long)delta_slow,
            (unsigned long)window_ms);
        if (len <= 0) {
            continue;
        }
        if (used + (DWORD)len >= sizeof(buffer)) {
            kbo_profiler_write_bytes(buffer, used);
            used = 0;
        }
        memcpy(buffer + used, line, (size_t)len);
        used += (DWORD)len;
    }

    if (used > 0) {
        kbo_profiler_write_bytes(buffer, used);
    }
}

void kbo_profiler_record_us(const char* name, unsigned long long elapsed_us)
{
    if (!kbo_profiler_is_enabled()) {
        return;
    }

    KboProfilerZone* zone = kbo_profiler_get_zone(name);
    if (zone == NULL) {
        return;
    }

    LONG64 us = elapsed_us > 0x7fffffffffffffffULL
        ? 0x7fffffffffffffffLL
        : (LONG64)elapsed_us;
    InterlockedIncrement64(&zone->total_calls);
    InterlockedExchangeAdd64(&zone->total_us, us);
    if (us >= 1000) {
        InterlockedIncrement64(&zone->total_slow_calls);
    }
    kbo_profiler_update_max(&zone->max_us, us);
    kbo_profiler_update_max(&zone->max_us_since_flush, us);
    if (g_kbo_profiler_thread_depth <= 0) {
        kbo_profiler_flush_if_due();
    }
}

void kbo_hook_profile_begin(KboHookProfileScope* scope)
{
    if (scope == NULL) {
        return;
    }
    memset(scope, 0, sizeof(*scope));
    if (!kbo_profiler_is_enabled()) {
        return;
    }
    if (!QueryPerformanceCounter(&scope->segment_start)) {
        return;
    }
    scope->active = 1;
    scope->segment_active = 1;
    g_kbo_profiler_thread_depth++;
}

void kbo_hook_profile_pause(KboHookProfileScope* scope)
{
    if (scope == NULL || !scope->active || !scope->segment_active) {
        return;
    }

    LARGE_INTEGER stop;
    if (QueryPerformanceCounter(&stop)) {
        scope->overhead_us += kbo_profiler_elapsed_us_between(&scope->segment_start, &stop);
    }
    scope->segment_active = 0;
}

void kbo_hook_profile_resume(KboHookProfileScope* scope)
{
    if (scope == NULL || !scope->active || scope->segment_active) {
        return;
    }
    if (!QueryPerformanceCounter(&scope->segment_start)) {
        return;
    }
    scope->segment_active = 1;
}

void kbo_hook_profile_end(const char* hook_name, KboHookProfileScope* scope)
{
    if (scope == NULL || !scope->active) {
        return;
    }

    kbo_hook_profile_pause(scope);

    char zone_name[KBO_PROFILER_NAME_BYTES] = {0};
    snprintf(
        zone_name,
        sizeof(zone_name),
        "hook.%s.overhead",
        (hook_name != NULL && hook_name[0] != '\0') ? hook_name : "unnamed");
    kbo_profiler_record_us(zone_name, scope->overhead_us);

    scope->active = 0;
    if (g_kbo_profiler_thread_depth > 0) {
        g_kbo_profiler_thread_depth--;
    }
    if (g_kbo_profiler_thread_depth <= 0) {
        kbo_profiler_flush_if_due();
    }
}

int kbo_profiler_begin(LARGE_INTEGER* out_start)
{
    if (out_start == NULL || !kbo_profiler_is_enabled()) {
        return 0;
    }
    if (!QueryPerformanceCounter(out_start)) {
        return 0;
    }
    g_kbo_profiler_thread_depth++;
    return 1;
}

void kbo_profiler_end(const char* name, const LARGE_INTEGER* start)
{
    if (start == NULL || !kbo_profiler_is_enabled()) {
        if (g_kbo_profiler_thread_depth > 0) {
            g_kbo_profiler_thread_depth--;
        }
        return;
    }

    LARGE_INTEGER stop;
    if (!QueryPerformanceCounter(&stop)) {
        if (g_kbo_profiler_thread_depth > 0) {
            g_kbo_profiler_thread_depth--;
        }
        return;
    }

    unsigned long long us = kbo_profiler_elapsed_us_between(start, &stop);
    if (g_kbo_profiler_thread_depth > 0) {
        g_kbo_profiler_thread_depth--;
    }
    kbo_profiler_record_us(name, us);
}
