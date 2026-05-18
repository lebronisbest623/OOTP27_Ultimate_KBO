#include "foreign_quota_counts_internal.h"


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

static void kbo_foreign_org_snapshot_add_player(uint32_t team_id, uint32_t player_id, int asian_quota)
{
    if (team_id == 0u) {
        return;
    }
    KboForeignOrgSnapshotEntry* entry = kbo_foreign_org_snapshot_entry(team_id);
    if (entry == NULL || kbo_foreign_injury_player_excluded_from_foreign_count(team_id, player_id)) {
        return;
    }
    entry->foreign_count++;
    if (asian_quota) {
        entry->asian_count++;
    } else {
        entry->non_asian_count++;
    }
}

static void kbo_foreign_org_snapshot_add_unique_player(
    uint32_t* team_ids,
    int* team_count,
    uint32_t team_id,
    uint32_t player_id,
    int asian_quota)
{
    if (team_id == 0u) {
        return;
    }
    for (int i = 0; i < *team_count; i++) {
        if (team_ids[i] == team_id) {
            return;
        }
    }
    if (*team_count >= 3) {
        return;
    }
    team_ids[(*team_count)++] = team_id;
    kbo_foreign_org_snapshot_add_player(team_id, player_id, asian_quota);
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

static void kbo_foreign_org_snapshot_cache_all_locked(DWORD now)
{
    for (int i = 0; i < g_kbo_foreign_org_snapshot_count; i++) {
        kbo_foreign_org_count_cache_store(
            g_kbo_foreign_org_snapshot[i].team_id,
            g_kbo_foreign_org_snapshot[i].foreign_count,
            g_kbo_foreign_org_snapshot[i].asian_count,
            g_kbo_foreign_org_snapshot[i].non_asian_count,
            now);
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

    kbo_foreign_org_list_add(&before_orgs[0], &before_count, kbo_foreign_org_team_id_for_team_id(before_current_team_id));
    kbo_foreign_org_list_add(&before_orgs[0], &before_count, kbo_foreign_org_team_id_for_team_id(before_active_team_id));
    kbo_foreign_org_list_add(&before_orgs[0], &before_count, kbo_foreign_org_team_id_for_team_id(before_loan_team_id));
    kbo_foreign_org_list_add(&after_orgs[0], &after_count, kbo_foreign_org_team_id_for_team_id(after_current_team_id));
    kbo_foreign_org_list_add(&after_orgs[0], &after_count, kbo_foreign_org_team_id_for_team_id(after_active_team_id));
    kbo_foreign_org_list_add(&after_orgs[0], &after_count, kbo_foreign_org_team_id_for_team_id(after_loan_team_id));

    for (int i = 0; i < before_count; i++) {
        kbo_foreign_org_count_cache_invalidate_team(before_orgs[i]);
        kbo_foreign_org_count_bump_team_generation(before_orgs[i]);
    }
    for (int i = 0; i < after_count; i++) {
        kbo_foreign_org_count_cache_invalidate_team(after_orgs[i]);
        kbo_foreign_org_count_bump_team_generation(after_orgs[i]);
    }

    if (g_kbo_foreign_org_snapshot_tick == 0u) {
        return;
    }
    kbo_lock_enter(&g_kbo_foreign_org_snapshot_lock);
    if (g_kbo_foreign_org_snapshot_tick != 0u) {
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
    }
    kbo_lock_leave(&g_kbo_foreign_org_snapshot_lock);
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
    kbo_lock_leave(&g_kbo_foreign_org_snapshot_lock);
}

void kbo_foreign_org_count_refresh_benchmark_snapshot_tick(void)
{
    if (g_kbo_foreign_org_snapshot_tick != 0u) {
        g_kbo_foreign_org_snapshot_tick = GetTickCount();
    }
}
#endif

static int kbo_foreign_org_snapshot_rebuild(DWORD now)
{
    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        return 0;
    }

    g_kbo_foreign_org_snapshot_count = 0;
    kbo_ensure_foreign_replacement_player_seeds_loaded();
    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }
        uint8_t* player = (uint8_t*)player_ptr;
        uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        uint32_t nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
        if (nation_id == 0u || nation_id == OOTP27_KBO_KOREA_NATION_ID) {
            continue;
        }
        uint8_t replacement_slot_type = 0u;
        if (kbo_foreign_replacement_player_seed_matches_loaded(player, &replacement_slot_type)) {
            continue;
        }

        int asian_quota = kbo_player_is_asian_quota_candidate(player);
        uint32_t team_ids[3] = {0u, 0u, 0u};
        int team_count = 0;
        uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
        uint32_t loan_team_id = *(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET);
        kbo_foreign_org_snapshot_add_unique_player(
            team_ids,
            &team_count,
            kbo_foreign_org_team_id_for_team_id(current_team_id),
            player_id,
            asian_quota);
        kbo_foreign_org_snapshot_add_unique_player(
            team_ids,
            &team_count,
            kbo_foreign_org_team_id_for_team_id(active_team_id),
            player_id,
            asian_quota);
        kbo_foreign_org_snapshot_add_unique_player(
            team_ids,
            &team_count,
            kbo_foreign_org_team_id_for_team_id(loan_team_id),
            player_id,
            asian_quota);
    }
    g_kbo_foreign_org_snapshot_tick = now;
    kbo_foreign_org_snapshot_cache_all_locked(now);
    return 1;
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
    if (kbo_foreign_org_team_id_for_team_id(team_id) != team_id) {
        return 0;
    }

    kbo_lock_enter(&g_kbo_foreign_org_snapshot_lock);
    if (g_kbo_foreign_org_snapshot_tick == 0u) {
        if (!kbo_foreign_org_snapshot_rebuild(now)) {
            kbo_lock_leave(&g_kbo_foreign_org_snapshot_lock);
            return 0;
        }
        if (out_rebuilt != NULL) {
            *out_rebuilt = 1;
        }
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
    kbo_lock_leave(&g_kbo_foreign_org_snapshot_lock);

    if (out_foreign_count != NULL) { *out_foreign_count = foreign_count; }
    if (out_asian_quota_count != NULL) { *out_asian_quota_count = asian_count; }
    if (out_non_asian_foreign_count != NULL) { *out_non_asian_foreign_count = non_asian_count; }
    kbo_foreign_org_count_cache_store(team_id, foreign_count, asian_count, non_asian_count, now);
    return 1;
}
