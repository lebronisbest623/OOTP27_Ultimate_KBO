#include "../internal/foreign_quota_internal.h"

/* Foreign-player callup limit policy and wrappers. Included from native/KBOFix.c. */

uint8_t kbo_custom_foreign_policy_callup_allows(
    uintptr_t team_ptr,
    uintptr_t player_ptr,
    int32_t active_count,
    int32_t ootp_limit,
    int check_type)
{
    if (player_ptr == 0 || !kbo_player_pointer_plausible(player_ptr)) {
        return 0u;
    }

    uint8_t* player = (uint8_t*)player_ptr;
    if (!memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0u;
    }
    if (!kbo_player_is_foreign_for_kbo_rights(player)) {
        return 1u;
    }
    if (team_ptr == 0 || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return 0u;
    }

    int candidate_pitcher = (*(uint8_t*)(player + OOTP27_PLAYER_POSITION_GROUP_OFFSET) == 1u);
    if (check_type == 1 && candidate_pitcher) {
        return 1u;
    }
    if (check_type == 2 && !candidate_pitcher) {
        return 1u;
    }

    uint32_t team_id = *(uint32_t*)(team_ptr + OOTP27_KBO_TEAM_ID_OFFSET);
    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    if (team_id == 0u || player_id == 0u) {
        return 0u;
    }
    if (kbo_foreign_quota_team_blocks_foreign_ownership(team_id)) {
        return 0u;
    }
    if (kbo_team_active_roster_contains_player(team_ptr, player_id)) {
        return 1u;
    }

    uint32_t asian_hitters = 0u;
    uint32_t asian_pitchers = 0u;
    uint32_t non_asian_hitters = 0u;
    uint32_t non_asian_pitchers = 0u;
    kbo_count_active_foreign_for_asian_quota(
        team_ptr,
        &asian_hitters,
        &asian_pitchers,
        &non_asian_hitters,
        &non_asian_pitchers);

    uint32_t asian_before = asian_hitters + asian_pitchers;
    uint32_t non_asian_before = non_asian_hitters + non_asian_pitchers;
    uint32_t asian_after = asian_before;
    uint32_t non_asian_after = non_asian_before;
    uint8_t candidate_asian = kbo_player_is_asian_quota_candidate(player) ? 1u : 0u;
    if (candidate_asian) {
        asian_after++;
    } else {
        non_asian_after++;
    }

    uint8_t slot_type = 0u;
    uint32_t injured_player_id = 0u;
    uint32_t extra_slots = 0u;
    if (kbo_foreign_quota_team_allows_injury_extra_slots(team_id)) {
        extra_slots = kbo_custom_foreign_policy_extra_slots_for_candidate(
            team_id,
            player,
            &slot_type,
            &injured_player_id);
    }
    uint32_t effective_before = kbo_foreign_quota_effective_count_for_team(
        team_id,
        asian_before,
        non_asian_before);
    uint32_t effective_after = kbo_foreign_quota_effective_count_for_team(
        team_id,
        asian_after,
        non_asian_after);
    uint32_t effective_limit = kbo_foreign_quota_base_effective_limit_for_team(team_id) + extra_slots;
    uint8_t allowed = effective_after <= effective_limit ? 1u : 0u;
    if (allowed && candidate_asian && asian_before > 0u) {
        allowed = 0u;
    }

    static volatile LONG log_count = 0;
    LONG slot = InterlockedIncrement(&log_count);
    if (slot <= 240) {
        kbo_log_runtimef(
            "custom foreign policy callup team=%u player=%u type=%s active_count=%d ootp_limit=%d allowed=%u effective_before=%u effective_after=%u limit=%u injury_slot=%s injured=%u active_asian_h=%u active_asian_p=%u active_non_asian_h=%u active_non_asian_p=%u",
            team_id,
            player_id,
            check_type == 1 ? "hitter" : (check_type == 2 ? "pitcher" : "total"),
            active_count,
            ootp_limit,
            (uint32_t)allowed,
            effective_before,
            effective_after,
            effective_limit,
            slot_type != 0u ? kbo_foreign_injury_slot_label(slot_type) : "none",
            injured_player_id,
            asian_hitters,
            asian_pitchers,
            non_asian_hitters,
            non_asian_pitchers);
    }
    return allowed;
}

