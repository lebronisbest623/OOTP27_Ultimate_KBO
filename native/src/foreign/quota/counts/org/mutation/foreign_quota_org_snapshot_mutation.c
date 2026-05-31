#include "../foreign_quota_counts_internal.h"

static KboForeignOrgSnapshotEntry* kbo_foreign_org_snapshot_entry(uint32_t team_id)
{
    for (int i = 0; i < g_kbo_foreign_org_snapshot_count; i++) {
        if (g_kbo_foreign_org_snapshot[i].team_id == team_id) {
            return &g_kbo_foreign_org_snapshot[i];
        }
    }
    if (g_kbo_foreign_org_snapshot_count >= KBO_FOREIGN_ORG_SNAPSHOT_MAX_TEAMS) {
        return NULL;
    }
    KboForeignOrgSnapshotEntry* entry = &g_kbo_foreign_org_snapshot[g_kbo_foreign_org_snapshot_count++];
    entry->team_id = team_id;
    entry->foreign_count = 0u;
    entry->asian_count = 0u;
    entry->non_asian_count = 0u;
    return entry;
}

static int kbo_foreign_org_list_contains(const uint32_t* team_ids, int team_count, uint32_t team_id)
{
    if (team_id == 0u) {
        return 0;
    }
    for (int i = 0; i < team_count; i++) {
        if (team_ids[i] == team_id) {
            return 1;
        }
    }
    return 0;
}

static void kbo_foreign_org_list_add(uint32_t* team_ids, int* team_count, uint32_t team_id)
{
    if (team_id == 0u || kbo_foreign_org_list_contains(team_ids, *team_count, team_id)) {
        return;
    }
    if (*team_count < 3) {
        team_ids[(*team_count)++] = team_id;
    }
}

static void kbo_foreign_org_snapshot_adjust_player(uint32_t team_id, uint32_t player_id, int asian_quota, int delta)
{
    if (team_id == 0u || player_id == 0u || delta == 0) {
        return;
    }
    if (kbo_foreign_injury_player_excluded_from_foreign_count(team_id, player_id)) {
        return;
    }
    KboForeignOrgSnapshotEntry* entry = delta > 0 ? kbo_foreign_org_snapshot_entry(team_id) : NULL;
    if (delta < 0) {
        for (int i = 0; i < g_kbo_foreign_org_snapshot_count; i++) {
            if (g_kbo_foreign_org_snapshot[i].team_id == team_id) {
                entry = &g_kbo_foreign_org_snapshot[i];
                break;
            }
        }
    }
    if (entry == NULL) {
        return;
    }
    if (delta > 0) {
        entry->foreign_count++;
        if (asian_quota) {
            entry->asian_count++;
        } else {
            entry->non_asian_count++;
        }
        return;
    }
    if (entry->foreign_count > 0u) {
        entry->foreign_count--;
    }
    if (asian_quota) {
        if (entry->asian_count > 0u) {
            entry->asian_count--;
        }
    } else if (entry->non_asian_count > 0u) {
        entry->non_asian_count--;
    }
}

static void kbo_foreign_org_snapshot_cache_team_locked(uint32_t team_id, DWORD now)
{
    if (team_id == 0u) {
        return;
    }

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
    kbo_foreign_org_count_cache_store(
        team_id,
        foreign_count,
        asian_count,
        non_asian_count,
        now);
}

static void kbo_foreign_org_snapshot_cache_team_list_locked(
    const uint32_t* team_ids,
    int team_count,
    DWORD now)
{
    if (team_ids == NULL || team_count <= 0) {
        return;
    }
    for (int i = 0; i < team_count; i++) {
        kbo_foreign_org_snapshot_cache_team_locked(team_ids[i], now);
    }
}

static void kbo_foreign_org_assignment_orgs(
    uint32_t current_team_id,
    uint32_t active_team_id,
    uint32_t loan_team_id,
    uint32_t* orgs,
    int* org_count)
{
    if (orgs == NULL || org_count == NULL) {
        return;
    }
    kbo_foreign_org_list_add(orgs, org_count, kbo_foreign_org_team_id_for_team_id(current_team_id));
    kbo_foreign_org_list_add(orgs, org_count, kbo_foreign_org_team_id_for_team_id(active_team_id));
    kbo_foreign_org_list_add(orgs, org_count, kbo_foreign_org_team_id_for_team_id(loan_team_id));
}

