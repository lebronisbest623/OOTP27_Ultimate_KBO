#include "../common/custom_events_common.h"
#include "custom_event_markers.h"
#include "custom_event_marker_prune.h"
#include <stdio.h>
#include <string.h>
#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/logging/core_log.h"
#include "../../../core/dates/core_current_date.h"
#include "../../../core/dates/core_text_date.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../core/sql/save_state/save_state_sqlite.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../sql/custom_event_sql_store.h"

int kbo_get_custom_event_processed_marker_path(char* out, size_t out_size)
{
    return kbo_save_state_db_path(out, out_size);
}

int kbo_custom_event_processed_marker_exists(uint32_t event_yyyymmdd, const char* name)
{
    if (event_yyyymmdd == 0u || name == NULL || name[0] == '\0') {
        return 0;
    }
    kbo_prune_rewound_custom_event_markers("marker_exists");
    return kbo_custom_event_sql_marker_exists(event_yyyymmdd, name);
}

int kbo_custom_event_processed_marker_exists_for_kind(uint32_t event_yyyymmdd, KboCustomEventKind kind)
{
    if (event_yyyymmdd == 0u || kind <= KBO_CUSTOM_EVENT_KIND_UNKNOWN || kind >= KBO_CUSTOM_EVENT_KIND_COUNT) {
        return 0;
    }
    kbo_prune_rewound_custom_event_markers("marker_exists_kind");
    return kbo_custom_event_sql_marker_exists_for_kind(event_yyyymmdd, kind);
}

void kbo_persist_custom_event_processed_marker(uint32_t event_yyyymmdd, const char* name, const char* source)
{
    if (event_yyyymmdd == 0u || name == NULL || name[0] == '\0') {
        return;
    }
    if (kbo_custom_event_processed_marker_exists(event_yyyymmdd, name)) {
        return;
    }
    kbo_custom_event_sql_marker_record(event_yyyymmdd, name, source);
}

void kbo_mark_custom_event_over(uintptr_t event_ptr)
{
    if (event_ptr == 0
            || !memory_range_readable((void*)event_ptr, OOTP27_LEAGUE_EVENT_EVENT_OVER_OFFSET + sizeof(uint16_t))) {
        return;
    }

    uint16_t* event_over = (uint16_t*)((uint8_t*)event_ptr + OOTP27_LEAGUE_EVENT_EVENT_OVER_OFFSET);
    DWORD old_protect = 0;
    if (VirtualProtect(event_over, sizeof(*event_over), PAGE_READWRITE, &old_protect)) {
        *event_over = 1u;
        DWORD ignored = 0;
        VirtualProtect(event_over, sizeof(*event_over), old_protect, &ignored);
    }
}

int kbo_custom_event_already_processed(uintptr_t event_ptr)
{
    LONG count = g_kbo_processed_event_count;
    if (count < 0) {
        count = 0;
    }
    if (count > (LONG)(sizeof(g_kbo_processed_event_ptrs) / sizeof(g_kbo_processed_event_ptrs[0]))) {
        count = (LONG)(sizeof(g_kbo_processed_event_ptrs) / sizeof(g_kbo_processed_event_ptrs[0]));
    }

    for (LONG i = 0; i < count; i++) {
        if (g_kbo_processed_event_ptrs[i] == event_ptr) {
            return 1;
        }
    }
    return 0;
}

void kbo_mark_custom_event_processed(uintptr_t event_ptr)
{
    if (event_ptr == 0) {
        return;
    }
    kbo_mark_custom_event_over(event_ptr);
    if (kbo_custom_event_already_processed(event_ptr)) {
        return;
    }

    LONG slot = InterlockedIncrement(&g_kbo_processed_event_count) - 1;
    if (slot < 0 || slot >= (LONG)(sizeof(g_kbo_processed_event_ptrs) / sizeof(g_kbo_processed_event_ptrs[0]))) {
        InterlockedDecrement(&g_kbo_processed_event_count);
        return;
    }
    g_kbo_processed_event_ptrs[slot] = event_ptr;
}
