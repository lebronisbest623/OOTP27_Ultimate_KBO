#include <windows.h>
#include <stdint.h>
#include <string.h>

#include "../../../../../bootstrap/abi/ootp_offsets.h"
#include "../../../../../bootstrap/profiling/profiler.h"
#include "../../../../../core/core_flags/api/flags_api.h"
#include "../../../../../core/logging/core_log.h"
#include "../../../../../offer_candidate/replacement/offer_candidate_replacement_dispatcher.h"
#include "../../../../../runtime_memory/runtime_memory.h"
#include "../../../../../team/assignment/org_query/team_org_assignment_query.h"
#include "../../../../common/dates/foreign_waiver_date.h"
#include "../../../../common/player_eval/foreign_waiver_player_eval.h"
#include "../../../../common/policy/foreign_player_policy.h"
#include "../../../../common/policy/foreign_waiver_policy.h"
#include "../../../../controller/foreign_ai_controller.h"
#include "../../../../quota/candidates/foreign_quota_retention_opportunity_probe.h"
#include "../../../../quota/candidates/retention_score/foreign_quota_retention_score_gate.h"
#include "../../../../rights/query/foreign_waiver_rights_query.h"
#include "../../../../../core/core_flags/keys/runtime_flag_keys.generated.h"

static int kbo_foreign_ai_offer_candidate_priority_enabled(void)
{
    enum { KBO_FOREIGN_AI_OFFER_PRIORITY_FLAG_CACHE_MS = 500u };
    static volatile LONG s_cached_tick = 0;
    static volatile LONG s_cached_enabled = 0;

    DWORD now = GetTickCount();
    LONG cached_tick = InterlockedCompareExchange(&s_cached_tick, 0, 0);
    if (cached_tick != 0
            && (DWORD)(now - (DWORD)cached_tick) <= KBO_FOREIGN_AI_OFFER_PRIORITY_FLAG_CACHE_MS) {
        return InterlockedCompareExchange(&s_cached_enabled, 0, 0) != 0;
    }

    int enabled = (read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_ENABLE_FOREIGN_AI_ROSTER_MANAGEMENT_FILE)
            || kbo_foreign_ai_controller_enabled()
            || read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_ENABLE_KBO_FOREIGN_AI_OFFER_CANDIDATE_PRIORITY_HOOK_FILE))
        && !read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_DISABLE_KBO_FOREIGN_AI_OFFER_CANDIDATE_PRIORITY_HOOK_FILE);
    InterlockedExchange(&s_cached_enabled, enabled ? 1 : 0);
    InterlockedExchange(&s_cached_tick, (LONG)now);
    return enabled;
}

static uint32_t kbo_offer_candidate_priority_team_id(uintptr_t frame_ptr)
{
    if (frame_ptr == 0
            || !memory_range_readable(
                (void*)(frame_ptr + OOTP27_AI_FA_OFFER_FRAME_TEAM_PTR_OFFSET),
                sizeof(uintptr_t))) {
        return 0u;
    }

    uintptr_t team_ptr = *(uintptr_t*)(frame_ptr + OOTP27_AI_FA_OFFER_FRAME_TEAM_PTR_OFFSET);
    if (team_ptr == 0
            || !memory_range_readable((void*)(team_ptr + OOTP27_KBO_TEAM_ID_OFFSET), sizeof(uint32_t))) {
        return 0u;
    }
    return *(uint32_t*)(team_ptr + OOTP27_KBO_TEAM_ID_OFFSET);
}

