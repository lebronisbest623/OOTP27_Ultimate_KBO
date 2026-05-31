#include "../../internal/foreign_quota_internal.h"
#include "../cache/foreign_quota_candidate_limit_cache.h"

uint32_t kbo_custom_foreign_policy_extra_slots_for_candidate(
    uint32_t team_id,
    uint8_t* candidate,
    uint8_t* out_slot_type,
    uint32_t* out_injured_player_id)
{
    if (out_slot_type != NULL) { *out_slot_type = 0u; }
    if (out_injured_player_id != NULL) { *out_injured_player_id = 0u; }

    if (!kbo_foreign_injury_replacement_enabled()
            || team_id == 0u
            || candidate == NULL
            || !memory_range_readable(candidate, OOTP27_PLAYER_SCAN_BYTES)
            || !kbo_player_is_foreign_for_kbo_rights(candidate)) {
        return 0u;
    }

    uint32_t candidate_id = *(uint32_t*)(candidate + OOTP27_PLAYER_ID_OFFSET);
    if (candidate_id == 0u) {
        return 0u;
    }

    uint32_t today = 0u;
    kbo_get_foreign_waiver_current_yyyymmdd(&today);
    uint32_t league_id = kbo_resolve_kbo_league_id();

    uint8_t candidate_asian = kbo_player_is_asian_quota_slot_candidate(candidate) ? 1u : 0u;
    if (!kbo_foreign_injury_replacement_in_season_window(
            league_id,
            today,
            "foreign_policy.candidate_extra_slot",
            "candidate_extra_slot")) {
        return 0u;
    }

    uint32_t cached_extra_slots = 0u;
    if (kbo_custom_foreign_extra_slot_cache_hit(
            team_id,
            candidate,
            candidate_id,
            league_id,
            candidate_asian,
            out_slot_type,
            out_injured_player_id,
            &cached_extra_slots)) {
        kbo_profiler_record_us("foreign_policy.candidate.extra_slots.cache_hit", 0);
        return cached_extra_slots;
    }

    int team_has_candidate_type_slot = 0;
    if (kbo_custom_foreign_extra_slot_team_cache_hit(
            team_id,
            league_id,
            candidate_asian,
            &team_has_candidate_type_slot)) {
        if (!team_has_candidate_type_slot) {
            kbo_profiler_record_us("foreign_policy.candidate.extra_slots.team_none_cache_hit", 0);
            kbo_custom_foreign_extra_slot_cache_store(
                team_id,
                candidate,
                candidate_id,
                league_id,
                candidate_asian,
                0u,
                0u,
                0u);
            return 0u;
        }
        kbo_profiler_record_us("foreign_policy.candidate.extra_slots.team_has_cache_hit", 0);
    } else {
        uint8_t available_slot_type = 0u;
        uint32_t available_injured_player_id = 0u;
        KBO_PROFILE_BEGIN(profile_custom_candidate_extra_team_slot);
        team_has_candidate_type_slot = kbo_team_has_foreign_injury_slot_for_candidate_type_any(
            team_id,
            candidate_asian != 0u,
            &available_slot_type,
            &available_injured_player_id);
        KBO_PROFILE_END(profile_custom_candidate_extra_team_slot, team_has_candidate_type_slot
            ? "foreign_policy.candidate.extra_slots.team_has_slot"
            : "foreign_policy.candidate.extra_slots.team_no_slot");
        (void)available_slot_type;
        (void)available_injured_player_id;
        kbo_custom_foreign_extra_slot_team_cache_store(
            team_id,
            league_id,
            candidate_asian,
            team_has_candidate_type_slot);
        if (!team_has_candidate_type_slot) {
            kbo_custom_foreign_extra_slot_cache_store(
                team_id,
                candidate,
                candidate_id,
                league_id,
                candidate_asian,
                0u,
                0u,
                0u);
            return 0u;
        }
    }

    uint32_t injured_player_id = 0u;
    uint8_t slot_type = 0u;
    if (kbo_team_has_foreign_injury_slot_for_candidate_any(
            team_id,
            candidate_asian != 0u,
            candidate_id,
            &slot_type,
            &injured_player_id,
            NULL)) {
        if (out_slot_type != NULL) { *out_slot_type = slot_type; }
        if (out_injured_player_id != NULL) { *out_injured_player_id = injured_player_id; }
        kbo_custom_foreign_extra_slot_cache_store(
            team_id,
            candidate,
            candidate_id,
            league_id,
            candidate_asian,
            1u,
            slot_type,
            injured_player_id);
        return 1u;
    }

    kbo_custom_foreign_extra_slot_cache_store(
        team_id,
        candidate,
        candidate_id,
        league_id,
        candidate_asian,
        0u,
        0u,
        0u);
    return 0u;
}
