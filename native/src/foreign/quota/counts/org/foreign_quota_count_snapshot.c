#include "foreign_quota_counts_internal.h"
#include "../../../../core/core_flags/api/flags_api.h"
#include <string.h>

LONG kbo_foreign_org_snapshot_note_mutation(void)
{
    LONG generation = InterlockedIncrement(&g_kbo_foreign_org_snapshot_mutation_generation);
    if (generation <= 0) {
        InterlockedExchange(&g_kbo_foreign_org_snapshot_mutation_generation, 1);
        InterlockedExchange(&g_kbo_foreign_org_snapshot_published_generation, 0);
        generation = 1;
    }
    return generation;
}


#ifdef KBO_BENCHMARK_BUILD
void kbo_foreign_org_count_seed_benchmark_snapshot(
    uint32_t team_id,
    uint32_t foreign_count,
    uint32_t asian_count,
    uint32_t non_asian_count)
{
    if (team_id == 0u) {
        return;
    }

    DWORD now = GetTickCount();
    kbo_foreign_org_count_cache_store(team_id, foreign_count, asian_count, non_asian_count, now);
    kbo_lock_enter(&g_kbo_foreign_org_snapshot_lock);
    g_kbo_foreign_org_snapshot_count = 1;
    g_kbo_foreign_org_snapshot[0].team_id = team_id;
    g_kbo_foreign_org_snapshot[0].foreign_count = foreign_count;
    g_kbo_foreign_org_snapshot[0].asian_count = asian_count;
    g_kbo_foreign_org_snapshot[0].non_asian_count = non_asian_count;
    g_kbo_foreign_org_snapshot_tick = now;
    InterlockedExchange(
        &g_kbo_foreign_org_snapshot_published_generation,
        InterlockedCompareExchange(&g_kbo_foreign_org_snapshot_mutation_generation, 0, 0));
    kbo_lock_leave(&g_kbo_foreign_org_snapshot_lock);
}

void kbo_foreign_org_count_refresh_benchmark_snapshot_tick(void)
{
    if (g_kbo_foreign_org_snapshot_tick != 0u) {
        g_kbo_foreign_org_snapshot_tick = GetTickCount();
        InterlockedExchange(
            &g_kbo_foreign_org_snapshot_published_generation,
            InterlockedCompareExchange(&g_kbo_foreign_org_snapshot_mutation_generation, 0, 0));
    }
}
#endif


static int kbo_foreign_org_snapshot_fresh_locked(void)
{
    LONG mutation_generation = InterlockedCompareExchange(
        &g_kbo_foreign_org_snapshot_mutation_generation,
        0,
        0);
    LONG published_generation = InterlockedCompareExchange(
        &g_kbo_foreign_org_snapshot_published_generation,
        0,
        0);
    return g_kbo_foreign_org_snapshot_tick != 0u
        && published_generation == mutation_generation;
}

static void kbo_foreign_org_snapshot_counts_locked(
    uint32_t team_id,
    uint32_t* out_foreign_count,
    uint32_t* out_asian_quota_count,
    uint32_t* out_non_asian_foreign_count)
{
    uint32_t foreign_count = 0u;
    uint32_t asian_count = 0u;
    uint32_t non_asian_count = 0u;
    for (int i = 0; i < g_kbo_foreign_org_snapshot_count; i++) {
        if (g_kbo_foreign_org_snapshot[i].team_id == team_id) {
            foreign_count = g_kbo_foreign_org_snapshot[i].foreign_count;
            asian_count = g_kbo_foreign_org_snapshot[i].asian_count;
            non_asian_count = g_kbo_foreign_org_snapshot[i].non_asian_count;
            break;
        }
    }
    if (out_foreign_count != NULL) { *out_foreign_count = foreign_count; }
    if (out_asian_quota_count != NULL) { *out_asian_quota_count = asian_count; }
    if (out_non_asian_foreign_count != NULL) { *out_non_asian_foreign_count = non_asian_count; }
}

