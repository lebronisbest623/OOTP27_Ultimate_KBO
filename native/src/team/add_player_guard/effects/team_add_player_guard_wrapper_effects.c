#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>

#include "../../../amateur_player_quality/api/amateur_player_quality.h"
#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../bootstrap/profiling/profiler.h"
#include "../../../core/logging/core_log.h"
#include "../../../foreign/common/player_eval/foreign_waiver_player_eval.h"
#include "../internal/team_add_player_guard_internal.h"
#include "../team_add_player_guard_ai_roster.h"

uintptr_t kbo_team_add_prepare_amateur_pre_original_reroute(
    uint32_t caller_rva,
    uintptr_t team_ptr,
    uintptr_t player_ptr,
    int amateur_generation_call,
    int team_readable,
    int player_plausible,
    uint8_t* team,
    uint8_t* player,
    uint32_t* out_amateur_league_id,
    int* out_amateur_pre_rerouted)
{
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
        int16_t age = *(int16_t*)(player + OOTP27_PLAYER_AGE_OFFSET);
        if (amateur_league_id != 0u && kbo_amateur_player_age_eligible(amateur_league_id, age)) {
            static volatile LONG amateur_caller_log_count = 0;
            LONG slot = InterlockedIncrement(&amateur_caller_log_count);
            if (slot <= 10 || (slot % 500) == 0) {
                uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
                uint32_t team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET);
                kbo_log_runtimef(
                    "amateur team_add caller trace #%ld caller_rva=0x%x player=%u league=%u age=%d original_team=%u rerouted=%d",
                    slot,
                    caller_rva,
                    player_id,
                    amateur_league_id,
                    (int)age,
                    team_id,
                    amateur_pre_rerouted);
            } else if (slot == 11) {
                kbo_log_runtime_line("amateur team_add caller trace suppressed after 10 calls; logging every 500th call");
            }
        }
    }

    if (out_amateur_league_id != NULL) {
        *out_amateur_league_id = amateur_league_id;
    }
    if (out_amateur_pre_rerouted != NULL) {
        *out_amateur_pre_rerouted = amateur_pre_rerouted;
    }
    return effective_team_ptr;
}

void kbo_team_add_apply_success_side_effects(
    uintptr_t effective_team_ptr,
    uintptr_t player_ptr,
    uint8_t* player,
    uint32_t before_current_team_id,
    uint32_t before_active_team_id,
    uint32_t before_loan_team_id,
    uint32_t before_original_team_id)
{
    int player_is_foreign = player != NULL && kbo_player_is_foreign_for_kbo_rights(player);
    if (player_is_foreign) {
        kbo_mark_foreign_ai_roster_daily_callup_dirty("team_add_foreign_assignment_success");
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
    }

    KBO_PROFILE_BEGIN(profile_team_add_fa_comp);
    kbo_team_add_player_record_fa_compensation_success(
        effective_team_ptr,
        player_ptr,
        before_current_team_id,
        before_active_team_id,
        before_original_team_id);
    KBO_PROFILE_END(profile_team_add_fa_comp, "team_add_guard.fa_comp_probe");
}
