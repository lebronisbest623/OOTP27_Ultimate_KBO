#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>

#include "../../amateur_player_quality/api/amateur_player_quality.h"
#include "../../bootstrap/abi/hook_entrypoints.h"
#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../bootstrap/profiling/profiler.h"
#include "../../core/core_flags/api/flags_api.h"
#include "../../core/logging/core_log.h"
#include "../../military_service/players/guards/military_team_add_guard.h"
#include "../../military_service/players/team_policy/military_service_team_policy.h"
#include "../../runtime_memory/runtime_memory.h"
#include "../../foreign/common/player_eval/foreign_waiver_player_eval.h"
#include "../../foreign/quota/counts/foreign_quota_counts.h"
#include "../../foreign/signability/api/foreign_signability_salary_floor.h"
#include "../assignment/org_query/team_org_assignment_query.h"
#include "../lookup/team_lookup.h"
#include "foreign_policy/purchase_restore/team_add_player_guard_foreign_purchase_restore.h"
#include "foreign_policy/retention_trace/team_add_player_guard_foreign_retention_trace.h"
#include "internal/team_add_player_guard_internal.h"
#include "original_call/team_add_player_guard_original_call.h"
#include "team_add_player_guard.h"
#include "team_add_player_guard_ai_roster.h"

__declspec(noinline) uint8_t ootp_kbo_team_add_player_guard_wrapper(
    uintptr_t team_ptr,
    uintptr_t player_ptr,
    uintptr_t arg3,
    uintptr_t arg4,
    uintptr_t arg5,
    uintptr_t arg6,
    uintptr_t arg7,
    uintptr_t arg8)
{
    KBO_HOOK_PROFILE_BEGIN(profile_hook);
    KBO_PROFILE_BEGIN(profile_team_add_guard_wrapper);
    uintptr_t caller_ptr = (uintptr_t)__builtin_return_address(0);
    HMODULE host_exe = GetModuleHandleA(NULL);
    uint32_t caller_rva = host_exe != NULL && caller_ptr >= (uintptr_t)host_exe
        ? (uint32_t)(caller_ptr - (uintptr_t)host_exe)
        : 0u;

    KboTeamAddPlayerOriginalFn original = kbo_team_add_player_guard_get_original();
    if (original == NULL) {
        KBO_PROFILE_END(profile_team_add_guard_wrapper, "team_add_guard.no_original");
        KBO_HOOK_PROFILE_RETURN(profile_hook, "team.add_player_guard", 0u);
    }

    int team_readable = team_ptr != 0
        && memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES);
    int original_args_readable = kbo_team_add_original_args_readable(team_ptr, player_ptr);
    int player_plausible = original_args_readable;
    uint8_t* team = team_readable ? (uint8_t*)team_ptr : NULL;
    uint8_t* player = player_plausible ? (uint8_t*)player_ptr : NULL;
    uint32_t team_id = team_readable
        ? *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET)
        : 0u;
    int is_military_team = team_id != 0u && kbo_team_id_is_military_service_team(team_id);
    int amateur_generation_call = kbo_amateur_generation_team_add_caller(caller_rva);

    if (!original_args_readable) {
        kbo_team_add_log_skipped_bad_original_args(
            caller_rva,
            "entry",
            team_ptr,
            player_ptr,
            team_ptr);
        KBO_PROFILE_END(profile_team_add_guard_wrapper, "team_add_guard.bad_args");
        KBO_HOOK_PROFILE_RETURN(profile_hook, "team.add_player_guard", 0u);
    }

    uint32_t before_current_team_id = 0u;
    uint32_t before_active_team_id = 0u;
    uint32_t before_original_team_id = 0u;
    uint32_t before_loan_team_id = 0u;
    if (player_plausible) {
        before_current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        before_active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
        before_loan_team_id = *(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET);
        if (memory_range_readable(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET, sizeof(uint32_t))) {
            before_original_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET);
        }
    }

    if (is_military_team && kbo_military_team_add_player_should_block(team_ptr, player_ptr)) {
        if (player != NULL
                && before_current_team_id == 0u
                && before_active_team_id == 0u
                && before_original_team_id != 0u) {
            kbo_team_add_restore_source_team_after_blocked_foreign_purchase(
                player,
                before_original_team_id,
                team_id,
                caller_rva);
        }
        kbo_log_foreign_team_add_trace(
            caller_rva,
            "military_blocked",
            0u,
            team_ptr,
            player_ptr,
            arg3,
            arg4,
            arg5,
            arg6,
            arg7,
            arg8,
            before_current_team_id,
            before_active_team_id,
            before_original_team_id);
        KBO_PROFILE_END(profile_team_add_guard_wrapper, "team_add_guard.blocked");
        KBO_HOOK_PROFILE_RETURN(profile_hook, "team.add_player_guard", 0u);
    }

    if (amateur_generation_call && kbo_amateur_defer_team_add_if_generation(
            caller_rva,
            team_ptr,
            player_ptr,
            arg3,
            arg4,
            arg5,
            arg6,
            arg7,
            arg8)) {
        kbo_log_foreign_team_add_trace(
            caller_rva,
            "amateur_deferred",
            1u,
            team_ptr,
            player_ptr,
            arg3,
            arg4,
            arg5,
            arg6,
            arg7,
            arg8,
            before_current_team_id,
            before_active_team_id,
            before_original_team_id);
        KBO_PROFILE_END(profile_team_add_guard_wrapper, "team_add_guard.amateur_deferred");
        KBO_HOOK_PROFILE_RETURN(profile_hook, "team.add_player_guard", 1u);
    }

    if (!is_military_team
            && !amateur_generation_call
            && kbo_team_add_foreign_policy_should_block(
                team_ptr,
                player_ptr,
                team_id,
                before_current_team_id,
                before_active_team_id,
                before_original_team_id,
                caller_rva)) {
        if (player != NULL && kbo_player_is_foreign_for_kbo_rights(player)) {
            kbo_mark_foreign_ai_roster_daily_callup_dirty("team_add_foreign_policy_blocked");
        }
        if (player != NULL
                && before_current_team_id == 0u
                && before_active_team_id == 0u
                && before_original_team_id != 0u) {
            kbo_team_add_restore_source_team_after_blocked_foreign_purchase(
                player,
                before_original_team_id,
                team_id,
                caller_rva);
        }
        kbo_log_foreign_team_add_trace(
            caller_rva,
            "foreign_policy_blocked",
            0u,
            team_ptr,
            player_ptr,
            arg3,
            arg4,
            arg5,
            arg6,
            arg7,
            arg8,
            before_current_team_id,
            before_active_team_id,
            before_original_team_id);
        KBO_PROFILE_END(profile_team_add_guard_wrapper, "team_add_guard.foreign_policy_blocked");
        KBO_HOOK_PROFILE_RETURN(profile_hook, "team.add_player_guard", 0u);
    }

    if (!is_military_team
            && !amateur_generation_call
            && before_current_team_id != 0u
            && before_active_team_id != 0u) {
        KBO_PROFILE_BEGIN(profile_team_add_original);
        KBO_HOOK_PROFILE_PAUSE(profile_hook);
        uint8_t result = original(team_ptr, player_ptr, arg3, arg4, arg5, arg6, arg7, arg8);
        KBO_HOOK_PROFILE_RESUME(profile_hook);
        KBO_PROFILE_END(profile_team_add_original, result != 0u ? "team_add_guard.original.fast_success" : "team_add_guard.original.fast_rejected");
        kbo_log_foreign_team_add_trace(
            caller_rva,
            result != 0u ? "fast_success" : "fast_rejected",
            result,
            team_ptr,
            player_ptr,
            arg3,
            arg4,
            arg5,
            arg6,
            arg7,
            arg8,
            before_current_team_id,
            before_active_team_id,
            before_original_team_id);
        KBO_PROFILE_END(profile_team_add_guard_wrapper, result != 0u ? "team_add_guard.fast_success" : "team_add_guard.fast_rejected");
        KBO_HOOK_PROFILE_RETURN(profile_hook, "team.add_player_guard", result);
    }

    uint32_t amateur_league_id = amateur_generation_call && team_readable
        ? kbo_team_add_cached_amateur_league_id(team)
        : 0u;
    uintptr_t effective_team_ptr = amateur_generation_call && amateur_league_id != 0u
        ? kbo_amateur_team_add_player_reroute_before_original(
            team_ptr,
            player_ptr,
            "team_add_player_before_original")
        : team_ptr;
    int amateur_pre_rerouted = effective_team_ptr != team_ptr;
    if (amateur_generation_call && player_plausible && team_readable) {
        uint32_t league_id = amateur_league_id;
        int16_t age = *(int16_t*)(player + OOTP27_PLAYER_AGE_OFFSET);
        if (league_id != 0u && kbo_amateur_player_age_eligible(league_id, age)) {
            static volatile LONG amateur_caller_log_count = 0;
            LONG slot = InterlockedIncrement(&amateur_caller_log_count);
            if (slot <= 200 || kbo_team_add_amateur_verbose_log_enabled_cached()) {
                uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
                uint32_t team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET);
                kbo_log_runtimef(
                    "amateur team_add caller trace #%ld caller_rva=0x%x player=%u league=%u age=%d original_team=%u rerouted=%d",
                    slot,
                    caller_rva,
                    player_id,
                    league_id,
                    (int)age,
                    team_id,
                    amateur_pre_rerouted);
            }
        }
    }

    if (!is_military_team
            && !amateur_generation_call
            && player_plausible
            && before_current_team_id == 0u
            && before_active_team_id == 0u
            && kbo_team_add_known_foreign_market_minor_caller(caller_rva)) {
        kbo_apply_foreign_contract_demand_floor(
            player_ptr,
            0u,
            "team_add_pre_original");
    }

    KBO_PROFILE_BEGIN(profile_team_add_original);
    if (!kbo_team_add_original_args_readable(effective_team_ptr, player_ptr)) {
        kbo_team_add_log_skipped_bad_original_args(
            caller_rva,
            "before_original",
            effective_team_ptr,
            player_ptr,
            team_ptr);
        KBO_PROFILE_END(profile_team_add_original, "team_add_guard.original.skipped_bad_args");
        KBO_PROFILE_END(profile_team_add_guard_wrapper, "team_add_guard.bad_effective_team");
        KBO_HOOK_PROFILE_RETURN(profile_hook, "team.add_player_guard", 0u);
    }
    KBO_HOOK_PROFILE_PAUSE(profile_hook);
    uint8_t result = original(effective_team_ptr, player_ptr, arg3, arg4, arg5, arg6, arg7, arg8);
    KBO_HOOK_PROFILE_RESUME(profile_hook);
    KBO_PROFILE_END(profile_team_add_original, result != 0u ? "team_add_guard.original.success" : "team_add_guard.original.rejected");
    if (result != 0u) {
        kbo_team_add_normalize_foreign_retention_contract_success(
            caller_rva,
            effective_team_ptr,
            player_ptr,
            before_current_team_id,
            before_active_team_id,
            before_original_team_id);
    }
    kbo_log_foreign_team_add_trace(
        caller_rva,
        result != 0u ? (effective_team_ptr != team_ptr ? "rerouted_success" : "success") : (effective_team_ptr != team_ptr ? "rerouted_rejected" : "rejected"),
        result,
        effective_team_ptr,
        player_ptr,
        arg3,
        arg4,
        arg5,
        arg6,
        arg7,
        arg8,
        before_current_team_id,
        before_active_team_id,
        before_original_team_id);
    kbo_team_add_log_foreign_retention_result(
        caller_rva,
        result,
        effective_team_ptr,
        player_ptr,
        before_current_team_id,
        before_active_team_id,
        before_original_team_id);
    int retry_rejected_targets = kbo_team_add_retry_rejected_targets_enabled_cached();
    for (int amateur_retry = 0; result == 0u && amateur_pre_rerouted && retry_rejected_targets && amateur_retry < 4; amateur_retry++) {
        static volatile LONG fallback_log_count = 0;
        LONG fallback_slot = InterlockedIncrement(&fallback_log_count);
        uint32_t original_team_id = 0u;
        uint32_t effective_team_id = 0u;
        uint32_t player_id = 0u;
        if (memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            original_team_id = *(uint32_t*)((uint8_t*)team_ptr + OOTP27_KBO_TEAM_ID_OFFSET);
        }
        if (memory_range_readable((void*)effective_team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            effective_team_id = *(uint32_t*)((uint8_t*)effective_team_ptr + OOTP27_KBO_TEAM_ID_OFFSET);
        }
        if (kbo_player_pointer_plausible(player_ptr)) {
            player_id = *(uint32_t*)((uint8_t*)player_ptr + OOTP27_PLAYER_ID_OFFSET);
        }
        if (effective_team_id != 0u) {
            uint8_t* rejected_team = (uint8_t*)effective_team_ptr;
            uint32_t rejected_league_id = kbo_resolve_amateur_assignment_league_id_for_team_and_player(
                rejected_team,
                (uint8_t*)player_ptr);
            kbo_amateur_assignment_mark_rejected_target(rejected_league_id, effective_team_id);
        }
        if (fallback_slot <= 80) {
            kbo_log_runtimef(
                "amateur assignment reroute target rejected; retrying alternate player=%u original_team=%u rejected_team=%u attempt=%d",
                player_id,
                original_team_id,
                effective_team_id,
                amateur_retry + 1);
        }

        uintptr_t retry_team_ptr = kbo_amateur_team_add_player_reroute_before_original(
            team_ptr,
            player_ptr,
            "team_add_player_reroute_retry");
        if (retry_team_ptr == team_ptr || retry_team_ptr == effective_team_ptr) {
            break;
        }
        if (!kbo_team_add_original_args_readable(retry_team_ptr, player_ptr)) {
            kbo_team_add_log_skipped_bad_original_args(
                caller_rva,
                "retry",
                retry_team_ptr,
                player_ptr,
                team_ptr);
            break;
        }
        effective_team_ptr = retry_team_ptr;
        KBO_PROFILE_BEGIN(profile_team_add_original);
        KBO_HOOK_PROFILE_PAUSE(profile_hook);
        result = original(effective_team_ptr, player_ptr, arg3, arg4, arg5, arg6, arg7, arg8);
        KBO_HOOK_PROFILE_RESUME(profile_hook);
        KBO_PROFILE_END(profile_team_add_original, result != 0u ? "team_add_guard.original_retry.success" : "team_add_guard.original_retry.rejected");
    }
    if (result == 0u && amateur_pre_rerouted) {
        if (!kbo_team_add_original_args_readable(team_ptr, player_ptr)) {
            kbo_team_add_log_skipped_bad_original_args(
                caller_rva,
                "fallback",
                team_ptr,
                player_ptr,
                effective_team_ptr);
            KBO_PROFILE_END(profile_team_add_guard_wrapper, "team_add_guard.fallback_bad_args");
            KBO_HOOK_PROFILE_RETURN(profile_hook, "team.add_player_guard", 0u);
        }
        KBO_PROFILE_BEGIN(profile_team_add_original);
        KBO_HOOK_PROFILE_PAUSE(profile_hook);
        result = original(team_ptr, player_ptr, arg3, arg4, arg5, arg6, arg7, arg8);
        KBO_HOOK_PROFILE_RESUME(profile_hook);
        KBO_PROFILE_END(profile_team_add_original, result != 0u ? "team_add_guard.original_fallback.success" : "team_add_guard.original_fallback.rejected");
        if (result != 0u) {
            effective_team_ptr = team_ptr;
            amateur_pre_rerouted = 0;
        }
    }
    if (result != 0u && amateur_generation_call && amateur_league_id != 0u) {
        KBO_PROFILE_BEGIN(profile_team_add_amateur_assignment);
        if (amateur_pre_rerouted) {
            kbo_amateur_team_add_player_note_original_success(
                effective_team_ptr,
                player_ptr,
                "team_add_player_pre_rerouted_original_success",
                result);
        } else {
            kbo_amateur_team_add_player_note_original_success(
                team_ptr,
                player_ptr,
                "team_add_player_original_success",
                result);
        }
        KBO_PROFILE_END(profile_team_add_amateur_assignment, "team_add_guard.amateur_assignment_after_original");
    }
    if (result != 0u) {
        if (player != NULL && kbo_player_is_foreign_for_kbo_rights(player)) {
            kbo_mark_foreign_ai_roster_daily_callup_dirty("team_add_foreign_assignment_success");
        }
        kbo_team_add_note_foreign_assignment_success(
            player,
            before_current_team_id,
            before_active_team_id,
            before_loan_team_id);
        KBO_PROFILE_BEGIN(profile_team_add_foreign_injury_attach);
        kbo_team_add_attach_foreign_injury_replacement_success(
            effective_team_ptr,
            player_ptr,
            "team_add_player_success");
        KBO_PROFILE_END(profile_team_add_foreign_injury_attach, "team_add_guard.foreign_injury_attach");

        KBO_PROFILE_BEGIN(profile_team_add_fa_comp);
        kbo_team_add_player_record_fa_compensation_success(
            effective_team_ptr,
            player_ptr,
            before_current_team_id,
            before_active_team_id,
            before_original_team_id);
        KBO_PROFILE_END(profile_team_add_fa_comp, "team_add_guard.fa_comp_probe");
    }
    KBO_PROFILE_END(profile_team_add_guard_wrapper, result != 0u ? "team_add_guard.success" : "team_add_guard.original_rejected");
    KBO_HOOK_PROFILE_RETURN(profile_hook, "team.add_player_guard", result);
}
