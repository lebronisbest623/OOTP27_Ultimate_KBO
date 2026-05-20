#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdlib.h>

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../core/logging/core_log.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../team/lookup/team_lookup.h"
#include "../config/foreign_waiver_config.h"
#include "../paths/foreign_waiver_paths.h"
#include "../policy/foreign_player_policy.h"
#include "foreign_waiver_player_eval.h"

typedef struct KboPlayerIdLookupCacheEntry {
    uintptr_t player_vector;
    int32_t player_count;
    uint32_t player_id;
    uintptr_t player_ptr;
    DWORD tick;
    uint8_t valid;
} KboPlayerIdLookupCacheEntry;

enum {
    KBO_PLAYER_ID_LOOKUP_CACHE_SIZE = 4096,
    KBO_PLAYER_ID_LOOKUP_CACHE_WAYS = 4,
    KBO_PLAYER_ID_LOOKUP_CACHE_TTL_MS = 10000u
};

static KboPlayerIdLookupCacheEntry
    g_kbo_player_id_lookup_cache[KBO_PLAYER_ID_LOOKUP_CACHE_SIZE];

static uint32_t kbo_player_id_lookup_cache_slot(uint32_t player_id)
{
    uint32_t h = player_id * 2654435761u;
    h ^= h >> 16;
    return h & (KBO_PLAYER_ID_LOOKUP_CACHE_SIZE - KBO_PLAYER_ID_LOOKUP_CACHE_WAYS);
}

static int kbo_player_id_lookup_cache_hit(
    uint32_t player_id,
    uintptr_t player_vector,
    int32_t player_count,
    uint8_t** out_player)
{
    if (out_player != NULL) { *out_player = NULL; }
    if (player_id == 0u || player_vector == 0 || player_count <= 0) {
        return 0;
    }

    DWORD now = GetTickCount();
    uint32_t base_slot = kbo_player_id_lookup_cache_slot(player_id);
    for (uint32_t way = 0; way < KBO_PLAYER_ID_LOOKUP_CACHE_WAYS; way++) {
        KboPlayerIdLookupCacheEntry* entry = &g_kbo_player_id_lookup_cache[base_slot + way];
        if (!entry->valid
                || entry->player_id != player_id
                || entry->player_vector != player_vector
                || entry->player_count != player_count
                || entry->player_ptr == 0
                || entry->tick == 0u
                || now - entry->tick > KBO_PLAYER_ID_LOOKUP_CACHE_TTL_MS) {
            continue;
        }
        if (!kbo_player_pointer_plausible(entry->player_ptr)) {
            entry->valid = 0u;
            continue;
        }
        uint8_t* player = (uint8_t*)entry->player_ptr;
        if (*(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET) != player_id) {
            entry->valid = 0u;
            continue;
        }
        if (out_player != NULL) {
            *out_player = player;
        }
        return 1;
    }
    return 0;
}

static void kbo_player_id_lookup_cache_store(
    uint32_t player_id,
    uintptr_t player_vector,
    int32_t player_count,
    uint8_t* player)
{
    if (player_id == 0u || player_vector == 0 || player_count <= 0 || player == NULL) {
        return;
    }

    uint32_t base_slot = kbo_player_id_lookup_cache_slot(player_id);
    KboPlayerIdLookupCacheEntry* entry = &g_kbo_player_id_lookup_cache[base_slot];
    for (uint32_t way = 0; way < KBO_PLAYER_ID_LOOKUP_CACHE_WAYS; way++) {
        KboPlayerIdLookupCacheEntry* candidate = &g_kbo_player_id_lookup_cache[base_slot + way];
        if (!candidate->valid || candidate->player_id == player_id) {
            entry = candidate;
            break;
        }
        if (candidate->tick < entry->tick) {
            entry = candidate;
        }
    }

    entry->valid = 0u;
    entry->player_vector = player_vector;
    entry->player_count = player_count;
    entry->player_id = player_id;
    entry->player_ptr = (uintptr_t)player;
    entry->tick = GetTickCount();
    entry->valid = 1u;
}

int16_t kbo_read_player_i16(uint8_t* player, uint32_t offset)
{
    if (player == NULL || offset + sizeof(int16_t) > OOTP27_PLAYER_SCAN_BYTES
            || !memory_range_readable(player + offset, sizeof(int16_t))) {
        return 0;
    }
    return *(int16_t*)(player + offset);
}

