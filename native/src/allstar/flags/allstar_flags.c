/* All-Star league flag enforcement and startup retry. */

#include "allstar_flags.h"

#include <windows.h>

#include "../allstar_league_context/allstar_league_context.h"
#include "../allstar_native_events/generation/event_generation.h"
#include "../allstar_native_events/schedule/schedule_dates.h"
#include "../team_patch/allstar_team_patch.h"
#include "../../build_verify/build_verify.h"
#include "../../core/core_flags/api/flags_api.h"
#include "../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../core/logging/core_log.h"
#include "../../runtime_memory/runtime_memory.h"
#include "../../core/dates/constants/kbo_date_constants.h"

static volatile PVOID g_cached_core_allstar_league_ptr = NULL;
static volatile LONG g_cached_core_allstar_league_id = 0;

static int league_matches_configured_allstar_id(uintptr_t league_ptr, uint32_t league_id, const KboAllstarLayout* layout)
{
    if (league_ptr == 0 || league_id == 0 || layout == NULL) {
        return 0;
    }

    uint8_t* league = (uint8_t*)league_ptr;
    if (!memory_range_readable(league, layout->league_id_fallback_offset + sizeof(uint32_t))) {
        return 0;
    }

    uint32_t legacy_id = 0u;
    if (memory_range_readable(league + OOTP27_KBO_LEAGUE_ID_OFFSET, sizeof(uint32_t))) {
        legacy_id = *(uint32_t*)(league + OOTP27_KBO_LEAGUE_ID_OFFSET);
    }
    uint32_t primary_id = kbo_allstar_read_u32(league, layout->league_id_primary_offset);
    uint32_t fallback_id = kbo_allstar_read_u32(league, layout->league_id_fallback_offset);

    return legacy_id == league_id || primary_id == league_id || fallback_id == league_id;
}

static int league_year_plausible(uintptr_t league_ptr)
{
    if (league_ptr == 0
            || !memory_range_readable((void*)(league_ptr + OOTP27_KBO_LEAGUE_YEAR_OFFSET), sizeof(uint32_t))) {
        return 0;
    }

    uint32_t year = *(uint32_t*)(league_ptr + OOTP27_KBO_LEAGUE_YEAR_OFFSET);
    return year >= KBO_SEASON_YEAR_MIN && year <= KBO_SIM_YEAR_MAX;
}

static uintptr_t normalize_kbo_allstar_league_ptr(uintptr_t league_ptr, uint32_t league_id, const KboAllstarLayout* layout, const char* source)
{
    if (league_ptr == 0 || layout == NULL) {
        return league_ptr;
    }

    if (league_matches_configured_allstar_id(league_ptr, league_id, layout) && league_year_plausible(league_ptr)) {
        return league_ptr;
    }

    static const uintptr_t candidate_deltas[] = {
        8u, (uintptr_t)-8
    };
    for (size_t i = 0; i < sizeof(candidate_deltas) / sizeof(candidate_deltas[0]); i++) {
        uintptr_t candidate = league_ptr + candidate_deltas[i];
        if (!league_matches_configured_allstar_id(candidate, league_id, layout) || !league_year_plausible(candidate)) {
            continue;
        }

        kbo_log_runtimef(
            "KBO all-star normalized league pointer source=%s league_id=%u raw=%p normalized=%p delta=%lld",
            source != NULL ? source : "",
            league_id,
            (void*)league_ptr,
            (void*)candidate,
            (long long)((intptr_t)candidate - (intptr_t)league_ptr));
        return candidate;
    }

    return league_ptr;
}

