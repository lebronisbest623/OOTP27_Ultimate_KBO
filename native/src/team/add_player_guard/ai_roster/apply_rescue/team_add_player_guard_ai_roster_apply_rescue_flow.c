#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>

#include "../../../../bootstrap/abi/ootp_offsets.h"
#include "../../../../core/core_flags/api/flags_api.h"
#include "../../../../core/logging/core_log.h"
#include "../../../../foreign/common/policy/foreign_waiver_policy.h"
#include "../../../../foreign/common/player_eval/foreign_waiver_player_eval.h"
#include "../../../../runtime_memory/runtime_memory.h"
#include "../../../lookup/team_lookup.h"
#include "../../team_add_player_guard.h"
#include "../internal/team_add_player_guard_ai_roster_internal.h"
#include "../../../../core/core_flags/keys/runtime_flag_keys.generated.h"

static volatile LONG g_kbo_ai_roster_foreign_apply_rescue_move_enabled_cached = -1;

int kbo_ai_roster_foreign_apply_rescue_enabled(void)
{
    LONG enabled = g_kbo_ai_roster_foreign_apply_rescue_move_enabled_cached;
    if (enabled < 0) {
        int explicit_enable = read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_ENABLE_AI_ROSTER_FOREIGN_APPLY_RESCUE_MOVE_FILE) ? 1 : 0;
        int disabled = read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_DISABLE_AI_ROSTER_FOREIGN_APPLY_RESCUE_FILE)
            || read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_DISABLE_AI_ROSTER_FOREIGN_APPLY_RESCUE_MOVE_FILE);
        LONG computed = (explicit_enable && !disabled) ? 1 : 0;
        if (InterlockedCompareExchange(
                &g_kbo_ai_roster_foreign_apply_rescue_move_enabled_cached,
                computed,
                -1) == -1) {
            kbo_log_runtimef(
                "ootp ai roster foreign apply rescue move mode enabled=%ld explicit=%d disabled=%d",
                computed,
                explicit_enable,
                disabled ? 1 : 0);
        }
        enabled = g_kbo_ai_roster_foreign_apply_rescue_move_enabled_cached;
    }
    return kbo_custom_foreign_policy_enabled() && enabled == 1;
}

static int kbo_ai_roster_foreign_apply_rescue_team_add_enabled(void)
{
    return kbo_ai_roster_foreign_apply_rescue_enabled()
        && read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_ENABLE_AI_ROSTER_FOREIGN_APPLY_RESCUE_TEAM_ADD_FILE)
        && !read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_DISABLE_AI_ROSTER_FOREIGN_APPLY_RESCUE_TEAM_ADD_FILE);
}

