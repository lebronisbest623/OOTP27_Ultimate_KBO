#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../foreign_injury_scanner_internal.h"

#include <stdio.h>
#include <string.h>

#include "../../../../core/dates/tick/current_date_tick_capture.h"
#include "../../../../core/files/save_paths/core_save_paths.h"

#define KBO_FOREIGN_INJURY_SQL_WATCH_PULSE_MS 50u
#define KBO_FOREIGN_INJURY_SQL_WATCH_SETTLE_MS 30u
#define KBO_FOREIGN_INJURY_SQL_WATCH_READ_RETRY_MS 80u
#define KBO_FOREIGN_INJURY_SQL_WATCH_SETTLED_READS 1u

typedef struct KboForeignInjurySqlWatchState {
    char save_path[MAX_PATH];
    char temp_dir[1024];
    char db_path[1024];
    ULONGLONG signature;
    int have_snapshot;
    HANDLE change;
} KboForeignInjurySqlWatchState;

static int kbo_foreign_injury_sql_watch_build_paths(
    const char* save_path,
    char* temp_dir,
    size_t temp_dir_size,
    char* db_path,
    size_t db_path_size)
{
    if (save_path == NULL || save_path[0] == '\0'
            || temp_dir == NULL || temp_dir_size == 0u
            || db_path == NULL || db_path_size == 0u) {
        return 0;
    }

    int temp_written = snprintf(temp_dir, temp_dir_size, "%s\\temp", save_path);
    int db_written = snprintf(db_path, db_path_size, "%s\\temp\\text_data.sqlite3", save_path);
    return temp_written > 0
        && db_written > 0
        && (size_t)temp_written < temp_dir_size
        && (size_t)db_written < db_path_size;
}

static ULONGLONG kbo_foreign_injury_sql_watch_filetime_value(FILETIME filetime)
{
    return ((ULONGLONG)filetime.dwHighDateTime << 32)
        | (ULONGLONG)filetime.dwLowDateTime;
}

static void kbo_foreign_injury_sql_watch_mix_signature(
    ULONGLONG* signature,
    ULONGLONG value)
{
    if (signature == NULL) {
        return;
    }
    *signature ^= value
        + 0x9e3779b97f4a7c15ULL
        + (*signature << 6)
        + (*signature >> 2);
}

