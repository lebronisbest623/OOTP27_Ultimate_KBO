#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>

#include "../../../bootstrap/abi/hook_entrypoints.h"
#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../bootstrap/profiling/perf_probe.h"
#include "../../../bootstrap/profiling/profiler.h"
#include "../../../core/dates/core_current_date.h"
#include "../../../core/dates/tick/current_date_tick_capture.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../core/logging/core_log.h"
#include "../../../foreign/common/events/foreign_priority_events.h"
#include "../../../team/lookup/team_lookup.h"
#include "../../players/loans/military_active_loan.h"
#include "../../selection/draft/military_draft_queue.h"
#include "../../players/state/military_player_state.h"
#include "../../returns/military_return.h"
#include "../../../core/dates/constants/kbo_date_constants.h"

typedef void (__fastcall *OotpMilitaryServiceEntryFn)(void* player);

static LONG g_military_service_entry_log_count = 0;

__declspec(noinline) void ootp_kbo_military_service_entry_wrapper(
    uintptr_t player_ptr,
    uintptr_t original_func_ptr)
{
    KBO_HOOK_PROFILE_BEGIN(profile_hook);
    static volatile LONG perf_total = 0;
    static volatile LONG perf_last = 0;
    static volatile LONG perf_ms = 0;
    static volatile LONG perf_max = 0;
    static volatile LONG perf_tick = 0;
    DWORD perf_start = GetTickCount();
    OotpMilitaryServiceEntryFn original_func = (OotpMilitaryServiceEntryFn)original_func_ptr;
    if (original_func != NULL) {
        KBO_HOOK_PROFILE_PAUSE(profile_hook);
        original_func((void*)player_ptr);
        KBO_HOOK_PROFILE_RESUME(profile_hook);
    }

    LONG log_index = InterlockedIncrement(&g_military_service_entry_log_count);
    if (log_index <= 120) {
        uint32_t player_id    = 0;
        uint32_t parent_team  = 0;
        uint32_t active_team  = 0;
        uint32_t cur_league   = 0;
        uint32_t loan_team    = 0;
        uint32_t loan_league  = 0;
        int32_t  days_left    = INT32_MIN;
        uint8_t  restricted   = 0;
        uint8_t  secondary    = 0;
        uint8_t  inj_active   = 0;
        uint8_t  mil_active   = 0;
        if (kbo_player_pointer_plausible(player_ptr)) {
            uint8_t* p = (uint8_t*)player_ptr;
            player_id   = *(uint32_t*)(p + OOTP27_PLAYER_ID_OFFSET);
            parent_team = *(uint32_t*)(p + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
            active_team = *(uint32_t*)(p + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
            cur_league  = *(uint32_t*)(p + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
            loan_team   = *(uint32_t*)(p + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET);
            loan_league = *(uint32_t*)(p + OOTP27_PLAYER_LOAN_LEAGUE_ID_OFFSET);
            days_left   = kbo_military_effective_days_left(p);
            restricted  = p[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET];
            secondary   = p[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET];
            inj_active  = p[OOTP27_PLAYER_INJURY_ACTIVE_OFFSET];
            mil_active  = p[OOTP27_PLAYER_MILITARY_ACTIVE_OFFSET];
        }
        kbo_log_runtimef(
            "KBO military service entry wrapper #%ld original=%p player=%p plausible=%d"
            " player_id=%u parent_team=%u active_team=%u cur_league=%u"
            " loan_team=%u loan_league=%u days_left=%d"
            " restricted=%u secondary=%u inj_active=%u mil_active=%u",
            log_index,
            (void*)original_func_ptr, (void*)player_ptr,
            kbo_player_pointer_plausible(player_ptr),
            player_id, parent_team, active_team, cur_league,
            loan_team, loan_league, days_left,
            (uint32_t)restricted, (uint32_t)secondary,
            (uint32_t)inj_active, (uint32_t)mil_active);
    }

    if (kbo_player_pointer_plausible(player_ptr)) {
        uint8_t* player = (uint8_t*)player_ptr;
        if (player[OOTP27_PLAYER_MILITARY_ACTIVE_OFFSET] != 0
                && kbo_military_effective_days_left(player) > 0) {
            uint32_t cur_year = 0;
            uint32_t cur_month = 0;
            uint32_t cur_day   = 0;
            uint32_t current_yyyymmdd = 0u;
            if (kbo_current_date_tick_latest_published_date(&current_yyyymmdd)) {
                cur_year = current_yyyymmdd / 10000u;
                cur_month = (current_yyyymmdd / 100u) % 100u;
                cur_day = current_yyyymmdd % 100u;
            }
            kbo_log_runtimef(
                "KBO military service entry deferred player=%p player_id=%u"
                " year=%u date=%04u-%02u-%02u days_left=%d",
                (void*)player_ptr,
                *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET),
                cur_year, cur_year, cur_month, cur_day,
                kbo_military_effective_days_left(player));
            uint32_t original_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
            uint32_t original_league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
            uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
            if (active_team_id != 0u) {
                original_team_id = active_team_id;
            }
            if (original_team_id != 0u && cur_year >= KBO_SEASON_YEAR_MIN && cur_year <= KBO_RECORD_YEAR_MAX) {
                kbo_queue_military_draft_candidate(
                    player_ptr,
                    *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET),
                    (uint16_t)cur_year,
                    original_team_id,
                    original_league_id,
                    "military_service_entry_wrapper");
            }
        }
    }
    kbo_perf_probe_record(
        "military_service_entry",
        &perf_total,
        &perf_last,
        &perf_ms,
        &perf_max,
        &perf_tick,
        GetTickCount() - perf_start);
    KBO_HOOK_PROFILE_END(profile_hook, "military.service_entry");
}

__declspec(noinline) void ootp_kbo_military_status_update_wrapper(
    uintptr_t player_ptr,
    uintptr_t original_func_ptr)
{
    KBO_HOOK_PROFILE_BEGIN(profile_hook);
    static volatile LONG perf_total = 0;
    static volatile LONG perf_last = 0;
    static volatile LONG perf_ms = 0;
    static volatile LONG perf_max = 0;
    static volatile LONG perf_tick = 0;
    DWORD perf_start = GetTickCount();
    OotpMilitaryServiceEntryFn original_func = (OotpMilitaryServiceEntryFn)original_func_ptr;
    if (original_func != NULL) {
        KBO_HOOK_PROFILE_PAUSE(profile_hook);
        original_func((void*)player_ptr);
        KBO_HOOK_PROFILE_RESUME(profile_hook);
    }

    kbo_flush_pending_foreign_priority_events("military_status_update_wrapper");

    if (!kbo_fix_enabled() || !kbo_player_is_registered_active_military_loan(player_ptr)) {
        kbo_perf_probe_record(
            "military_status_update",
            &perf_total,
            &perf_last,
            &perf_ms,
            &perf_max,
            &perf_tick,
            GetTickCount() - perf_start);
        KBO_HOOK_PROFILE_RETURN_VOID(profile_hook, "military.status_update");
    }

    kbo_return_completed_military_loan_player(
        (uint8_t*)player_ptr, "military_status_update_wrapper", 0, 1);
    kbo_perf_probe_record(
        "military_status_update",
        &perf_total,
        &perf_last,
        &perf_ms,
        &perf_max,
        &perf_tick,
        GetTickCount() - perf_start);
    KBO_HOOK_PROFILE_END(profile_hook, "military.status_update");
}
