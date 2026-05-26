#include "foreign_quota_candidate_limit_cache.h"
#include "../../../rights/query/foreign_waiver_rights_query.h"

typedef struct KboCustomForeignCandidateCacheEntry {
    uint32_t team_id;
    uint32_t player_id;
    uint32_t today;
    uintptr_t player_ptr;
    uint32_t current_team_id;
    uint32_t active_team_id;
    uint32_t original_team_id;
    uint32_t org_count_generation;
    LONG waiver_rights_generation;
    LONG pending_generation;
    uint64_t injury_replacement_fingerprint;
    DWORD tick;
    uint32_t effective_before;
    uint32_t effective_after;
    uint32_t effective_limit;
    uint32_t injured_player_id;
    uint8_t slot_type;
    uint8_t allowed;
    uint8_t valid;
} KboCustomForeignCandidateCacheEntry;

typedef struct KboCustomForeignExtraSlotCacheEntry {
    uint32_t team_id;
    uint32_t player_id;
    uint32_t today;
    uint32_t league_id;
    uintptr_t player_ptr;
    uint64_t injury_replacement_fingerprint;
    DWORD tick;
    uint32_t extra_slots;
    uint32_t injured_player_id;
    uint8_t candidate_asian;
    uint8_t slot_type;
    uint8_t valid;
} KboCustomForeignExtraSlotCacheEntry;

typedef struct KboCustomForeignExtraSlotTeamCacheEntry {
    uint32_t team_id;
    uint32_t today;
    uint32_t league_id;
    uint64_t injury_replacement_fingerprint;
    DWORD tick;
    uint8_t candidate_asian;
    uint8_t has_slot;
    uint8_t valid;
} KboCustomForeignExtraSlotTeamCacheEntry;

enum {
    KBO_CUSTOM_FOREIGN_CANDIDATE_CACHE_SIZE = 8192,
    KBO_CUSTOM_FOREIGN_CANDIDATE_CACHE_WAYS = 4,
    KBO_CUSTOM_FOREIGN_EXTRA_SLOT_CACHE_SIZE = 4096,
    KBO_CUSTOM_FOREIGN_EXTRA_SLOT_CACHE_WAYS = 4,
    KBO_CUSTOM_FOREIGN_EXTRA_SLOT_TEAM_CACHE_SIZE = 512,
    KBO_CUSTOM_FOREIGN_EXTRA_SLOT_TEAM_CACHE_WAYS = 4
};

static KboCustomForeignCandidateCacheEntry g_kbo_custom_foreign_candidate_cache[KBO_CUSTOM_FOREIGN_CANDIDATE_CACHE_SIZE];
static KboCustomForeignExtraSlotCacheEntry
    g_kbo_custom_foreign_extra_slot_cache[KBO_CUSTOM_FOREIGN_EXTRA_SLOT_CACHE_SIZE];
static KboCustomForeignExtraSlotTeamCacheEntry
    g_kbo_custom_foreign_extra_slot_team_cache[KBO_CUSTOM_FOREIGN_EXTRA_SLOT_TEAM_CACHE_SIZE];

static uint32_t kbo_custom_foreign_candidate_cache_slot(uint32_t team_id, uint32_t player_id)
{
    uint32_t h = player_id * 2654435761u;
    h ^= team_id * 2246822519u;
    h ^= h >> 16;
    return h & (KBO_CUSTOM_FOREIGN_CANDIDATE_CACHE_SIZE - KBO_CUSTOM_FOREIGN_CANDIDATE_CACHE_WAYS);
}

static uint32_t kbo_custom_foreign_extra_slot_cache_slot(
    uint32_t team_id,
    uint32_t player_id,
    uint32_t today,
    uint32_t league_id)
{
    uint32_t h = player_id * 2654435761u;
    h ^= team_id * 2246822519u;
    h ^= today * 3266489917u;
    h ^= league_id * 668265263u;
    h ^= h >> 16;
    return h & (KBO_CUSTOM_FOREIGN_EXTRA_SLOT_CACHE_SIZE - KBO_CUSTOM_FOREIGN_EXTRA_SLOT_CACHE_WAYS);
}

