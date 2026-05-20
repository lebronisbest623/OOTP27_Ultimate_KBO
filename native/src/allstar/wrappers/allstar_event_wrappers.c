/* All-Star event and voting preparation wrappers. */

#include "../../bootstrap/abi/hook_entrypoints.h"

#include <stdint.h>
#include <windows.h>

#include "../flags/allstar_flags.h"
#include "../allstar_league_context/allstar_league_context.h"
#include "../allstar_native_events/schedule/schedule_dates.h"
#include "../team_patch/allstar_team_patch.h"
#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../bootstrap/profiling/perf_probe.h"
#include "../../bootstrap/profiling/profiler.h"
#include "../../build_verify/build_verify.h"
#include "../../core/league_roles/kbo_league_roles.h"
#include "../../core/logging/core_log.h"
#include "../../runtime_memory/runtime_memory.h"

__declspec(noinline) void ootp_kbo_prepare_allstar_events(uintptr_t league_ptr)
{
    KBO_HOOK_PROFILE_BEGIN(profile_hook);
    static volatile LONG perf_total = 0;
    static volatile LONG perf_last = 0;
    static volatile LONG perf_ms = 0;
    static volatile LONG perf_max = 0;
    static volatile LONG perf_tick = 0;
    DWORD perf_start = GetTickCount();
    int enabled = 0;
    if (kbo_allstar_league_context_enabled(league_ptr)) {
        enable_kbo_allstar_flags(league_ptr, "make_allstar_game_events");
        enabled = 1;
    } else {
        enabled = enable_kbo_allstar_raw_flags_if_kbo_context(league_ptr, "make_allstar_game_events_raw");
    }
    if (!enabled) {
        force_kbo_allstar_flags_for_league_pointer(league_ptr, "make_allstar_game_events_fallback");
    }
    kbo_perf_probe_record(
        "allstar_events_prepare",
        &perf_total,
        &perf_last,
        &perf_ms,
        &perf_max,
        &perf_tick,
        GetTickCount() - perf_start);
    KBO_HOOK_PROFILE_END(profile_hook, "allstar.prepare_events");
}

static int force_kbo_allstar_candidate_rebuild_once(uintptr_t league_ptr, uint32_t league_id, uint32_t fallback_league_id)
{
    static volatile LONG64 s_last_rebuild_league_ptr = 0;

    if (league_ptr == 0) {
        return 0;
    }

    LONG64 key = (LONG64)league_ptr;
    if (InterlockedCompareExchange64(&s_last_rebuild_league_ptr, key, key) == key) {
        return 0;
    }

    HMODULE exe = GetModuleHandleW(NULL);
    if (exe == NULL) {
        kbo_log_runtime_line("KBO all-star candidate rebuild skipped: host exe unavailable");
        return 0;
    }

    uintptr_t rebuild_addr = (uintptr_t)kbo_resolve_build_specific_rva_ptr(
        exe,
        OOTP27_ALLSTAR_CANDIDATE_REBUILD_FUNC_RVA);
    if (!memory_range_readable((void*)rebuild_addr, 16u)) {
        kbo_log_runtimef(
            "KBO all-star candidate rebuild skipped: target unreadable league=%p rebuild=%p",
            (void*)league_ptr,
            (void*)rebuild_addr);
        return 0;
    }

    OotpAllstarCandidateRebuildFn rebuild_candidates = (OotpAllstarCandidateRebuildFn)rebuild_addr;
    rebuild_candidates(league_ptr, 1u);
    InterlockedExchange64(&s_last_rebuild_league_ptr, key);

    patch_kbo_allstar_team_names_for_league_id(
        league_id != 0u ? league_id : fallback_league_id,
        "allstar_voting_begin_candidate_rebuild");
    patch_kbo_allstar_team_names_for_known_exhibition_teams("allstar_voting_begin_candidate_rebuild");
    kbo_log_runtimef(
        "forced KBO all-star candidate rebuild source=allstar_voting_begin league=%p league_id=%u/%u rebuild=%p",
        (void*)league_ptr,
        league_id,
        fallback_league_id,
        (void*)rebuild_addr);
    return 1;
}

