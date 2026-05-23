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

static KboForeignOrgSnapshotEntry* kbo_foreign_org_snapshot_entry_in(
    KboForeignOrgSnapshotEntry* entries,
    int* entry_count,
    uint32_t team_id)
{
    if (entries == NULL || entry_count == NULL || team_id == 0u) {
        return NULL;
    }
    for (int i = 0; i < *entry_count; i++) {
        if (entries[i].team_id == team_id) {
            return &entries[i];
        }
    }
    if (*entry_count >= KBO_FOREIGN_ORG_SNAPSHOT_MAX_TEAMS) {
        return NULL;
    }
    KboForeignOrgSnapshotEntry* entry = &entries[(*entry_count)++];
    entry->team_id = team_id;
    entry->foreign_count = 0u;
    entry->asian_count = 0u;
    entry->non_asian_count = 0u;
    return entry;
}

static void kbo_foreign_org_snapshot_add_player_to(
    KboForeignOrgSnapshotEntry* entries,
    int* entry_count,
    uint32_t team_id,
    uint32_t player_id,
    int asian_quota)
{
    if (team_id == 0u) {
        return;
    }
    KboForeignOrgSnapshotEntry* entry = kbo_foreign_org_snapshot_entry_in(entries, entry_count, team_id);
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

static void kbo_foreign_org_snapshot_add_unique_player_to(
    KboForeignOrgSnapshotEntry* entries,
    int* entry_count,
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
    kbo_foreign_org_snapshot_add_player_to(entries, entry_count, team_id, player_id, asian_quota);
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

enum {
    KBO_FOREIGN_ORG_PLAYER_INDEX_MAX = 2048
};

typedef struct KboForeignOrgPlayerIndexEntry {
    uintptr_t player_ptr;
    uint32_t player_id;
} KboForeignOrgPlayerIndexEntry;

static KboForeignOrgPlayerIndexEntry
    g_kbo_foreign_org_player_index[KBO_FOREIGN_ORG_PLAYER_INDEX_MAX];
static int g_kbo_foreign_org_player_index_count = 0;
static int g_kbo_foreign_org_player_index_valid = 0;
static uintptr_t g_kbo_foreign_org_player_index_vector = 0;
static int32_t g_kbo_foreign_org_player_index_player_count = 0;

static void kbo_foreign_org_player_index_reset(void)
{
    g_kbo_foreign_org_player_index_count = 0;
    g_kbo_foreign_org_player_index_valid = 0;
    g_kbo_foreign_org_player_index_vector = 0;
    g_kbo_foreign_org_player_index_player_count = 0;
}

static int kbo_foreign_org_player_index_scope_matches(
    uintptr_t player_vector,
    int32_t player_count)
{
    return g_kbo_foreign_org_player_index_valid
        && g_kbo_foreign_org_player_index_vector == player_vector
        && g_kbo_foreign_org_player_index_player_count == player_count;
}

static int kbo_foreign_org_player_index_rebuild(
    uintptr_t player_vector,
    int32_t player_count)
{
    KBO_PROFILE_BEGIN(profile_foreign_org_player_index_rebuild);
    if (player_vector == 0 || player_count <= 0) {
        kbo_foreign_org_player_index_reset();
        KBO_PROFILE_END(profile_foreign_org_player_index_rebuild, "foreign_org_count.player_index.invalid_scope");
        return 0;
    }

    int indexed = 0;
    int scanned = 0;
    int overflow = 0;
    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }

        uint8_t* player = (uint8_t*)player_ptr;
        if (!kbo_player_is_active_for_roster_scan(player)) {
            continue;
        }
        uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        uint32_t nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
        scanned++;
        if (player_id == 0u
                || nation_id == 0u
                || nation_id == OOTP27_KBO_KOREA_NATION_ID) {
            continue;
        }
        if (indexed >= KBO_FOREIGN_ORG_PLAYER_INDEX_MAX) {
            overflow = 1;
            break;
        }
        g_kbo_foreign_org_player_index[indexed].player_ptr = player_ptr;
        g_kbo_foreign_org_player_index[indexed].player_id = player_id;
        indexed++;
    }

    if (overflow) {
        kbo_foreign_org_player_index_reset();
        KBO_PROFILE_END(profile_foreign_org_player_index_rebuild, "foreign_org_count.player_index.overflow");
        return 0;
    }

    g_kbo_foreign_org_player_index_count = indexed;
    g_kbo_foreign_org_player_index_vector = player_vector;
    g_kbo_foreign_org_player_index_player_count = player_count;
    g_kbo_foreign_org_player_index_valid = 1;

    static volatile LONG log_count = 0;
    LONG log_slot = InterlockedIncrement(&log_count);
    if (log_slot <= 8) {
        kbo_log_runtimef(
            "foreign org player index rebuilt scanned=%d indexed=%d player_count=%d",
            scanned,
            indexed,
            player_count);
    }
    KBO_PROFILE_END(profile_foreign_org_player_index_rebuild, "foreign_org_count.player_index.rebuild");
    return 1;
}

