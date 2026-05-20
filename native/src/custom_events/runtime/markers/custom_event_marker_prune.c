#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdint.h>
#include <stddef.h>

#include "custom_event_marker_prune.h"
#include "../../../core/dates/tick/current_date_tick_capture.h"
#include "../../../core/sync/lock.h"
#include "../sql/custom_event_sql_store.h"

uint32_t kbo_custom_event_marker_parse_date(const char* line, size_t line_len)
{
    if (line == NULL || line_len < 9u || line[8] != '|') {
        return 0u;
    }

    uint32_t value = 0u;
    for (size_t i = 0; i < 8u; i++) {
        if (line[i] < '0' || line[i] > '9') {
            return 0u;
        }
        value = (value * 10u) + (uint32_t)(line[i] - '0');
    }
    return value;
}

void kbo_prune_rewound_custom_event_markers(const char* source)
{
    static uint32_t last_pruned_current_date = 0u;
    static KboLock prune_lock = KBO_LOCK_INIT;

    uint32_t current_date = 0u;
    if (!kbo_current_date_tick_latest_published_date(&current_date)) {
        return;
    }
    if (current_date == 0u || current_date == last_pruned_current_date) {
        return;
    }
    if (!kbo_lock_try_enter(&prune_lock)) {
        return;
    }

    kbo_custom_event_sql_prune_rewound_markers(source);
    last_pruned_current_date = current_date;
    kbo_lock_leave(&prune_lock);
}
