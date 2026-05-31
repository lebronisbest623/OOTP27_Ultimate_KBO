#include "../log_event_internal.h"

#include "../../../files/save_paths/platform/core_path_io.h"
#include "../../../sync/spin_lock.h"

#include <stdio.h>
#include <string.h>
#include <windows.h>

#define KBO_LOG_BUFFERED_SINK_COUNT 4u
#define KBO_LOG_BUFFER_BYTES 32768u
#define KBO_LOG_BUFFER_FLUSH_INTERVAL_MS 1000u

static KboSpinLock g_kbo_log_event_file_lock = KBO_SPIN_LOCK_INIT;
typedef struct KboLogBufferedSink {
    char path[MAX_PATH];
    HANDLE file;
    char buffer[KBO_LOG_BUFFER_BYTES];
    size_t used;
    uint64_t known_size;
    ULONGLONG last_flush_tick;
} KboLogBufferedSink;

static KboLogBufferedSink g_kbo_log_buffered_sinks[KBO_LOG_BUFFERED_SINK_COUNT];

static void kbo_log_rotate_if_needed(const char* path, size_t max_bytes, int archive_count);

static int kbo_log_path_equals(const char* left, const char* right)
{
    return left != NULL && right != NULL && _stricmp(left, right) == 0;
}

static int kbo_log_append_ndjson_path(const char* path, const char* json)
{
    HANDLE file = kbo_create_file_utf8(
        path,
        FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL);
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }
    DWORD written = 0;
    DWORD len = (DWORD)strlen(json);
    int ok = WriteFile(file, json, len, &written, NULL) && written == len;
    const char newline[] = "\r\n";
    DWORD newline_written = 0;
    ok = ok && WriteFile(file, newline, (DWORD)(sizeof(newline) - 1u), &newline_written, NULL)
        && newline_written == (DWORD)(sizeof(newline) - 1u);
    CloseHandle(file);
    return ok;
}

static int kbo_log_file_handle_valid(HANDLE file)
{
    return file != NULL && file != INVALID_HANDLE_VALUE;
}

static uint64_t kbo_log_file_size_or_zero(const char* path)
{
    WIN32_FILE_ATTRIBUTE_DATA attrs;
    if (!kbo_get_file_attributes_ex_utf8(path, &attrs)) {
        return 0u;
    }
    return ((uint64_t)attrs.nFileSizeHigh << 32) | (uint64_t)attrs.nFileSizeLow;
}

static int kbo_log_buffered_sink_open(KboLogBufferedSink* sink)
{
    if (sink == NULL || sink->path[0] == '\0') {
        return 0;
    }
    if (kbo_log_file_handle_valid(sink->file)) {
        return 1;
    }
    sink->file = kbo_create_file_utf8(
        sink->path,
        FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL);
    if (!kbo_log_file_handle_valid(sink->file)) {
        sink->file = NULL;
        return 0;
    }
    sink->known_size = kbo_log_file_size_or_zero(sink->path);
    sink->last_flush_tick = GetTickCount64();
    return 1;
}

static int kbo_log_buffered_sink_flush_unlocked(KboLogBufferedSink* sink)
{
    if (sink == NULL || sink->used == 0u) {
        return 1;
    }
    if (!kbo_log_buffered_sink_open(sink)) {
        sink->used = 0u;
        return 0;
    }

    DWORD len = (DWORD)sink->used;
    DWORD written = 0;
    int ok = WriteFile(sink->file, sink->buffer, len, &written, NULL) && written == len;
    if (ok) {
        sink->known_size += (uint64_t)written;
    }
    sink->used = 0u;
    sink->last_flush_tick = GetTickCount64();
    return ok;
}

static void kbo_log_buffered_sink_close_unlocked(KboLogBufferedSink* sink)
{
    if (sink == NULL) {
        return;
    }
    kbo_log_buffered_sink_flush_unlocked(sink);
    if (kbo_log_file_handle_valid(sink->file)) {
        CloseHandle(sink->file);
    }
    memset(sink, 0, sizeof(*sink));
}

