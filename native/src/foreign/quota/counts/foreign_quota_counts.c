#include "org/foreign_quota_counts_internal.h"

static int kbo_count_team_asian_quota_scan_live(
    uint32_t team_id,
    uint32_t* out_foreign_count,
    uint32_t* out_asian_quota_count,
    uint32_t* out_non_asian_foreign_count)
{
    if (out_foreign_count != NULL) { *out_foreign_count = 0u; }
    if (out_asian_quota_count != NULL) { *out_asian_quota_count = 0u; }
    if (out_non_asian_foreign_count != NULL) { *out_non_asian_foreign_count = 0u; }
    if (team_id == 0u) {
        return 0;
    }
    uint32_t org_team_id = kbo_foreign_org_team_id_for_team_id(team_id);
    if (org_team_id != 0u) {
        team_id = org_team_id;
    }

    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        return 0;
    }

    uint32_t foreign_count = 0u;
    uint32_t asian_count = 0u;
    uint32_t non_asian_count = 0u;
    kbo_ensure_foreign_replacement_player_seeds_loaded();
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
        if (nation_id == 0u || nation_id == OOTP27_KBO_KOREA_NATION_ID) {
            continue;
        }
        if (!kbo_player_current_assignment_matches_team_or_affiliate(player, team_id)) {
            continue;
        }
        uint8_t replacement_slot_type = 0u;
        if (kbo_foreign_replacement_player_seed_matches_loaded(player, &replacement_slot_type)) {
            static volatile LONG replacement_seed_count_log_count = 0;
            LONG slot = InterlockedIncrement(&replacement_seed_count_log_count);
            if (slot <= 20) {
                kbo_log_runtimef(
                    "foreign replacement player seed excluded from org count team=%u player=%u key_slot=%s nation=%u",
                    team_id,
                    *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET),
                    replacement_slot_type == KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA ? "Asian quota" : "Regular",
                    nation_id);
            } else if (slot == 21) {
                kbo_log_runtime_line("foreign replacement player seed org-count exclusion log suppressed after 20 entries");
            }
            continue;
        }
        if (kbo_foreign_injury_player_excluded_from_foreign_count(team_id, player_id)) {
            continue;
        }
        foreign_count++;
        if (kbo_player_is_asian_quota_candidate(player)) {
            asian_count++;
        } else {
            non_asian_count++;
        }
    }

    if (out_foreign_count != NULL) { *out_foreign_count = foreign_count; }
    if (out_asian_quota_count != NULL) { *out_asian_quota_count = asian_count; }
    if (out_non_asian_foreign_count != NULL) { *out_non_asian_foreign_count = non_asian_count; }
    return 1;
}

void kbo_count_team_asian_quota_probe(
    uint32_t team_id,
    uint32_t* out_foreign_count,
    uint32_t* out_asian_quota_count,
    uint32_t* out_non_asian_foreign_count)
{
    KBO_PROFILE_BEGIN(profile_foreign_org_count);
    uint32_t foreign_count = 0u;
    uint32_t asian_count = 0u;
    uint32_t non_asian_count = 0u;
    if (kbo_foreign_org_count_cache_hit(
            team_id,
            &foreign_count,
            &asian_count,
            &non_asian_count)) {
        if (out_foreign_count != NULL) { *out_foreign_count = foreign_count; }
        if (out_asian_quota_count != NULL) { *out_asian_quota_count = asian_count; }
        if (out_non_asian_foreign_count != NULL) { *out_non_asian_foreign_count = non_asian_count; }
        KBO_PROFILE_END(profile_foreign_org_count, "foreign_policy.org_count.cache_hit");
        return;
    }

    DWORD now = GetTickCount();
    uint32_t org_team_id = kbo_foreign_org_team_id_for_team_id(team_id);
    int snapshot_rebuilt = 0;
    if (kbo_foreign_org_snapshot_get(
            org_team_id != 0u ? org_team_id : team_id,
            now,
            &foreign_count,
            &asian_count,
            &non_asian_count,
            &snapshot_rebuilt)) {
        if (out_foreign_count != NULL) { *out_foreign_count = foreign_count; }
        if (out_asian_quota_count != NULL) { *out_asian_quota_count = asian_count; }
        if (out_non_asian_foreign_count != NULL) { *out_non_asian_foreign_count = non_asian_count; }
        if (team_id != 0u && team_id != org_team_id) {
            kbo_foreign_org_count_cache_store(
                team_id,
                foreign_count,
                asian_count,
                non_asian_count,
                now);
        }
        KBO_PROFILE_END(
            profile_foreign_org_count,
            snapshot_rebuilt ? "foreign_policy.org_count.snapshot_rebuild" : "foreign_policy.org_count.snapshot_hit");
        return;
    }

    uint32_t generation_before = kbo_foreign_org_count_cache_generation_for_team(team_id);
    int scanned = kbo_count_team_asian_quota_scan_live(
        team_id,
        &foreign_count,
        &asian_count,
        &non_asian_count);
    if (out_foreign_count != NULL) { *out_foreign_count = foreign_count; }
    if (out_asian_quota_count != NULL) { *out_asian_quota_count = asian_count; }
    if (out_non_asian_foreign_count != NULL) { *out_non_asian_foreign_count = non_asian_count; }
    uint32_t generation_after = kbo_foreign_org_count_cache_generation_for_team(team_id);
    if (scanned && generation_before == generation_after) {
        kbo_foreign_org_count_cache_store(
            team_id,
            foreign_count,
            asian_count,
            non_asian_count,
            now);
    }
    KBO_PROFILE_END(
        profile_foreign_org_count,
        scanned ? "foreign_policy.org_count.live_scan" : "foreign_policy.org_count.no_vector");
}