static uint32_t kbo_custom_foreign_extra_slot_team_cache_slot(
    uint32_t team_id,
    uint32_t today,
    uint32_t league_id,
    uint8_t candidate_asian)
{
    uint32_t h = team_id * 2246822519u;
    h ^= today * 3266489917u;
    h ^= league_id * 668265263u;
    h ^= (uint32_t)candidate_asian * 374761393u;
    h ^= h >> 16;
    return h & (KBO_CUSTOM_FOREIGN_EXTRA_SLOT_TEAM_CACHE_SIZE - KBO_CUSTOM_FOREIGN_EXTRA_SLOT_TEAM_CACHE_WAYS);
}

void kbo_custom_foreign_candidate_cache_store(
    uint32_t team_id,
    uint8_t* candidate,
    uint32_t candidate_id,
    uint32_t today,
    uint32_t current_team_id,
    uint32_t active_team_id,
    uint32_t original_team_id,
    uint32_t org_count_generation,
    LONG pending_generation,
    uint32_t effective_before,
    uint32_t effective_after,
    uint32_t effective_limit,
    uint8_t slot_type,
    uint32_t injured_player_id,
    int allowed)
{
    uint32_t base_slot = kbo_custom_foreign_candidate_cache_slot(team_id, candidate_id);
    KboCustomForeignCandidateCacheEntry* entry = &g_kbo_custom_foreign_candidate_cache[base_slot];
    for (uint32_t way = 0; way < KBO_CUSTOM_FOREIGN_CANDIDATE_CACHE_WAYS; way++) {
        KboCustomForeignCandidateCacheEntry* candidate_entry =
            &g_kbo_custom_foreign_candidate_cache[base_slot + way];
        if (!candidate_entry->valid) {
            entry = candidate_entry;
            break;
        }
        if (candidate_entry->team_id == team_id
                && candidate_entry->player_id == candidate_id
                && candidate_entry->player_ptr == (uintptr_t)candidate) {
            entry = candidate_entry;
            break;
        }
        if (candidate_entry->tick < entry->tick) {
            entry = candidate_entry;
        }
    }
    entry->valid = 0u;
    entry->team_id = team_id;
    entry->player_id = candidate_id;
    entry->today = today;
    entry->player_ptr = (uintptr_t)candidate;
    entry->current_team_id = current_team_id;
    entry->active_team_id = active_team_id;
    entry->original_team_id = original_team_id;
    entry->org_count_generation = org_count_generation;
    entry->waiver_rights_generation = InterlockedCompareExchange(&g_kbo_foreign_waiver_rights_generation, 0, 0);
    entry->pending_generation = pending_generation;
    entry->injury_replacement_fingerprint = kbo_foreign_injury_replacement_fingerprint();
    entry->effective_before = effective_before;
    entry->effective_after = effective_after;
    entry->effective_limit = effective_limit;
    entry->slot_type = slot_type;
    entry->injured_player_id = injured_player_id;
    entry->allowed = allowed ? 1u : 0u;
    entry->tick = GetTickCount();
    entry->valid = 1u;
}

int kbo_custom_foreign_extra_slot_cache_hit(
    uint32_t team_id,
    uint8_t* candidate,
    uint32_t candidate_id,
    uint32_t today,
    uint32_t league_id,
    uint8_t candidate_asian,
    uint8_t* out_slot_type,
    uint32_t* out_injured_player_id,
    uint32_t* out_extra_slots)
{
    uint64_t injury_replacement_fingerprint = kbo_foreign_injury_replacement_fingerprint();
    uint32_t base_slot = kbo_custom_foreign_extra_slot_cache_slot(team_id, candidate_id, today, league_id);
    KboCustomForeignExtraSlotCacheEntry* entry = NULL;
    for (uint32_t way = 0; way < KBO_CUSTOM_FOREIGN_EXTRA_SLOT_CACHE_WAYS; way++) {
        KboCustomForeignExtraSlotCacheEntry* candidate_entry =
            &g_kbo_custom_foreign_extra_slot_cache[base_slot + way];
        if (candidate_entry->valid
                && candidate_entry->team_id == team_id
                && candidate_entry->player_id == candidate_id
                && candidate_entry->today == today
                && candidate_entry->league_id == league_id
                && candidate_entry->player_ptr == (uintptr_t)candidate
                && candidate_entry->candidate_asian == candidate_asian
                && candidate_entry->injury_replacement_fingerprint == injury_replacement_fingerprint
                && candidate_entry->tick != 0u) {
            entry = candidate_entry;
            break;
        }
    }
    if (entry == NULL) {
        return 0;
    }

    if (out_slot_type != NULL) { *out_slot_type = entry->slot_type; }
    if (out_injured_player_id != NULL) { *out_injured_player_id = entry->injured_player_id; }
    if (out_extra_slots != NULL) { *out_extra_slots = entry->extra_slots; }
    return 1;
}