static int kbo_foreign_org_player_index_ensure(
    uintptr_t player_vector,
    int32_t player_count)
{
    if (kbo_foreign_org_player_index_scope_matches(player_vector, player_count)) {
        kbo_profiler_record_us("foreign_org_count.player_index.hit", 0);
        return 1;
    }
    return kbo_foreign_org_player_index_rebuild(player_vector, player_count);
}

static int kbo_foreign_org_snapshot_rebuild_from_player_index(
    KboForeignOrgSnapshotEntry* entries,
    int* out_entry_count,
    uintptr_t player_vector,
    int32_t player_count)
{
    if (entries == NULL || out_entry_count == NULL) {
        return 0;
    }

    if (!kbo_foreign_org_player_index_ensure(player_vector, player_count)) {
        return 0;
    }

    KBO_PROFILE_BEGIN(profile_foreign_org_player_index_count);
    int stale = 0;
    int members = 0;
    int32_t asian_quota_salary_limit = kbo_get_asian_quota_salary_limit();
    *out_entry_count = 0;
    for (int i = 0; i < g_kbo_foreign_org_player_index_count; i++) {
        uintptr_t player_ptr = g_kbo_foreign_org_player_index[i].player_ptr;
        if (!kbo_player_pointer_plausible(player_ptr)) {
            stale = 1;
            break;
        }

        uint8_t* player = (uint8_t*)player_ptr;
        uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        if (player_id == 0u || player_id != g_kbo_foreign_org_player_index[i].player_id) {
            stale = 1;
            break;
        }

        uint32_t nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
        if (nation_id == 0u || nation_id == OOTP27_KBO_KOREA_NATION_ID) {
            continue;
        }
        uint8_t replacement_slot_type = 0u;
        if (kbo_foreign_replacement_player_seed_matches_loaded(player, &replacement_slot_type)) {
            continue;
        }

        int asian_quota = kbo_player_is_asian_quota_candidate_with_salary_limit(
            player,
            asian_quota_salary_limit);
        uint32_t team_ids[3] = {0u, 0u, 0u};
        int team_count = 0;
        uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
        uint32_t loan_team_id = *(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET);
        kbo_foreign_org_snapshot_add_unique_player_to(
            entries,
            out_entry_count,
            team_ids,
            &team_count,
            kbo_foreign_org_team_id_for_team_id(current_team_id),
            player_id,
            asian_quota);
        kbo_foreign_org_snapshot_add_unique_player_to(
            entries,
            out_entry_count,
            team_ids,
            &team_count,
            kbo_foreign_org_team_id_for_team_id(active_team_id),
            player_id,
            asian_quota);
        kbo_foreign_org_snapshot_add_unique_player_to(
            entries,
            out_entry_count,
            team_ids,
            &team_count,
            kbo_foreign_org_team_id_for_team_id(loan_team_id),
            player_id,
            asian_quota);
        members += team_count;
    }

    if (stale) {
        kbo_foreign_org_player_index_reset();
        *out_entry_count = 0;
        KBO_PROFILE_END(profile_foreign_org_player_index_count, "foreign_org_count.player_index.count_stale");
        return 0;
    }

    (void)members;
    KBO_PROFILE_END(profile_foreign_org_player_index_count, "foreign_org_count.player_index.count");
    return 1;
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

static int kbo_foreign_org_snapshot_rebuild_into(
    KboForeignOrgSnapshotEntry* entries,
    int* out_entry_count)
{
    if (entries == NULL || out_entry_count == NULL) {
        return 0;
    }
    *out_entry_count = 0;

    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        return 0;
    }

    kbo_ensure_foreign_replacement_player_seeds_loaded();
    if (kbo_foreign_org_snapshot_rebuild_from_player_index(
            entries,
            out_entry_count,
            player_vector,
            player_count)) {
        return 1;
    }

    *out_entry_count = 0;
    KBO_PROFILE_BEGIN(profile_foreign_org_snapshot_full_scan_fallback);
    int32_t asian_quota_salary_limit = kbo_get_asian_quota_salary_limit();
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

        int asian_quota = kbo_player_is_asian_quota_candidate_with_salary_limit(
            player,
            asian_quota_salary_limit);
        uint32_t team_ids[3] = {0u, 0u, 0u};
        int team_count = 0;
        uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
        uint32_t loan_team_id = *(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET);
        kbo_foreign_org_snapshot_add_unique_player_to(
            entries,
            out_entry_count,
            team_ids,
            &team_count,
            kbo_foreign_org_team_id_for_team_id(current_team_id),
            player_id,
            asian_quota);
        kbo_foreign_org_snapshot_add_unique_player_to(
            entries,
            out_entry_count,
            team_ids,
            &team_count,
            kbo_foreign_org_team_id_for_team_id(active_team_id),
            player_id,
            asian_quota);
        kbo_foreign_org_snapshot_add_unique_player_to(
            entries,
            out_entry_count,
            team_ids,
            &team_count,
            kbo_foreign_org_team_id_for_team_id(loan_team_id),
            player_id,
            asian_quota);
    }
    KBO_PROFILE_END(profile_foreign_org_snapshot_full_scan_fallback, "foreign_org_count.snapshot_full_scan_fallback");
    return 1;
}

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
