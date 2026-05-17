#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>

#include "../../../../../bootstrap/profiling/profiler.h"
#include "../../../../../bootstrap/abi/ootp_offsets.h"
#include "../../../../../core/core_flags/api/flags_api.h"
#include "../../../../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../../../../core/dates/core_current_date.h"
#include "../../../../../core/logging/core_log.h"
#include "../../../../../fa_compensation/history/fa_compensation_history.h"
#include "../../../../../military_service/players/team_policy/military_service_team_policy.h"
#include "../../../../../runtime_memory/runtime_memory.h"
#include "../../../../../team/lookup/team_lookup.h"
#include "../../../../common/dates/foreign_waiver_date.h"
#include "../../../../common/player_eval/foreign_waiver_player_eval.h"
#include "../../../../common/policy/foreign_waiver_policy.h"
#include "../../../../injury/api/foreign_injury.h"
#include "../../../state/foreign_fa_block_state.h"

static volatile LONG g_kbo_fa_signing_branch_skip_log_count = 0;

static int kbo_fa_signing_team_ptr_is_kbo(
    uintptr_t team_ptr,
    uint32_t* out_team_id,
    uint32_t* out_league_id)
{
    if (out_team_id != NULL) {
        *out_team_id = 0u;
    }
    if (out_league_id != NULL) {
        *out_league_id = 0u;
    }
    if (team_ptr == 0 || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return 0;
    }

    uint8_t* team = (uint8_t*)team_ptr;
    uint32_t team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET);
    uint32_t league_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
    if (out_team_id != NULL) {
        *out_team_id = team_id;
    }
    if (out_league_id != NULL) {
        *out_league_id = league_id;
    }
    if (team_id == 0u || team_id > 100000u || league_id == 0u || league_id > 100000u) {
        return 0;
    }

    uint32_t kbo_league_id = kbo_resolve_kbo_league_id();
    return kbo_league_id == 0u || league_id == kbo_league_id;
}

__declspec(noinline) int ootp_kbo_fa_signing_branch_wrapper(uintptr_t player_ptr, uintptr_t team_ptr)
{
    KBO_HOOK_PROFILE_BEGIN(profile_hook);
    if (!kbo_fix_enabled()) {
        KBO_HOOK_PROFILE_RETURN(profile_hook, "foreign.fa_signing_branch", 1);
    }
    if (!kbo_player_pointer_plausible(player_ptr)) {
        LONG slot = InterlockedIncrement(&g_kbo_fa_signing_branch_skip_log_count);
        if (slot <= 20) {
            kbo_log_runtimef("KBO FA signing branch skipped reason=bad_player player_ptr=%p team_ptr=%p", (void*)player_ptr, (void*)team_ptr);
        }
        KBO_HOOK_PROFILE_RETURN(profile_hook, "foreign.fa_signing_branch", 1);
    }

    uint32_t team_id = 0u;
    uint32_t league_id = 0u;

    uint8_t* player = (uint8_t*)player_ptr;
    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    if (player_id == 0u || player_id > 1000000u) {
        LONG slot = InterlockedIncrement(&g_kbo_fa_signing_branch_skip_log_count);
        if (slot <= 20) {
            kbo_log_runtimef("KBO FA signing branch skipped reason=bad_player_id player=%u team=%u league=%u", player_id, team_id, league_id);
        }
        KBO_HOOK_PROFILE_RETURN(profile_hook, "foreign.fa_signing_branch", 1);
    }

    int is_kbo_team = kbo_fa_signing_team_ptr_is_kbo(team_ptr, &team_id, &league_id);

    if (kbo_team_id_is_military_service_team(team_id)) {
        static volatile LONG military_fa_signing_block_log_count = 0;
        LONG slot = InterlockedIncrement(&military_fa_signing_block_log_count);
        if (slot <= 200) {
            kbo_log_runtimef(
                "military service team FA signing blocked player=%u team=%u league=%u",
                player_id,
                team_id,
                league_id);
        }
        KBO_HOOK_PROFILE_RETURN(profile_hook, "foreign.fa_signing_branch", 0);
    }

    if (!is_kbo_team) {
        LONG slot = InterlockedIncrement(&g_kbo_fa_signing_branch_skip_log_count);
        if (slot <= 20) {
            kbo_log_runtimef("KBO FA signing branch skipped reason=non_kbo_team player=%u team_ptr=%p team=%u league=%u", player_id, (void*)team_ptr, team_id, league_id);
        }
        KBO_HOOK_PROFILE_RETURN(profile_hook, "foreign.fa_signing_branch", 1);
    }

    if (kbo_custom_foreign_policy_enabled()
            && memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)
            && kbo_player_is_foreign_for_kbo_rights(player)) {
        uint32_t effective_before = 0u;
        uint32_t effective_after = 0u;
        uint32_t effective_limit = 0u;
        uint8_t slot_type = 0u;
        uint32_t injured_player_id = 0u;
        int allowed = kbo_custom_foreign_policy_team_allows_final_signing(
            team_id,
            player,
            &effective_before,
            &effective_after,
            &effective_limit,
            &slot_type,
            &injured_player_id);
        if (!allowed) {
            uint32_t today = 0u;
            kbo_get_foreign_waiver_current_yyyymmdd(&today);
            kbo_record_recent_custom_foreign_policy_block(player_id, team_id, today);
            static volatile LONG final_block_log_count = 0;
            LONG slot = InterlockedIncrement(&final_block_log_count);
            if (slot <= 200) {
                kbo_log_runtimef(
                    "custom foreign policy FA signing blocked player=%u team=%u effective_before=%u effective_after=%u limit=%u injury_slot=%s injured=%u today=%u",
                    player_id,
                    team_id,
                    effective_before,
                    effective_after,
                    effective_limit,
                    kbo_foreign_injury_slot_label(slot_type),
                    injured_player_id,
                    today);
            }
            KBO_HOOK_PROFILE_RETURN(profile_hook, "foreign.fa_signing_branch", 0);
        }
    }

    KBO_HOOK_PROFILE_RETURN(profile_hook, "foreign.fa_signing_branch", 1);
}

