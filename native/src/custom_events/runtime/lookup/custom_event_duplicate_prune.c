#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdint.h>

#include "custom_event_duplicate_prune.h"
#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/core_league_context_parts/event_manager/event_manager.h"
#include "../../../core/logging/core_log.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../team/names/team_string.h"
#include "../names/custom_event_names.h"

static int kbo_custom_event_row_matches_kind_date(
    uint8_t* event,
    uint32_t league_id,
    uint32_t event_yyyymmdd,
    KboCustomEventKind kind)
{
    if (event == NULL
            || event[OOTP27_LEAGUE_EVENT_DELETED_OFFSET] != 0
            || *(uint32_t*)(event + OOTP27_LEAGUE_EVENT_LEAGUE_ID_OFFSET) != league_id
            || *(uint16_t*)(event + OOTP27_LEAGUE_EVENT_TYPE_OFFSET) != (uint16_t)OOTP27_EVENT_TYPE_CUSTOM_EVENT
            || *(uint16_t*)(event + OOTP27_LEAGUE_EVENT_YEAR_OFFSET) != (uint16_t)(event_yyyymmdd / 10000u)
            || event[OOTP27_LEAGUE_EVENT_MONTH_OFFSET] != (uint8_t)((event_yyyymmdd / 100u) % 100u)
            || event[OOTP27_LEAGUE_EVENT_DAY_OFFSET] != (uint8_t)(event_yyyymmdd % 100u)) {
        return 0;
    }

    char name[160] = {0};
    return copy_ootp_string_object_raw_text(event, OOTP27_LEAGUE_EVENT_NAME_STRING_OFFSET, name, sizeof(name))
        && kbo_custom_event_name_is_kind(name, kind);
}

static void kbo_mark_custom_event_deleted(uint8_t* event)
{
    if (event == NULL
            || !memory_range_readable(event, OOTP27_LEAGUE_EVENT_EVENT_OVER_OFFSET + sizeof(uint16_t))) {
        return;
    }

    DWORD old_deleted_protect = 0;
    if (VirtualProtect(event + OOTP27_LEAGUE_EVENT_DELETED_OFFSET, sizeof(uint8_t), PAGE_READWRITE, &old_deleted_protect)) {
        event[OOTP27_LEAGUE_EVENT_DELETED_OFFSET] = 1u;
        DWORD ignored = 0;
        VirtualProtect(event + OOTP27_LEAGUE_EVENT_DELETED_OFFSET, sizeof(uint8_t), old_deleted_protect, &ignored);
    }

    uint16_t* event_over = (uint16_t*)(event + OOTP27_LEAGUE_EVENT_EVENT_OVER_OFFSET);
    DWORD old_over_protect = 0;
    if (VirtualProtect(event_over, sizeof(*event_over), PAGE_READWRITE, &old_over_protect)) {
        *event_over = 1u;
        DWORD ignored = 0;
        VirtualProtect(event_over, sizeof(*event_over), old_over_protect, &ignored);
    }
}

int kbo_prune_duplicate_custom_events_by_kind_for_date(
    uint32_t league_id,
    uint32_t event_yyyymmdd,
    KboCustomEventKind kind,
    const char* source)
{
    if (league_id == 0u
            || event_yyyymmdd == 0u
            || kind <= KBO_CUSTOM_EVENT_KIND_UNKNOWN
            || kind >= KBO_CUSTOM_EVENT_KIND_COUNT) {
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

    uintptr_t keep_event = 0;
    int matched = 0;
    int pruned = 0;
    for (int32_t i = 0; i < event_count; i++) {
        uintptr_t event_ptr = *(uintptr_t*)(event_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (event_ptr == 0 || !memory_range_readable((void*)event_ptr, 0x48)) {
            continue;
        }

        uint8_t* event = (uint8_t*)event_ptr;
        if (!kbo_custom_event_row_matches_kind_date(event, league_id, event_yyyymmdd, kind)) {
            continue;
        }

        matched++;
        if (keep_event == 0) {
            keep_event = event_ptr;
            continue;
        }

        kbo_mark_custom_event_deleted(event);
        pruned++;
    }

    if (pruned > 0) {
        kbo_log_runtimef(
            "KBO custom event duplicate prune source=%s league=%u date=%u kind=%d matched=%d pruned=%d keep=%p",
            source != NULL ? source : "",
            league_id,
            event_yyyymmdd,
            (int)kind,
            matched,
            pruned,
            (void*)keep_event);
    }
    return pruned;
}

int kbo_delete_custom_events_by_kind_for_date(
    uint32_t league_id,
    uint32_t event_yyyymmdd,
    KboCustomEventKind kind,
    const char* source)
{
    if (league_id == 0u
            || event_yyyymmdd == 0u
            || kind <= KBO_CUSTOM_EVENT_KIND_UNKNOWN
            || kind >= KBO_CUSTOM_EVENT_KIND_COUNT) {
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

    int deleted = 0;
    for (int32_t i = 0; i < event_count; i++) {
        uintptr_t event_ptr = *(uintptr_t*)(event_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (event_ptr == 0 || !memory_range_readable((void*)event_ptr, 0x48)) {
            continue;
        }

        uint8_t* event = (uint8_t*)event_ptr;
        if (!kbo_custom_event_row_matches_kind_date(event, league_id, event_yyyymmdd, kind)) {
            continue;
        }

        kbo_mark_custom_event_deleted(event);
        deleted++;
    }

    if (deleted > 0) {
        kbo_log_runtimef(
            "KBO custom event deleted source=%s league=%u date=%u kind=%d deleted=%d reason=rescheduled",
            source != NULL ? source : "",
            league_id,
            event_yyyymmdd,
            (int)kind,
            deleted);
    }
    return deleted;
}