static KboLogBufferedSink* kbo_log_buffered_sink_for_path_unlocked(const char* path)
{
    if (path == NULL || path[0] == '\0') {
        return NULL;
    }
    for (size_t i = 0u; i < KBO_LOG_BUFFERED_SINK_COUNT; i++) {
        if (g_kbo_log_buffered_sinks[i].path[0] != '\0'
                && _stricmp(g_kbo_log_buffered_sinks[i].path, path) == 0) {
            return &g_kbo_log_buffered_sinks[i];
        }
    }
    for (size_t i = 0u; i < KBO_LOG_BUFFERED_SINK_COUNT; i++) {
        if (g_kbo_log_buffered_sinks[i].path[0] == '\0') {
            snprintf(g_kbo_log_buffered_sinks[i].path, sizeof(g_kbo_log_buffered_sinks[i].path), "%s", path);
            g_kbo_log_buffered_sinks[i].known_size = kbo_log_file_size_or_zero(path);
            g_kbo_log_buffered_sinks[i].last_flush_tick = GetTickCount64();
            return &g_kbo_log_buffered_sinks[i];
        }
    }

    kbo_log_buffered_sink_close_unlocked(&g_kbo_log_buffered_sinks[0]);
    snprintf(g_kbo_log_buffered_sinks[0].path, sizeof(g_kbo_log_buffered_sinks[0].path), "%s", path);
    g_kbo_log_buffered_sinks[0].known_size = kbo_log_file_size_or_zero(path);
    g_kbo_log_buffered_sinks[0].last_flush_tick = GetTickCount64();
    return &g_kbo_log_buffered_sinks[0];
}

static void kbo_log_buffered_sink_rotate_if_needed_unlocked(
    KboLogBufferedSink* sink,
    size_t max_bytes,
    int archive_count)
{
    if (sink == NULL || sink->path[0] == '\0' || max_bytes == 0u) {
        return;
    }
    if (sink->known_size + sink->used < (uint64_t)max_bytes) {
        return;
    }

    kbo_log_buffered_sink_flush_unlocked(sink);
    if (sink->known_size < (uint64_t)max_bytes) {
        return;
    }
    if (kbo_log_file_handle_valid(sink->file)) {
        CloseHandle(sink->file);
        sink->file = NULL;
    }
    kbo_log_rotate_if_needed(sink->path, max_bytes, archive_count);
    sink->known_size = kbo_log_file_size_or_zero(sink->path);
}

static int kbo_log_buffered_append_ndjson_path_unlocked(
    const char* path,
    const char* json,
    size_t max_bytes,
    int archive_count,
    int flush_now)
{
    if (path == NULL || path[0] == '\0' || json == NULL) {
        return 0;
    }

    size_t json_len = strlen(json);
    size_t line_len = json_len + 2u;
    if (line_len >= KBO_LOG_BUFFER_BYTES) {
        KboLogBufferedSink* direct_sink = kbo_log_buffered_sink_for_path_unlocked(path);
        kbo_log_buffered_sink_flush_unlocked(direct_sink);
        kbo_log_buffered_sink_rotate_if_needed_unlocked(direct_sink, max_bytes, archive_count);
        int ok = kbo_log_append_ndjson_path(path, json);
        if (direct_sink != NULL) {
            direct_sink->known_size = kbo_log_file_size_or_zero(path);
        }
        return ok;
    }

    KboLogBufferedSink* sink = kbo_log_buffered_sink_for_path_unlocked(path);
    if (sink == NULL) {
        return 0;
    }

    kbo_log_buffered_sink_rotate_if_needed_unlocked(sink, max_bytes, archive_count);
    if (sink->used + line_len > KBO_LOG_BUFFER_BYTES) {
        kbo_log_buffered_sink_flush_unlocked(sink);
        kbo_log_buffered_sink_rotate_if_needed_unlocked(sink, max_bytes, archive_count);
    }
    if (sink->used + line_len > KBO_LOG_BUFFER_BYTES) {
        int ok = kbo_log_append_ndjson_path(path, json);
        sink->known_size = kbo_log_file_size_or_zero(path);
        return ok;
    }

    memcpy(sink->buffer + sink->used, json, json_len);
    sink->used += json_len;
    sink->buffer[sink->used++] = '\r';
    sink->buffer[sink->used++] = '\n';

    ULONGLONG now = GetTickCount64();
    if (flush_now || sink->used >= KBO_LOG_BUFFER_BYTES / 2u
            || now - sink->last_flush_tick >= KBO_LOG_BUFFER_FLUSH_INTERVAL_MS) {
        return kbo_log_buffered_sink_flush_unlocked(sink);
    }
    return 1;
}

