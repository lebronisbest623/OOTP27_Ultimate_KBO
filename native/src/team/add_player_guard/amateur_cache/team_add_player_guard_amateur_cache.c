#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>

#include "../../../amateur_player_quality/api/amateur_player_quality.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../core/sync/lock.h"
#include "../internal/team_add_player_guard_internal.h"
#include "../../../core/core_flags/keys/runtime_flag_keys.generated.h"

#define KBO_TEAM_ADD_AMATEUR_LEAGUE_CACHE_MAX 512

typedef struct KboTeamAddAmateurLeagueCacheEntry {
    uintptr_t team_ptr;
    uint32_t league_id;
} KboTeamAddAmateurLeagueCacheEntry;

static volatile LONG g_kbo_team_add_amateur_verbose_cached = -1;
static volatile LONG g_kbo_team_add_amateur_verbose_tick = 0;
static volatile LONG g_kbo_team_add_retry_rejected_cached = -1;
static volatile LONG g_kbo_team_add_retry_rejected_tick = 0;
static KboTeamAddAmateurLeagueCacheEntry g_kbo_team_add_amateur_league_cache[KBO_TEAM_ADD_AMATEUR_LEAGUE_CACHE_MAX];
static volatile LONG g_kbo_team_add_amateur_league_cache_count = 0;
static KboLock g_kbo_team_add_amateur_league_cache_lock = KBO_LOCK_INIT;

static int kbo_team_add_cached_bool_flag(
    const char* file_name,
    volatile LONG* cached_value,
    volatile LONG* cached_tick,
    DWORD ttl_ms)
{
    DWORD now = GetTickCount();
    LONG value = *cached_value;
    LONG tick = *cached_tick;
    if (value >= 0 && now - (DWORD)tick < ttl_ms) {
        return value != 0;
    }

    int fresh = read_kbo_localappdata_flag_file(file_name) ? 1 : 0;
    InterlockedExchange(cached_value, fresh);
    InterlockedExchange(cached_tick, (LONG)now);
    return fresh;
}

uint32_t kbo_team_add_cached_amateur_league_id(uint8_t* team)
{
    if (team == NULL) {
        return 0u;
    }

    uintptr_t team_ptr = (uintptr_t)team;
    LONG count = g_kbo_team_add_amateur_league_cache_count;
    if (count > KBO_TEAM_ADD_AMATEUR_LEAGUE_CACHE_MAX) {
        count = KBO_TEAM_ADD_AMATEUR_LEAGUE_CACHE_MAX;
    }
    for (LONG i = 0; i < count; i++) {
        if (g_kbo_team_add_amateur_league_cache[i].team_ptr == team_ptr) {
            return g_kbo_team_add_amateur_league_cache[i].league_id;
        }
    }

    uint32_t league_id = kbo_resolve_amateur_assignment_league_id_for_team_ptr(team);

    kbo_lock_enter(&g_kbo_team_add_amateur_league_cache_lock);
    count = g_kbo_team_add_amateur_league_cache_count;
    if (count > KBO_TEAM_ADD_AMATEUR_LEAGUE_CACHE_MAX) {
        count = KBO_TEAM_ADD_AMATEUR_LEAGUE_CACHE_MAX;
    }
    for (LONG i = 0; i < count; i++) {
        if (g_kbo_team_add_amateur_league_cache[i].team_ptr == team_ptr) {
            uint32_t cached = g_kbo_team_add_amateur_league_cache[i].league_id;
            kbo_lock_leave(&g_kbo_team_add_amateur_league_cache_lock);
            return cached;
        }
    }
    LONG slot = count;
    if (slot >= KBO_TEAM_ADD_AMATEUR_LEAGUE_CACHE_MAX) {
        slot = (LONG)(team_ptr % KBO_TEAM_ADD_AMATEUR_LEAGUE_CACHE_MAX);
    } else {
        InterlockedExchange(&g_kbo_team_add_amateur_league_cache_count, count + 1);
    }
    g_kbo_team_add_amateur_league_cache[slot].team_ptr = team_ptr;
    g_kbo_team_add_amateur_league_cache[slot].league_id = league_id;
    kbo_lock_leave(&g_kbo_team_add_amateur_league_cache_lock);
    return league_id;
}

int kbo_team_add_amateur_verbose_log_enabled_cached(void)
{
    return kbo_team_add_cached_bool_flag(
        KBO_RUNTIME_FLAG_ENABLE_AMATEUR_ASSIGNMENT_VERBOSE_LOG_FILE,
        &g_kbo_team_add_amateur_verbose_cached,
        &g_kbo_team_add_amateur_verbose_tick,
        5000u);
}

int kbo_team_add_retry_rejected_targets_enabled_cached(void)
{
    return kbo_team_add_cached_bool_flag(
        KBO_RUNTIME_FLAG_ENABLE_AMATEUR_ASSIGNMENT_RETRY_REJECTED_TARGETS_FILE,
        &g_kbo_team_add_retry_rejected_cached,
        &g_kbo_team_add_retry_rejected_tick,
        5000u);
}