static void kbo_foreign_org_invalidate_org_list(const uint32_t* orgs, int org_count)
{
    if (orgs == NULL) {
        return;
    }
    for (int i = 0; i < org_count; i++) {
        kbo_foreign_org_count_cache_invalidate_team(orgs[i]);
        kbo_foreign_org_count_bump_team_generation(orgs[i]);
    }
}

void kbo_foreign_org_count_cache_note_player_assignment_change(
    uint32_t before_current_team_id,
    uint32_t before_active_team_id,
    uint32_t before_loan_team_id,
    uint32_t after_current_team_id,
    uint32_t after_active_team_id,
    uint32_t after_loan_team_id,
    uint32_t player_id,
    int asian_quota)
{
    uint32_t before_orgs[3] = {0u, 0u, 0u};
    uint32_t after_orgs[3] = {0u, 0u, 0u};
    int before_count = 0;
    int after_count = 0;

    kbo_foreign_org_assignment_orgs(
        before_current_team_id,
        before_active_team_id,
        before_loan_team_id,
        before_orgs,
        &before_count);
    kbo_foreign_org_assignment_orgs(
        after_current_team_id,
        after_active_team_id,
        after_loan_team_id,
        after_orgs,
        &after_count);

    kbo_foreign_org_invalidate_org_list(before_orgs, before_count);
    kbo_foreign_org_invalidate_org_list(after_orgs, after_count);
    LONG mutation_generation = kbo_foreign_org_snapshot_note_mutation();

    if (g_kbo_foreign_org_snapshot_tick == 0u) {
        return;
    }
    kbo_lock_enter(&g_kbo_foreign_org_snapshot_lock);
    LONG published_generation = InterlockedCompareExchange(
        &g_kbo_foreign_org_snapshot_published_generation,
        0,
        0);
    if (g_kbo_foreign_org_snapshot_tick != 0u
            && published_generation == mutation_generation - 1) {
        for (int i = 0; i < before_count; i++) {
            if (!kbo_foreign_org_list_contains(after_orgs, after_count, before_orgs[i])) {
                kbo_foreign_org_snapshot_adjust_player(before_orgs[i], player_id, asian_quota, -1);
            }
        }
        for (int i = 0; i < after_count; i++) {
            if (!kbo_foreign_org_list_contains(before_orgs, before_count, after_orgs[i])) {
                kbo_foreign_org_snapshot_adjust_player(after_orgs[i], player_id, asian_quota, 1);
            }
        }
        DWORD now = GetTickCount();
        kbo_foreign_org_snapshot_cache_team_list_locked(before_orgs, before_count, now);
        kbo_foreign_org_snapshot_cache_team_list_locked(after_orgs, after_count, now);
        InterlockedExchange(&g_kbo_foreign_org_snapshot_published_generation, mutation_generation);
    } else if (g_kbo_foreign_org_snapshot_tick != 0u) {
        g_kbo_foreign_org_snapshot_tick = 0u;
        g_kbo_foreign_org_snapshot_count = 0;
        InterlockedExchange(&g_kbo_foreign_org_snapshot_published_generation, 0);
    }
    kbo_lock_leave(&g_kbo_foreign_org_snapshot_lock);
}

void kbo_foreign_org_count_cache_note_observed_assignment_change(
    uint32_t before_current_team_id,
    uint32_t before_active_team_id,
    uint32_t before_loan_team_id,
    uint32_t after_current_team_id,
    uint32_t after_active_team_id,
    uint32_t after_loan_team_id)
{
    uint32_t before_orgs[3] = {0u, 0u, 0u};
    uint32_t after_orgs[3] = {0u, 0u, 0u};
    int before_count = 0;
    int after_count = 0;

    kbo_foreign_org_assignment_orgs(
        before_current_team_id,
        before_active_team_id,
        before_loan_team_id,
        before_orgs,
        &before_count);
    kbo_foreign_org_assignment_orgs(
        after_current_team_id,
        after_active_team_id,
        after_loan_team_id,
        after_orgs,
        &after_count);

    if (before_count == 0 && after_count == 0) {
        return;
    }

    kbo_foreign_org_invalidate_org_list(before_orgs, before_count);
    kbo_foreign_org_invalidate_org_list(after_orgs, after_count);
    kbo_foreign_org_snapshot_note_mutation();

    kbo_lock_enter(&g_kbo_foreign_org_snapshot_lock);
    g_kbo_foreign_org_snapshot_tick = 0u;
    g_kbo_foreign_org_snapshot_count = 0;
    InterlockedExchange(&g_kbo_foreign_org_snapshot_published_generation, 0);
    kbo_lock_leave(&g_kbo_foreign_org_snapshot_lock);
}