static uint32_t kbo_offer_candidate_priority_team_league_id(uintptr_t frame_ptr)
{
    if (frame_ptr == 0
            || !memory_range_readable(
                (void*)(frame_ptr + OOTP27_AI_FA_OFFER_FRAME_TEAM_PTR_OFFSET),
                sizeof(uintptr_t))) {
        return 0u;
    }

    uintptr_t team_ptr = *(uintptr_t*)(frame_ptr + OOTP27_AI_FA_OFFER_FRAME_TEAM_PTR_OFFSET);
    if (team_ptr == 0
            || !memory_range_readable((void*)(team_ptr + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET), sizeof(uint32_t))) {
        return 0u;
    }
    return *(uint32_t*)(team_ptr + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
}

static uint32_t kbo_offer_candidate_priority_player_u32(uint8_t* player, uint32_t offset)
{
    if (player == NULL || !memory_range_readable(player + offset, sizeof(uint32_t))) {
        return 0u;
    }
    return *(uint32_t*)(player + offset);
}

static int kbo_offer_candidate_priority_market_retained(
    uint8_t* player,
    uint32_t expected_player_id,
    uint32_t team_id,
    uint32_t today)
{
    if (player == NULL
            || expected_player_id == 0u
            || team_id == 0u
            || today == 0u
            || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }
    if (*(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET) != expected_player_id
            || player[OOTP27_PLAYER_RETIRED_FLAG_OFFSET] != 0u
            || !kbo_player_is_foreign_for_kbo_rights(player)
            || !kbo_has_active_foreign_waiver_right(team_id, expected_player_id, today)
            || kbo_player_current_assignment_matches_team_or_affiliate(player, team_id)) {
        return 0;
    }

    uint32_t current_team_id = kbo_offer_candidate_priority_player_u32(player, OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t active_team_id = kbo_offer_candidate_priority_player_u32(player, OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    uint32_t loan_team_id = kbo_offer_candidate_priority_player_u32(player, OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET);
    uint32_t draft_league_id = kbo_offer_candidate_priority_player_u32(player, OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET);
    if (current_team_id != 0u
            || active_team_id != 0u
            || loan_team_id != 0u
            || draft_league_id != 0u) {
        return 0;
    }
    if (!memory_range_readable(player + OOTP27_PLAYER_FA_DEMAND_SALARY_OFFSET, sizeof(int32_t))
            || !kbo_foreign_policy_demand_salary_plausible(
                *(int32_t*)(player + OOTP27_PLAYER_FA_DEMAND_SALARY_OFFSET))) {
        return 0;
    }

    int32_t score = kbo_foreign_waiver_value_score(player);
    return score >= kbo_get_foreign_waiver_value_threshold_for_player(player);
}

static void kbo_offer_candidate_priority_log(
    const char* reason,
    uint32_t team_id,
    uint32_t today,
    uint32_t original_id,
    uint32_t retained_id,
    int32_t original_score,
    int32_t retained_score,
    int32_t margin)
{
    static volatile LONG log_count = 0;
    LONG slot = InterlockedIncrement(&log_count);
    if (slot > 800) {
        return;
    }
    if (slot > 300
            && (reason == NULL || strcmp(reason, "replaced") != 0)
            && !read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_ENABLE_KBO_CUSTOM_FOREIGN_OFFER_LOGS_FILE)) {
        return;
    }

    kbo_log_runtimef(
        "foreign ai offer candidate priority: reason=%s team=%u original=%u retained=%u original_score=%d retained_score=%d margin=%d today=%u",
        reason != NULL ? reason : "unknown",
        team_id,
        original_id,
        retained_id,
        original_score,
        retained_score,
        margin,
        today);
}

static int kbo_offer_candidate_priority_candidate_slot_context(
    uint32_t team_id,
    uint32_t today,
    uint8_t* candidate,
    uint32_t candidate_id,
    uint32_t* out_effective_after,
    uint32_t* out_effective_limit)
{
    if (out_effective_after != NULL) { *out_effective_after = 0u; }
    if (out_effective_limit != NULL) { *out_effective_limit = 0u; }
    if (team_id == 0u
            || today == 0u
            || candidate == NULL
            || candidate_id == 0u
            || !memory_range_readable(candidate, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    uint32_t effective_limit = KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT;
    if (out_effective_limit != NULL) { *out_effective_limit = effective_limit; }
    if (effective_limit == 0u) {
        return 0;
    }

    uint32_t foreign_count = 0u;
    uint32_t asian_count = 0u;
    uint32_t non_asian_count = 0u;
    kbo_count_team_asian_quota_probe(team_id, &foreign_count, &asian_count, &non_asian_count);
    (void)foreign_count;

    uint32_t pending_asian_count = 0u;
    uint32_t pending_non_asian_count = 0u;
    int candidate_pending = 0;
    kbo_custom_foreign_count_pending_offers(
        team_id,
        today,
        candidate_id,
        &pending_asian_count,
        &pending_non_asian_count,
        &candidate_pending);
    asian_count += pending_asian_count;
    non_asian_count += pending_non_asian_count;

    uint32_t asian_after = asian_count;
    uint32_t non_asian_after = non_asian_count;
    if (!candidate_pending
            && !kbo_player_current_assignment_matches_team_or_affiliate(candidate, team_id)) {
        if (kbo_player_is_asian_quota_slot_candidate(candidate)) {
            asian_after++;
        } else {
            non_asian_after++;
        }
    }

    uint32_t effective_after = kbo_effective_foreign_count_with_asian_quota(
        asian_after,
        non_asian_after);
    if (out_effective_after != NULL) { *out_effective_after = effective_after; }
    return 1;
}

__declspec(noinline) uintptr_t ootp_kbo_foreign_ai_offer_candidate_priority_wrapper(
    uintptr_t frame_ptr,
    uintptr_t candidate_player_ptr)
{
    KBO_HOOK_PROFILE_BEGIN(profile_hook);
    if (candidate_player_ptr == 0
            || !memory_range_readable(
                (void*)candidate_player_ptr,
                OOTP27_PLAYER_NATION_ID_OFFSET + sizeof(uint32_t))) {
        KBO_HOOK_PROFILE_RETURN(profile_hook, "foreign.ai_offer_candidate_priority", candidate_player_ptr);
    }

    uint32_t team_id = kbo_offer_candidate_priority_team_id(frame_ptr);
    uint32_t team_league_id = kbo_offer_candidate_priority_team_league_id(frame_ptr);
    uint32_t today = 0u;
    if (team_id != 0u && kbo_get_foreign_waiver_current_yyyymmdd(&today) && today != 0u) {
        KboOfferCandidateReplacementResult replacement =
            kbo_offer_candidate_replacement_dispatch(
                candidate_player_ptr,
                team_id,
                team_league_id,
                today);
        if (replacement.source != KBO_OFFER_CANDIDATE_REPLACEMENT_NONE) {
            KBO_HOOK_PROFILE_RETURN(
                profile_hook,
                "foreign.ai_offer_candidate_priority",
                replacement.player_ptr);
        }
    }

    if (!kbo_foreign_ai_offer_candidate_priority_enabled()) {
        KBO_HOOK_PROFILE_RETURN(profile_hook, "foreign.ai_offer_candidate_priority", candidate_player_ptr);
    }

    uint8_t* candidate = (uint8_t*)candidate_player_ptr;
    if (!kbo_player_is_foreign_for_kbo_rights(candidate)) {
        KBO_HOOK_PROFILE_RETURN(profile_hook, "foreign.ai_offer_candidate_priority", candidate_player_ptr);
    }
    if (!memory_range_readable((void*)candidate_player_ptr, OOTP27_PLAYER_SCAN_BYTES)) {
        KBO_HOOK_PROFILE_RETURN(profile_hook, "foreign.ai_offer_candidate_priority", candidate_player_ptr);
    }

    if (team_id == 0u || today == 0u) {
        KBO_HOOK_PROFILE_RETURN(profile_hook, "foreign.ai_offer_candidate_priority", candidate_player_ptr);
    }

    uint32_t candidate_id = *(uint32_t*)(candidate + OOTP27_PLAYER_ID_OFFSET);
    if (candidate_id == 0u) {
        KBO_HOOK_PROFILE_RETURN(profile_hook, "foreign.ai_offer_candidate_priority", candidate_player_ptr);
    }
    int candidate_retained_by_team = kbo_has_active_foreign_waiver_right(team_id, candidate_id, today);

    KboForeignRetentionOpportunitySummary opportunity = {0};
    if (!kbo_retention_opportunity_get_summary(team_id, today, &opportunity)
            || opportunity.best_player_id == 0u) {
        KBO_HOOK_PROFILE_RETURN(profile_hook, "foreign.ai_offer_candidate_priority", candidate_player_ptr);
    }
    if (candidate_retained_by_team && candidate_id == opportunity.best_player_id) {
        KBO_HOOK_PROFILE_RETURN(profile_hook, "foreign.ai_offer_candidate_priority", candidate_player_ptr);
    }

    uint8_t* retained = kbo_find_player_by_id(opportunity.best_player_id, NULL, NULL);
    if (!kbo_offer_candidate_priority_market_retained(
            retained,
            opportunity.best_player_id,
            team_id,
            today)) {
        KBO_HOOK_PROFILE_RETURN(profile_hook, "foreign.ai_offer_candidate_priority", candidate_player_ptr);
    }

    if ((uintptr_t)retained == candidate_player_ptr) {
        KBO_HOOK_PROFILE_RETURN(profile_hook, "foreign.ai_offer_candidate_priority", candidate_player_ptr);
    }

    int32_t candidate_score = kbo_foreign_waiver_value_score(candidate);
    int32_t margin = kbo_retention_opportunity_score_margin_for_best(opportunity.best_score);
    int candidate_clears_retained = kbo_retention_candidate_score_clears_best(
        candidate_score,
        opportunity.best_score);
    if (!candidate_retained_by_team) {
        int reserve_active = kbo_retention_candidate_slot_reservation_active(
            opportunity.best_retained_on_yyyymmdd,
            today,
            (uint32_t)kbo_foreign_player_policy()->retention_slot_reserve_days);
        uint32_t effective_after = 0u;
        uint32_t effective_limit = 0u;
        int slot_context_ready = kbo_offer_candidate_priority_candidate_slot_context(
            team_id,
            today,
            candidate,
            candidate_id,
            &effective_after,
            &effective_limit);
        if (!kbo_retention_open_market_candidate_replacement_allowed(
                effective_after,
                effective_limit,
                reserve_active,
                candidate_clears_retained)) {
            const char* reason = "slot_open_after_candidate";
            if (!reserve_active) {
                reason = "retention_reserve_expired";
            } else if (candidate_clears_retained) {
                reason = "candidate_clears_retained_score";
            } else if (!slot_context_ready) {
                reason = "slot_context_unavailable";
            }
            kbo_offer_candidate_priority_log(
                reason,
                team_id,
                today,
                candidate_id,
                opportunity.best_player_id,
                candidate_score,
                opportunity.best_score,
                margin);
            KBO_HOOK_PROFILE_RETURN(profile_hook, "foreign.ai_offer_candidate_priority", candidate_player_ptr);
        }
    } else if (candidate_clears_retained) {
        KBO_HOOK_PROFILE_RETURN(profile_hook, "foreign.ai_offer_candidate_priority", candidate_player_ptr);
    }

    kbo_offer_candidate_priority_log(
        candidate_retained_by_team ? "retained_reordered" : "replaced",
        team_id,
        today,
        candidate_id,
        opportunity.best_player_id,
        candidate_score,
        opportunity.best_score,
        margin);
    KBO_HOOK_PROFILE_RETURN(profile_hook, "foreign.ai_offer_candidate_priority", (uintptr_t)retained);
}
