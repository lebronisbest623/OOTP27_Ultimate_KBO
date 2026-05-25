#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>

#include "foreign_signability_foreign_ai_fa_retained_candidates.h"
#include "../../../../../bootstrap/abi/ootp_offsets.h"
#include "../../../../../core/core_flags/api/flags_api.h"
#include "../../../../../core/logging/core_log.h"
#include "../../../../../runtime_memory/runtime_memory.h"
#include "../../../../../team/assignment/org_query/team_org_assignment_query.h"
#include "../../../../../team/assignment/roster_arrays/team_roster_arrays.h"
#include "../../../../../team/lookup/team_lookup.h"
#include "../../../../common/player_eval/foreign_waiver_player_eval.h"
#include "../../../../common/policy/foreign_player_policy.h"
#include "../../../../controller/foreign_ai_controller.h"
#include "../../../../rights/query/foreign_waiver_rights_query.h"
#include "../../../../../core/core_flags/keys/runtime_flag_keys.generated.h"

int kbo_ai_fa_status_retention_recently_attempted(
    uintptr_t frame_ptr,
    uintptr_t candidate_array,
    uint32_t requester_team_id,
    uint32_t today)
{
    static uintptr_t last_frame_ptr = 0;
    static uintptr_t last_candidate_array = 0;
    static uint32_t last_requester_team_id = 0u;
    static uint32_t last_today = 0u;
    static DWORD last_attempt_tick = 0u;

    DWORD now = GetTickCount();
    if (last_frame_ptr == frame_ptr
            && last_candidate_array == candidate_array
            && last_requester_team_id == requester_team_id
            && last_today == today
            && last_attempt_tick != 0u
            && now - last_attempt_tick < 1000u) {
        return 1;
    }

    last_frame_ptr = frame_ptr;
    last_candidate_array = candidate_array;
    last_requester_team_id = requester_team_id;
    last_today = today;
    last_attempt_tick = now;
    return 0;
}

int kbo_ai_fa_status_retention_priority_enabled(void)
{
    enum { KBO_RETENTION_PRIORITY_FLAG_CACHE_MS = 500u };
    static volatile LONG s_cached_tick = 0;
    static volatile LONG s_cached_enabled = 0;

    DWORD now = GetTickCount();
    LONG cached_tick = InterlockedCompareExchange(&s_cached_tick, 0, 0);
    if (cached_tick != 0
            && (DWORD)(now - (DWORD)cached_tick) <= KBO_RETENTION_PRIORITY_FLAG_CACHE_MS) {
        return InterlockedCompareExchange(&s_cached_enabled, 0, 0) != 0;
    }

    int enabled = read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_ENABLE_FOREIGN_AI_ROSTER_MANAGEMENT_FILE)
        || kbo_foreign_ai_controller_enabled();
    InterlockedExchange(&s_cached_enabled, enabled ? 1 : 0);
    InterlockedExchange(&s_cached_tick, (LONG)now);
    return enabled;
}

static uint32_t kbo_ai_fa_status_player_u32(uint8_t* player, uint32_t offset)
{
    if (player == NULL || !memory_range_readable(player + offset, sizeof(uint32_t))) {
        return 0u;
    }
    return *(uint32_t*)(player + offset);
}

static uint8_t kbo_ai_fa_status_player_u8(uint8_t* player, uint32_t offset)
{
    if (player == NULL || !memory_range_readable(player + offset, sizeof(uint8_t))) {
        return 0u;
    }
    return player[offset];
}

static int32_t kbo_ai_fa_status_player_i32(uint8_t* player, uint32_t offset)
{
    if (player == NULL || !memory_range_readable(player + offset, sizeof(int32_t))) {
        return 0;
    }
    return *(int32_t*)(player + offset);
}