static void kbo_log_rotate_if_needed(const char* path, size_t max_bytes, int archive_count)
{
    if (path == NULL || path[0] == '\0' || max_bytes == 0u) {
        return;
    }
    WIN32_FILE_ATTRIBUTE_DATA attrs;
    if (!kbo_get_file_attributes_ex_utf8(path, &attrs)) {
        return;
    }
    uint64_t size = ((uint64_t)attrs.nFileSizeHigh << 32) | (uint64_t)attrs.nFileSizeLow;
    if (size < (uint64_t)max_bytes) {
        return;
    }
    if (archive_count <= 0) {
        kbo_delete_file_utf8(path);
        return;
    }
    for (int i = archive_count; i >= 1; i--) {
        char from[KBO_UTF8_PATH_BYTES] = {0};
        char to[KBO_UTF8_PATH_BYTES] = {0};
        snprintf(from, sizeof(from), "%s.%d", path, i);
        if (i == archive_count) {
            kbo_delete_file_utf8(from);
            continue;
        }
        snprintf(to, sizeof(to), "%s.%d", path, i + 1);
        kbo_move_file_replace_utf8(from, to);
    }
    char first[KBO_UTF8_PATH_BYTES] = {0};
    snprintf(first, sizeof(first), "%s.1", path);
    kbo_move_file_replace_utf8(path, first);
}


int kbo_log_event_write_paths(
    int has_global,
    const char* global_path,
    int has_save,
    const char* save_path,
    const char* json,
    KboLogLevel level,
    size_t max_bytes,
    int archive_count)
{
    int wrote_any = 0;
    int flush_now = level >= KBO_LOG_LEVEL_WARN;
    kbo_spin_lock(&g_kbo_log_event_file_lock);
    if (has_global) {
        wrote_any |= kbo_log_buffered_append_ndjson_path_unlocked(
            global_path,
            json,
            max_bytes,
            archive_count,
            flush_now);
    }
    if (has_save && (!has_global || !kbo_log_path_equals(global_path, save_path))) {
        wrote_any |= kbo_log_buffered_append_ndjson_path_unlocked(
            save_path,
            json,
            max_bytes,
            archive_count,
            flush_now);
    }
    kbo_spin_unlock(&g_kbo_log_event_file_lock);
    return wrote_any;
}

void kbo_log_event_flush(void)
{
    kbo_spin_lock(&g_kbo_log_event_file_lock);
    for (size_t i = 0u; i < KBO_LOG_BUFFERED_SINK_COUNT; i++) {
        kbo_log_buffered_sink_flush_unlocked(&g_kbo_log_buffered_sinks[i]);
    }
    kbo_spin_unlock(&g_kbo_log_event_file_lock);
}

void kbo_log_event_shutdown(void)
{
    kbo_spin_lock(&g_kbo_log_event_file_lock);
    for (size_t i = 0u; i < KBO_LOG_BUFFERED_SINK_COUNT; i++) {
        kbo_log_buffered_sink_close_unlocked(&g_kbo_log_buffered_sinks[i]);
    }
    kbo_spin_unlock(&g_kbo_log_event_file_lock);
}
