#include "foreign_quota_counts_internal.h"

uint32_t kbo_foreign_org_count_cache_generation(void)
{
    LONG generation = InterlockedCompareExchange(&g_kbo_foreign_org_count_cache_generation, 0, 0);
    return generation > 0 ? (uint32_t)generation : 0u;
}

static uint32_t kbo_foreign_org_team_generation_slot(uint32_t team_id)
{
    uint32_t h = team_id * 2654435761u;
    h ^= h >> 16;
    return h & (KBO_FOREIGN_ORG_TEAM_GENERATION_CACHE_SIZE - 1u);
}

static uint32_t kbo_foreign_org_count_team_generation(uint32_t team_id)
{
    if (team_id == 0u) {
        return 0u;
    }
    uint32_t slot = kbo_foreign_org_team_generation_slot(team_id);
    if (g_kbo_foreign_org_team_generation_team_ids[slot] != team_id) {
        return 0u;
    }
    LONG generation = InterlockedCompareExchange(&g_kbo_foreign_org_team_generations[slot], 0, 0);
    return generation > 0 ? (uint32_t)generation : 0u;
}

uint32_t kbo_foreign_org_count_cache_generation_for_team(uint32_t team_id)
{
    uint32_t org_team_id = kbo_foreign_org_team_id_for_team_id(team_id);
    uint32_t global_generation = kbo_foreign_org_count_cache_generation();
    uint32_t team_generation = kbo_foreign_org_count_team_generation(org_team_id != 0u ? org_team_id : team_id);
    return global_generation ^ (team_generation * 2246822519u);
}

void kbo_foreign_org_count_bump_team_generation(uint32_t team_id)
{
    if (team_id == 0u) {
        return;
    }
    uint32_t slot = kbo_foreign_org_team_generation_slot(team_id);
    if (g_kbo_foreign_org_team_generation_team_ids[slot] != 0u
            && g_kbo_foreign_org_team_generation_team_ids[slot] != team_id) {
        InterlockedIncrement(&g_kbo_foreign_org_count_cache_generation);
    }
    g_kbo_foreign_org_team_generation_team_ids[slot] = team_id;
    InterlockedIncrement(&g_kbo_foreign_org_team_generations[slot]);
}

void kbo_foreign_org_count_cache_note_roster_mutation(void)
{
    InterlockedIncrement(&g_kbo_foreign_org_count_cache_generation);
    kbo_foreign_org_snapshot_note_mutation();
    for (int i = 0; i < KBO_FOREIGN_ORG_COUNT_CACHE_SIZE; i++) {
        g_kbo_foreign_org_count_cache[i].tick = 0u;
    }
    kbo_lock_enter(&g_kbo_foreign_org_snapshot_lock);
    g_kbo_foreign_org_snapshot_tick = 0u;
    g_kbo_foreign_org_snapshot_count = 0;
    InterlockedExchange(&g_kbo_foreign_org_snapshot_published_generation, 0);
    kbo_lock_leave(&g_kbo_foreign_org_snapshot_lock);
}

static uint32_t kbo_foreign_org_count_cache_slot(uint32_t team_id)
{
    return (team_id ^ (team_id >> 4)) % KBO_FOREIGN_ORG_COUNT_CACHE_SIZE;
}

uint32_t kbo_foreign_org_team_id_for_team_id(uint32_t team_id)
{
    if (team_id == 0u) {
        return 0u;
    }

    uint32_t slot = (team_id * 2654435761u) & (KBO_FOREIGN_ORG_PARENT_CACHE_SIZE - 1u);
    if (g_kbo_foreign_org_parent_cache_team_ids[slot] == team_id) {
        return g_kbo_foreign_org_parent_cache_org_ids[slot];
    }

    uint8_t* team = find_kbo_team_by_numeric_id_any_league(team_id, 1);
    uint32_t org_team_id = team_id;
    if (team != NULL && memory_range_readable(team + OOTP27_KBO_TEAM_PARENT_TEAM_ID_OFFSET, sizeof(uint32_t))) {
        uint32_t parent_team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_PARENT_TEAM_ID_OFFSET);
        if (parent_team_id != 0u) {
            org_team_id = parent_team_id;
        }
    }
    g_kbo_foreign_org_parent_cache_org_ids[slot] = org_team_id;
    g_kbo_foreign_org_parent_cache_team_ids[slot] = team_id;
    return org_team_id;
}

void kbo_foreign_org_count_cache_store(
    uint32_t team_id,
    uint32_t foreign_count,
    uint32_t asian_count,
    uint32_t non_asian_count,
    DWORD now)
{
    if (team_id == 0u) {
        return;
    }
    uint32_t generation = kbo_foreign_org_count_cache_generation_for_team(team_id);
    if (now == 0u) {
        now = 1u;
    }
    KboForeignOrgCountCacheEntry* cached =
        &g_kbo_foreign_org_count_cache[kbo_foreign_org_count_cache_slot(team_id)];
    cached->team_id = team_id;
    cached->foreign_count = foreign_count;
    cached->asian_count = asian_count;
    cached->non_asian_count = non_asian_count;
    cached->generation = generation;
    cached->tick = now;
}

int kbo_foreign_org_count_cache_hit(
    uint32_t team_id,
    uint32_t* out_foreign_count,
    uint32_t* out_asian_quota_count,
    uint32_t* out_non_asian_foreign_count)
{
    if (team_id == 0u) {
        return 0;
    }
    uint32_t generation = kbo_foreign_org_count_cache_generation_for_team(team_id);
    KboForeignOrgCountCacheEntry* cached =
        &g_kbo_foreign_org_count_cache[kbo_foreign_org_count_cache_slot(team_id)];
    if (cached->team_id != team_id
            || cached->tick == 0u
            || cached->generation != generation) {
        return 0;
    }

    if (out_foreign_count != NULL) { *out_foreign_count = cached->foreign_count; }
    if (out_asian_quota_count != NULL) { *out_asian_quota_count = cached->asian_count; }
    if (out_non_asian_foreign_count != NULL) { *out_non_asian_foreign_count = cached->non_asian_count; }
    return 1;
}

void kbo_foreign_org_count_cache_invalidate_team(uint32_t team_id)
{
    if (team_id == 0u) {
        return;
    }
    KboForeignOrgCountCacheEntry* cached =
        &g_kbo_foreign_org_count_cache[kbo_foreign_org_count_cache_slot(team_id)];
    if (cached->team_id == team_id) {
        cached->tick = 0u;
    }
}