void kbo_custom_foreign_extra_slot_cache_store(
    uint32_t team_id,
    uint8_t* candidate,
    uint32_t candidate_id,
    uint32_t today,
    uint32_t league_id,
    uint8_t candidate_asian,
    uint32_t extra_slots,
    uint8_t slot_type,
    uint32_t injured_player_id)
{
    if (team_id == 0u || candidate == NULL || candidate_id == 0u) {
        return;
    }

    uint32_t base_slot = kbo_custom_foreign_extra_slot_cache_slot(team_id, candidate_id, today, league_id);
    KboCustomForeignExtraSlotCacheEntry* entry = &g_kbo_custom_foreign_extra_slot_cache[base_slot];
    for (uint32_t way = 0; way < KBO_CUSTOM_FOREIGN_EXTRA_SLOT_CACHE_WAYS; way++) {
        KboCustomForeignExtraSlotCacheEntry* candidate_entry =
            &g_kbo_custom_foreign_extra_slot_cache[base_slot + way];
        if (!candidate_entry->valid) {
            entry = candidate_entry;
            break;
        }
        if (candidate_entry->team_id == team_id
                && candidate_entry->player_id == candidate_id
                && candidate_entry->today == today
                && candidate_entry->league_id == league_id
                && candidate_entry->player_ptr == (uintptr_t)candidate) {
            entry = candidate_entry;
            break;
        }
        if (candidate_entry->tick < entry->tick) {
            entry = candidate_entry;
        }
    }
    entry->valid = 0u;
    entry->team_id = team_id;
    entry->player_id = candidate_id;
    entry->today = today;
    entry->league_id = league_id;
    entry->player_ptr = (uintptr_t)candidate;
    entry->injury_replacement_fingerprint = kbo_foreign_injury_replacement_fingerprint();
    entry->tick = GetTickCount();
    entry->extra_slots = extra_slots;
    entry->candidate_asian = candidate_asian;
    entry->slot_type = slot_type;
    entry->injured_player_id = injured_player_id;
    entry->valid = 1u;
}

int kbo_custom_foreign_extra_slot_team_cache_hit(
    uint32_t team_id,
    uint32_t today,
    uint32_t league_id,
    uint8_t candidate_asian,
    int* out_has_slot)
{
    uint64_t injury_replacement_fingerprint = kbo_foreign_injury_replacement_fingerprint();
    uint32_t base_slot = kbo_custom_foreign_extra_slot_team_cache_slot(
        team_id,
        today,
        league_id,
        candidate_asian);
    KboCustomForeignExtraSlotTeamCacheEntry* entry = NULL;
    for (uint32_t way = 0; way < KBO_CUSTOM_FOREIGN_EXTRA_SLOT_TEAM_CACHE_WAYS; way++) {
        KboCustomForeignExtraSlotTeamCacheEntry* candidate_entry =
            &g_kbo_custom_foreign_extra_slot_team_cache[base_slot + way];
        if (candidate_entry->valid
                && candidate_entry->team_id == team_id
                && candidate_entry->today == today
                && candidate_entry->league_id == league_id
                && candidate_entry->candidate_asian == candidate_asian
                && candidate_entry->injury_replacement_fingerprint == injury_replacement_fingerprint
                && candidate_entry->tick != 0u) {
            entry = candidate_entry;
            break;
        }
    }
    if (entry == NULL) {
        return 0;
    }

    if (out_has_slot != NULL) {
        *out_has_slot = entry->has_slot ? 1 : 0;
    }
    return 1;
}

