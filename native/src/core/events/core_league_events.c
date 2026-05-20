/* Core league event creation helpers. */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "core_league_events.h"
#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../bootstrap/abi/ootp_typedefs.h"
#include "../../build_verify/build_verify.h"
#include "../../runtime_memory/runtime_memory.h"
#include "../../team/names/team_string.h"
#include "../core_league_context_parts/event_manager/event_manager.h"
#include "../dates/constants/kbo_date_constants.h"
#include "../logging/core_log.h"
#include "../dates/core_text_date.h"
#include "../text/ootp_text_encoding.h"

static int kbo_event_title_ascii_segment_equals_ignore_case(const char* text, size_t len, const char* expected)
{
    if (text == NULL || expected == NULL) {
        return 0;
    }
    size_t expected_len = strlen(expected);
    if (len != expected_len) {
        return 0;
    }
    for (size_t i = 0u; i < len; i++) {
        char a = text[i];
        char b = expected[i];
        if (a >= 'A' && a <= 'Z') {
            a = (char)(a - 'A' + 'a');
        }
        if (b >= 'A' && b <= 'Z') {
            b = (char)(b - 'A' + 'a');
        }
        if (a != b) {
            return 0;
        }
    }
    return 1;
}

static int kbo_league_event_title_prefix_is_known(const char* text, size_t len)
{
    return kbo_event_title_ascii_segment_equals_ignore_case(text, len, "KBO")
        || kbo_event_title_ascii_segment_equals_ignore_case(text, len, "KBO CBT");
}

static int kbo_league_event_title_has_plain_prefix(const char* source, const char* prefix)
{
    if (source == NULL || prefix == NULL) {
        return 0;
    }
    size_t i = 0u;
    for (; prefix[i] != '\0'; i++) {
        if (source[i] == '\0') {
            return 0;
        }
        char a = source[i];
        char b = prefix[i];
        if (a >= 'A' && a <= 'Z') {
            a = (char)(a - 'A' + 'a');
        }
        if (b >= 'A' && b <= 'Z') {
            b = (char)(b - 'A' + 'a');
        }
        if (a != b) {
            return 0;
        }
    }
    return source[i] == ' ';
}

static size_t kbo_league_event_title_content_start(const char* source)
{
    if (source == NULL || source[0] == '\0') {
        return 0u;
    }
    if (source[0] == '[') {
        size_t close_index = 1u;
        while (close_index < 64u && source[close_index] != '\0' && source[close_index] != ']') {
            unsigned char c = (unsigned char)source[close_index];
            if (c < 0x20u) {
                break;
            }
            close_index++;
        }
        if (source[close_index] == ']'
                && source[close_index + 1u] == ' '
                && close_index > 1u
                && kbo_league_event_title_prefix_is_known(source + 1u, close_index - 1u)) {
            return close_index + 2u;
        }
    }
    if (kbo_league_event_title_has_plain_prefix(source, "KBO CBT")) {
        return 8u;
    }
    if (kbo_league_event_title_has_plain_prefix(source, "KBO")) {
        return 4u;
    }
    return 0u;
}

static int kbo_copy_event_title_without_known_prefix(const char* source, char* out, size_t out_size)
{
    if (source == NULL || out == NULL || out_size == 0u) {
        return 0;
    }
    out[0] = '\0';

    size_t source_index = kbo_league_event_title_content_start(source);
    size_t out_index = 0u;
    for (size_t i = source_index; source[i] != '\0' && out_index + 1u < out_size; i++) {
        out[out_index++] = source[i];
    }
    out[out_index] = '\0';
    return out_index > 0u;
}

static int kbo_league_event_title_text_matches(const char* existing_title, const char* expected_title)
{
    if (existing_title == NULL || expected_title == NULL || existing_title[0] == '\0' || expected_title[0] == '\0') {
        return 0;
    }
    if (strcmp(existing_title, expected_title) == 0 || ascii_equals_ignore_case(existing_title, expected_title)) {
        return 1;
    }

    char normalized_existing[160] = {0};
    char normalized_expected[160] = {0};
    if (!kbo_copy_event_title_without_known_prefix(existing_title, normalized_existing, sizeof(normalized_existing))
            || !kbo_copy_event_title_without_known_prefix(expected_title, normalized_expected, sizeof(normalized_expected))) {
        return 0;
    }
    return strcmp(normalized_existing, normalized_expected) == 0
        || ascii_equals_ignore_case(normalized_existing, normalized_expected);
}

static int kbo_league_event_title_matches(const char* existing_title, const char* expected_title)
{
    if (existing_title == NULL || expected_title == NULL || existing_title[0] == '\0' || expected_title[0] == '\0') {
        return 0;
    }
    if (kbo_league_event_title_text_matches(existing_title, expected_title)) {
        return 1;
    }

    char* internal_title = kbo_alloc_ootp_internal_text(expected_title);
    int matches = internal_title != NULL
        && kbo_league_event_title_text_matches(existing_title, internal_title);
    kbo_free_ootp_internal_text(internal_title);
    return matches;
}