static int write_kbo_allstar_flags(uintptr_t league_ptr, const char* source)
{
    KboAllstarLayout layout = kbo_get_allstar_layout();
    uint8_t* league = (uint8_t*)league_ptr;
    if (!memory_range_readable(league, layout.team_b_offset + sizeof(uint32_t))) {
        return 0;
    }

    uint8_t old_rules_allstar = league[layout.rules_flag_offset];
    uint8_t old_allstar_game = league[layout.game_flag_offset];
    uint8_t old_auto_schedule_allstar = league[layout.auto_schedule_offset];

    league[layout.rules_flag_offset] = 1;
    league[layout.game_flag_offset] = 1;
    league[layout.auto_schedule_offset] = 1;
    InterlockedExchangePointer((PVOID volatile*)&g_allstar_schedule_import_league_ptr, (PVOID)league_ptr);
    ensure_kbo_allstar_team_ids(league_ptr, source);
    seed_kbo_allstar_schedule_dates(league_ptr, source != NULL ? source : "allstar_flags");

    if (old_rules_allstar != 1 || old_allstar_game != 1 || old_auto_schedule_allstar != 1) {
        uint32_t league_year = *(uint32_t*)(league + OOTP27_KBO_LEAGUE_YEAR_OFFSET);
        kbo_log_runtimef(
            "enabled KBO all-star flags source=%s league=%p year=%u rules 0x%x=%u->1 game 0x%x=%u->1 auto 0x%x=%u->1",
            source,
            league,
            league_year,
            layout.rules_flag_offset,
            old_rules_allstar,
            layout.game_flag_offset,
            old_allstar_game,
            layout.auto_schedule_offset,
            old_auto_schedule_allstar);
    }
    return 1;
}

int enable_kbo_allstar_raw_flags_if_kbo_context(uintptr_t league_ptr, const char* source)
{
    if (!kbo_allstar_raw_kbo_league_context_enabled(league_ptr)) {
        static volatile LONG s_skip_log_count = 0;
        LONG log_index = InterlockedIncrement(&s_skip_log_count);
        if (log_index <= 80) {
            kbo_log_runtimef(
                "KBO all-star raw flag write skipped source=%s league=%p reason=raw_context_not_enabled",
                source != NULL ? source : "",
                (void*)league_ptr);
        }
        return 0;
    }

    KboAllstarLayout layout = kbo_get_allstar_layout();
    uint8_t* league = (uint8_t*)league_ptr;
    uint32_t max_off = layout.game_flag_offset;
    max_off = max_off > layout.auto_schedule_offset ? max_off : layout.auto_schedule_offset;
    max_off = max_off > layout.rules_flag_offset ? max_off : layout.rules_flag_offset;
    if (!memory_range_readable(league, max_off + 1u)) {
        kbo_log_runtimef(
            "KBO all-star raw flag write skipped source=%s league=%p reason=flag_offsets_unreadable max_off=0x%x",
            source != NULL ? source : "",
            league,
            max_off);
        return 0;
    }

    uint8_t old_rules_allstar = league[layout.rules_flag_offset];
    uint8_t old_allstar_game = league[layout.game_flag_offset];
    uint8_t old_auto_schedule_allstar = league[layout.auto_schedule_offset];
    league[layout.rules_flag_offset] = 1;
    league[layout.game_flag_offset] = 1;
    league[layout.auto_schedule_offset] = 1;
    InterlockedExchangePointer((PVOID volatile*)&g_allstar_schedule_import_league_ptr, (PVOID)league_ptr);
    ensure_kbo_allstar_team_ids(league_ptr, source);

    uint32_t league_year = memory_range_readable(league + OOTP27_KBO_LEAGUE_YEAR_OFFSET, sizeof(uint32_t))
        ? *(uint32_t*)(league + OOTP27_KBO_LEAGUE_YEAR_OFFSET)
        : 0u;
    uint32_t league_id = kbo_allstar_read_u32(league, layout.league_id_primary_offset);
    uint32_t fallback_league_id = kbo_allstar_read_u32(league, layout.league_id_fallback_offset);
    if (old_rules_allstar != 1 || old_allstar_game != 1 || old_auto_schedule_allstar != 1) {
        kbo_log_runtimef(
            "enabled KBO all-star raw flags source=%s league=%p ids=%u/%u year=%u rules 0x%x=%u->1 game 0x%x=%u->1 auto 0x%x=%u->1",
            source != NULL ? source : "",
            league,
            league_id,
            fallback_league_id,
            league_year,
            layout.rules_flag_offset,
            old_rules_allstar,
            layout.game_flag_offset,
            old_allstar_game,
            layout.auto_schedule_offset,
            old_auto_schedule_allstar);
    }
    seed_kbo_allstar_schedule_dates(league_ptr, source != NULL ? source : "raw_allstar_flags");
    return 1;
}