int kbo_foreign_org_snapshot_get(
    uint32_t team_id,
    DWORD now,
    uint32_t* out_foreign_count,
    uint32_t* out_asian_quota_count,
    uint32_t* out_non_asian_foreign_count,
    int* out_rebuilt)
{
    if (out_rebuilt != NULL) {
        *out_rebuilt = 0;
    }
    team_id = kbo_foreign_org_team_id_for_team_id(team_id);
    if (team_id == 0u) {
        return 0;
    }

    uint32_t foreign_count = 0u;
    uint32_t asian_count = 0u;
    uint32_t non_asian_count = 0u;

    kbo_lock_enter(&g_kbo_foreign_org_snapshot_lock);
    if (kbo_foreign_org_snapshot_fresh_locked()) {
        kbo_foreign_org_snapshot_counts_locked(
            team_id,
            &foreign_count,
            &asian_count,
            &non_asian_count);
        kbo_lock_leave(&g_kbo_foreign_org_snapshot_lock);
        if (out_foreign_count != NULL) { *out_foreign_count = foreign_count; }
        if (out_asian_quota_count != NULL) { *out_asian_quota_count = asian_count; }
        if (out_non_asian_foreign_count != NULL) { *out_non_asian_foreign_count = non_asian_count; }
        kbo_foreign_org_count_cache_store(team_id, foreign_count, asian_count, non_asian_count, now);
        return 1;
    }
    kbo_lock_leave(&g_kbo_foreign_org_snapshot_lock);

    if (InterlockedCompareExchange(&g_kbo_foreign_org_snapshot_rebuild_in_progress, 1, 0) != 0) {
        kbo_lock_enter(&g_kbo_foreign_org_snapshot_lock);
        if (!kbo_foreign_org_snapshot_fresh_locked()) {
            kbo_lock_leave(&g_kbo_foreign_org_snapshot_lock);
            return 0;
        }
        kbo_foreign_org_snapshot_counts_locked(
            team_id,
            &foreign_count,
            &asian_count,
            &non_asian_count);
        kbo_lock_leave(&g_kbo_foreign_org_snapshot_lock);
        if (out_foreign_count != NULL) { *out_foreign_count = foreign_count; }
        if (out_asian_quota_count != NULL) { *out_asian_quota_count = asian_count; }
        if (out_non_asian_foreign_count != NULL) { *out_non_asian_foreign_count = non_asian_count; }
        kbo_foreign_org_count_cache_store(team_id, foreign_count, asian_count, non_asian_count, now);
        return 1;
    }

    KboForeignOrgSnapshotEntry rebuilt[KBO_FOREIGN_ORG_SNAPSHOT_MAX_TEAMS];
    int rebuilt_count = 0;
    memset(rebuilt, 0, sizeof(rebuilt));
    LONG rebuild_generation = InterlockedCompareExchange(
        &g_kbo_foreign_org_snapshot_mutation_generation,
        0,
        0);
    int rebuilt_ok = kbo_foreign_org_snapshot_rebuild_into(rebuilt, &rebuilt_count);
    if (rebuilt_ok
            && rebuild_generation != InterlockedCompareExchange(
                &g_kbo_foreign_org_snapshot_mutation_generation,
                0,
                0)) {
        rebuilt_ok = 0;
    }
    kbo_lock_enter(&g_kbo_foreign_org_snapshot_lock);
    if (rebuilt_ok) {
        memcpy(g_kbo_foreign_org_snapshot, rebuilt, sizeof(rebuilt));
        g_kbo_foreign_org_snapshot_count = rebuilt_count;
        g_kbo_foreign_org_snapshot_tick = now != 0u ? now : 1u;
        InterlockedExchange(&g_kbo_foreign_org_snapshot_published_generation, rebuild_generation);
        if (out_rebuilt != NULL) {
            *out_rebuilt = 1;
        }
    } else if (!kbo_foreign_org_snapshot_fresh_locked()) {
        InterlockedExchange(&g_kbo_foreign_org_snapshot_rebuild_in_progress, 0);
        kbo_lock_leave(&g_kbo_foreign_org_snapshot_lock);
        return 0;
    }

    kbo_foreign_org_snapshot_counts_locked(
        team_id,
        &foreign_count,
        &asian_count,
        &non_asian_count);
    kbo_lock_leave(&g_kbo_foreign_org_snapshot_lock);
    InterlockedExchange(&g_kbo_foreign_org_snapshot_rebuild_in_progress, 0);

    if (out_foreign_count != NULL) { *out_foreign_count = foreign_count; }
    if (out_asian_quota_count != NULL) { *out_asian_quota_count = asian_count; }
    if (out_non_asian_foreign_count != NULL) { *out_non_asian_foreign_count = non_asian_count; }
    kbo_foreign_org_count_cache_store(team_id, foreign_count, asian_count, non_asian_count, now);
    return 1;
}
