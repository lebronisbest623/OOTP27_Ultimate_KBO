#include "../internal/state_league_lookup_internal.h"
#include "../../../../core/core_flags/api/flags_api.h"
#include "../../../../core/dates/constants/kbo_date_constants.h"

static volatile LONG g_kbo_league_display_cache_prewarm_running = 0;

void kbo_hub_prewarm_league_display_cache(void)
{
    kbo_hub_refresh_league_cache_context();
    uintptr_t global = g_kbo_league_display_cache_global;
    if (global == 0 || g_kbo_league_display_cache_prewarmed_global == global) {
        return;
    }

    uint32_t league_ids[KBO_LEAGUE_DISPLAY_CACHE_MAX] = {0};
    int league_count = kbo_hub_collect_visible_league_ids(league_ids, KBO_LEAGUE_DISPLAY_CACHE_MAX);
    if (league_count <= 0) {
        return;
    }

    ULONGLONG started_ms = GetTickCount64();
    int found = kbo_scan_named_league_ptrs_for_ids(league_ids, league_count, (SIZE_T)0x00040000u);
    if (found < league_count) {
        found = kbo_scan_named_league_ptrs_for_ids(league_ids, league_count, (SIZE_T)0x00400000u);
    }
    g_kbo_league_display_cache_prewarmed_global = global;
    kbo_log_runtimef(
        "KBO: F2 league cache prewarmed leagues=%d found=%d ms=%llu",
        league_count,
        found,
        (unsigned long long)(GetTickCount64() - started_ms));
}

static DWORD WINAPI kbo_hub_league_display_cache_prewarm_thread(LPVOID parameter)
{
    (void)parameter;
    kbo_hub_prewarm_league_display_cache();
    InterlockedExchange(&g_kbo_league_display_cache_prewarm_running, 0);
    return 0;
}

void kbo_hub_queue_league_display_cache_prewarm(void)
{
    kbo_hub_refresh_league_cache_context();
    uintptr_t global = g_kbo_league_display_cache_global;
    if (global == 0 || g_kbo_league_display_cache_prewarmed_global == global) {
        return;
    }

    if (InterlockedCompareExchange(&g_kbo_league_display_cache_prewarm_running, 1, 0) != 0) {
        return;
    }

    if (!kbo_start_runtime_thread(
            kbo_hub_league_display_cache_prewarm_thread,
            NULL,
            "F2 league display cache prewarm")) {
        InterlockedExchange(&g_kbo_league_display_cache_prewarm_running, 0);
        kbo_log_runtime_line("KBO: F2 league cache prewarm queue failed");
    }
}

uintptr_t kbo_find_league_ptr(uint32_t league_id)
{
    if (league_id == 0) {
        return 0;
    }

    kbo_hub_refresh_league_cache_context();

    ULONGLONG now_ms = GetTickCount64();
    if (kbo_league_ptr_recent_miss(league_id, now_ms)) {
        return 0;
    }

    uintptr_t cached_league_ptr = 0;
    if (kbo_hub_try_get_cached_league_ptr(league_id, &cached_league_ptr)) {
        return cached_league_ptr;
    }

    if (g_kbo_league_ptr_cache_id == league_id && g_kbo_league_ptr_cache_ptr != 0) {
        int cached_score = kbo_hub_named_league_candidate_score(
            g_kbo_league_ptr_cache_ptr,
            league_id,
            NULL,
            0,
            NULL,
            0);
        if (cached_score >= KBO_NAMED_LEAGUE_SCAN_MIN_SCORE) {
            return g_kbo_league_ptr_cache_ptr;
        }
        g_kbo_league_ptr_cache_id  = 0;
        g_kbo_league_ptr_cache_ptr = 0;
    }

    kbo_log_runtimef("KBO: named league ptr scan started id=%u", league_id);

    int score = -1000;
    char name[96] = {0};
    uintptr_t league_ptr = kbo_scan_named_league_ptr(league_id, (SIZE_T)0x00040000u, &score, name, sizeof(name));
    if (score < KBO_NAMED_LEAGUE_SCAN_MIN_SCORE) {
        league_ptr = kbo_scan_named_league_ptr(league_id, (SIZE_T)0x00400000u, &score, name, sizeof(name));
    }

    if (league_ptr != 0 && score >= KBO_NAMED_LEAGUE_SCAN_MIN_SCORE) {
        uint32_t year = *(uint32_t*)(league_ptr + OOTP27_KBO_LEAGUE_YEAR_OFFSET);
        kbo_log_runtimef("KBO: named league ptr FOUND id=%u ptr=%p score=%d year=%u name=%s",
            league_id, (void*)league_ptr, score, year, name);
        g_kbo_league_ptr_cache_id  = league_id;
        g_kbo_league_ptr_cache_ptr = league_ptr;
        kbo_forget_league_ptr_miss(league_id);
        return league_ptr;
    }

    kbo_log_runtimef("KBO: named league ptr scan missed id=%u best_score=%d name=%s",
        league_id, score, name[0] != '\0' ? name : "(none)");
    kbo_remember_league_ptr_miss(league_id, now_ms);
    return 0;
}

