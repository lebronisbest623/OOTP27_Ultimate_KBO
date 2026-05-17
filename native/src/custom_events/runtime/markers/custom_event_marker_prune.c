#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "custom_event_marker_prune.h"
#include "custom_event_markers.h"
#include "../../../core/dates/core_current_date.h"
#include "../../../core/dates/tick/current_date_tick_capture.h"
#include "../../../core/files/save_paths/core_save_paths.h"
#include "../../../core/logging/core_log.h"
#include "../../../core/sync/lock.h"

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

    char path[MAX_PATH] = {0};
    if (!kbo_get_custom_event_processed_marker_path(path, sizeof(path))) {
        kbo_lock_leave(&prune_lock);
        return;
    }

    HANDLE file = CreateFileA(
        path,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == INVALID_HANDLE_VALUE) {
        last_pruned_current_date = current_date;
        kbo_lock_leave(&prune_lock);
        return;
    }

    DWORD high = 0;
    DWORD size = GetFileSize(file, &high);
    if (size == INVALID_FILE_SIZE || high != 0 || size == 0u || size > (256u * 1024u)) {
        CloseHandle(file);
        last_pruned_current_date = current_date;
        kbo_lock_leave(&prune_lock);
        return;
    }

    char* buffer = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)size + 1u);
    char* rewritten = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)size + 2u);
    if (buffer == NULL || rewritten == NULL) {
        if (buffer != NULL) { HeapFree(GetProcessHeap(), 0, buffer); }
        if (rewritten != NULL) { HeapFree(GetProcessHeap(), 0, rewritten); }
        CloseHandle(file);
        last_pruned_current_date = current_date;
        kbo_lock_leave(&prune_lock);
        return;
    }

    DWORD read = 0;
    int should_rewrite = 0;
    int removed = 0;
    size_t rewritten_len = 0u;
    uint32_t max_marker_date = 0u;
    if (ReadFile(file, buffer, size, &read, NULL) && read > 0u) {
        buffer[read] = '\0';

        char* cursor = buffer;
        char* end = buffer + read;
        while (cursor < end) {
            char* line_end = cursor;
            while (line_end < end && *line_end != '\n' && *line_end != '\r') {
                line_end++;
            }
            size_t line_len = (size_t)(line_end - cursor);
            uint32_t marker_date = kbo_custom_event_marker_parse_date(cursor, line_len);
            if (marker_date > max_marker_date) {
                max_marker_date = marker_date;
            }
            while (line_end < end && (*line_end == '\n' || *line_end == '\r')) {
                line_end++;
            }
            cursor = line_end;
        }

        int rewind_detected = max_marker_date > current_date;
        uint32_t current_year = current_date / 10000u;
        cursor = buffer;
        while (cursor < end) {
            char* line_end = cursor;
            while (line_end < end && *line_end != '\n' && *line_end != '\r') {
                line_end++;
            }
            size_t line_len = (size_t)(line_end - cursor);
            uint32_t marker_date = kbo_custom_event_marker_parse_date(cursor, line_len);
            int remove_line = rewind_detected
                && marker_date != 0u
                && (marker_date / 10000u) >= current_year;
            if (remove_line) {
                removed++;
                should_rewrite = 1;
            } else if (line_len > 0u) {
                memcpy(rewritten + rewritten_len, cursor, line_len);
                rewritten_len += line_len;
                rewritten[rewritten_len++] = '\r';
                rewritten[rewritten_len++] = '\n';
            }
            while (line_end < end && (*line_end == '\n' || *line_end == '\r')) {
                line_end++;
            }
            cursor = line_end;
        }
    }
    CloseHandle(file);

    if (should_rewrite) {
        HANDLE out = CreateFileA(
            path,
            GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            NULL);
        if (out != INVALID_HANDLE_VALUE) {
            DWORD written = 0;
            int write_ok = rewritten_len == 0u
                || WriteFile(out, rewritten, (DWORD)rewritten_len, &written, NULL);
            CloseHandle(out);
            kbo_log_runtimef(
                "KBO custom event marker rewind prune source=%s current=%u max_marker=%u removed=%d ok=%d path=%s",
                source != NULL ? source : "",
                current_date,
                max_marker_date,
                removed,
                write_ok ? 1 : 0,
                path);
        }
    }

    HeapFree(GetProcessHeap(), 0, buffer);
    HeapFree(GetProcessHeap(), 0, rewritten);
    last_pruned_current_date = current_date;
    kbo_lock_leave(&prune_lock);
}
