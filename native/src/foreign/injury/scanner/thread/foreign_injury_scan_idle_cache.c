#include "../foreign_injury_scanner_internal.h"

#include <string.h>

static volatile LONG g_kbo_foreign_injury_idle_scan_yyyymmdd = 0;
static volatile LONG g_kbo_foreign_injury_idle_scan_tick_ms = 0;
static volatile LONG g_kbo_foreign_injury_existing_idle_yyyymmdd = 0;
static volatile LONG64 g_kbo_foreign_injury_existing_idle_fingerprint = 0;

#define KBO_FOREIGN_INJURY_IDLE_THREAD_SCAN_CACHE_MS 15000u

int kbo_foreign_injury_replacement_scan_source_is_read_only(const char* source)
{
    return source != NULL && (strcmp(source, "foreign_policy_webview") == 0
        || strcmp(source, "foreign_policy_text") == 0 || strcmp(source, "hotkey_text") == 0
        || strcmp(source, "foreign_slot_cache") == 0);
}

static int kbo_foreign_injury_replacement_scan_source_uses_idle_cache(const char* source)
{
    (void)source;
    return 0;
}

int kbo_foreign_injury_same_date_idle_scan_cached(uint32_t today, const char* source)
{
    if (!kbo_foreign_injury_replacement_scan_source_uses_idle_cache(source) || today == 0u) {
        return 0;
    }
    LONG cached_date = InterlockedCompareExchange(&g_kbo_foreign_injury_idle_scan_yyyymmdd, 0, 0);
    if ((uint32_t)cached_date != today) {
        return 0;
    }
    DWORD last_tick = (DWORD)InterlockedCompareExchange(&g_kbo_foreign_injury_idle_scan_tick_ms, 0, 0);
    if (last_tick == 0u) {
        return 0;
    }
    return (DWORD)(GetTickCount() - last_tick) <= KBO_FOREIGN_INJURY_IDLE_THREAD_SCAN_CACHE_MS;
}

void kbo_foreign_injury_note_same_date_idle_scan(uint32_t today, const char* source, int idle)
{
    if (!kbo_foreign_injury_replacement_scan_source_uses_idle_cache(source)) {
        return;
    }
    if (!idle || today == 0u) {
        InterlockedExchange(&g_kbo_foreign_injury_idle_scan_yyyymmdd, 0);
        InterlockedExchange(&g_kbo_foreign_injury_idle_scan_tick_ms, 0);
        return;
    }
    InterlockedExchange(&g_kbo_foreign_injury_idle_scan_yyyymmdd, (LONG)today);
    InterlockedExchange(&g_kbo_foreign_injury_idle_scan_tick_ms, (LONG)GetTickCount());
}

int kbo_foreign_injury_same_date_existing_idle_cached(uint32_t today, uint64_t fingerprint)
{
    if (today == 0u || fingerprint == 0u) {
        return 0;
    }
    LONG cached_date = InterlockedCompareExchange(&g_kbo_foreign_injury_existing_idle_yyyymmdd, 0, 0);
    if ((uint32_t)cached_date != today) {
        return 0;
    }
    uint64_t cached_fingerprint =
        (uint64_t)InterlockedCompareExchange64(&g_kbo_foreign_injury_existing_idle_fingerprint, 0, 0);
    return cached_fingerprint == fingerprint;
}

void kbo_foreign_injury_note_same_date_existing_idle(uint32_t today, uint64_t fingerprint, int idle)
{
    if (!idle || today == 0u || fingerprint == 0u) {
        InterlockedExchange(&g_kbo_foreign_injury_existing_idle_yyyymmdd, 0);
        InterlockedExchange64(&g_kbo_foreign_injury_existing_idle_fingerprint, 0);
        return;
    }
    InterlockedExchange64(&g_kbo_foreign_injury_existing_idle_fingerprint, (LONG64)fingerprint);
    InterlockedExchange(&g_kbo_foreign_injury_existing_idle_yyyymmdd, (LONG)today);
}