int32_t kbo_foreign_waiver_value_score(uint8_t* player)
{
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    int32_t overall = *(int16_t*)(player + OOTP27_PLAYER_OVERALL_VALUE_OFFSET);
    int32_t talent = *(int16_t*)(player + OOTP27_PLAYER_TALENT_VALUE_OFFSET);
    int32_t ratings = *(int16_t*)(player + OOTP27_PLAYER_RATINGS_VALUE_OFFSET);
    int32_t career = *(int16_t*)(player + OOTP27_PLAYER_CAREER_VALUE_OFFSET);

    const KboForeignPlayerPolicy* policy = kbo_foreign_player_policy();
    return talent * policy->value_score_talent_weight
        + overall * policy->value_score_overall_weight
        + ratings * policy->value_score_ratings_weight
        + career * policy->value_score_career_weight;
}

int32_t kbo_get_foreign_waiver_value_threshold(void)
{
    enum { KBO_FOREIGN_VALUE_THRESHOLD_CACHE_MS = 1000u };
    static volatile LONG s_cached_tick = 0;
    static volatile LONG s_cached_threshold = 0;

    DWORD now = GetTickCount();
    LONG cached_tick = InterlockedCompareExchange(&s_cached_tick, 0, 0);
    if (cached_tick != 0
            && (DWORD)(now - (DWORD)cached_tick) <= KBO_FOREIGN_VALUE_THRESHOLD_CACHE_MS) {
        LONG cached_threshold = InterlockedCompareExchange(&s_cached_threshold, 0, 0);
        if (cached_threshold > 0) {
            return (int32_t)cached_threshold;
        }
    }

    uint32_t configured =
        kbo_read_u32_leading_number_from_foreign_policy_file(KBO_FOREIGN_POLICY_VALUE_THRESHOLD_FILE);
    if (configured != 0u && configured <= 250000u) {
        InterlockedExchange(&s_cached_threshold, (LONG)configured);
        InterlockedExchange(&s_cached_tick, (LONG)now);
        return (int32_t)configured;
    }
    int32_t threshold = kbo_foreign_player_policy()->regular_value_threshold;
    InterlockedExchange(&s_cached_threshold, (LONG)threshold);
    InterlockedExchange(&s_cached_tick, (LONG)now);
    return threshold;
}

int32_t kbo_get_foreign_waiver_asian_value_threshold(void)
{
    enum { KBO_FOREIGN_ASIAN_VALUE_THRESHOLD_CACHE_MS = 1000u };
    static volatile LONG s_cached_tick = 0;
    static volatile LONG s_cached_threshold = 0;

    DWORD now = GetTickCount();
    LONG cached_tick = InterlockedCompareExchange(&s_cached_tick, 0, 0);
    if (cached_tick != 0
            && (DWORD)(now - (DWORD)cached_tick) <= KBO_FOREIGN_ASIAN_VALUE_THRESHOLD_CACHE_MS) {
        LONG cached_threshold = InterlockedCompareExchange(&s_cached_threshold, 0, 0);
        if (cached_threshold > 0) {
            return (int32_t)cached_threshold;
        }
    }

    uint32_t configured =
        kbo_read_u32_leading_number_from_foreign_policy_file(KBO_FOREIGN_POLICY_ASIAN_VALUE_THRESHOLD_FILE);
    if (configured != 0u && configured <= 250000u) {
        InterlockedExchange(&s_cached_threshold, (LONG)configured);
        InterlockedExchange(&s_cached_tick, (LONG)now);
        return (int32_t)configured;
    }
    int32_t threshold = kbo_foreign_player_policy()->asian_value_threshold;
    InterlockedExchange(&s_cached_threshold, (LONG)threshold);
    InterlockedExchange(&s_cached_tick, (LONG)now);
    return threshold;
}

int32_t kbo_get_foreign_waiver_value_threshold_for_player(uint8_t* player)
{
    if (player != NULL && kbo_player_is_asian_quota_slot_candidate(player)) {
        return kbo_get_foreign_waiver_asian_value_threshold();
    }
    return kbo_get_foreign_waiver_value_threshold();
}