void kbo_custom_foreign_extra_slot_team_cache_store(
    uint32_t team_id,
    uint32_t today,
    uint32_t league_id,
    uint8_t candidate_asian,
    int has_slot)
{
    if (team_id == 0u || today == 0u || league_id == 0u) {
        return;
    }

    uint32_t base_slot = kbo_custom_foreign_extra_slot_team_cache_slot(
        team_id,
        today,
        league_id,
        candidate_asian);
    KboCustomForeignExtraSlotTeamCacheEntry* entry =
        &g_kbo_custom_foreign_extra_slot_team_cache[base_slot];
    for (uint32_t way = 0; way < KBO_CUSTOM_FOREIGN_EXTRA_SLOT_TEAM_CACHE_WAYS; way++) {
        KboCustomForeignExtraSlotTeamCacheEntry* candidate_entry =
            &g_kbo_custom_foreign_extra_slot_team_cache[base_slot + way];
        if (!candidate_entry->valid) {
            entry = candidate_entry;
            break;
        }
        if (candidate_entry->team_id == team_id
                && candidate_entry->today == today
                && candidate_entry->league_id == league_id
                && candidate_entry->candidate_asian == candidate_asian) {
            entry = candidate_entry;
            break;
        }
        if (candidate_entry->tick < entry->tick) {
            entry = candidate_entry;
        }
    }
    entry->valid = 0u;
    entry->team_id = team_id;
    entry->today = today;
    entry->league_id = league_id;
    entry->injury_replacement_fingerprint = kbo_foreign_injury_replacement_fingerprint();
    entry->tick = GetTickCount();
    entry->candidate_asian = candidate_asian;
    entry->has_slot = has_slot ? 1u : 0u;
    entry->valid = 1u;
}

int kbo_custom_foreign_candidate_cache_hit(
    uint32_t team_id,
    uint8_t* candidate,
    uint32_t candidate_id,
    uint32_t today,
    uint32_t current_team_id,
    uint32_t active_team_id,
    uint32_t original_team_id,
    uint32_t* out_effective_before,
    uint32_t* out_effective_after,
    uint32_t* out_effective_limit,
    uint8_t* out_slot_type,
    uint32_t* out_injured_player_id,
    int* out_allowed)
{
    LONG pending_generation = kbo_custom_foreign_pending_offer_generation_for_team(team_id);
    LONG waiver_rights_generation = InterlockedCompareExchange(&g_kbo_foreign_waiver_rights_generation, 0, 0);
    uint32_t org_count_generation = kbo_foreign_org_count_cache_generation_for_team(team_id);
    uint64_t injury_replacement_fingerprint = kbo_foreign_injury_replacement_fingerprint();
    uint32_t base_slot = kbo_custom_foreign_candidate_cache_slot(team_id, candidate_id);
    KboCustomForeignCandidateCacheEntry* entry = NULL;
    for (uint32_t way = 0; way < KBO_CUSTOM_FOREIGN_CANDIDATE_CACHE_WAYS; way++) {
        KboCustomForeignCandidateCacheEntry* candidate_entry =
            &g_kbo_custom_foreign_candidate_cache[base_slot + way];
        if (candidate_entry->valid
                && candidate_entry->team_id == team_id
                && candidate_entry->player_id == candidate_id
                && candidate_entry->today == today
                && candidate_entry->player_ptr == (uintptr_t)candidate
                && candidate_entry->current_team_id == current_team_id
                && candidate_entry->active_team_id == active_team_id
                && candidate_entry->original_team_id == original_team_id
                && candidate_entry->org_count_generation == org_count_generation
                && candidate_entry->waiver_rights_generation == waiver_rights_generation
                && candidate_entry->pending_generation == pending_generation
                && candidate_entry->injury_replacement_fingerprint == injury_replacement_fingerprint
                && candidate_entry->tick != 0u) {
            entry = candidate_entry;
            break;
        }
    }
    if (entry == NULL) {
        return 0;
    }

    if (out_effective_before != NULL) { *out_effective_before = entry->effective_before; }
    if (out_effective_after != NULL) { *out_effective_after = entry->effective_after; }
    if (out_effective_limit != NULL) { *out_effective_limit = entry->effective_limit; }
    if (out_slot_type != NULL) { *out_slot_type = entry->slot_type; }
    if (out_injured_player_id != NULL) { *out_injured_player_id = entry->injured_player_id; }
    if (out_allowed != NULL) { *out_allowed = entry->allowed ? 1 : 0; }
    return 1;
}