__declspec(noinline) void ootp_kbo_prepare_allstar_voting_begin(uintptr_t league_ptr, uintptr_t allstar_team_setup_ptr)
{
    KBO_HOOK_PROFILE_BEGIN(profile_hook);
    KboAllstarLayout layout = kbo_get_allstar_layout();
    if (league_ptr == 0 || !memory_range_readable((void*)league_ptr, layout.team_b_offset + sizeof(uint32_t))) {
        KBO_HOOK_PROFILE_RETURN_VOID(profile_hook, "allstar.voting_begin");
    }

    uint8_t* league = (uint8_t*)league_ptr;
    uint32_t league_id = kbo_allstar_read_u32(league, layout.league_id_primary_offset);
    uint32_t fallback_league_id = kbo_allstar_read_u32(league, layout.league_id_fallback_offset);
    uint32_t league_year = memory_range_readable(league + OOTP27_KBO_LEAGUE_YEAR_OFFSET, sizeof(uint32_t))
        ? *(uint32_t*)(league + OOTP27_KBO_LEAGUE_YEAR_OFFSET)
        : 0u;
    uint8_t old_allstar_game = league[layout.game_flag_offset];
    uint32_t old_team_a = kbo_allstar_read_u32(league, layout.team_a_offset);
    uint32_t old_team_b = kbo_allstar_read_u32(league, layout.team_b_offset);
    int setup_called = 0;
    int rebuild_called = 0;
    int scoped_context = kbo_allstar_league_context_enabled(league_ptr);

    if (scoped_context) {
        enable_kbo_allstar_flags(league_ptr, "allstar_voting_begin");
    } else {
        uint32_t configured_league_id = kbo_get_foreign_waiver_league_id();
        if (configured_league_id == 0u) {
            configured_league_id = kbo_league_role_main_league_id();
        }
        uint32_t default_league_id = kbo_league_role_main_league_id();
        if (league_id != configured_league_id
                && fallback_league_id != configured_league_id
                && league_id != default_league_id
                && fallback_league_id != default_league_id) {
            kbo_log_runtimef(
                "KBO all-star voting begin skipped league=%p reason=unscoped_non_kbo league_id=%u/%u configured=%u year=%u",
                league,
                league_id,
                fallback_league_id,
                configured_league_id,
                league_year);
            KBO_HOOK_PROFILE_RETURN_VOID(profile_hook, "allstar.voting_begin");
        }

        uint32_t max_off = layout.game_flag_offset > layout.auto_schedule_offset
            ? layout.game_flag_offset : layout.auto_schedule_offset;
        max_off = max_off > layout.rules_flag_offset ? max_off : layout.rules_flag_offset;
        if (!memory_range_readable((void*)(league_ptr + max_off), 1)) {
            kbo_log_runtimef(
                "KBO all-star voting begin skipped league=%p reason=flag_offsets_unreadable max_off=0x%x",
                league,
                max_off);
            KBO_HOOK_PROFILE_RETURN_VOID(profile_hook, "allstar.voting_begin");
        }
        if (!enable_kbo_allstar_raw_flags_if_kbo_context(league_ptr, "allstar_voting_begin_raw")) {
            KBO_HOOK_PROFILE_RETURN_VOID(profile_hook, "allstar.voting_begin");
        }
        ensure_kbo_allstar_team_ids(league_ptr, "allstar_voting_begin_fallback");
        kbo_log_runtimef(
            "KBO all-star voting begin fallback accepted raw league=%p league_id=%u/%u year=%u rules 0x%x game 0x%x auto 0x%x",
            league,
            league_id,
            fallback_league_id,
            league_year,
            layout.rules_flag_offset,
            layout.game_flag_offset,
            layout.auto_schedule_offset);
    }

    if (allstar_team_setup_ptr != 0 && memory_range_readable((void*)allstar_team_setup_ptr, 16u)) {
        OotpAllstarTeamSetupFn setup_allstar_teams = (OotpAllstarTeamSetupFn)allstar_team_setup_ptr;
        setup_allstar_teams(league_ptr);
        setup_called = 1;
        patch_kbo_allstar_team_names_for_league_id(league_id != 0u ? league_id : fallback_league_id, "allstar_voting_begin_setup");
        patch_kbo_allstar_team_names_for_known_exhibition_teams("allstar_voting_begin_setup");
    }

    rebuild_called = force_kbo_allstar_candidate_rebuild_once(league_ptr, league_id, fallback_league_id);

    LONG log_index = InterlockedIncrement(&g_allstar_voting_prepare_log_count);
    if (log_index <= 20) {
        kbo_log_runtimef(
            "prepared KBO all-star voting begin league=%p league_id=%u/%u year=%u game=%u->%u teams=%u/%u->%u/%u setup=%p called=%d rebuild=%d",
            league,
            league_id,
            fallback_league_id,
            league_year,
            old_allstar_game,
            league[layout.game_flag_offset],
            old_team_a,
            old_team_b,
            kbo_allstar_read_u32(league, layout.team_a_offset),
            kbo_allstar_read_u32(league, layout.team_b_offset),
            (void*)allstar_team_setup_ptr,
            setup_called,
            rebuild_called);
    }
    KBO_HOOK_PROFILE_END(profile_hook, "allstar.voting_begin");
}