void kbo_count_team_asian_quota_probe_fresh(
    uint32_t team_id,
    uint32_t* out_foreign_count,
    uint32_t* out_asian_quota_count,
    uint32_t* out_non_asian_foreign_count)
{
    kbo_count_team_asian_quota_scan_live(
        team_id,
        out_foreign_count,
        out_asian_quota_count,
        out_non_asian_foreign_count);
}

uint32_t kbo_effective_foreign_count_with_asian_quota(uint32_t asian_count, uint32_t non_asian_foreign_count)
{
    return non_asian_foreign_count + (asian_count > 0u ? asian_count - 1u : 0u);
}

void kbo_count_active_asian_quota_by_position(
    uintptr_t team_ptr,
    uint32_t* out_asian_hitters,
    uint32_t* out_asian_pitchers)
{
    if (out_asian_hitters != NULL) { *out_asian_hitters = 0u; }
    if (out_asian_pitchers != NULL) { *out_asian_pitchers = 0u; }
    if (team_ptr == 0 || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return;
    }

    uint32_t asian_hitters = 0u;
    uint32_t asian_pitchers = 0u;
    uint32_t team_id = *(uint32_t*)(team_ptr + OOTP27_KBO_TEAM_ID_OFFSET);
    uint32_t* active_ids = (uint32_t*)(team_ptr + OOTP27_TEAM_PLAYER_IDS_2A80_OFFSET);
    for (uint32_t i = 0; i < OOTP27_TEAM_PLAYER_ID_ARRAY_COUNT; i++) {
        uint32_t player_id = active_ids[i];
        if (player_id == 0u) {
            continue;
        }

        uint8_t* player = kbo_find_player_by_id(player_id, NULL, NULL);
        if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
            continue;
        }
        if (!kbo_player_is_asian_quota_candidate(player)) {
            continue;
        }
        if (kbo_foreign_injury_player_excluded_from_foreign_count(team_id, player_id)) {
            continue;
        }

        if (*(uint8_t*)(player + OOTP27_PLAYER_POSITION_GROUP_OFFSET) == 1u) {
            asian_pitchers++;
        } else {
            asian_hitters++;
        }
    }

    if (out_asian_hitters != NULL) { *out_asian_hitters = asian_hitters; }
    if (out_asian_pitchers != NULL) { *out_asian_pitchers = asian_pitchers; }
}

void kbo_count_active_foreign_for_asian_quota(
    uintptr_t team_ptr,
    uint32_t* out_asian_hitters,
    uint32_t* out_asian_pitchers,
    uint32_t* out_non_asian_hitters,
    uint32_t* out_non_asian_pitchers)
{
    if (out_asian_hitters != NULL) { *out_asian_hitters = 0u; }
    if (out_asian_pitchers != NULL) { *out_asian_pitchers = 0u; }
    if (out_non_asian_hitters != NULL) { *out_non_asian_hitters = 0u; }
    if (out_non_asian_pitchers != NULL) { *out_non_asian_pitchers = 0u; }
    if (team_ptr == 0 || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return;
    }

    uint32_t asian_hitters = 0u;
    uint32_t asian_pitchers = 0u;
    uint32_t non_asian_hitters = 0u;
    uint32_t non_asian_pitchers = 0u;
    uint32_t team_id = *(uint32_t*)(team_ptr + OOTP27_KBO_TEAM_ID_OFFSET);
    uint32_t* active_ids = (uint32_t*)(team_ptr + OOTP27_TEAM_PLAYER_IDS_2A80_OFFSET);
    for (uint32_t i = 0; i < OOTP27_TEAM_PLAYER_ID_ARRAY_COUNT; i++) {
        uint32_t player_id = active_ids[i];
        if (player_id == 0u) {
            continue;
        }
        uint8_t* player = kbo_find_player_by_id(player_id, NULL, NULL);
        if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
            continue;
        }
        uint32_t nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
        if (nation_id == 0u || nation_id == OOTP27_KBO_KOREA_NATION_ID) {
            continue;
        }
        if (kbo_foreign_injury_player_excluded_from_foreign_count(team_id, player_id)) {
            continue;
        }
        int pitcher = (*(uint8_t*)(player + OOTP27_PLAYER_POSITION_GROUP_OFFSET) == 1u);
        int asian = kbo_player_is_asian_quota_candidate(player);
        if (pitcher) {
            if (asian) { asian_pitchers++; } else { non_asian_pitchers++; }
        } else {
            if (asian) { asian_hitters++; } else { non_asian_hitters++; }
        }
    }

    if (out_asian_hitters != NULL) { *out_asian_hitters = asian_hitters; }
    if (out_asian_pitchers != NULL) { *out_asian_pitchers = asian_pitchers; }
    if (out_non_asian_hitters != NULL) { *out_non_asian_hitters = non_asian_hitters; }
    if (out_non_asian_pitchers != NULL) { *out_non_asian_pitchers = non_asian_pitchers; }
}

int kbo_team_active_roster_contains_player(uintptr_t team_ptr, uint32_t player_id)
{
    if (team_ptr == 0
            || player_id == 0u
            || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return 0;
    }

    uint32_t* active_ids = (uint32_t*)(team_ptr + OOTP27_TEAM_PLAYER_IDS_2A80_OFFSET);
    for (uint32_t i = 0; i < OOTP27_TEAM_PLAYER_ID_ARRAY_COUNT; i++) {
        if (active_ids[i] == player_id) {
            return 1;
        }
    }
    return 0;
}
