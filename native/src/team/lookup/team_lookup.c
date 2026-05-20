#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>

#include "team_lookup.h"
#include "../names/team_string.h"
#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../runtime_memory/runtime_memory.h"

int kbo_player_pointer_plausible(uintptr_t player_ptr)
{
    if (player_ptr == 0 || !memory_range_readable((void*)player_ptr, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    uint8_t* player   = (uint8_t*)player_ptr;
    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    uint16_t age       = *(uint16_t*)(player + OOTP27_PLAYER_AGE_OFFSET);
    return player_id > 0 && player_id < 200000000u && age < 80;
}

int kbo_player_is_draft_pool_candidate(uint8_t* player)
{
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_DRAFT_ELIGIBLE_OFFSET + sizeof(uint8_t))) {
        return 0;
    }
    return player[OOTP27_PLAYER_DRAFT_ELIGIBLE_OFFSET] != 0u;
}

int kbo_player_has_nonzero_evaluation(uint8_t* player)
{
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_CAREER_VALUE_OFFSET + sizeof(int16_t))) {
        return 0;
    }

    int16_t overall = *(int16_t*)(player + OOTP27_PLAYER_OVERALL_VALUE_OFFSET);
    int16_t talent = *(int16_t*)(player + OOTP27_PLAYER_TALENT_VALUE_OFFSET);
    int16_t ratings = *(int16_t*)(player + OOTP27_PLAYER_RATINGS_VALUE_OFFSET);
    int16_t career = *(int16_t*)(player + OOTP27_PLAYER_CAREER_VALUE_OFFSET);
    return overall > 0 || talent > 0 || ratings > 0 || career > 0;
}

int find_kbo_global_player_vector(uintptr_t* out_vector, int32_t* out_count, uint32_t* out_offset)
{
    if (out_vector != NULL) { *out_vector = 0; }
    if (out_count  != NULL) { *out_count  = 0; }
    if (out_offset != NULL) { *out_offset = 0; }

    uintptr_t global = get_ootp_global_database();
    if (global == 0) {
        return 0;
    }

    typedef struct KboPlayerVectorLookupCache {
        uintptr_t global;
        uintptr_t vector;
        int32_t count;
        uint32_t offset;
        DWORD tick;
    } KboPlayerVectorLookupCache;
    static KboPlayerVectorLookupCache cache = {0};

    DWORD now = GetTickCount();
    if (cache.global == global
            && cache.vector != 0
            && cache.count > 0
            && cache.tick != 0u
            && now - cache.tick <= 1000u
            && memory_range_readable((void*)cache.vector, (SIZE_T)cache.count * sizeof(uintptr_t))) {
        if (out_vector != NULL) { *out_vector = cache.vector; }
        if (out_count  != NULL) { *out_count  = cache.count;  }
        if (out_offset != NULL) { *out_offset = cache.offset; }
        return 1;
    }

    int best_matches     = 0;
    uintptr_t best_vector = 0;
    int32_t   best_count  = 0;
    uint32_t  best_offset = 0;

    for (uint32_t offset = 0x20u; offset <= 0x500u; offset += 8u) {
        if (!memory_range_readable((void*)(global + offset), 0x10)) {
            continue;
        }

        uintptr_t vector = *(uintptr_t*)(global + offset);
        int32_t   count  = *(int32_t*)(global + offset + OOTP27_GLOBAL_VECTOR_COUNT_DELTA);
        if (vector == 0 || count <= 0 || count > 200000) {
            continue;
        }
        if (!memory_range_readable((void*)vector, (SIZE_T)count * sizeof(uintptr_t))) {
            continue;
        }

        int matches = 0;
        int sample_count = count < 2000 ? count : 2000;
        for (int32_t i = 0; i < sample_count; i++) {
            uintptr_t player_ptr = *(uintptr_t*)(vector + ((uintptr_t)i * sizeof(uintptr_t)));
            if (kbo_player_pointer_plausible(player_ptr)) {
                matches++;
            }
        }

        if (matches > best_matches) {
            best_matches = matches;
            best_vector  = vector;
            best_count   = count;
            best_offset  = offset;
        }
    }

    if (best_matches <= 0 || best_vector == 0 || best_count <= 0) {
        return 0;
    }

    if (out_vector != NULL) { *out_vector = best_vector; }
    if (out_count  != NULL) { *out_count  = best_count;  }
    if (out_offset != NULL) { *out_offset = best_offset; }
    cache.global = global;
    cache.vector = best_vector;
    cache.count = best_count;
    cache.offset = best_offset;
    cache.tick = now;
    return 1;
}

