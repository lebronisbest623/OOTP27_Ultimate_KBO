#include "../foreign_priority_event_schedule_internal.h"

#include <windows.h>

#include "../../../../bootstrap/abi/ootp_offsets.h"
#include "../../../../core/core_league_context_parts/event_manager/event_manager.h"
#include "../../../../runtime_memory/runtime_memory.h"

typedef struct KboForeignPriorityReadyCache {
    uint32_t anchor_date;
    uint32_t league_id;
    uintptr_t event_manager;
    uintptr_t event_vector;
    int32_t event_count;
    uint8_t valid;
} KboForeignPriorityReadyCache;

static KboForeignPriorityReadyCache g_kbo_foreign_priority_ready_cache = {0};

static int kbo_foreign_priority_event_state(
    uintptr_t* out_event_manager,
    uintptr_t* out_event_vector,
    int32_t* out_event_count)
{
    if (out_event_manager != NULL) { *out_event_manager = 0; }
    if (out_event_vector != NULL) { *out_event_vector = 0; }
    if (out_event_count != NULL) { *out_event_count = 0; }

    uintptr_t event_manager = get_kbo_league_event_manager();
    if (event_manager == 0
            || !memory_range_readable((void*)event_manager, OOTP27_EVENT_MANAGER_EVENT_COUNT_OFFSET + sizeof(int32_t))) {
        return 0;
    }

    uintptr_t event_vector = *(uintptr_t*)(event_manager + OOTP27_EVENT_MANAGER_EVENT_VECTOR_OFFSET);
    int32_t event_count = *(int32_t*)(event_manager + OOTP27_EVENT_MANAGER_EVENT_COUNT_OFFSET);
    if (event_vector == 0 || event_count < 0 || event_count > KBO_RUNTIME_MAX_EVENT_VECTOR_COUNT
            || !memory_range_readable((void*)event_vector, (SIZE_T)event_count * sizeof(uintptr_t))) {
        return 0;
    }

    if (out_event_manager != NULL) { *out_event_manager = event_manager; }
    if (out_event_vector != NULL) { *out_event_vector = event_vector; }
    if (out_event_count != NULL) { *out_event_count = event_count; }
    return 1;
}

int kbo_foreign_priority_ready_cache_hit(uint32_t anchor_date, uint32_t league_id)
{
    KboForeignPriorityReadyCache cached = g_kbo_foreign_priority_ready_cache;
    if (!cached.valid
            || cached.anchor_date != anchor_date
            || cached.league_id != league_id) {
        return 0;
    }

    uintptr_t event_manager = 0;
    uintptr_t event_vector = 0;
    int32_t event_count = 0;
    if (!kbo_foreign_priority_event_state(&event_manager, &event_vector, &event_count)) {
        return 0;
    }
    return cached.event_manager == event_manager
        && cached.event_vector == event_vector
        && cached.event_count == event_count;
}

void kbo_foreign_priority_ready_cache_store(uint32_t anchor_date, uint32_t league_id)
{
    uintptr_t event_manager = 0;
    uintptr_t event_vector = 0;
    int32_t event_count = 0;
    if (!kbo_foreign_priority_event_state(&event_manager, &event_vector, &event_count)) {
        g_kbo_foreign_priority_ready_cache.valid = 0u;
        return;
    }
    g_kbo_foreign_priority_ready_cache = (KboForeignPriorityReadyCache){
        .anchor_date = anchor_date,
        .league_id = league_id,
        .event_manager = event_manager,
        .event_vector = event_vector,
        .event_count = event_count,
        .valid = 1u,
    };
}
