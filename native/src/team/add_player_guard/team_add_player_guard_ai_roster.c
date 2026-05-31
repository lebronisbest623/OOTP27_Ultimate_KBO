#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>

#include "../../bootstrap/abi/hook_entrypoints.h"
#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../bootstrap/profiling/profiler.h"
#include "../../core/core_flags/api/flags_api.h"
#include "../../core/logging/core_log.h"
#include "../../foreign/common/policy/foreign_waiver_policy.h"
#include "../../foreign/common/player_eval/foreign_waiver_player_eval.h"
#include "../../foreign/quota/team_policy/foreign_quota_team_policy.h"
#include "../../runtime_memory/runtime_memory.h"
#include "../lookup/team_lookup.h"
#include "ai_roster/internal/team_add_player_guard_ai_roster_internal.h"
#include "team_add_player_guard.h"
#include "team_add_player_guard_ai_roster.h"
#include "../../core/core_flags/keys/runtime_flag_keys.generated.h"

static OotpKboAiRosterSelectFn g_kbo_ai_roster_select_trace_trampoline = NULL;
static OotpKboAiRosterContextFlowFn g_kbo_ai_roster_primary_apply_flow_trace_trampoline = NULL;
static OotpKboAiRosterApplySelectionFn g_kbo_ai_roster_apply_selection_trace_trampoline = NULL;

static int32_t kbo_read_ai_roster_select_score(uintptr_t player_ptr, uint32_t offset)
{
    if (player_ptr == 0u
            || !kbo_player_pointer_plausible(player_ptr)
            || !memory_range_readable((void*)(player_ptr + offset), sizeof(int32_t))) {
        return 0;
    }
    return *(int32_t*)(player_ptr + offset);
}

void kbo_set_ai_roster_select_trace_trampoline(void* trampoline)
{
    g_kbo_ai_roster_select_trace_trampoline = (OotpKboAiRosterSelectFn)(uintptr_t)trampoline;
}

void kbo_set_ai_roster_primary_apply_flow_trace_trampoline(void* trampoline)
{
    g_kbo_ai_roster_primary_apply_flow_trace_trampoline =
        (OotpKboAiRosterContextFlowFn)(uintptr_t)trampoline;
}

void kbo_set_ai_roster_apply_selection_trace_trampoline(void* trampoline)
{
    g_kbo_ai_roster_apply_selection_trace_trampoline =
        (OotpKboAiRosterApplySelectionFn)(uintptr_t)trampoline;
}

static int kbo_ai_roster_foreign_release_pressure_allows_native_replace(
    uint32_t team_id,
    uint32_t outgoing_player_id,
    uint32_t incoming_player_id,
    int32_t target_slot,
    int32_t roster_code,
    uint32_t* out_effective_count,
    int32_t* out_outgoing_score,
    int32_t* out_threshold)
{
    if (out_effective_count != NULL) { *out_effective_count = 0u; }
    if (out_outgoing_score != NULL) { *out_outgoing_score = 0; }
    if (out_threshold != NULL) { *out_threshold = 0; }

    if (!read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_ENABLE_FOREIGN_AI_ROSTER_MANAGEMENT_FILE)
            || team_id == 0u
            || outgoing_player_id == 0u
            || incoming_player_id == 0u) {
        return 0;
    }

    uint8_t* outgoing = kbo_find_player_by_id(outgoing_player_id, NULL, NULL);
    if (outgoing == NULL
            || !memory_range_readable(outgoing, OOTP27_PLAYER_SCAN_BYTES)
            || !kbo_player_is_foreign_for_kbo_rights(outgoing)) {
        return 0;
    }

    uint32_t foreign_count = 0u;
    uint32_t asian_count = 0u;
    uint32_t non_asian_count = 0u;
    kbo_count_team_asian_quota_probe(team_id, &foreign_count, &asian_count, &non_asian_count);
    (void)foreign_count;
    uint32_t effective_count = kbo_foreign_quota_effective_count_for_team(
        team_id,
        asian_count,
        non_asian_count);
    uint32_t effective_limit = kbo_foreign_quota_base_effective_limit_for_team(team_id);
    if (out_effective_count != NULL) { *out_effective_count = effective_count; }
    if (effective_count < effective_limit) {
        return 0;
    }

    int32_t outgoing_score = kbo_foreign_waiver_value_score(outgoing);
    int32_t threshold = kbo_get_foreign_waiver_value_threshold_for_player(outgoing);
    if (out_outgoing_score != NULL) { *out_outgoing_score = outgoing_score; }
    if (out_threshold != NULL) { *out_threshold = threshold; }
    if (outgoing_score >= threshold) {
        return 0;
    }

    static volatile LONG pressure_log_count = 0;
    LONG slot = InterlockedIncrement(&pressure_log_count);
    if (slot <= 300) {
        kbo_log_runtimef(
            "foreign release pressure: allow_native_replace team=%u outgoing=%u incoming=%u target_slot=%d roster_code=%d effective=%u limit=%u outgoing_score=%d threshold=%d",
            team_id,
            outgoing_player_id,
            incoming_player_id,
            target_slot,
            roster_code,
            effective_count,
            effective_limit,
            outgoing_score,
            threshold);
    }
    return 1;
}