uint8_t* find_kbo_team_by_csv_id_any_league(const char* team_id, int allow_deleted)
{
    if (team_id == NULL || team_id[0] == '\0') {
        return NULL;
    }

    uintptr_t global = get_ootp_global_database();
    if (global == 0) {
        return NULL;
    }

    uintptr_t team_vector = *(uintptr_t*)(global + OOTP27_KBO_TEAM_VECTOR_OFFSET);
    int32_t   team_count  = *(int32_t*)(global  + OOTP27_KBO_TEAM_COUNT_OFFSET);
    if (team_vector == 0 || team_count <= 0 || team_count > KBO_RUNTIME_MAX_TEAM_VECTOR_COUNT
            || !memory_range_readable((void*)team_vector, (SIZE_T)team_count * sizeof(uintptr_t))) {
        return NULL;
    }

    for (int32_t i = 0; i < team_count; i++) {
        uintptr_t team_ptr = *(uintptr_t*)(team_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (team_ptr == 0 || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            continue;
        }

        uint8_t* team = (uint8_t*)team_ptr;
        if (!allow_deleted && team[OOTP27_KBO_TEAM_DELETED_OFFSET] != 0) {
            continue;
        }
        if (team_has_ootp_string_text(team, team_id)) {
            return team;
        }
    }

    return NULL;
}

uint8_t* find_kbo_team_by_numeric_id_any_league(uint32_t team_id, int allow_deleted)
{
    if (team_id == 0) {
        return NULL;
    }

    uintptr_t global = get_ootp_global_database();
    if (global == 0) {
        return NULL;
    }

    enum { KBO_TEAM_NUMERIC_LOOKUP_CACHE_SIZE = 256 };
    typedef struct KboTeamNumericLookupCacheEntry {
        uintptr_t global;
        uint32_t team_id;
        int allow_deleted;
        uintptr_t team_ptr;
        DWORD tick;
    } KboTeamNumericLookupCacheEntry;
    static KboTeamNumericLookupCacheEntry lookup_cache[KBO_TEAM_NUMERIC_LOOKUP_CACHE_SIZE] = {{0}};

    DWORD now = GetTickCount();
    uint32_t slot_index = (team_id ^ (team_id >> 8) ^ (allow_deleted ? 0x9e3779b9u : 0u)) % KBO_TEAM_NUMERIC_LOOKUP_CACHE_SIZE;
    KboTeamNumericLookupCacheEntry* cached = &lookup_cache[slot_index];
    if (cached->global == global
            && cached->team_id == team_id
            && cached->allow_deleted == allow_deleted
            && cached->team_ptr != 0
            && cached->tick != 0u
            && now - cached->tick <= 1000u
            && memory_range_readable((void*)cached->team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return (uint8_t*)cached->team_ptr;
    }

    uintptr_t team_vector = *(uintptr_t*)(global + OOTP27_KBO_TEAM_VECTOR_OFFSET);
    int32_t   team_count  = *(int32_t*)(global  + OOTP27_KBO_TEAM_COUNT_OFFSET);
    if (team_vector == 0 || team_count <= 0 || team_count > KBO_RUNTIME_MAX_TEAM_VECTOR_COUNT
            || !memory_range_readable((void*)team_vector, (SIZE_T)team_count * sizeof(uintptr_t))) {
        return NULL;
    }

    for (int32_t i = 0; i < team_count; i++) {
        uintptr_t team_ptr = *(uintptr_t*)(team_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (team_ptr == 0 || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            continue;
        }

        uint8_t* team = (uint8_t*)team_ptr;
        if (!allow_deleted && team[OOTP27_KBO_TEAM_DELETED_OFFSET] != 0) {
            continue;
        }
        if (*(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET) == team_id) {
            cached->global = global;
            cached->team_id = team_id;
            cached->allow_deleted = allow_deleted;
            cached->team_ptr = team_ptr;
            cached->tick = now;
            return team;
        }
    }

    cached->global = global;
    cached->team_id = team_id;
    cached->allow_deleted = allow_deleted;
    cached->team_ptr = 0;
    cached->tick = now;
    return NULL;
}