__declspec(noinline) void ootp_kbo_fa_signing_success_post_wrapper(uintptr_t player_ptr, uintptr_t team_ptr)
{
    KBO_HOOK_PROFILE_BEGIN(profile_hook);
    if (!kbo_fix_enabled() || !kbo_player_pointer_plausible(player_ptr)) {
        KBO_HOOK_PROFILE_RETURN_VOID(profile_hook, "foreign.fa_signing_success_post");
    }

    uint32_t team_id = 0u;
    uint32_t league_id = 0u;
    if (!kbo_fa_signing_team_ptr_is_kbo(team_ptr, &team_id, &league_id)) {
        KBO_HOOK_PROFILE_RETURN_VOID(profile_hook, "foreign.fa_signing_success_post");
    }
    if (kbo_team_id_is_military_service_team(team_id)) {
        KBO_HOOK_PROFILE_RETURN_VOID(profile_hook, "foreign.fa_signing_success_post");
    }

    uint8_t* player = (uint8_t*)player_ptr;
    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    uint32_t original_team_id = memory_range_readable(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET, sizeof(uint32_t))
        ? *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET)
        : 0u;

    static volatile LONG post_log_count = 0;
    LONG slot = InterlockedIncrement(&post_log_count);
    if (slot <= 120) {
        kbo_log_runtimef(
            "KBO FA signing success post player=%u team=%u league=%u current_team=%u active_team=%u original_team=%u",
            player_id,
            team_id,
            league_id,
            current_team_id,
            active_team_id,
            original_team_id);
    }

    if (memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)
            && kbo_player_is_foreign_for_kbo_rights(player)) {
        uint32_t effective_before = 0u;
        uint32_t effective_after = 0u;
        uint32_t effective_limit = 0u;
        uint8_t slot_type = 0u;
        uint32_t injured_player_id = 0u;
        (void)kbo_custom_foreign_policy_team_allows_final_signing(
            team_id,
            player,
            &effective_before,
            &effective_after,
            &effective_limit,
            &slot_type,
            &injured_player_id);
        if (slot_type != 0u && injured_player_id != 0u) {
            kbo_attach_foreign_injury_replacement_after_signing(
                team_id,
                player,
                slot_type,
                injured_player_id,
                "fa_signing_success_post");
        }
    }

    kbo_record_fa_compensation_signing(player_ptr, team_id, league_id, "fa_signing_success_post");
    KBO_HOOK_PROFILE_END(profile_hook, "foreign.fa_signing_success_post");
}