__declspec(noinline) uintptr_t ootp_kbo_ai_roster_select_trace_wrapper(
    uintptr_t context_ptr,
    int32_t slot_index,
    int32_t depth_hint)
{
    KBO_HOOK_PROFILE_BEGIN(profile_hook);
    uintptr_t caller_ptr = (uintptr_t)__builtin_return_address(0);
    HMODULE host_exe = GetModuleHandleA(NULL);
    uint32_t caller_rva = host_exe != NULL && caller_ptr >= (uintptr_t)host_exe
        ? (uint32_t)(caller_ptr - (uintptr_t)host_exe)
        : 0u;

    OotpKboAiRosterSelectFn original = g_kbo_ai_roster_select_trace_trampoline;
    uintptr_t result_ptr = 0u;
    if (original != NULL) {
        KBO_HOOK_PROFILE_PAUSE(profile_hook);
        result_ptr = original(context_ptr, slot_index, depth_hint);
        KBO_HOOK_PROFILE_RESUME(profile_hook);
    }

    uintptr_t slot_block_ptr = 0u;
    uintptr_t source_vector_ptr = 0u;
    int32_t source_count = -1;
    if (context_ptr != 0u && slot_index >= 0 && slot_index < 64) {
        slot_block_ptr = kbo_ai_roster_context_slot_block(context_ptr, (uint16_t)slot_index);
        if (slot_block_ptr != 0u) {
            source_vector_ptr = slot_block_ptr + KBO_AI_ROSTER_SLOT_BLOCK_CANDIDATE_VECTOR_OFFSET;
            source_count = kbo_pointer_vector_count(source_vector_ptr);
        }
    }

    int32_t rescue_source_index = -1;
    uint32_t rescue_active_team_id = 0u;
    int64_t rescue_score = 0;
    KboAiRosterForeignCandidateSummary rescue_summary;
    uintptr_t rescue_ptr = kbo_ai_roster_choose_source_select_rescue_candidate(
        source_vector_ptr,
        source_count,
        result_ptr,
        &rescue_source_index,
        &rescue_active_team_id,
        &rescue_score,
        &rescue_summary);
    if (rescue_ptr == 0u || rescue_ptr == result_ptr) {
        KBO_HOOK_PROFILE_RETURN(profile_hook, "team.ai_roster_select_trace", result_ptr);
    }
    uint32_t native_player_id = 0u;
    int native_foreign = 0;
    int32_t native_score_fe0 = 0;
    int32_t native_score_fe4 = 0;
    int16_t native_overall = 0;
    int16_t native_ratings = 0;
    if (kbo_player_pointer_plausible(result_ptr)
            && memory_range_readable((void*)result_ptr, OOTP27_PLAYER_SCAN_BYTES)) {
        uint8_t* native_player = (uint8_t*)result_ptr;
        native_player_id = *(uint32_t*)(native_player + OOTP27_PLAYER_ID_OFFSET);
        native_foreign = kbo_player_is_foreign_for_kbo_rights(native_player);
        native_score_fe0 = kbo_read_ai_roster_select_score(result_ptr, KBO_AI_ROSTER_SELECT_SCORE_FE0_OFFSET);
        native_score_fe4 = kbo_read_ai_roster_select_score(result_ptr, KBO_AI_ROSTER_SELECT_SCORE_FE4_OFFSET);
        native_overall = kbo_read_player_i16(native_player, OOTP27_PLAYER_OVERALL_VALUE_OFFSET);
        native_ratings = kbo_read_player_i16(native_player, OOTP27_PLAYER_RATINGS_VALUE_OFFSET);
    }

    static volatile LONG rescue_log_count = 0;
    LONG rescue_slot = InterlockedIncrement(&rescue_log_count);
    kbo_log_runtimef(
        "ootp ai roster foreign source-select rescue #%ld caller_rva=0x%x context=%p slot_index=%d depth_hint=%d source_count=%d native=%u native_foreign=%d native_fe0=%d native_fe4=%d native_overall=%d native_ratings=%d override=%u source_idx=%d active_team=%u score=%lld current=%u active=%u league=%u default_team=%u status24=%u status25=%u status26=%u f25=%u f62=%u f65=%u f06=%d fe0=%d fe4=%d overall=%d talent=%d ratings=%d",
        rescue_slot,
        caller_rva,
        (void*)context_ptr,
        slot_index,
        depth_hint,
        source_count,
        native_player_id,
        native_foreign,
        native_score_fe0,
        native_score_fe4,
        native_overall,
        native_ratings,
        rescue_summary.player_id,
        rescue_source_index,
        rescue_active_team_id,
        (long long)rescue_score,
        rescue_summary.current_team_id,
        rescue_summary.active_team_id,
        rescue_summary.league_id,
        rescue_summary.default_team_id,
        rescue_summary.status24,
        rescue_summary.status25,
        rescue_summary.status26,
        rescue_summary.f25,
        rescue_summary.f62,
        rescue_summary.f65,
        rescue_summary.f06,
        rescue_summary.score_fe0,
        rescue_summary.score_fe4,
        rescue_summary.overall,
        rescue_summary.talent,
        rescue_summary.ratings);
    KBO_HOOK_PROFILE_RETURN(profile_hook, "team.ai_roster_select_trace", rescue_ptr);
}

