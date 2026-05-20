#include "league_event_type_inventory.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/core_league_context_parts/event_manager/event_manager.h"
#include "../../../core/logging/core_log.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../team/names/team_string.h"

static volatile LONG g_kbo_league_event_type_inventory_dumped = 0;
static volatile uint32_t g_kbo_league_event_type_inventory_last_date = 0u;

void kbo_log_league_event_type_inventory_once(uint32_t current_date)
{
    /* Dump on first call AND when date changes (to catch event_over progression). */
    LONG already = InterlockedCompareExchange(&g_kbo_league_event_type_inventory_dumped, 1, 0);
    uint32_t last_date = g_kbo_league_event_type_inventory_last_date;
    if (already != 0 && last_date == current_date) {
        return;
    }
    g_kbo_league_event_type_inventory_last_date = current_date;

    uintptr_t event_manager = get_kbo_league_event_manager();
    if (event_manager == 0
            || !memory_range_readable(
                (void*)event_manager,
                OOTP27_EVENT_MANAGER_EVENT_COUNT_OFFSET + sizeof(int32_t))) {
        kbo_log_runtimef(
            "KBO league event type inventory skipped date=%08u reason=event_manager_unreadable",
            current_date);
        return;
    }

    uintptr_t event_vector = *(uintptr_t*)(event_manager + OOTP27_EVENT_MANAGER_EVENT_VECTOR_OFFSET);
    int32_t event_count = *(int32_t*)(event_manager + OOTP27_EVENT_MANAGER_EVENT_COUNT_OFFSET);
    if (event_vector == 0 || event_count <= 0 || event_count > KBO_RUNTIME_MAX_EVENT_VECTOR_COUNT
            || !memory_range_readable((void*)event_vector, (SIZE_T)event_count * sizeof(uintptr_t))) {
        kbo_log_runtimef(
            "KBO league event type inventory skipped date=%08u manager=%p count=%d reason=event_vector_unreadable",
            current_date,
            (void*)event_manager,
            event_count);
        return;
    }

    int total_active = 0;
    uint32_t type_counts[256] = {0};
    int type_overflow = 0;

    for (int32_t i = 0; i < event_count; i++) {
        uintptr_t event_ptr = *(uintptr_t*)(event_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (event_ptr == 0 || !memory_range_readable((void*)event_ptr, 0x48)) {
            continue;
        }
        uint8_t* event = (uint8_t*)event_ptr;
        if (event[OOTP27_LEAGUE_EVENT_DELETED_OFFSET] != 0) {
            continue;
        }
        total_active++;

        uint32_t event_type = *(uint16_t*)(event + OOTP27_LEAGUE_EVENT_TYPE_OFFSET);
        if (event_type < 256u) {
            type_counts[event_type]++;
        } else {
            type_overflow++;
        }

        char title[160] = {0};
        copy_ootp_string_object_raw_text(event, OOTP27_LEAGUE_EVENT_NAME_STRING_OFFSET, title, sizeof(title));
        uint32_t league_id = *(uint32_t*)(event + OOTP27_LEAGUE_EVENT_LEAGUE_ID_OFFSET);
        uint32_t event_year = *(uint16_t*)(event + OOTP27_LEAGUE_EVENT_YEAR_OFFSET);
        uint32_t event_month = event[OOTP27_LEAGUE_EVENT_MONTH_OFFSET];
        uint32_t event_day = event[OOTP27_LEAGUE_EVENT_DAY_OFFSET];
        uint32_t event_over = *(uint16_t*)(event + OOTP27_LEAGUE_EVENT_EVENT_OVER_OFFSET);
        uint32_t deleted_byte = event[OOTP27_LEAGUE_EVENT_DELETED_OFFSET];

        kbo_log_runtimef(
            "KBO league event type inventory item date=%08u league_id=%u event=%p type=%u event_date=%04u-%02u-%02u over=%u deleted=%u title=%s",
            current_date,
            league_id,
            (void*)event_ptr,
            event_type,
            event_year,
            event_month,
            event_day,
            event_over,
            deleted_byte,
            title);
    }

    char histogram[1024] = {0};
    size_t pos = 0u;
    for (int t = 0; t < 256; t++) {
        if (type_counts[t] == 0u) {
            continue;
        }
        int written = snprintf(
            histogram + pos,
            sizeof(histogram) - pos,
            "type%d=%u ",
            t,
            (unsigned int)type_counts[t]);
        if (written <= 0 || (size_t)written >= sizeof(histogram) - pos) {
            break;
        }
        pos += (size_t)written;
    }

    kbo_log_runtimef(
        "KBO league event type inventory summary date=%08u manager=%p count=%d active=%d overflow=%d %s",
        current_date,
        (void*)event_manager,
        event_count,
        total_active,
        type_overflow,
        histogram);
}