static uint8_t kbo_ai_roster_context_flow_apply_rescue_team_add(
    uintptr_t context_ptr,
    const KboAiRosterFlowContextSnapshot* before,
    uint32_t active_team_id,
    int callup_allowed,
    uintptr_t slot_block_ptr,
    uint16_t target_slot)
{
    if (context_ptr == 0u
            || before == NULL
            || !before->selected.plausible
            || before->selected.ptr == 0u
            || before->selected.player_id == 0u
            || active_team_id == 0u
            || !callup_allowed
            || !kbo_ai_roster_foreign_apply_rescue_team_add_enabled()) {
        return 0u;
    }

    uint8_t* active_team = find_kbo_team_by_numeric_id_any_league(active_team_id, 1);
    if (active_team == NULL || !memory_range_readable(active_team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return 0u;
    }

    uint32_t team_id = *(uint32_t*)(active_team + OOTP27_KBO_TEAM_ID_OFFSET);
    uint32_t team_league_id = *(uint32_t*)(active_team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
    if (team_id != active_team_id
            || (before->selected.current_team_id == team_id
                && before->selected.active_team_id == team_id
                && before->selected.league_id == team_league_id)) {
        return 0u;
    }

    uint8_t result = kbo_team_add_player_guard_call_original(
        (uintptr_t)active_team,
        before->selected.ptr,
        0u,
        0u,
        0u,
        0u,
        0u,
        0u);

    KboAiRosterFlowPlayerSnapshot after_player;
    kbo_ai_roster_flow_read_player(before->selected.ptr, &after_player);

    static volatile LONG trace_log_count = 0;
    LONG trace_slot = InterlockedIncrement(&trace_log_count);
    if (trace_slot <= 300) {
        kbo_log_runtimef(
            "ootp ai roster foreign apply rescue team-add #%ld result=%u context=%p slot_block=%p target_slot=%u team=%u team_league=%u player=%u foreign=%d nation=%u current=%u->%u active=%u->%u league=%u->%u status24=%u->%u status25=%u->%u status26=%u->%u f62=%u->%u f65=%u->%u f68=%u->%u f1a=%u->%u overall=%d ratings=%d",
            trace_slot,
            (uint32_t)result,
            (void*)context_ptr,
            (void*)slot_block_ptr,
            target_slot,
            team_id,
            team_league_id,
            before->selected.player_id,
            before->selected.foreign,
            before->selected.nation_id,
            before->selected.current_team_id,
            after_player.current_team_id,
            before->selected.active_team_id,
            after_player.active_team_id,
            before->selected.league_id,
            after_player.league_id,
            before->selected.status24,
            after_player.status24,
            before->selected.status25,
            after_player.status25,
            before->selected.status26,
            after_player.status26,
            before->selected.f62,
            after_player.f62,
            before->selected.f65,
            after_player.f65,
            before->selected.f68,
            after_player.f68,
            before->selected.f1a,
            after_player.f1a,
            after_player.overall,
            after_player.ratings);
    }
    return result;
}

int kbo_ai_roster_context_flow_apply_rescue(
    uintptr_t context_ptr,
    const KboAiRosterFlowContextSnapshot* before,
    const KboAiRosterFlowContextSnapshot* after,
    OotpKboAiRosterApplySelectionFn apply_selection_trampoline)
{
    if (context_ptr == 0u
            || before == NULL
            || after == NULL
            || apply_selection_trampoline == NULL
            || !kbo_ai_roster_foreign_apply_rescue_enabled()) {
        return 0;
    }
    if (!before->selected.plausible
            || !before->selected.foreign
            || after->selected.ptr != 0u) {
        return 0;
    }
    if (before->selected.ptr != before->ptr528.ptr
            && before->selected.ptr != after->ptr528.ptr) {
        return 0;
    }
    if (before->primary_slot >= 64u
            || before->primary_target_slot > KBO_AI_ROSTER_SLOT_MAX
            || before->primary_slot_block == 0u) {
        return 0;
    }

    int team_add_rescue_enabled = kbo_ai_roster_foreign_apply_rescue_team_add_enabled();
    if (before->selected.f65 != 0u
            || (before->selected.status26 != 0u
                && (!team_add_rescue_enabled || before->selected.status26 != 1u))) {
        return 0;
    }

    uint32_t active_team_id = 0u;
    int callup_allowed = 0;
    if (!kbo_ai_roster_minor_foreign_callup_allows(
            (int32_t)before->selected.league_id,
            (uint8_t*)before->selected.ptr,
            &active_team_id,
            &callup_allowed)) {
        return 0;
    }

    uint16_t target_slot = before->primary_target_slot;
    uintptr_t slot_block_ptr = before->primary_slot_block;
    uint32_t slot_team_id = kbo_ai_roster_context_slot_team_id(context_ptr, before->primary_slot);
    uint32_t before_slot_code = kbo_ai_roster_slot_code_at(slot_block_ptr, target_slot);
    uint32_t before_slot_player_id = kbo_ai_roster_slot_player_at(slot_block_ptr, target_slot);

    apply_selection_trampoline(
        context_ptr,
        (int32_t)before->primary_slot,
        before->selected.ptr,
        (int32_t)target_slot,
        11);

    uint32_t after_slot_code = kbo_ai_roster_slot_code_at(slot_block_ptr, target_slot);
    uint32_t after_slot_player_id = kbo_ai_roster_slot_player_at(slot_block_ptr, target_slot);
    if (after_slot_player_id == before->selected.player_id) {
        kbo_ai_roster_context_flow_apply_rescue_team_add(
            context_ptr,
            before,
            active_team_id,
            callup_allowed,
            slot_block_ptr,
            target_slot);
        kbo_ai_roster_record_foreign_apply_rescue(
            context_ptr,
            slot_block_ptr,
            before->selected.ptr,
            before->selected.player_id,
            before->primary_slot,
            target_slot);
    }

    KboAiRosterFlowPlayerSnapshot after_player;
    kbo_ai_roster_flow_read_player(before->selected.ptr, &after_player);

    static volatile LONG trace_log_count = 0;
    LONG trace_slot = InterlockedIncrement(&trace_log_count);
    if (trace_slot <= 1000) {
        kbo_log_runtimef(
            "ootp ai roster foreign apply rescue #%ld context=%p slot_index=%u target_slot=%u roster_code=11 slot_block=%p slot_team=%u active_team=%u before_slot_code=%u after_slot_code=%u before_slot_player=%u after_slot_player=%u player=%u foreign=%d nation=%u current=%u->%u active=%u->%u league=%u status24=%u->%u status25=%u->%u status26=%u->%u f62=%u->%u f65=%u->%u f68=%u->%u f1a=%u->%u score_fe0=%d score_fe4=%d overall=%d ratings=%d",
            trace_slot,
            (void*)context_ptr,
            before->primary_slot,
            target_slot,
            (void*)slot_block_ptr,
            slot_team_id,
            active_team_id,
            before_slot_code,
            after_slot_code,
            before_slot_player_id,
            after_slot_player_id,
            before->selected.player_id,
            before->selected.foreign,
            before->selected.nation_id,
            before->selected.current_team_id,
            after_player.current_team_id,
            before->selected.active_team_id,
            after_player.active_team_id,
            before->selected.league_id,
            before->selected.status24,
            after_player.status24,
            before->selected.status25,
            after_player.status25,
            before->selected.status26,
            after_player.status26,
            before->selected.f62,
            after_player.f62,
            before->selected.f65,
            after_player.f65,
            before->selected.f68,
            after_player.f68,
            before->selected.f1a,
            after_player.f1a,
            before->selected.score_fe0,
            before->selected.score_fe4,
            after_player.overall,
            after_player.ratings);
    }
    return 1;
}