__declspec(noinline) void ootp_kbo_ai_roster_primary_apply_flow_trace_wrapper(
    uintptr_t context_ptr)
{
    KBO_HOOK_PROFILE_BEGIN(profile_hook);
    OotpKboAiRosterContextFlowFn original = g_kbo_ai_roster_primary_apply_flow_trace_trampoline;
    if (!kbo_ai_roster_foreign_apply_rescue_enabled()) {
        if (original != NULL) {
            KBO_HOOK_PROFILE_PAUSE(profile_hook);
            original(context_ptr);
            KBO_HOOK_PROFILE_RESUME(profile_hook);
        }
        KBO_HOOK_PROFILE_END(profile_hook, "team.ai_roster_primary_apply_flow");
        return;
    }

    KboAiRosterFlowContextSnapshot before;
    KboAiRosterFlowContextSnapshot after;
    kbo_ai_roster_flow_read_context(context_ptr, &before);

    if (original != NULL) {
        KBO_HOOK_PROFILE_PAUSE(profile_hook);
        original(context_ptr);
        KBO_HOOK_PROFILE_RESUME(profile_hook);
    }

    kbo_ai_roster_flow_read_context(context_ptr, &after);
    if (kbo_ai_roster_context_flow_apply_rescue(
            context_ptr,
            &before,
            &after,
            g_kbo_ai_roster_apply_selection_trace_trampoline)) {
        kbo_ai_roster_flow_read_context(context_ptr, &after);
    }
    KBO_HOOK_PROFILE_END(profile_hook, "team.ai_roster_primary_apply_flow");
}