static int kbo_foreign_injury_sql_watch_signature(
    const char* db_path,
    ULONGLONG* out_signature)
{
    if (out_signature != NULL) {
        *out_signature = 0u;
    }
    if (db_path == NULL || db_path[0] == '\0') {
        return 0;
    }

    const char* suffixes[] = {"", "-wal", "-shm"};
    ULONGLONG signature = 1469598103934665603ULL;
    int found = 0;
    for (int i = 0; i < (int)(sizeof(suffixes) / sizeof(suffixes[0])); i++) {
        char path[1200] = {0};
        int written = snprintf(path, sizeof(path), "%s%s", db_path, suffixes[i]);
        if (written <= 0 || (size_t)written >= sizeof(path)) {
            continue;
        }

        WIN32_FILE_ATTRIBUTE_DATA attrs;
        memset(&attrs, 0, sizeof(attrs));
        if (!GetFileAttributesExA(path, GetFileExInfoStandard, &attrs)
                || (attrs.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            continue;
        }

        found = 1;
        ULONGLONG size = ((ULONGLONG)attrs.nFileSizeHigh << 32)
            | (ULONGLONG)attrs.nFileSizeLow;
        kbo_foreign_injury_sql_watch_mix_signature(&signature, (ULONGLONG)(i + 1));
        kbo_foreign_injury_sql_watch_mix_signature(
            &signature,
            kbo_foreign_injury_sql_watch_filetime_value(attrs.ftLastWriteTime));
        kbo_foreign_injury_sql_watch_mix_signature(&signature, size);
    }

    if (out_signature != NULL) {
        *out_signature = signature;
    }
    return found;
}

static int kbo_foreign_injury_sql_watch_snapshot_changed(
    KboForeignInjurySqlWatchState* state)
{
    if (state == NULL || state->db_path[0] == '\0') {
        return 0;
    }

    ULONGLONG signature = 0u;
    if (!kbo_foreign_injury_sql_watch_signature(state->db_path, &signature)) {
        return 0;
    }

    int changed = !state->have_snapshot
        || state->signature != signature;

    state->signature = signature;
    state->have_snapshot = 1;
    return changed;
}

static void kbo_foreign_injury_sql_watch_close(KboForeignInjurySqlWatchState* state)
{
    if (state == NULL) {
        return;
    }
    if (state->change != NULL && state->change != INVALID_HANDLE_VALUE) {
        FindCloseChangeNotification(state->change);
        state->change = INVALID_HANDLE_VALUE;
    }
}

static int kbo_foreign_injury_sql_watch_reset(
    KboForeignInjurySqlWatchState* state,
    const char* save_path)
{
    if (state == NULL || save_path == NULL || save_path[0] == '\0') {
        return 0;
    }

    kbo_foreign_injury_sql_watch_close(state);
    memset(state, 0, sizeof(*state));
    state->change = INVALID_HANDLE_VALUE;
    snprintf(state->save_path, sizeof(state->save_path), "%s", save_path);
    if (!kbo_foreign_injury_sql_watch_build_paths(
            save_path,
            state->temp_dir,
            sizeof(state->temp_dir),
            state->db_path,
            sizeof(state->db_path))) {
        return 0;
    }

    DWORD temp_attrs = GetFileAttributesA(state->temp_dir);
    if (temp_attrs == INVALID_FILE_ATTRIBUTES || (temp_attrs & FILE_ATTRIBUTE_DIRECTORY) == 0) {
        return 0;
    }

    kbo_foreign_injury_sql_watch_snapshot_changed(state);
    state->change = FindFirstChangeNotificationA(
        state->temp_dir,
        FALSE,
        FILE_NOTIFY_CHANGE_FILE_NAME
            | FILE_NOTIFY_CHANGE_LAST_WRITE
            | FILE_NOTIFY_CHANGE_SIZE);
    if (state->change == INVALID_HANDLE_VALUE) {
        kbo_log_runtimef(
            "foreign injury sql watch skipped path=%s reason=change_notification_failed gle=%lu",
            state->temp_dir,
            GetLastError());
        return 0;
    }

    kbo_log_runtimef(
        "foreign injury sql watch armed save=%s db=%s snapshot=%d",
        state->save_path,
        state->db_path,
        state->have_snapshot);
    return 1;
}

static void kbo_foreign_injury_sql_watch_process_change(
    KboForeignInjurySqlWatchState* state)
{
    if (state == NULL || state->db_path[0] == '\0') {
        return;
    }
    if (!kbo_foreign_injury_sql_watch_snapshot_changed(state)) {
        return;
    }

    for (uint32_t attempt = 0u;
            attempt < KBO_FOREIGN_INJURY_SQL_WATCH_SETTLED_READS;
            attempt++) {
        if (attempt > 0u
                && !kbo_runtime_sleep_should_continue(KBO_FOREIGN_INJURY_SQL_WATCH_READ_RETRY_MS)) {
            return;
        }

        uint32_t today = 0u;
        if (!kbo_current_date_tick_latest_published_date(&today) || today == 0u) {
            kbo_log_runtimef(
                "foreign injury sql watch skipped attempt=%u reason=no_ssot_date",
                attempt + 1u);
            continue;
        }

        kbo_foreign_injury_sql_cache_invalidate_all("foreign_injury_text_data_sql_watch");
        kbo_log_runtimef(
            "foreign injury sql watch event date=%u attempt=%u db=%s",
            today,
            attempt + 1u,
            state->db_path);
        kbo_foreign_injury_replacement_scan_captured_date(
            "foreign_injury_text_data_sql_watch",
            today);
    }

    /* Absorb shared-memory touches caused by our read-only sqlite probes. */
    kbo_foreign_injury_sql_watch_snapshot_changed(state);
}

DWORD WINAPI kbo_foreign_injury_sql_watch_thread(LPVOID parameter)
{
    (void)parameter;
    kbo_log_runtime_line("foreign injury sql watch thread started");

    KboForeignInjurySqlWatchState state;
    memset(&state, 0, sizeof(state));
    state.change = INVALID_HANDLE_VALUE;

    while (kbo_runtime_threads_should_continue()) {
        if (!kbo_runtime_pause_for_save_if_needed("foreign_injury_sql_watch")) {
            break;
        }

        char save_path[MAX_PATH] = {0};
        if (!kbo_get_current_save_path(save_path, sizeof(save_path))) {
            kbo_foreign_injury_sql_watch_close(&state);
            state.save_path[0] = '\0';
            if (!kbo_runtime_sleep_should_continue(KBO_FOREIGN_INJURY_SQL_WATCH_PULSE_MS)) {
                break;
            }
            continue;
        }

        if (state.change == INVALID_HANDLE_VALUE
                || strcmp(state.save_path, save_path) != 0) {
            if (!kbo_foreign_injury_sql_watch_reset(&state, save_path)) {
                if (!kbo_runtime_sleep_should_continue(KBO_FOREIGN_INJURY_SQL_WATCH_PULSE_MS)) {
                    break;
                }
                continue;
            }
        }

        DWORD wait = WaitForSingleObject(
            state.change,
            KBO_FOREIGN_INJURY_SQL_WATCH_PULSE_MS);
        if (wait == WAIT_OBJECT_0) {
            if (!kbo_runtime_sleep_should_continue(KBO_FOREIGN_INJURY_SQL_WATCH_SETTLE_MS)) {
                break;
            }
            kbo_foreign_injury_sql_watch_process_change(&state);
            if (!FindNextChangeNotification(state.change)) {
                kbo_log_runtimef(
                    "foreign injury sql watch rearm failed path=%s gle=%lu",
                    state.temp_dir,
                    GetLastError());
                kbo_foreign_injury_sql_watch_close(&state);
            }
        } else if (wait != WAIT_TIMEOUT) {
            kbo_log_runtimef(
                "foreign injury sql watch wait failed path=%s wait=%lu gle=%lu",
                state.temp_dir,
                wait,
                GetLastError());
            kbo_foreign_injury_sql_watch_close(&state);
        }
    }

    kbo_foreign_injury_sql_watch_close(&state);
    InterlockedExchange(&g_kbo_foreign_injury_sql_watch_thread_started, 0);
    kbo_log_runtime_line("foreign injury sql watch thread stopped");
    return 0;
}

void start_kbo_foreign_injury_sql_watch_thread(void)
{
    if (!kbo_foreign_injury_replacement_enabled()) {
        kbo_log_runtime_line("foreign injury sql watch: disabled");
        return;
    }
    if (InterlockedCompareExchange(&g_kbo_foreign_injury_sql_watch_thread_started, 1, 0) != 0) {
        return;
    }

    if (kbo_start_runtime_thread(
            kbo_foreign_injury_sql_watch_thread,
            NULL,
            "foreign injury sql watch")) {
        kbo_log_runtime_line("foreign injury sql watch thread requested");
    } else {
        InterlockedExchange(&g_kbo_foreign_injury_sql_watch_thread_started, 0);
    }
}
