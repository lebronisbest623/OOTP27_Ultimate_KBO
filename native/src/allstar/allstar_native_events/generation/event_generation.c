#include "event_generation.h"

#include <stdint.h>
#include <windows.h>

#include "../../flags/allstar_flags.h"
#include "../../allstar_league_context/allstar_league_context.h"
#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../build_verify/build_verify.h"
#include "../../../core/league_roles/kbo_league_roles.h"
#include "../../../core/logging/core_log.h"
#include "../../../patch_helpers/patch_helpers.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../logging/native_event_logging.h"
#include "../schedule/schedule_dates.h"

int run_kbo_allstar_native_event_generation(uintptr_t league_ptr, const char* source)
{
    if (InterlockedCompareExchange(&g_allstar_native_event_generation_done, 0, 0) != 0) {
        return 1;
    }
    if (InterlockedCompareExchange(&g_allstar_native_event_generation_in_progress, 1, 0) != 0) {
        return 0;
    }

    int generated = 0;
    do {
        KboAllstarLayout layout = kbo_get_allstar_layout();
        uint8_t* league = (uint8_t*)league_ptr;
        if (!memory_range_readable(league, layout.league_id_fallback_offset + sizeof(uint32_t))) {
            kbo_log_runtimef("KBO all-star native events skipped source=%s league=%p reason=league_unreadable", source != NULL ? source : "", league);
            break;
        }

        uint32_t configured_league_id = kbo_get_foreign_waiver_league_id();
        if (configured_league_id == 0u) {
            configured_league_id = kbo_league_role_main_league_id();
        }
        uint32_t legacy_id = memory_range_readable(league + OOTP27_KBO_LEAGUE_ID_OFFSET, sizeof(uint32_t))
            ? *(uint32_t*)(league + OOTP27_KBO_LEAGUE_ID_OFFSET)
            : 0u;
        uint32_t primary_id = kbo_allstar_read_u32(league, layout.league_id_primary_offset);
        uint32_t fallback_id = kbo_allstar_read_u32(league, layout.league_id_fallback_offset);
        int scoped_context = kbo_allstar_league_context_enabled(league_ptr);
        int id_context = legacy_id == configured_league_id
            || primary_id == configured_league_id
            || fallback_id == configured_league_id;

        uintptr_t imported_league_ptr = (uintptr_t)InterlockedCompareExchangePointer(
            (PVOID volatile*)&g_allstar_schedule_import_league_ptr,
            NULL,
            NULL);
        if (imported_league_ptr != league_ptr) {
            kbo_log_runtimef(
                "KBO all-star native events skipped source=%s league=%p imported=%p reason=not_schedule_import_league",
                source != NULL ? source : "",
                league,
                (void*)imported_league_ptr);
            break;
        }
        if (!scoped_context && !id_context) {
            kbo_log_runtimef(
                "KBO all-star native events skipped source=%s league=%p reason=context_not_enabled ids=%u/%u/%u configured=%u",
                source != NULL ? source : "",
                league,
                legacy_id,
                primary_id,
                fallback_id,
                configured_league_id);
            break;
        }

        if (scoped_context) {
            enable_kbo_allstar_flags(league_ptr, source != NULL ? source : "native_allstar_events");
        } else if (enable_kbo_allstar_raw_flags_if_kbo_context(
                league_ptr,
                source != NULL ? source : "native_allstar_events_raw")) {
            /* The native event call tests the raw league object's all-star flag. */
        } else if (!force_kbo_allstar_flags_for_league_pointer(
                league_ptr,
                source != NULL ? source : "native_allstar_events_core_fallback")) {
            break;
        }
        if ((!kbo_allstar_season_start_date_ready(league) || !kbo_allstar_schedule_date_ready(league))
                && !seed_kbo_allstar_schedule_dates(league_ptr, source)) {
            log_kbo_allstar_native_event_state("waiting_date", league_ptr, source);
            break;
        }
        if (!kbo_allstar_season_start_date_ready(league) || !kbo_allstar_schedule_date_ready(league)) {
            log_kbo_allstar_native_event_state("waiting_date", league_ptr, source);
            break;
        }

        OotpMakeAllstarGameEventsFn make_events =
            (OotpMakeAllstarGameEventsFn)InterlockedCompareExchangePointer(
                (PVOID volatile*)&g_allstar_make_events_ptr,
                NULL,
                NULL);
        if (make_events == NULL) {
            HMODULE exe = GetModuleHandleA(NULL);
            if (exe != NULL) {
                make_events = (OotpMakeAllstarGameEventsFn)kbo_resolve_build_specific_rva_ptr(
                    exe,
                    OOTP27_MAKE_ALLSTAR_GAME_EVENTS_RVA);
                if (memory_range_readable((void*)make_events, 16u)) {
                    InterlockedExchangePointer(
                        (PVOID volatile*)&g_allstar_make_events_ptr,
                        (PVOID)make_events);
                } else {
                    make_events = NULL;
                }
            }
        }
        if (make_events == NULL) {
            kbo_log_runtimef("KBO all-star native events skipped source=%s reason=target_unresolved", source != NULL ? source : "");
            break;
        }
        if (!memory_range_readable((void*)make_events, 32u)) {
            kbo_log_runtimef("KBO all-star native events skipped source=%s target=%p reason=target_unreadable", source != NULL ? source : "", (void*)make_events);
            break;
        }
        if (!kbo_allstar_league_vtable_plausible(league_ptr)) {
            kbo_log_runtimef("KBO all-star native events skipped source=%s league=%p reason=league_vtable_invalid", source != NULL ? source : "", league);
            break;
        }

        log_kbo_allstar_native_event_state("before_call", league_ptr, source);
        make_events(league_ptr, 1);
        log_kbo_allstar_native_event_state("after_call", league_ptr, source);

        InterlockedExchange(&g_allstar_native_event_generation_done, 1);
        generated = 1;
    } while (0);

    InterlockedExchange(&g_allstar_native_event_generation_in_progress, 0);
    return generated;
}