uint8_t kbo_callup_foreign_limit_allows_with_asian_quota(
    uintptr_t team_ptr,
    uintptr_t player_ptr,
    int32_t active_count,
    int32_t limit,
    int check_type)
{
    if (kbo_custom_foreign_policy_enabled()) {
        return kbo_custom_foreign_policy_callup_allows(team_ptr, player_ptr, active_count, limit, check_type);
    }
    if (team_ptr != 0
            && player_ptr != 0
            && kbo_player_pointer_plausible(player_ptr)
            && memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)
            && memory_range_readable((void*)player_ptr, OOTP27_PLAYER_SCAN_BYTES)) {
        uint8_t* probe_player = (uint8_t*)player_ptr;
        uint32_t team_id = *(uint32_t*)(team_ptr + OOTP27_KBO_TEAM_ID_OFFSET);
        if (kbo_player_is_foreign_for_kbo_rights(probe_player)
                && kbo_foreign_quota_team_blocks_foreign_ownership(team_id)) {
            return 0u;
        }
    }
    if (active_count < limit) {
        return 1u;
    }
    if (limit <= 0 || player_ptr == 0 || !kbo_player_pointer_plausible(player_ptr)) {
        return 0u;
    }

    uint8_t* player = (uint8_t*)player_ptr;
    uint32_t nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
    uint32_t injury_effective_after = 0u;
    uint8_t injury_slot_type = 0u;
    if (kbo_foreign_injury_replacement_callup_exception_available(
            team_ptr,
            player,
            limit,
            check_type,
            &injury_effective_after,
            &injury_slot_type)) {
        static volatile LONG injury_log_count = 0;
        LONG injury_slot = InterlockedIncrement(&injury_log_count);
        if (injury_slot <= 160) {
            uint32_t team_id = 0u;
            if (memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
                team_id = *(uint32_t*)(team_ptr + OOTP27_KBO_TEAM_ID_OFFSET);
            }
            kbo_log_runtimef(
                "foreign injury replacement callup exception team=%u player=%u type=%s active_count=%d limit=%d effective_after=%u",
                team_id,
                *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET),
                kbo_foreign_injury_slot_label(injury_slot_type),
                active_count,
                limit,
                injury_effective_after);
        }
        return 1u;
    }
    if (!kbo_player_is_asian_quota_candidate(player)) {
        return 0u;
    }

    uint32_t asian_hitters = 0u;
    uint32_t asian_pitchers = 0u;
    uint32_t non_asian_hitters = 0u;
    uint32_t non_asian_pitchers = 0u;
    kbo_count_active_foreign_for_asian_quota(
        team_ptr,
        &asian_hitters,
        &asian_pitchers,
        &non_asian_hitters,
        &non_asian_pitchers);
    if (asian_hitters + asian_pitchers > 0u) {
        return 0u;
    }

    int candidate_pitcher = (*(uint8_t*)(player + OOTP27_PLAYER_POSITION_GROUP_OFFSET) == 1u);
    uint32_t effective_after = 0u;
    if (check_type == 1) {
        uint32_t asian_after = asian_hitters + (candidate_pitcher ? 0u : 1u);
        effective_after = kbo_effective_foreign_count_with_asian_quota(asian_after, non_asian_hitters);
    } else if (check_type == 2) {
        uint32_t asian_after = asian_pitchers + (candidate_pitcher ? 1u : 0u);
        effective_after = kbo_effective_foreign_count_with_asian_quota(asian_after, non_asian_pitchers);
    } else {
        uint32_t asian_after = asian_hitters + asian_pitchers + 1u;
        uint32_t non_asian_after = non_asian_hitters + non_asian_pitchers;
        effective_after = kbo_effective_foreign_count_with_asian_quota(asian_after, non_asian_after);
    }

    uint8_t allowed = (effective_after <= (uint32_t)limit) ? 1u : 0u;
    static volatile LONG log_count = 0;
    LONG slot = InterlockedIncrement(&log_count);
    if (slot <= 160) {
        uint32_t team_id = 0u;
        if (memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            team_id = *(uint32_t*)(team_ptr + OOTP27_KBO_TEAM_ID_OFFSET);
        }
        kbo_log_runtimef(
            "asian quota callup limit check team=%u player=%u nation=%u type=%s active_count=%d limit=%d effective_after=%u allowed=%u active_asian_h=%u active_asian_p=%u active_non_asian_h=%u active_non_asian_p=%u",
            team_id,
            *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET),
            nation_id,
            check_type == 1 ? "hitter" : (check_type == 2 ? "pitcher" : "total"),
            active_count,
            limit,
            effective_after,
            (uint32_t)allowed,
            asian_hitters,
            asian_pitchers,
            non_asian_hitters,
            non_asian_pitchers);
    }
    return allowed;
}

__declspec(noinline) uint8_t ootp_kbo_callup_foreign_hitter_limit_allows_wrapper(
    uintptr_t team_ptr, uintptr_t player_ptr, int32_t active_count, int32_t limit)
{
    KBO_HOOK_PROFILE_BEGIN(profile_hook);
    uint8_t result = kbo_callup_foreign_limit_allows_with_asian_quota(team_ptr, player_ptr, active_count, limit, 1);
    KBO_HOOK_PROFILE_RETURN(profile_hook, "foreign.callup_hitter_limit", result);
}

__declspec(noinline) uint8_t ootp_kbo_callup_foreign_pitcher_limit_allows_wrapper(
    uintptr_t team_ptr, uintptr_t player_ptr, int32_t active_count, int32_t limit)
{
    KBO_HOOK_PROFILE_BEGIN(profile_hook);
    uint8_t result = kbo_callup_foreign_limit_allows_with_asian_quota(team_ptr, player_ptr, active_count, limit, 2);
    KBO_HOOK_PROFILE_RETURN(profile_hook, "foreign.callup_pitcher_limit", result);
}

__declspec(noinline) uint8_t ootp_kbo_callup_foreign_total_limit_allows_wrapper(
    uintptr_t team_ptr, uintptr_t player_ptr, int32_t active_count, int32_t limit)
{
    KBO_HOOK_PROFILE_BEGIN(profile_hook);
    uint8_t result = kbo_callup_foreign_limit_allows_with_asian_quota(team_ptr, player_ptr, active_count, limit, 3);
    KBO_HOOK_PROFILE_RETURN(profile_hook, "foreign.callup_total_limit", result);
}

