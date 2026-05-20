#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>

#include "../core/logging/core_log.h"
#include "../runtime_memory/runtime_memory.h"
#include "season_phase_monitor_internal.h"

void kbo_log_season_phase_changed_words(uintptr_t league_ptr, uint32_t date_key)
{
    enum { BASE_OFFSET = 0x4000u, END_OFFSET = 0x4d00u, WORD_COUNT = (END_OFFSET - BASE_OFFSET) / 4u };
    static uintptr_t last_ptr = 0;
    static uint32_t last_date_key = 0;
    static uint32_t last_words[WORD_COUNT];
    static int has_snapshot = 0;

    if (league_ptr == 0 || date_key == 0) {
        return;
    }
    if (!memory_range_readable((void*)(league_ptr + BASE_OFFSET), END_OFFSET - BASE_OFFSET)) {
        has_snapshot = 0;
        last_ptr = 0;
        return;
    }
    if (league_ptr != last_ptr || !has_snapshot) {
        for (uint32_t i = 0; i < WORD_COUNT; i++) {
            last_words[i] = *(uint32_t*)(league_ptr + BASE_OFFSET + i * 4u);
        }
        last_ptr = league_ptr;
        last_date_key = date_key;
        has_snapshot = 1;
        kbo_log_runtimef("KBO season phase diff baseline league=%p date=%u range=0x%x-0x%x", (void*)league_ptr, date_key, BASE_OFFSET, END_OFFSET);
        return;
    }
    if (date_key == last_date_key) {
        return;
    }

    char changes[1600];
    size_t used = 0;
    int count = 0;
    int truncated = 0;
    changes[0] = '\0';

    for (uint32_t i = 0; i < WORD_COUNT; i++) {
        uint32_t offset = BASE_OFFSET + i * 4u;
        uint32_t old_value = last_words[i];
        uint32_t new_value = *(uint32_t*)(league_ptr + offset);
        if (old_value == new_value) {
            continue;
        }

        count++;
        if (used + 64u < sizeof(changes)) {
            int wrote = snprintf(
                changes + used,
                sizeof(changes) - used,
                "%s%04x:%u->%u",
                used == 0 ? "" : " ",
                offset,
                old_value,
                new_value);
            if (wrote > 0) {
                used += (size_t)wrote;
            }
        } else {
            truncated = 1;
        }
        last_words[i] = new_value;
    }

    if (count > 0) {
        kbo_log_runtimef(
            "KBO season phase diff league=%p date=%u prev_date=%u count=%d%s changes=%s",
            (void*)league_ptr,
            date_key,
            last_date_key,
            count,
            truncated ? " truncated=1" : "",
            changes);
    }
    last_date_key = date_key;
}
