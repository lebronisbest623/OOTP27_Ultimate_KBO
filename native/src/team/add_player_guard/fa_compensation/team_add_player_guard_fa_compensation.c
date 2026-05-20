#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../bootstrap/profiling/profiler.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../core/logging/core_log.h"
#include "../../../fa_compensation/history/fa_compensation_history.h"
#include "../../../fa_filing/fa_filing.h"
#include "../../../foreign/common/player_eval/foreign_waiver_player_eval.h"
#include "../../../military_service/players/team_policy/military_service_team_policy.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../lookup/team_lookup.h"
#include "../internal/team_add_player_guard_internal.h"
#include "../../../core/core_flags/keys/runtime_flag_keys.generated.h"

void kbo_team_add_player_record_fa_compensation_success(
    uintptr_t team_ptr,
    uintptr_t player_ptr,
    uint32_t before_current_team_id,
    uint32_t before_active_team_id,
    uint32_t before_original_team_id)
{
    KBO_PROFILE_BEGIN(profile_fa_comp_probe_inner);
    if (!kbo_fix_enabled()
            || read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_DISABLE_KBO_FA_COMPENSATION_FILE)
            || team_ptr == 0
            || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)
            || !kbo_player_pointer_plausible(player_ptr)) {
        KBO_PROFILE_END(profile_fa_comp_probe_inner, "team_add_guard.fa_comp_inner.precheck_reject");
        return;
    }

    if (before_current_team_id != 0u && before_active_team_id != 0u) {
        KBO_PROFILE_END(profile_fa_comp_probe_inner, "team_add_guard.fa_comp_inner.not_teamless_before");
        return;
    }

    uint8_t* team = (uint8_t*)team_ptr;
    uint8_t* player = (uint8_t*)player_ptr;
    uint32_t team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET);
    uint32_t league_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
    if (team_id == 0u || league_id == 0u || kbo_team_id_is_military_service_team(team_id)) {
        KBO_PROFILE_END(profile_fa_comp_probe_inner, "team_add_guard.fa_comp_inner.bad_team");
        return;
    }

    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    uint32_t filing_original_team_id = 0u;
    uint32_t filing_league_id = 0u;
    uint32_t filing_season = 0u;
    if (!kbo_fa_filing_find_latest_player(player_id, &filing_original_team_id, &filing_league_id, &filing_season)) {
        KBO_PROFILE_END(profile_fa_comp_probe_inner, "team_add_guard.fa_comp_inner.no_filing");
        return;
    }

    if (filing_league_id != 0u) {
        league_id = filing_league_id;
    }

    uint32_t after_active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    uint32_t signing_team_id = after_active_team_id != 0u ? after_active_team_id : team_id;
    if (signing_team_id == filing_original_team_id) {
        KBO_PROFILE_END(profile_fa_comp_probe_inner, "team_add_guard.fa_comp_inner.same_team");
        return;
    }

    static volatile LONG log_count = 0;
    LONG slot = InterlockedIncrement(&log_count);
    if (slot <= 160) {
        uint32_t after_current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        kbo_log_runtimef(
            "KBO team-add FA compensation probe player=%u team=%u league=%u before_current=%u before_active=%u before_original=%u filing_original=%u filing_league=%u filing_season=%u after_current=%u after_active=%u",
            player_id,
            signing_team_id,
            league_id,
            before_current_team_id,
            before_active_team_id,
            before_original_team_id,
            filing_original_team_id,
            filing_league_id,
            filing_season,
            after_current_team_id,
            after_active_team_id);
    }

    kbo_record_fa_compensation_signing(player_ptr, signing_team_id, league_id, "team_add_player_success");
    KBO_PROFILE_END(profile_fa_comp_probe_inner, "team_add_guard.fa_comp_inner.record_attempt");
}