static void kbo_ai_fa_status_refresh_candidate_assignment(
    uint8_t* player,
    uint32_t requester_team_id,
    KboAiFaStatusRetainedCandidate* candidate)
{
    if (player == NULL || candidate == NULL) {
        return;
    }
    candidate->current_team_id = kbo_ai_fa_status_player_u32(player, OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    candidate->active_team_id = kbo_ai_fa_status_player_u32(player, OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    candidate->original_team_id = kbo_ai_fa_status_player_u32(player, OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET);
    candidate->default_team_id = kbo_ai_fa_status_player_u32(player, OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET);
    candidate->loan_team_id = kbo_ai_fa_status_player_u32(player, OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET);
    candidate->draft_league_id = kbo_ai_fa_status_player_u32(player, OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET);
    candidate->contract_level = kbo_ai_fa_status_player_u8(player, OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET);
    candidate->fa_demand = kbo_ai_fa_status_player_i32(player, OOTP27_PLAYER_FA_DEMAND_SALARY_OFFSET);
    candidate->already_in_org = kbo_player_current_assignment_matches_team_or_affiliate(player, requester_team_id);
    candidate->market_free_agent =
        candidate->current_team_id == 0u
        && candidate->loan_team_id == 0u
        && candidate->draft_league_id == 0u;
    candidate->holder_org_candidate =
        candidate->loan_team_id == 0u
        && (candidate->already_in_org
            || (candidate->current_team_id == 0u
                && candidate->active_team_id == requester_team_id));
}

static int kbo_ai_fa_status_restore_rights_only_assignment(
    uint8_t* player,
    uint32_t requester_team_id,
    uint32_t today,
    KboAiFaStatusRetainedCandidate* candidate)
{
    if (player == NULL || candidate == NULL
            || requester_team_id == 0u
            || today == 0u
            || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)
            || candidate->contract_level == 1u
            || candidate->current_team_id != requester_team_id
            || candidate->active_team_id != requester_team_id
            || !kbo_has_active_foreign_waiver_right(requester_team_id, candidate->player_id, today)) {
        return 0;
    }

    uint8_t* team = find_kbo_team_by_numeric_id_any_league(requester_team_id, 1);
    if (team == NULL || !memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return 0;
    }
    uint32_t league_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
    if (league_id == 0u) {
        return 0;
    }

    uint32_t before_current = candidate->current_team_id;
    uint32_t before_active = candidate->active_team_id;
    uint32_t before_original = candidate->original_team_id;
    uint32_t before_default = candidate->default_team_id;
    uint32_t before_league = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
    uint32_t before_draft = candidate->draft_league_id;
    uint8_t before_level = candidate->contract_level;
    uint8_t before_restricted = player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET];
    uint8_t before_secondary = player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET];
    uint8_t before_dfa = player[OOTP27_PLAYER_DFA_FLAG_OFFSET];

    int removed_roster = kbo_remove_player_id_from_known_team_roster_arrays(team, candidate->player_id);
    *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET) = 0u;
    *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET) = 0u;
    if (*(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET) == requester_team_id) {
        *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET) = 0u;
    }
    if (*(uint32_t*)(player + OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET) == league_id) {
        *(uint32_t*)(player + OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET) = 0u;
    }
    if (*(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET) == 0u) {
        *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET) = requester_team_id;
    }
    if (*(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_LEAGUE_ID_OFFSET) == 0u) {
        *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_LEAGUE_ID_OFFSET) = league_id;
    }
    if (memory_range_readable(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET, sizeof(uint32_t))
            && before_default != 0u
            && before_default != requester_team_id) {
        *(uint32_t*)(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET) = 0u;
    }
    player[OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET] = 0u;
    player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET] = 1u;
    player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET] = 1u;
    player[OOTP27_PLAYER_DFA_FLAG_OFFSET] = 0u;
    int added_restricted = kbo_add_player_id_to_team_fixed_array(
        team,
        OOTP27_TEAM_RESTRICTED_PLAYER_IDS_OFFSET,
        candidate->player_id);
    kbo_ai_fa_status_refresh_candidate_assignment(player, requester_team_id, candidate);

    static volatile LONG repair_log_count = 0;
    LONG slot = InterlockedIncrement(&repair_log_count);
    if (slot <= 200) {
        kbo_log_runtimef(
            "foreign retention priority: repaired rights-only assignment team=%u player=%u today=%u before_current=%u before_active=%u before_original=%u before_default=%u before_league=%u before_draft=%u before_level=%u before_restricted=%u before_secondary=%u before_dfa=%u after_current=%u after_active=%u after_original=%u after_default=%u after_league=%u after_draft=%u after_level=%u after_restricted=%u after_secondary=%u after_dfa=%u removed_roster=%d added_restricted=%d",
            requester_team_id,
            candidate->player_id,
            today,
            before_current,
            before_active,
            before_original,
            before_default,
            before_league,
            before_draft,
            (uint32_t)before_level,
            (uint32_t)before_restricted,
            (uint32_t)before_secondary,
            (uint32_t)before_dfa,
            candidate->current_team_id,
            candidate->active_team_id,
            candidate->original_team_id,
            candidate->default_team_id,
            *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET),
            candidate->draft_league_id,
            (uint32_t)candidate->contract_level,
            (uint32_t)player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET],
            (uint32_t)player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET],
            (uint32_t)player[OOTP27_PLAYER_DFA_FLAG_OFFSET],
            removed_roster,
            added_restricted);
    }
    return 1;
}

