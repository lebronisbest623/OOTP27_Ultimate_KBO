#include "../foreign_quota_counts_internal.h"
#include "../../../../../core/core_flags/api/flags_api.h"
#include <string.h>

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
    const KboForeignInjuryExclusionSnapshot* exclusion,
    uint32_t team_id,
    uint32_t player_id,
    int asian_quota)
{
    if (team_id == 0u) {
        return;
    }
    KboForeignOrgSnapshotEntry* entry = kbo_foreign_org_snapshot_entry_in(entries, entry_count, team_id);
    if (entry == NULL
            || kbo_foreign_injury_player_excluded_from_foreign_count_snapshot(exclusion, team_id, player_id)) {
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
    const KboForeignInjuryExclusionSnapshot* exclusion,
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
    kbo_foreign_org_snapshot_add_player_to(entries, entry_count, exclusion, team_id, player_id, asian_quota);
}

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
    const KboForeignInjuryExclusionSnapshot* exclusion,
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
            exclusion,
            team_ids,
            &team_count,
            kbo_foreign_org_team_id_for_team_id(current_team_id),
            player_id,
            asian_quota);
        kbo_foreign_org_snapshot_add_unique_player_to(
            entries,
            out_entry_count,
            exclusion,
            team_ids,
            &team_count,
            kbo_foreign_org_team_id_for_team_id(active_team_id),
            player_id,
            asian_quota);
        kbo_foreign_org_snapshot_add_unique_player_to(
            entries,
            out_entry_count,
            exclusion,
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

int kbo_foreign_org_snapshot_rebuild_into(
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
    KboForeignInjuryExclusionSnapshot exclusion;
    kbo_foreign_injury_build_exclusion_snapshot(&exclusion);
    if (kbo_foreign_org_snapshot_rebuild_from_player_index(
            entries,
            out_entry_count,
            &exclusion,
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
            &exclusion,
            team_ids,
            &team_count,
            kbo_foreign_org_team_id_for_team_id(current_team_id),
            player_id,
            asian_quota);
        kbo_foreign_org_snapshot_add_unique_player_to(
            entries,
            out_entry_count,
            &exclusion,
            team_ids,
            &team_count,
            kbo_foreign_org_team_id_for_team_id(active_team_id),
            player_id,
            asian_quota);
        kbo_foreign_org_snapshot_add_unique_player_to(
            entries,
            out_entry_count,
            &exclusion,
            team_ids,
            &team_count,
            kbo_foreign_org_team_id_for_team_id(loan_team_id),
            player_id,
            asian_quota);
    }
    KBO_PROFILE_END(profile_foreign_org_snapshot_full_scan_fallback, "foreign_org_count.snapshot_full_scan_fallback");
    return 1;
}