void kbo_hub_read_league_name(uintptr_t league_ptr, char* out, size_t out_size)
{
    if (league_ptr == 0 || out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';

    char name[96] = {0};
    if (!copy_ootp_string_object_text((uint8_t*)league_ptr, OOTP27_KBO_LEAGUE_NAME_STRING_OFFSET, name, sizeof(name))) {
        kbo_log_runtimef("KBO: league name read failed ptr=%p offset=0x%x",
            (void*)league_ptr, OOTP27_KBO_LEAGUE_NAME_STRING_OFFSET);
        return;
    }

    int score = kbo_hub_league_name_likeness_score(name);
    if (score < 30) {
        kbo_log_runtimef("KBO: league name rejected ptr=%p score=%d name=%s",
            (void*)league_ptr, score, name);
        return;
    }

    kbo_log_runtimef("KBO: league name read ptr=%p offset=0x%x score=%d name=%s",
        (void*)league_ptr, OOTP27_KBO_LEAGUE_NAME_STRING_OFFSET, score, name);
    snprintf(out, out_size, "%s", name);
}

void kbo_hub_copy_league_display_name_fast(uint32_t league_id, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';

    if (league_id == 0) {
        snprintf(out, out_size, "%s", kbo_hub_text("\xeb\xa6\xac\xea\xb7\xb8 \xec\x97\x86\xec\x9d\x8c", "No league"));
        return;
    }

    if (kbo_hub_try_copy_cached_league_name(league_id, out, out_size)) {
        return;
    }

    snprintf(out, out_size, "%s %u", kbo_hub_text("\xeb\xa6\xac\xea\xb7\xb8", "League"), league_id);
}

void kbo_hub_copy_league_display_name(uint32_t league_id, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';

    if (league_id == 0) {
        snprintf(out, out_size, "%s", kbo_hub_text("\xeb\xa6\xac\xea\xb7\xb8 \xec\x97\x86\xec\x9d\x8c", "No league"));
        return;
    }

    if (kbo_hub_try_copy_cached_league_name(league_id, out, out_size)) {
        return;
    }

    uintptr_t league_ptr = kbo_find_league_ptr(league_id);
    if (league_ptr == 0) {
        snprintf(out, out_size, "%s %u", kbo_hub_text("\xeb\xa6\xac\xea\xb7\xb8", "League"), league_id);
        return;
    }

    char name[96] = {0};
    kbo_hub_read_league_name(league_ptr, name, sizeof(name));

    uint32_t year = 0;
    if (memory_range_readable((void*)(league_ptr + OOTP27_KBO_LEAGUE_YEAR_OFFSET), sizeof(uint32_t))) {
        year = *(uint32_t*)(league_ptr + OOTP27_KBO_LEAGUE_YEAR_OFFSET);
    }

    if (name[0] != '\0') {
        snprintf(out, out_size, "%s", name);
    } else if (year >= KBO_SEASON_YEAR_MIN && year <= KBO_RUNTIME_HISTORICAL_YEAR_MAX) {
        snprintf(out, out_size, "%s %u / %u", kbo_hub_text("\xeb\xa6\xac\xea\xb7\xb8", "League"), league_id, year);
    } else {
        snprintf(out, out_size, "%s %u", kbo_hub_text("\xeb\xa6\xac\xea\xb7\xb8", "League"), league_id);
    }
}
