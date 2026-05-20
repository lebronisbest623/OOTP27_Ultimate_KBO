#include "../internal/foreign_quota_internal.h"
#include "cache/foreign_quota_candidate_limit_cache.h"
#include "../../controller/foreign_ai_controller.h"
#include "../../rights/query/foreign_waiver_rights_query.h"
#include "foreign_quota_retention_opportunity_probe.h"
#include <string.h>

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
    uint32_t cached_extra_slots = 0u;
    if (kbo_custom_foreign_extra_slot_cache_hit(
            team_id,
            candidate,
            candidate_id,
            today,
            league_id,
            candidate_asian,
            out_slot_type,
            out_injured_player_id,
            &cached_extra_slots)) {
        kbo_profiler_record_us("foreign_policy.candidate.extra_slots.cache_hit", 0);
        return cached_extra_slots;
    }

    if (!kbo_foreign_injury_replacement_in_season_window(
            league_id,
            today,
            "foreign_policy.candidate_extra_slot",
            "candidate_extra_slot")) {
        kbo_custom_foreign_extra_slot_cache_store(
            team_id,
            candidate,
            candidate_id,
            today,
            league_id,
            candidate_asian,
            0u,
            0u,
            0u);
        return 0u;
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
            today,
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
        today,
        league_id,
        candidate_asian,
        0u,
        0u,
        0u);
    return 0u;
}