__declspec(noinline) void ootp_kbo_ai_roster_apply_selection_trace_wrapper(
    uintptr_t context_ptr,
    int32_t slot_index,
    uintptr_t player_ptr,
    int32_t target_slot,
    int32_t roster_code)
{
    KBO_HOOK_PROFILE_BEGIN(profile_hook);
    OotpKboAiRosterApplySelectionFn original = g_kbo_ai_roster_apply_selection_trace_trampoline;
    if (!kbo_ai_roster_foreign_apply_rescue_enabled()) {
        if (original != NULL) {
            KBO_HOOK_PROFILE_PAUSE(profile_hook);
            original(context_ptr, slot_index, player_ptr, target_slot, roster_code);
            KBO_HOOK_PROFILE_RESUME(profile_hook);
        }
        KBO_HOOK_PROFILE_END(profile_hook, "team.ai_roster_apply_selection");
        return;
    }

    uintptr_t slot_block_ptr = 0u;
    uint32_t slot_team_id = 0u;
    if (context_ptr != 0u && slot_index >= 0 && slot_index < 64) {
        slot_block_ptr = kbo_ai_roster_context_slot_block(context_ptr, (uint16_t)slot_index);
        slot_team_id = kbo_ai_roster_context_slot_team_id(context_ptr, (uint16_t)slot_index);
    }

    uint32_t before_slot_code = 0u;
    uint32_t before_slot_player_id = 0u;
    if (slot_block_ptr != 0u && target_slot >= 0 && target_slot <= KBO_AI_ROSTER_SLOT_MAX) {
        before_slot_code = kbo_ai_roster_slot_code_at(slot_block_ptr, (uint16_t)target_slot);
        before_slot_player_id = kbo_ai_roster_slot_player_at(slot_block_ptr, (uint16_t)target_slot);
    }

    int player_plausible = kbo_player_pointer_plausible(player_ptr)
        && memory_range_readable((void*)player_ptr, OOTP27_PLAYER_SCAN_BYTES);
    uint8_t* player = player_plausible ? (uint8_t*)player_ptr : NULL;
    uint32_t player_id = player_plausible ? *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET) : 0u;
    int incoming_foreign = player_plausible && kbo_player_is_foreign_for_kbo_rights(player);
    DWORD rescue_age_ms = 0u;
    uint32_t release_pressure_effective = 0u;
    int32_t release_pressure_score = 0;
    int32_t release_pressure_threshold = 0;
    int release_pressure_allows_replace = 0;
    if (slot_team_id != 0u
            && before_slot_player_id != 0u
            && player_id != 0u
            && !incoming_foreign) {
        release_pressure_allows_replace =
            kbo_ai_roster_foreign_release_pressure_allows_native_replace(
                slot_team_id,
                before_slot_player_id,
                player_id,
                target_slot,
                roster_code,
                &release_pressure_effective,
                &release_pressure_score,
                &release_pressure_threshold);
    }

    if (original != NULL
            && slot_block_ptr != 0u
            && target_slot >= 0
            && target_slot <= KBO_AI_ROSTER_SLOT_MAX
            && player_plausible
            && player_id != 0u
            && !incoming_foreign
            && before_slot_player_id != 0u
            && before_slot_player_id != player_id
            && !release_pressure_allows_replace
            && kbo_ai_roster_recent_foreign_apply_rescue_match(
                context_ptr,
                slot_block_ptr,
                (uint16_t)target_slot,
                before_slot_player_id,
                &rescue_age_ms)) {
        uintptr_t caller_ptr = (uintptr_t)__builtin_return_address(0);
        HMODULE host_exe = GetModuleHandleA(NULL);
        uint32_t caller_rva = host_exe != NULL && caller_ptr >= (uintptr_t)host_exe
            ? (uint32_t)(caller_ptr - (uintptr_t)host_exe)
            : 0u;

        static volatile LONG shield_log_count = 0;
        LONG shield_slot = InterlockedIncrement(&shield_log_count);
        if (shield_slot <= 1000) {
            kbo_log_runtimef(
                "ootp ai roster foreign apply rescue shield #%ld caller_rva=0x%x context=%p slot_index=%d target_slot=%d roster_code=%d age_ms=%lu slot_block=%p slot_team=%u before_slot_code=%u before_slot_player=%u incoming_player=%u incoming_foreign=%d incoming_nation=%u incoming_current=%u incoming_active=%u incoming_league=%u incoming_status24=%u incoming_status25=%u incoming_status26=%u incoming_f62=%u incoming_f65=%u incoming_f68=%u incoming_f1a=%u incoming_score_fe0=%d incoming_score_fe4=%d incoming_overall=%d incoming_ratings=%d",
                shield_slot,
                caller_rva,
                (void*)context_ptr,
                slot_index,
                target_slot,
                roster_code,
                (unsigned long)rescue_age_ms,
                (void*)slot_block_ptr,
                slot_team_id,
                before_slot_code,
                before_slot_player_id,
                player_id,
                incoming_foreign,
                *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET),
                *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET),
                *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET),
                *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET),
                0u,
                0u,
                0u,
                (uint32_t)player[0xf62u],
                (uint32_t)player[0xf65u],
                (uint32_t)player[0xf68u],
                (uint32_t)player[0xf1au],
                kbo_read_ai_roster_select_score(player_ptr, KBO_AI_ROSTER_SELECT_SCORE_FE0_OFFSET),
                kbo_read_ai_roster_select_score(player_ptr, KBO_AI_ROSTER_SELECT_SCORE_FE4_OFFSET),
                kbo_read_player_i16(player, OOTP27_PLAYER_OVERALL_VALUE_OFFSET),
                kbo_read_player_i16(player, OOTP27_PLAYER_RATINGS_VALUE_OFFSET));
        }
        KBO_HOOK_PROFILE_RETURN_VOID(profile_hook, "team.ai_roster_apply_selection");
    }

    if (release_pressure_allows_replace) {
        static volatile LONG release_pressure_apply_log_count = 0;
        LONG slot = InterlockedIncrement(&release_pressure_apply_log_count);
        if (slot <= 300) {
            kbo_log_runtimef(
                "foreign release pressure: shield_bypassed context=%p slot_index=%d target_slot=%d roster_code=%d slot_team=%u outgoing=%u incoming=%u effective=%u outgoing_score=%d threshold=%d",
                (void*)context_ptr,
                slot_index,
                target_slot,
                roster_code,
                slot_team_id,
                before_slot_player_id,
                player_id,
                release_pressure_effective,
                release_pressure_score,
                release_pressure_threshold);
        }
    }

    if (original != NULL) {
        KBO_HOOK_PROFILE_PAUSE(profile_hook);
        original(context_ptr, slot_index, player_ptr, target_slot, roster_code);
        KBO_HOOK_PROFILE_RESUME(profile_hook);
    }
    KBO_HOOK_PROFILE_END(profile_hook, "team.ai_roster_apply_selection");
}