static int kbo_ai_fa_status_retained_market_demand_ready(
    uint8_t* player,
    KboAiFaStatusRetainedCandidate* candidate)
{
    if (player == NULL || candidate == NULL
            || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)
            || !memory_range_readable(player + OOTP27_PLAYER_FA_DEMAND_SALARY_OFFSET, sizeof(int32_t))) {
        return 0;
    }

    candidate->fa_demand = *(int32_t*)(player + OOTP27_PLAYER_FA_DEMAND_SALARY_OFFSET);
    if (kbo_foreign_policy_demand_salary_plausible(candidate->fa_demand)) {
        return 1;
    }

    return 0;
}

void kbo_ai_fa_status_log_retained_candidate_eval(
    uint32_t requester_team_id, uint32_t today, const KboAiFaStatusRetainedCandidate* candidate, int can_enter)
{
    static volatile LONG eval_log_count = 0;
    LONG slot = InterlockedIncrement(&eval_log_count);
    if (slot > 600 || candidate == NULL) {
        return;
    }

    kbo_log_runtimef(
        "foreign retention priority: candidate_eval team=%u player=%u can_enter=%d reason=%s score=%d threshold=%d demand=%d pos=%u/%u asian=%u in_org=%d market=%d holder_org=%d current=%u active=%u original=%u default=%u loan=%u draft=%u level=%u today=%u",
        requester_team_id,
        candidate->player_id,
        can_enter,
        candidate->reject_reason != NULL ? candidate->reject_reason : "ok",
        candidate->score,
        candidate->threshold,
        candidate->fa_demand,
        (uint32_t)candidate->position_group,
        (uint32_t)candidate->position_role,
        (uint32_t)candidate->asian,
        candidate->already_in_org,
        candidate->market_free_agent,
        candidate->holder_org_candidate,
        candidate->current_team_id,
        candidate->active_team_id,
        candidate->original_team_id,
        candidate->default_team_id,
        candidate->loan_team_id,
        candidate->draft_league_id,
        (uint32_t)candidate->contract_level,
        today);
}