void enable_kbo_allstar_flags(uintptr_t league_ptr, const char* source)
{
    if (!kbo_allstar_league_context_enabled(league_ptr)) {
        static volatile LONG s_skip_log_count = 0;
        LONG log_index = InterlockedIncrement(&s_skip_log_count);
        if (log_index <= 40) {
            kbo_log_runtimef(
                "KBO all-star strict flag enable skipped source=%s league=%p reason=context_not_enabled",
                source != NULL ? source : "",
                (void*)league_ptr);
        }
        return;
    }

    (void)write_kbo_allstar_flags(league_ptr, source);
}

int enable_kbo_allstar_flags_for_core_league(uintptr_t league_ptr, uint32_t league_id, const char* source)
{
    OotpBuildInfo info = read_ootp_build_info();
    if (kbo_ootp_build_uses_steam_2026_05_04_layout(info)) {
        static volatile LONG skip_log_count = 0;
        LONG log_index = InterlockedIncrement(&skip_log_count);
        if (log_index <= 60) {
            kbo_log_runtimef(
                "KBO all-star core fallback flag write skipped source=%s league_id=%u league=%p reason=latest_build_preserve_serialized_objects",
                source != NULL ? source : "",
                league_id,
                (void*)league_ptr);
        }
        return 0;
    }

    KboAllstarLayout layout = kbo_get_allstar_layout();
    league_ptr = normalize_kbo_allstar_league_ptr(league_ptr, league_id, &layout, source);
    if (!league_matches_configured_allstar_id(league_ptr, league_id, &layout)) {
        kbo_log_runtimef(
            "KBO all-star core fallback rejected source=%s league_id=%u league=%p reason=id_mismatch_or_unreadable",
            source != NULL ? source : "",
            league_id,
            (void*)league_ptr);
        return 0;
    }
    if (!league_year_plausible(league_ptr)) {
        kbo_log_runtimef(
            "KBO all-star core fallback rejected source=%s league_id=%u league=%p reason=year_unusable",
            source != NULL ? source : "",
            league_id,
            (void*)league_ptr);
        return 0;
    }

    if (!write_kbo_allstar_flags(league_ptr, source)) {
        kbo_log_runtimef(
            "KBO all-star core fallback rejected source=%s league_id=%u league=%p reason=layout_unreadable",
            source != NULL ? source : "",
            league_id,
            (void*)league_ptr);
        return 0;
    }

    kbo_log_runtimef(
        "KBO all-star flags enabled by core league fallback source=%s league_id=%u league=%p",
        source != NULL ? source : "",
        league_id,
        (void*)league_ptr);
    InterlockedExchangePointer((PVOID volatile*)&g_cached_core_allstar_league_ptr, (PVOID)league_ptr);
    InterlockedExchange(&g_cached_core_allstar_league_id, (LONG)league_id);
    return 1;
}

static uintptr_t find_kbo_allstar_core_league_from_extended_global_vectors(uint32_t league_id)
{
    uintptr_t global = get_ootp_global_database();
    if (global == 0 || league_id == 0) {
        return 0;
    }

    KboAllstarLayout layout = kbo_get_allstar_layout();
    for (uint32_t vec_off = 0xa0u; vec_off <= 0x5f8u; vec_off += 8u) {
        uint32_t cnt_off = vec_off + 8u;
        if (!memory_range_readable((void*)(global + vec_off), 16u)) {
            continue;
        }

        uintptr_t candidate_vec = *(uintptr_t*)(global + vec_off);
        int32_t candidate_count = *(int32_t*)(global + cnt_off);
        if (candidate_vec == 0 || candidate_count <= 0 || candidate_count > 10000
                || !memory_range_readable((void*)candidate_vec, (SIZE_T)candidate_count * sizeof(uintptr_t))) {
            continue;
        }

        for (int32_t j = 0; j < candidate_count; j++) {
            uintptr_t candidate = *(uintptr_t*)(candidate_vec + (uintptr_t)j * sizeof(uintptr_t));
            if (!league_matches_configured_allstar_id(candidate, league_id, &layout)) {
                continue;
            }
            uint8_t* league = (uint8_t*)candidate;
            if (!memory_range_readable(league + OOTP27_KBO_LEAGUE_YEAR_OFFSET, sizeof(uint32_t))) {
                continue;
            }
            uint32_t year = *(uint32_t*)(league + OOTP27_KBO_LEAGUE_YEAR_OFFSET);
            if (year < KBO_SEASON_YEAR_MIN || year > KBO_SIM_YEAR_MAX) {
                continue;
            }

            kbo_log_runtimef(
                "KBO all-star core fallback found league in extended global vector league_id=%u vec=0x%x index=%d league=%p year=%u",
                league_id,
                vec_off,
                j,
                (void*)candidate,
                year);
            return candidate;
        }
    }

    return 0;
}

