#include "../common/custom_events_common.h"
#include "custom_event_lookup.h"
#include <stdio.h>
#include <string.h>
#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/logging/core_log.h"
#include "../../../core/dates/core_current_date.h"
#include "../../../core/files/save_paths/core_save_paths.h"
#include "../../../core/dates/core_text_date.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../allstar/allstar_league_context/allstar_league_context.h"
#include "../../../core/core_league_context_parts/event_manager/event_manager.h"
#include "../../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../../foreign/common/policy/foreign_waiver_policy.h"
#include "../../../team/names/team_string.h"
#include "../names/custom_event_names.h"
#include "../../../core/dates/constants/kbo_date_constants.h"

typedef struct KboLatestOffseasonStartsCache {
    uintptr_t event_manager;
    uintptr_t event_vector;
    int32_t event_count;
    uint32_t league_id;
    uint32_t cached_at_yyyymmdd;
    uint32_t latest_offseason_start;
    uint32_t next_offseason_start;
    uint8_t valid;
} KboLatestOffseasonStartsCache;

static KboLatestOffseasonStartsCache g_kbo_latest_offseason_starts_cache = {0};

uint32_t kbo_get_latest_offseason_starts_event(uint32_t today_yyyymmdd)
{
    if (today_yyyymmdd == 0u) {
        return 0u;
    }
    uint32_t league_id = kbo_resolve_kbo_league_id();

    uintptr_t event_manager = get_kbo_league_event_manager();
    if (event_manager == 0
            || !memory_range_readable((void*)event_manager, OOTP27_EVENT_MANAGER_EVENT_COUNT_OFFSET + sizeof(int32_t))) {
        return 0u;
    }

    uintptr_t event_vector = *(uintptr_t*)(event_manager + OOTP27_EVENT_MANAGER_EVENT_VECTOR_OFFSET);
    int32_t event_count = *(int32_t*)(event_manager + OOTP27_EVENT_MANAGER_EVENT_COUNT_OFFSET);
    if (event_vector == 0 || event_count <= 0 || event_count > KBO_RUNTIME_MAX_EVENT_VECTOR_COUNT
            || !memory_range_readable((void*)event_vector, (SIZE_T)event_count * sizeof(uintptr_t))) {
        return 0u;
    }

    KboLatestOffseasonStartsCache cached = g_kbo_latest_offseason_starts_cache;
    /*
     * A missing next Offseason starts event is not stable: OOTP can materialize
     * the next offseason marker later without changing the event vector shape.
     */
    int cache_covers_today = cached.next_offseason_start != 0u
        ? today_yyyymmdd < cached.next_offseason_start
        : today_yyyymmdd == cached.cached_at_yyyymmdd;
    if (cached.valid
            && cached.event_manager == event_manager
            && cached.event_vector == event_vector
            && cached.event_count == event_count
            && cached.league_id == league_id
            && today_yyyymmdd >= cached.cached_at_yyyymmdd
            && cache_covers_today) {
        return cached.latest_offseason_start;
    }

    uint32_t latest_offseason_start = 0u;
    uint32_t next_offseason_start = 0u;
    for (int32_t i = 0; i < event_count; i++) {
        uintptr_t event_ptr = *(uintptr_t*)(event_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (event_ptr == 0 || !memory_range_readable((void*)event_ptr, 0x48)) {
            continue;
        }

        uint8_t* event = (uint8_t*)event_ptr;
        if (event[OOTP27_LEAGUE_EVENT_DELETED_OFFSET] != 0) {
            continue;
        }
        if (*(uint32_t*)(event + OOTP27_LEAGUE_EVENT_LEAGUE_ID_OFFSET) != league_id) {
            continue;
        }

        uint32_t event_year = *(uint16_t*)(event + OOTP27_LEAGUE_EVENT_YEAR_OFFSET);
        uint32_t event_month = event[OOTP27_LEAGUE_EVENT_MONTH_OFFSET];
        uint32_t event_day = event[OOTP27_LEAGUE_EVENT_DAY_OFFSET];
        if (event_year < KBO_TEXT_DATE_YEAR_MIN || event_year > KBO_SIM_YEAR_MAX || event_month < 1u || event_month > 12u || event_day < 1u || event_day > 31u) {
            continue;
        }

        uint32_t event_yyyymmdd = event_year * 10000u + event_month * 100u + event_day;

        char name[160] = {0};
        if (!copy_ootp_string_object_raw_text(event, OOTP27_LEAGUE_EVENT_NAME_STRING_OFFSET, name, sizeof(name))) {
            continue;
        }

        if (_stricmp(name, "Offseason starts") == 0) {
            if (event_yyyymmdd <= today_yyyymmdd) {
                if (event_yyyymmdd > latest_offseason_start) {
                    latest_offseason_start = event_yyyymmdd;
                }
            } else if (next_offseason_start == 0u || event_yyyymmdd < next_offseason_start) {
                next_offseason_start = event_yyyymmdd;
            }
        }
    }

    g_kbo_latest_offseason_starts_cache = (KboLatestOffseasonStartsCache){
        .event_manager = event_manager,
        .event_vector = event_vector,
        .event_count = event_count,
        .league_id = league_id,
        .cached_at_yyyymmdd = today_yyyymmdd,
        .latest_offseason_start = latest_offseason_start,
        .next_offseason_start = next_offseason_start,
        .valid = 1u,
    };
    return latest_offseason_start;
}

uint32_t kbo_detect_offseason_anchor_by_league_year(uint32_t league_id, uint32_t today_yyyymmdd, const char* source)
{
    if (league_id == 0u || today_yyyymmdd == 0u) {
        return 0u;
    }

    uint32_t current_year = today_yyyymmdd / 10000u;
    if ((today_yyyymmdd % 10000u) < 1001u) {
        return 0u;
    }

    uint32_t league_year = kbo_find_league_year_from_id_no_scan(league_id);
    if (league_year < KBO_TEXT_DATE_YEAR_MIN || league_year > KBO_RECORD_YEAR_MAX) {
        return 0u;
    }

    uint32_t previous_observed = g_kbo_custom_event_last_observed_league_year;

    if (league_year > current_year) {
        if (previous_observed == 0u || league_year > previous_observed) {
            g_kbo_custom_event_last_observed_league_year = league_year;
        }
        kbo_log_runtimef(
            "KBO custom event schedule fallback source=%s reason=league_year_ahead today=%u current_year=%u league_year=%u previous_league_year=%u",
            source != NULL ? source : "",
            today_yyyymmdd,
            current_year,
            league_year,
            previous_observed);
        return today_yyyymmdd;
    }

    if (previous_observed != 0u && league_year > previous_observed && (today_yyyymmdd % 10000u) >= 1001u) {
        g_kbo_custom_event_last_observed_league_year = league_year;
        kbo_log_runtimef(
            "KBO custom event schedule fallback source=%s reason=league_year_advanced today=%u current_year=%u league_year=%u previous_league_year=%u",
            source != NULL ? source : "",
            today_yyyymmdd,
            current_year,
            league_year,
            previous_observed);
        return today_yyyymmdd;
    }

    if (previous_observed == 0u || league_year > previous_observed) {
        g_kbo_custom_event_last_observed_league_year = league_year;
    }

    return 0u;
}

int kbo_custom_event_exists_for_date(
    uint32_t league_id,
    uint32_t event_yyyymmdd,
    int expect_open)
{
    if (event_yyyymmdd == 0u) {
        return 0;
    }

    uintptr_t event_manager = get_kbo_league_event_manager();
    if (event_manager == 0
            || !memory_range_readable((void*)event_manager, OOTP27_EVENT_MANAGER_EVENT_COUNT_OFFSET + sizeof(int32_t))) {
        return 0;
    }

    uintptr_t event_vector = *(uintptr_t*)(event_manager + OOTP27_EVENT_MANAGER_EVENT_VECTOR_OFFSET);
    int32_t event_count = *(int32_t*)(event_manager + OOTP27_EVENT_MANAGER_EVENT_COUNT_OFFSET);
    if (event_vector == 0 || event_count <= 0 || event_count > KBO_RUNTIME_MAX_EVENT_VECTOR_COUNT
            || !memory_range_readable((void*)event_vector, (SIZE_T)event_count * sizeof(uintptr_t))) {
        return 0;
    }

    for (int32_t i = 0; i < event_count; i++) {
        uintptr_t event_ptr = *(uintptr_t*)(event_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (event_ptr == 0 || !memory_range_readable((void*)event_ptr, 0x48)) {
            continue;
        }

        uint8_t* event = (uint8_t*)event_ptr;
        if (event[OOTP27_LEAGUE_EVENT_DELETED_OFFSET] != 0) {
            continue;
        }
        if (*(uint32_t*)(event + OOTP27_LEAGUE_EVENT_LEAGUE_ID_OFFSET) != league_id) {
            continue;
        }
        if (*(uint16_t*)(event + OOTP27_LEAGUE_EVENT_YEAR_OFFSET) != (uint16_t)(event_yyyymmdd / 10000u)) {
            continue;
        }
        if (event[OOTP27_LEAGUE_EVENT_MONTH_OFFSET] != (uint8_t)((event_yyyymmdd / 100u) % 100u)) {
            continue;
        }
        if (event[OOTP27_LEAGUE_EVENT_DAY_OFFSET] != (uint8_t)(event_yyyymmdd % 100u)) {
            continue;
        }
        if (*(uint16_t*)(event + OOTP27_LEAGUE_EVENT_TYPE_OFFSET) != (uint16_t)OOTP27_EVENT_TYPE_CUSTOM_EVENT) {
            continue;
        }

        char name[160] = {0};
        if (!copy_ootp_string_object_raw_text(event, OOTP27_LEAGUE_EVENT_NAME_STRING_OFFSET, name, sizeof(name))) {
            continue;
        }
        if (expect_open ? kbo_custom_event_name_is_open(name) : kbo_custom_event_name_is_close(name)) {
            return 1;
        }
    }

    return 0;
}

int kbo_custom_event_name_matches_local(const char* name)
{
    return kbo_custom_event_name_is_open(name)
        || kbo_custom_event_name_is_close(name)
        || kbo_custom_event_name_is_military_selection(name)
        || kbo_custom_event_name_is_asian_games_selection(name)
        || kbo_custom_event_name_is_asian_games_departure(name)
        || kbo_custom_event_name_is_asian_games_final(name)
        || kbo_custom_event_name_is_cbt_exception_deadline(name)
        || kbo_custom_event_name_is_cbt_announcement(name)
        || kbo_custom_event_name_is_fa_declaration(name)
        || kbo_custom_event_name_is_intl_established_fa(name)
        || kbo_custom_event_name_is_independent_team_acquisition_open(name)
        || kbo_custom_event_name_is_secondary_draft(name);
}

int kbo_custom_event_exists_by_title_for_date(
    uint32_t league_id,
    uint32_t event_yyyymmdd,
    const char* expected_title)
{
    if (event_yyyymmdd == 0u || expected_title == NULL || expected_title[0] == '\0') {
        return 0;
    }

    uintptr_t event_manager = get_kbo_league_event_manager();
    if (event_manager == 0
            || !memory_range_readable((void*)event_manager, OOTP27_EVENT_MANAGER_EVENT_COUNT_OFFSET + sizeof(int32_t))) {
        return 0;
    }

    uintptr_t event_vector = *(uintptr_t*)(event_manager + OOTP27_EVENT_MANAGER_EVENT_VECTOR_OFFSET);
    int32_t event_count = *(int32_t*)(event_manager + OOTP27_EVENT_MANAGER_EVENT_COUNT_OFFSET);
    if (event_vector == 0 || event_count <= 0 || event_count > KBO_RUNTIME_MAX_EVENT_VECTOR_COUNT
            || !memory_range_readable((void*)event_vector, (SIZE_T)event_count * sizeof(uintptr_t))) {
        return 0;
    }

    uint32_t year = event_yyyymmdd / 10000u;
    uint32_t month = (event_yyyymmdd / 100u) % 100u;
    uint32_t day = event_yyyymmdd % 100u;
    for (int32_t i = 0; i < event_count; i++) {
        uintptr_t event_ptr = *(uintptr_t*)(event_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (event_ptr == 0 || !memory_range_readable((void*)event_ptr, 0x48)) {
            continue;
        }

        uint8_t* event = (uint8_t*)event_ptr;
        if (event[OOTP27_LEAGUE_EVENT_DELETED_OFFSET] != 0) {
            continue;
        }
        if (*(uint32_t*)(event + OOTP27_LEAGUE_EVENT_LEAGUE_ID_OFFSET) != league_id) {
            continue;
        }
        if (*(uint16_t*)(event + OOTP27_LEAGUE_EVENT_TYPE_OFFSET) != (uint16_t)OOTP27_EVENT_TYPE_CUSTOM_EVENT) {
            continue;
        }
        if (*(uint16_t*)(event + OOTP27_LEAGUE_EVENT_YEAR_OFFSET) != (uint16_t)year
                || event[OOTP27_LEAGUE_EVENT_MONTH_OFFSET] != (uint8_t)month
                || event[OOTP27_LEAGUE_EVENT_DAY_OFFSET] != (uint8_t)day) {
            continue;
        }

        char name[160] = {0};
        if (copy_ootp_string_object_raw_text(event, OOTP27_LEAGUE_EVENT_NAME_STRING_OFFSET, name, sizeof(name))
                && kbo_custom_event_name_equals_title(name, expected_title)) {
            return 1;
        }
    }

    return 0;
}

int kbo_custom_event_exists_by_kind_for_date(
    uint32_t league_id,
    uint32_t event_yyyymmdd,
    KboCustomEventKind kind)
{
    if (event_yyyymmdd == 0u || kind <= KBO_CUSTOM_EVENT_KIND_UNKNOWN || kind >= KBO_CUSTOM_EVENT_KIND_COUNT) {
        return 0;
    }

    uintptr_t event_manager = get_kbo_league_event_manager();
    if (event_manager == 0
            || !memory_range_readable((void*)event_manager, OOTP27_EVENT_MANAGER_EVENT_COUNT_OFFSET + sizeof(int32_t))) {
        return 0;
    }

    uintptr_t event_vector = *(uintptr_t*)(event_manager + OOTP27_EVENT_MANAGER_EVENT_VECTOR_OFFSET);
    int32_t event_count = *(int32_t*)(event_manager + OOTP27_EVENT_MANAGER_EVENT_COUNT_OFFSET);
    if (event_vector == 0 || event_count <= 0 || event_count > KBO_RUNTIME_MAX_EVENT_VECTOR_COUNT
            || !memory_range_readable((void*)event_vector, (SIZE_T)event_count * sizeof(uintptr_t))) {
        return 0;
    }

    uint32_t year = event_yyyymmdd / 10000u;
    uint32_t month = (event_yyyymmdd / 100u) % 100u;
    uint32_t day = event_yyyymmdd % 100u;
    for (int32_t i = 0; i < event_count; i++) {
        uintptr_t event_ptr = *(uintptr_t*)(event_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (event_ptr == 0 || !memory_range_readable((void*)event_ptr, 0x48)) {
            continue;
        }

        uint8_t* event = (uint8_t*)event_ptr;
        if (event[OOTP27_LEAGUE_EVENT_DELETED_OFFSET] != 0) {
            continue;
        }
        if (*(uint32_t*)(event + OOTP27_LEAGUE_EVENT_LEAGUE_ID_OFFSET) != league_id) {
            continue;
        }
        if (*(uint16_t*)(event + OOTP27_LEAGUE_EVENT_TYPE_OFFSET) != (uint16_t)OOTP27_EVENT_TYPE_CUSTOM_EVENT) {
            continue;
        }
        if (*(uint16_t*)(event + OOTP27_LEAGUE_EVENT_YEAR_OFFSET) != (uint16_t)year
                || event[OOTP27_LEAGUE_EVENT_MONTH_OFFSET] != (uint8_t)month
                || event[OOTP27_LEAGUE_EVENT_DAY_OFFSET] != (uint8_t)day) {
            continue;
        }

        char name[160] = {0};
        if (copy_ootp_string_object_raw_text(event, OOTP27_LEAGUE_EVENT_NAME_STRING_OFFSET, name, sizeof(name))
                && kbo_custom_event_name_is_kind(name, kind)) {
            return 1;
        }
    }

    return 0;
}