int kbo_ai_fa_status_evaluate_retained_market_candidate(
    uint8_t* player, uint32_t expected_player_id, uint32_t requester_team_id,
    uint32_t today, KboAiFaStatusRetainedCandidate* out_candidate)
{
    KboAiFaStatusRetainedCandidate candidate = {0};
    candidate.player_id = expected_player_id;
    candidate.reject_reason = "ok";
    if (out_candidate != NULL) {
        *out_candidate = candidate;
    }

    if (player == NULL
            || requester_team_id == 0u
            || today == 0u) {
        candidate.reject_reason = "bad_args";
        if (out_candidate != NULL) { *out_candidate = candidate; }
        return 0;
    }
    if (!memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        candidate.reject_reason = "unreadable";
        if (out_candidate != NULL) { *out_candidate = candidate; }
        return 0;
    }

    candidate.player_ptr = (uintptr_t)player;
    candidate.player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    candidate.current_team_id = kbo_ai_fa_status_player_u32(player, OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    candidate.active_team_id = kbo_ai_fa_status_player_u32(player, OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    candidate.original_team_id = kbo_ai_fa_status_player_u32(player, OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET);
    candidate.default_team_id = kbo_ai_fa_status_player_u32(player, OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET);
    candidate.loan_team_id = kbo_ai_fa_status_player_u32(player, OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET);
    candidate.draft_league_id = kbo_ai_fa_status_player_u32(player, OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET);
    candidate.position_group = player[OOTP27_PLAYER_POSITION_GROUP_OFFSET];
    candidate.position_role = player[OOTP27_PLAYER_POSITION_ROLE_OFFSET];
    candidate.asian = kbo_player_is_asian_quota_slot_candidate(player) ? 1u : 0u;
    candidate.contract_level = kbo_ai_fa_status_player_u8(player, OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET);
    candidate.fa_demand = kbo_ai_fa_status_player_i32(player, OOTP27_PLAYER_FA_DEMAND_SALARY_OFFSET);
    kbo_ai_fa_status_refresh_candidate_assignment(player, requester_team_id, &candidate);

    if (candidate.player_id != expected_player_id) {
        candidate.reject_reason = "id_mismatch";
        if (out_candidate != NULL) { *out_candidate = candidate; }
        return 0;
    }
    if (player[OOTP27_PLAYER_RETIRED_FLAG_OFFSET] != 0u) {
        candidate.reject_reason = "retired";
        if (out_candidate != NULL) { *out_candidate = candidate; }
        return 0;
    }
    if (!kbo_player_is_foreign_for_kbo_rights(player)) {
        candidate.reject_reason = "not_foreign";
        if (out_candidate != NULL) { *out_candidate = candidate; }
        return 0;
    }
    if (!kbo_has_active_foreign_waiver_right(requester_team_id, expected_player_id, today)) {
        candidate.reject_reason = "no_active_right";
        if (out_candidate != NULL) { *out_candidate = candidate; }
        return 0;
    }
    kbo_ai_fa_status_restore_rights_only_assignment(
        player,
        requester_team_id,
        today,
        &candidate);

    if (!candidate.already_in_org && candidate.market_free_agent) {
        (void)kbo_ai_fa_status_retained_market_demand_ready(player, &candidate);
    }
    const char* gate_reason = NULL;
    if (!kbo_ai_fa_status_retained_candidate_force_gate(&candidate, &gate_reason)) {
        candidate.reject_reason = gate_reason != NULL ? gate_reason : "not_market_forceable";
        if (out_candidate != NULL) { *out_candidate = candidate; }
        return 0;
    }

    candidate.score = kbo_foreign_waiver_value_score(player);
    candidate.threshold = kbo_get_foreign_waiver_value_threshold_for_player(player);
    if (candidate.score < candidate.threshold) {
        candidate.reject_reason = "below_threshold";
        if (out_candidate != NULL) { *out_candidate = candidate; }
        return 0;
    }

    if (out_candidate != NULL) { *out_candidate = candidate; }
    return 1;
}

void kbo_ai_fa_status_sort_retained_candidates(KboAiFaStatusRetainedCandidate* candidates, int count)
{
    if (candidates == NULL || count <= 1) {
        return;
    }

    for (int i = 0; i < count - 1; i++) {
        int best = i;
        for (int j = i + 1; j < count; j++) {
            if (candidates[j].score > candidates[best].score) {
                best = j;
            }
        }
        if (best != i) {
            KboAiFaStatusRetainedCandidate tmp = candidates[i];
            candidates[i] = candidates[best];
            candidates[best] = tmp;
        }
    }
}