static int ootp_kbo_allow_single_division_allstar_native_gate(uintptr_t league_ptr, const char* source)
{
    int enabled = enable_kbo_allstar_raw_flags_if_kbo_context(league_ptr, source);
    if (!enabled && kbo_allstar_league_context_enabled(league_ptr)) {
        enable_kbo_allstar_flags(league_ptr, source);
        enabled = 1;
    }
    if (!enabled) {
        return 0;
    }

    InterlockedExchangePointer(
        (PVOID volatile*)&g_allstar_schedule_import_league_ptr,
        (PVOID)league_ptr);
    seed_kbo_allstar_schedule_dates(league_ptr, source);

    static volatile LONG s_log_count = 0;
    LONG log_index = InterlockedIncrement(&s_log_count);
    if (log_index <= 80) {
        KboAllstarLayout layout = kbo_get_allstar_layout();
        uint8_t game_flag = 0;
        uint32_t league_id = 0;
        uint32_t fallback_id = 0;
        uint32_t team_a = 0;
        uint32_t team_b = 0;
        if (memory_range_readable((void*)league_ptr, layout.team_b_offset + sizeof(uint32_t))) {
            uint8_t* league = (uint8_t*)league_ptr;
            game_flag = league[layout.game_flag_offset];
            league_id = kbo_allstar_read_u32(league, layout.league_id_primary_offset);
            fallback_id = kbo_allstar_read_u32(league, layout.league_id_fallback_offset);
            team_a = kbo_allstar_read_u32(league, layout.team_a_offset);
            team_b = kbo_allstar_read_u32(league, layout.team_b_offset);
        }
        kbo_log_runtimef(
            "KBO all-star single-division native gate bypassed source=%s league=%p ids=%u/%u game=0x%x:%u teams=%u/%u",
            source != NULL ? source : "",
            (void*)league_ptr,
            league_id,
            fallback_id,
            layout.game_flag_offset,
            game_flag,
            team_a,
            team_b);
    }

    return 1;
}

__declspec(noinline) int ootp_kbo_allow_single_division_allstar_prep(uintptr_t league_ptr)
{
    KBO_HOOK_PROFILE_BEGIN(profile_hook);
    int result = ootp_kbo_allow_single_division_allstar_native_gate(
        league_ptr,
        "allstar_prep_single_division_gate");
    KBO_HOOK_PROFILE_RETURN(profile_hook, "allstar.single_division_prep", result);
}

__declspec(noinline) int ootp_kbo_allow_single_division_allstar_roster(uintptr_t league_ptr)
{
    KBO_HOOK_PROFILE_BEGIN(profile_hook);
    int result = ootp_kbo_allow_single_division_allstar_native_gate(
        league_ptr,
        "allstar_roster_single_division_gate");
    KBO_HOOK_PROFILE_RETURN(profile_hook, "allstar.single_division_roster", result);
}

__declspec(noinline) int ootp_kbo_allow_single_division_allstar_team_setup(uintptr_t league_ptr)
{
    KBO_HOOK_PROFILE_BEGIN(profile_hook);
    int enabled = enable_kbo_allstar_raw_flags_if_kbo_context(
        league_ptr,
        "allstar_team_setup_single_division_gate_raw");
    if (!enabled && kbo_allstar_league_context_enabled(league_ptr)) {
        enable_kbo_allstar_flags(league_ptr, "allstar_team_setup_single_division_gate");
        enabled = 1;
    }
    if (!enabled) {
        KBO_HOOK_PROFILE_RETURN(profile_hook, "allstar.single_division_team_setup", 0);
    }

    InterlockedExchangePointer(
        (PVOID volatile*)&g_allstar_schedule_import_league_ptr,
        (PVOID)league_ptr);
    seed_kbo_allstar_schedule_dates(league_ptr, "allstar_team_setup_single_division_gate");

    static volatile LONG s_log_count = 0;
    LONG log_index = InterlockedIncrement(&s_log_count);
    if (log_index <= 40) {
        KboAllstarLayout layout = kbo_get_allstar_layout();
        uint8_t game_flag = 0;
        uint32_t league_id = 0;
        uint32_t fallback_id = 0;
        if (memory_range_readable((void*)league_ptr, layout.league_id_fallback_offset + sizeof(uint32_t))) {
            uint8_t* league = (uint8_t*)league_ptr;
            game_flag = league[layout.game_flag_offset];
            league_id = kbo_allstar_read_u32(league, layout.league_id_primary_offset);
            fallback_id = kbo_allstar_read_u32(league, layout.league_id_fallback_offset);
        }
        kbo_log_runtimef(
            "KBO all-star team setup single-division gate bypassed league=%p ids=%u/%u game=0x%x:%u",
            (void*)league_ptr,
            league_id,
            fallback_id,
            layout.game_flag_offset,
            game_flag);
    }
    KBO_HOOK_PROFILE_RETURN(profile_hook, "allstar.single_division_team_setup", 1);
}
