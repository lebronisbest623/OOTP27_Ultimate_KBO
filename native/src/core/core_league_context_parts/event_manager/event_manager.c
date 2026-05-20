#include "event_manager.h"
#include <stdio.h>
#include <string.h>

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../dates/constants/kbo_date_constants.h"
#include "../../files/save_paths/core_save_paths.h"
#include "../../logging/core_log.h"
#include "../api/league_context_lookup.h"
#include "../../../runtime_memory/runtime_memory.h"

/* Core KBO league context resolution. */

static int kbo_event_manager_candidate_plausible(uintptr_t event_manager)
{
    if (event_manager == 0
            || !memory_range_readable((void*)event_manager, OOTP27_EVENT_MANAGER_EVENT_COUNT_OFFSET + sizeof(int32_t))) {
        return 0;
    }

    int32_t event_count = *(int32_t*)(event_manager + OOTP27_EVENT_MANAGER_EVENT_COUNT_OFFSET);
    if (event_count < 0 || event_count > KBO_RUNTIME_MAX_EVENT_VECTOR_COUNT) {
        return 0;
    }

    uintptr_t event_vector = *(uintptr_t*)(event_manager + OOTP27_EVENT_MANAGER_EVENT_VECTOR_OFFSET);
    if (event_count == 0) {
        return 1;
    }
    if (event_vector == 0
            || !memory_range_readable((void*)event_vector, (SIZE_T)event_count * sizeof(uintptr_t))) {
        return 0;
    }

    int plausible_events = 0;
    int checked = event_count < 16 ? event_count : 16;
    for (int32_t i = 0; i < checked; i++) {
        uintptr_t event_ptr = *(uintptr_t*)(event_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (event_ptr == 0 || !memory_range_readable((void*)event_ptr, 0x48)) {
            continue;
        }

        uint16_t year = *(uint16_t*)(event_ptr + OOTP27_LEAGUE_EVENT_YEAR_OFFSET);
        uint8_t month = *(uint8_t*)(event_ptr + OOTP27_LEAGUE_EVENT_MONTH_OFFSET);
        uint8_t day = *(uint8_t*)(event_ptr + OOTP27_LEAGUE_EVENT_DAY_OFFSET);
        uint16_t type = *(uint16_t*)(event_ptr + OOTP27_LEAGUE_EVENT_TYPE_OFFSET);
        if (year >= KBO_HISTORY_YEAR_MIN && year <= KBO_SIM_YEAR_MAX
                && month >= 1 && month <= 12
                && day >= 1 && day <= 31
                && type <= 64) {
            plausible_events++;
        }
    }

    return plausible_events > 0;
}

uintptr_t get_kbo_league_event_manager(void)
{
    static uintptr_t cached_event_manager = 0;
    static uint32_t cached_league_id = 0;
    static uint32_t cached_offset = 0;
    static char cached_save_path[MAX_PATH] = {0};
    static int logged_current_date_rejected = 0;

    char current_save_path[MAX_PATH] = {0};
    int have_current_save_path = kbo_get_current_save_path(current_save_path, sizeof(current_save_path));
    uint32_t league_id = kbo_resolve_kbo_league_id();

    if (cached_event_manager != 0) {
        int save_matches = have_current_save_path
            && cached_save_path[0] != '\0'
            && strcmp(cached_save_path, current_save_path) == 0;
        if (save_matches && kbo_event_manager_candidate_plausible(cached_event_manager)) {
            return cached_event_manager;
        }
        if (!save_matches && cached_save_path[0] != '\0') {
            kbo_log_runtimef(
                "KBO event manager cache invalidated reason=save_changed old=%s new=%s manager=%p",
                cached_save_path,
                have_current_save_path ? current_save_path : "",
                (void*)cached_event_manager);
        }
    }
    cached_event_manager = 0;
    cached_save_path[0] = '\0';

    uintptr_t global = get_ootp_global_database();
    if (global != 0 && memory_range_readable((void*)(global + OOTP27_GLOBAL_CURRENT_DATE_OFFSET), sizeof(uintptr_t))) {
        uintptr_t legacy_candidate = *(uintptr_t*)(global + OOTP27_GLOBAL_CURRENT_DATE_OFFSET);
        if (kbo_event_manager_candidate_plausible(legacy_candidate)) {
            cached_event_manager = legacy_candidate;
            cached_league_id = league_id;
            cached_offset = OOTP27_GLOBAL_CURRENT_DATE_OFFSET;
            if (have_current_save_path) {
                snprintf(cached_save_path, sizeof(cached_save_path), "%s", current_save_path);
            }
            kbo_log_runtimef(
                "KBO event manager resolved source=legacy_global league_id=%u offset=0x%x manager=%p",
                cached_league_id,
                cached_offset,
                (void*)cached_event_manager);
            return cached_event_manager;
        }
        if (!logged_current_date_rejected) {
            logged_current_date_rejected = 1;
            kbo_log_runtimef(
                "KBO event manager legacy global candidate rejected current_date_ptr=%p",
                (void*)legacy_candidate);
        }
    }

    uintptr_t league_ptr = league_id != 0 ? kbo_find_league_ptr_from_id(league_id) : 0;
    if (league_ptr == 0 || !memory_range_readable((void*)league_ptr, OOTP27_KBO_LEAGUE_SCHEDULE_FILE_STRING_OFFSET + sizeof(uintptr_t))) {
        return 0;
    }

    for (uint32_t offset = 0; offset <= OOTP27_KBO_LEAGUE_SCHEDULE_FILE_STRING_OFFSET; offset += (uint32_t)sizeof(uintptr_t)) {
        if (!memory_range_readable((void*)(league_ptr + offset), sizeof(uintptr_t))) {
            continue;
        }
        uintptr_t candidate = *(uintptr_t*)(league_ptr + offset);
        if (!kbo_event_manager_candidate_plausible(candidate)) {
            continue;
        }

        cached_event_manager = candidate;
        cached_league_id = league_id;
        cached_offset = offset;
        if (have_current_save_path) {
            snprintf(cached_save_path, sizeof(cached_save_path), "%s", current_save_path);
        }
        kbo_log_runtimef(
            "KBO event manager resolved source=league_scan league_id=%u league=%p offset=0x%x manager=%p",
            cached_league_id,
            (void*)league_ptr,
            cached_offset,
            (void*)cached_event_manager);
        return cached_event_manager;
    }

    return 0;
}