static int kbo_league_event_exists(
    uintptr_t event_manager,
    uint32_t year,
    uint32_t month,
    uint32_t day,
    uint32_t league_id,
    uint32_t event_type,
    const char* title)
{
    if (event_manager == 0 || title == NULL || title[0] == '\0') {
        return 0;
    }
    if (!memory_range_readable((void*)(event_manager + OOTP27_EVENT_MANAGER_EVENT_COUNT_OFFSET), sizeof(int32_t))) {
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
        if (*(uint16_t*)(event + OOTP27_LEAGUE_EVENT_TYPE_OFFSET) != (uint16_t)event_type) {
            continue;
        }
        if (*(uint16_t*)(event + OOTP27_LEAGUE_EVENT_YEAR_OFFSET) != (uint16_t)year) {
            continue;
        }
        if (event[OOTP27_LEAGUE_EVENT_MONTH_OFFSET] != (uint8_t)month
                || event[OOTP27_LEAGUE_EVENT_DAY_OFFSET] != (uint8_t)day) {
            continue;
        }

        char existing_title[160] = {0};
        if (!copy_ootp_string_object_raw_text(event, OOTP27_LEAGUE_EVENT_NAME_STRING_OFFSET, existing_title, sizeof(existing_title))) {
            continue;
        }
        if (kbo_league_event_title_matches(existing_title, title)) {
            return 1;
        }
    }

    return 0;
}

int create_kbo_league_event(
    uint32_t year,
    uint32_t month,
    uint32_t day,
    uint32_t league_id,
    uint32_t event_type,
    const char* title,
    uint16_t aux_id,
    const char* source)
{
    if (title == NULL || title[0] == '\0' || league_id == 0
            || year < (int)KBO_HISTORY_YEAR_MIN || year > (int)KBO_SIM_YEAR_MAX
            || month < 1 || month > 12 || day < 1 || day > 31) {
        return 0;
    }

    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        kbo_log_runtimef("league event create skipped source=%s title=%s reason=no_exe_module", source != NULL ? source : "", title);
        return 0;
    }

    uintptr_t event_manager = get_kbo_league_event_manager();
    if (event_manager == 0) {
        kbo_log_runtimef(
            "league event create skipped source=%s title=%s reason=event_manager_unreadable",
            source != NULL ? source : "",
            title);
        return 0;
    }

    if (kbo_league_event_exists(event_manager, year, month, day, league_id, event_type, title)) {
        kbo_log_runtimef(
            "league event create skipped source=%s title=%s reason=already_exists date=%04u-%02u-%02u league_id=%u type=%u manager=%p",
            source != NULL ? source : "",
            title,
            year,
            month,
            day,
            league_id,
            event_type,
            (void*)event_manager);
        return 0;
    }

    OotpCreateLeagueEventFn create_event =
        (OotpCreateLeagueEventFn)kbo_resolve_build_specific_rva_ptr(exe, OOTP27_CREATE_LEAGUE_EVENT_RVA);
    if (!memory_range_readable((void*)create_event, 16)) {
        kbo_log_runtimef("league event create skipped source=%s title=%s reason=build_specific_create_func_unavailable fn=%p", source != NULL ? source : "", title, (void*)create_event);
        return 0;
    }

    uint8_t ootp_date[16] = {0};
    *(uint16_t*)(ootp_date + 8) = (uint16_t)year;
    ootp_date[10] = (uint8_t)day;
    ootp_date[11] = (uint8_t)month;

    char* internal_title = kbo_alloc_ootp_internal_text(title);
    const char* title_for_ootp = internal_title != NULL ? internal_title : title;

    void* event = create_event(
        (void*)event_manager,
        ootp_date,
        event_type,
        league_id,
        title_for_ootp,
        aux_id);

    uint32_t stored_league_id = 0u;
    uint32_t stored_type = 0u;
    if (event != NULL && memory_range_readable(event, 0x48)) {
        stored_league_id = *(uint32_t*)((uint8_t*)event + OOTP27_LEAGUE_EVENT_LEAGUE_ID_OFFSET);
        stored_type = *(uint16_t*)((uint8_t*)event + OOTP27_LEAGUE_EVENT_TYPE_OFFSET);
    }
    kbo_log_runtimef(
        "league event create source=%s title=%s date=%04u-%02u-%02u league_id=%u type=%u aux=%u manager=%p event=%p stored_league_id=%u stored_type=%u",
        source != NULL ? source : "",
        title,
        year,
        month,
        day,
        league_id,
        event_type,
        (uint32_t)aux_id,
        (void*)event_manager,
        event,
        stored_league_id,
        stored_type);
    kbo_log_runtimef(
        "league event verify source=%s title=%s date=%04u-%02u-%02u league_id=%u type=%u present=%d",
        source != NULL ? source : "",
        title,
        year,
        month,
        day,
        league_id,
        event_type,
        kbo_league_event_exists(event_manager, year, month, day, league_id, event_type, title));
    kbo_free_ootp_internal_text(internal_title);
    return event != NULL;
}