uintptr_t find_kbo_allstar_core_fallback_league(uint32_t league_id)
{
    uintptr_t cached = (uintptr_t)g_cached_core_allstar_league_ptr;
    LONG cached_id = g_cached_core_allstar_league_id;
    KboAllstarLayout layout = kbo_get_allstar_layout();
    if (cached != 0
            && (uint32_t)cached_id == league_id
            && league_matches_configured_allstar_id(cached, league_id, &layout)
            && league_year_plausible(cached)) {
        kbo_log_runtimef(
            "KBO all-star core fallback using cached league league_id=%u league=%p",
            league_id,
            (void*)cached);
        return cached;
    }

    uintptr_t league_ptr = find_kbo_allstar_core_league_from_extended_global_vectors(league_id);
    if (league_ptr != 0) {
        return league_ptr;
    }

    league_ptr = kbo_find_league_ptr_from_global_vectors(league_id);
    if (league_ptr != 0) {
        kbo_log_runtimef(
            "KBO all-star core fallback found league in standard global vectors league_id=%u league=%p",
            league_id,
            (void*)league_ptr);
        return league_ptr;
    }

    return 0;
}

void force_kbo_allstar_flags_for_configured_league(const char* source)
{
    uint32_t league_id = kbo_resolve_kbo_league_id();
    uintptr_t league_ptr = kbo_find_allstar_league_ptr(league_id);
    int enabled = 0;
    if (league_ptr != 0) {
        enable_kbo_allstar_flags(league_ptr, source != NULL ? source : "allstar_force_flags");
        enabled = 1;
    } else {
        league_ptr = find_kbo_allstar_core_fallback_league(league_id);
        if (league_ptr != 0) {
            enabled = enable_kbo_allstar_flags_for_core_league(
                league_ptr,
                league_id,
                source != NULL ? source : "allstar_force_flags_core_fallback");
        }
    }
    if (league_ptr == 0 || !enabled) {
        kbo_log_runtimef("KBO all-star force flags skipped source=%s league_id=%u reason=league_not_found", source != NULL ? source : "", league_id);
        return;
    }
    InterlockedExchangePointer((PVOID volatile*)&g_allstar_schedule_import_league_ptr, (PVOID)league_ptr);
    seed_kbo_allstar_schedule_dates(league_ptr, source != NULL ? source : "allstar_force_flags");
}

int force_kbo_allstar_flags_for_league_pointer(uintptr_t league_ptr, const char* source)
{
    uint32_t league_id = kbo_resolve_kbo_league_id();

    if (!enable_kbo_allstar_flags_for_core_league(
            league_ptr,
            league_id,
            source != NULL ? source : "allstar_force_flags_pointer")) {
        kbo_log_runtimef(
            "KBO all-star force flags skipped source=%s league_id=%u league=%p reason=pointer_rejected",
            source != NULL ? source : "",
            league_id,
            (void*)league_ptr);
        return 0;
    }

    uintptr_t normalized = (uintptr_t)g_cached_core_allstar_league_ptr;
    if (normalized == 0 || (uint32_t)g_cached_core_allstar_league_id != league_id) {
        normalized = league_ptr;
    }
    InterlockedExchangePointer((PVOID volatile*)&g_allstar_schedule_import_league_ptr, (PVOID)normalized);
    seed_kbo_allstar_schedule_dates(normalized, source != NULL ? source : "allstar_force_flags_pointer");
    return 1;
}