uint8_t* kbo_find_player_by_id(uint32_t player_id, uint32_t* out_current_team_id, uint32_t* out_current_league_id)
{
    if (player_id == 0) {
        return NULL;
    }

    if (out_current_team_id != NULL) { *out_current_team_id = 0; }
    if (out_current_league_id != NULL) { *out_current_league_id = 0; }

    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        return NULL;
    }

    uint8_t* cached_player = NULL;
    if (kbo_player_id_lookup_cache_hit(player_id, player_vector, player_count, &cached_player)) {
        if (out_current_team_id != NULL) {
            *out_current_team_id = *(uint32_t*)(cached_player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        }
        if (out_current_league_id != NULL) {
            *out_current_league_id = *(uint32_t*)(cached_player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
        }
        return cached_player;
    }

    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }

        uint8_t* player = (uint8_t*)player_ptr;
        uint32_t candidate_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        if (candidate_id != player_id) {
            continue;
        }

        if (out_current_team_id != NULL) {
            *out_current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        }
        if (out_current_league_id != NULL) {
            *out_current_league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
        }
        kbo_player_id_lookup_cache_store(player_id, player_vector, player_count, player);
        return player;
    }
    return NULL;
}

int kbo_player_is_foreign_for_kbo_rights(uint8_t* player)
{
    if (player == NULL
            || !memory_range_readable(
                player,
                OOTP27_PLAYER_NATION_ID_OFFSET + sizeof(uint32_t))) {
        return 0;
    }

    uint32_t nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
    return nation_id != 0u && nation_id != OOTP27_KBO_KOREA_NATION_ID;
}

static int32_t kbo_player_asian_quota_salary(uint8_t* player)
{
    if (player == NULL
            || !memory_range_readable(player + OOTP27_PLAYER_CONTRACT_START_YEAR_OFFSET, sizeof(int32_t))
            || !memory_range_readable(player + OOTP27_PLAYER_CONTRACT_SALARY_Y1_OFFSET, OOTP27_PLAYER_CONTRACT_SALARY_YEARS * sizeof(int32_t))) {
        return 0;
    }

    int32_t y1_salary = *(int32_t*)(player + OOTP27_PLAYER_CONTRACT_SALARY_Y1_OFFSET);
    if (y1_salary > 0) {
        return y1_salary;
    }

    if (memory_range_readable(player + OOTP27_PLAYER_FA_DEMAND_SALARY_OFFSET, sizeof(int32_t))) {
        int32_t demand = *(int32_t*)(player + OOTP27_PLAYER_FA_DEMAND_SALARY_OFFSET);
        if (demand > 0) {
            return demand;
        }
    }

    for (uint32_t i = 1; i < OOTP27_PLAYER_CONTRACT_SALARY_YEARS; i++) {
        int32_t salary = *(int32_t*)(player + OOTP27_PLAYER_CONTRACT_SALARY_Y1_OFFSET + (i * sizeof(int32_t)));
        if (salary > 0) {
            return salary;
        }
    }

    return 0;
}

#define KBO_FOREIGN_INJURY_SLOT_REGULAR         1
#define KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA     2

static int32_t kbo_cached_asian_quota_salary_limit(void)
{
    enum { KBO_ASIAN_QUOTA_SALARY_LIMIT_CACHE_MS = 1000u };
    static volatile LONG s_cached_tick = 0;
    static volatile LONG s_cached_limit = 0;

    DWORD now = GetTickCount();
    LONG cached_tick = InterlockedCompareExchange(&s_cached_tick, 0, 0);
    if (cached_tick != 0
            && (DWORD)(now - (DWORD)cached_tick) <= KBO_ASIAN_QUOTA_SALARY_LIMIT_CACHE_MS) {
        LONG cached_limit = InterlockedCompareExchange(&s_cached_limit, 0, 0);
        if (cached_limit > 0) {
            return (int32_t)cached_limit;
        }
    }

    int32_t limit = kbo_get_asian_quota_salary_limit();
    InterlockedExchange(&s_cached_limit, (LONG)limit);
    InterlockedExchange(&s_cached_tick, (LONG)now);
    return limit;
}

int kbo_player_is_asian_quota_candidate(uint8_t* player)
{
    if (!kbo_player_is_asian_quota_slot_candidate(player)) {
        return 0;
    }
    int32_t salary = kbo_player_asian_quota_salary(player);
    return salary > 0 && salary <= kbo_cached_asian_quota_salary_limit();
}

int kbo_player_is_asian_quota_slot_candidate(uint8_t* player)
{
    if (player == NULL
            || !memory_range_readable(
                player,
                OOTP27_PLAYER_NATION_ID_OFFSET + sizeof(uint32_t))) {
        return 0;
    }
    return kbo_nation_is_asian_quota_candidate(
        *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET));
}