int kbo_custom_foreign_policy_can_override_original_block(uint8_t* candidate, uint32_t team_id)
{
    if (candidate == NULL || team_id == 0u || !memory_range_readable(candidate, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    uint32_t current_team_id = *(uint32_t*)(candidate + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    return current_team_id == 0u || kbo_player_current_assignment_matches_team_or_affiliate(candidate, team_id);
}

int kbo_custom_foreign_policy_team_allows_candidate(
    uint32_t team_id,
    uint8_t* candidate,
    uint32_t* out_effective_before,
    uint32_t* out_effective_after,
    uint32_t* out_effective_limit,
    uint8_t* out_slot_type,
    uint32_t* out_injured_player_id)
{
    if (out_effective_before != NULL) { *out_effective_before = 0u; }
    if (out_effective_after != NULL) { *out_effective_after = 0u; }
    if (out_effective_limit != NULL) { *out_effective_limit = KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT; }
    if (out_slot_type != NULL) { *out_slot_type = 0u; }
    if (out_injured_player_id != NULL) { *out_injured_player_id = 0u; }

    KBO_PROFILE_BEGIN(profile_custom_candidate);
    if (team_id == 0u
            || candidate == NULL
            || !memory_range_readable(candidate, OOTP27_PLAYER_SCAN_BYTES)
            || !kbo_player_is_foreign_for_kbo_rights(candidate)) {
        KBO_PROFILE_END(profile_custom_candidate, "foreign_policy.candidate.invalid");
        return 0;
    }

    uint32_t today = 0u;
    uint32_t candidate_id = *(uint32_t*)(candidate + OOTP27_PLAYER_ID_OFFSET);
    kbo_get_foreign_waiver_current_yyyymmdd(&today);

    /* Team quota depends on live assignment fields for every foreign player in the org. */
    uint32_t foreign_count = 0u;
    uint32_t asian_count = 0u;
    uint32_t non_asian_count = 0u;
    KBO_PROFILE_BEGIN(profile_custom_candidate_count);
    kbo_count_team_asian_quota_probe(team_id, &foreign_count, &asian_count, &non_asian_count);
    KBO_PROFILE_END(profile_custom_candidate_count, "foreign_policy.candidate.org_count");
    (void)foreign_count;

    uint32_t pending_asian_count = 0u;
    uint32_t pending_non_asian_count = 0u;
    int candidate_pending = 0;
    KBO_PROFILE_BEGIN(profile_custom_candidate_pending);
    kbo_custom_foreign_count_pending_offers(
        team_id,
        today,
        candidate_id,
        &pending_asian_count,
        &pending_non_asian_count,
        &candidate_pending);
    KBO_PROFILE_END(profile_custom_candidate_pending, "foreign_policy.candidate.pending");
    asian_count += pending_asian_count;
    non_asian_count += pending_non_asian_count;

    uint32_t effective_before = kbo_effective_foreign_count_with_asian_quota(asian_count, non_asian_count);
    uint32_t asian_after = asian_count;
    uint32_t non_asian_after = non_asian_count;
    int already_in_org = kbo_player_current_assignment_matches_team_or_affiliate(candidate, team_id);
    int retained_by_team = today != 0u
        && candidate_id != 0u
        && kbo_has_active_foreign_waiver_right(team_id, candidate_id, today);
    int counts_as_existing_candidate = already_in_org || retained_by_team;
    if (!counts_as_existing_candidate && !candidate_pending) {
        if (kbo_player_is_asian_quota_slot_candidate(candidate)) {
            asian_after++;
        } else {
            non_asian_after++;
        }
    }

    uint8_t slot_type = 0u;
    uint32_t injured_player_id = 0u;
    uint32_t effective_after = kbo_effective_foreign_count_with_asian_quota(asian_after, non_asian_after);
    int details_requested = out_effective_limit != NULL
        || out_slot_type != NULL
        || out_injured_player_id != NULL;
    int need_extra_slots = details_requested || (!counts_as_existing_candidate && effective_after > KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT);
    if (!need_extra_slots && kbo_foreign_ai_controller_enabled()) {
        KboForeignRetentionOpportunitySummary opportunity;
        memset(&opportunity, 0, sizeof(opportunity));
        if (kbo_retention_opportunity_get_summary(team_id, today, &opportunity)) {
            uint8_t candidate_asian = kbo_player_is_asian_quota_slot_candidate(candidate) ? 1u : 0u;
            uint32_t reserve_asian = opportunity.protectable_asian;
            uint32_t reserve_non_asian = opportunity.protectable_non_asian;
            if (retained_by_team) {
                if (candidate_asian && reserve_asian > 0u) {
                    reserve_asian--;
                } else if (!candidate_asian && reserve_non_asian > 0u) {
                    reserve_non_asian--;
                }
            }
            uint32_t reserved_after = kbo_effective_foreign_count_with_asian_quota(
                asian_after + reserve_asian,
                non_asian_after + reserve_non_asian);
            need_extra_slots = reserved_after > KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT;
        }
    }
    uint32_t extra_slots = 0u;
    if (need_extra_slots) {
        KBO_PROFILE_BEGIN(profile_custom_candidate_extra);
        extra_slots = kbo_custom_foreign_policy_extra_slots_for_candidate(
            team_id,
            candidate,
            &slot_type,
            &injured_player_id);
        KBO_PROFILE_END(profile_custom_candidate_extra, "foreign_policy.candidate.extra_slots");
    } else {
        KBO_PROFILE_BEGIN(profile_custom_candidate_extra_skipped);
        KBO_PROFILE_END(profile_custom_candidate_extra_skipped, "foreign_policy.candidate.extra_slots_skipped");
    }
    uint32_t effective_limit = KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT + extra_slots;

    if (out_effective_before != NULL) { *out_effective_before = effective_before; }
    if (out_effective_after != NULL) { *out_effective_after = effective_after; }
    if (out_effective_limit != NULL) { *out_effective_limit = effective_limit; }
    if (out_slot_type != NULL) { *out_slot_type = slot_type; }
    if (out_injured_player_id != NULL) { *out_injured_player_id = injured_player_id; }

    int allowed = counts_as_existing_candidate || effective_after <= effective_limit;
    int opportunity_block = allowed
        ? kbo_retention_opportunity_probe_should_block(
            team_id,
            candidate,
            today,
            asian_count,
            non_asian_count,
            pending_asian_count,
            pending_non_asian_count,
            asian_after,
            non_asian_after,
            effective_before,
            effective_after,
            effective_limit,
            candidate_pending,
            counts_as_existing_candidate,
            allowed)
        : 0;
    if (allowed && opportunity_block) {
        allowed = 0;
    }
    KBO_PROFILE_END(profile_custom_candidate, allowed
        ? "foreign_policy.candidate.allowed"
        : "foreign_policy.candidate.blocked");
    return allowed;
}

int kbo_custom_foreign_policy_team_allows_final_signing(
    uint32_t team_id,
    uint8_t* candidate,
    uint32_t* out_effective_before,
    uint32_t* out_effective_after,
    uint32_t* out_effective_limit,
    uint8_t* out_slot_type,
    uint32_t* out_injured_player_id)
{
    if (out_effective_before != NULL) { *out_effective_before = 0u; }
    if (out_effective_after != NULL) { *out_effective_after = 0u; }
    if (out_effective_limit != NULL) { *out_effective_limit = KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT; }
    if (out_slot_type != NULL) { *out_slot_type = 0u; }
    if (out_injured_player_id != NULL) { *out_injured_player_id = 0u; }

    if (team_id == 0u
            || candidate == NULL
            || !memory_range_readable(candidate, OOTP27_PLAYER_SCAN_BYTES)
            || !kbo_player_is_foreign_for_kbo_rights(candidate)) {
        return 0;
    }

    uint32_t foreign_count = 0u;
    uint32_t asian_count = 0u;
    uint32_t non_asian_count = 0u;
    kbo_count_team_asian_quota_probe_fresh(team_id, &foreign_count, &asian_count, &non_asian_count);
    (void)foreign_count;

    uint32_t effective_before = kbo_effective_foreign_count_with_asian_quota(asian_count, non_asian_count);
    uint32_t asian_after = asian_count;
    uint32_t non_asian_after = non_asian_count;
    int already_in_org = kbo_player_current_assignment_matches_team_or_affiliate(candidate, team_id);
    if (!already_in_org) {
        if (kbo_player_is_asian_quota_slot_candidate(candidate)) {
            asian_after++;
        } else {
            non_asian_after++;
        }
    }

    uint8_t slot_type = 0u;
    uint32_t injured_player_id = 0u;
    uint32_t extra_slots = kbo_custom_foreign_policy_extra_slots_for_candidate(
        team_id,
        candidate,
        &slot_type,
        &injured_player_id);
    uint32_t effective_after = kbo_effective_foreign_count_with_asian_quota(asian_after, non_asian_after);
    uint32_t effective_limit = KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT + extra_slots;

    if (out_effective_before != NULL) { *out_effective_before = effective_before; }
    if (out_effective_after != NULL) { *out_effective_after = effective_after; }
    if (out_effective_limit != NULL) { *out_effective_limit = effective_limit; }
    if (out_slot_type != NULL) { *out_slot_type = slot_type; }
    if (out_injured_player_id != NULL) { *out_injured_player_id = injured_player_id; }

    if (already_in_org) {
        return 1;
    }

    uint32_t today = 0u;
    kbo_get_foreign_waiver_current_yyyymmdd(&today);
    int allowed = effective_after <= effective_limit;
    int opportunity_block = allowed
        ? kbo_retention_opportunity_probe_should_block(
            team_id,
            candidate,
            today,
            asian_count,
            non_asian_count,
            0u,
            0u,
            asian_after,
            non_asian_after,
            effective_before,
            effective_after,
            effective_limit,
            0,
            already_in_org,
            allowed)
        : 0;
    return allowed && !opportunity_block;
}

