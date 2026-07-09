#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../../core/core_flags/api/flags_api.h"
#include "../../../core/files/save_paths/core_save_paths.h"
#include "../../../core/logging/core_log.h"
#include "../transform/roster_export_transform.h"
#include "roster_export_watcher.h"

#define KBO_ROSTER_EXPORT_WATCH_MS 500u
#define KBO_ROSTER_EXPORT_STABLE_POLLS 2
#define KBO_ROSTER_EXPORT_MAX_BYTES (32u * 1024u * 1024u)

typedef struct KboRosterExportFileSignature {
    FILETIME write_time;
    DWORD size_high;
    DWORD size_low;
    int exists;
} KboRosterExportFileSignature;

typedef struct KboRosterExportWatchState {
    KboRosterExportFileSignature seen;
    KboRosterExportFileSignature processed;
    int stable_polls;
} KboRosterExportWatchState;

static volatile LONG g_kbo_roster_export_watcher_started = 0;

static int kbo_roster_export_signature_equal(
    const KboRosterExportFileSignature* a,
    const KboRosterExportFileSignature* b)
{
    if (a == NULL || b == NULL) {
        return 0;
    }
    return a->exists == b->exists
        && a->size_high == b->size_high
        && a->size_low == b->size_low
        && CompareFileTime(&a->write_time, &b->write_time) == 0;
}

static int kbo_roster_export_read_signature(const char* path, KboRosterExportFileSignature* out)
{
    if (out == NULL) {
        return 0;
    }
    memset(out, 0, sizeof(*out));
    if (path == NULL || path[0] == '\0') {
        return 0;
    }

    WIN32_FILE_ATTRIBUTE_DATA data;
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &data)) {
        return 0;
    }
    if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0u) {
        return 0;
    }

    out->exists = 1;
    out->write_time = data.ftLastWriteTime;
    out->size_high = data.nFileSizeHigh;
    out->size_low = data.nFileSizeLow;
    return 1;
}

static int kbo_roster_export_signature_has_content(const KboRosterExportFileSignature* sig)
{
    if (sig == NULL || !sig->exists) {
        return 0;
    }
    return sig->size_high != 0u || sig->size_low != 0u;
}

static int kbo_roster_export_path_for_save(
    const char* save_path,
    const char* file_name,
    char* out,
    size_t out_size)
{
    if (save_path == NULL || file_name == NULL || out == NULL || out_size == 0u) {
        return 0;
    }
    int written = snprintf(out, out_size, "%s\\import_export\\%s", save_path, file_name);
    return written > 0 && (size_t)written < out_size;
}

static int kbo_roster_export_read_whole_file(const char* path, char** out, size_t* out_len)
{
    if (out == NULL || out_len == NULL) {
        return 0;
    }
    *out = NULL;
    *out_len = 0u;

    HANDLE file = CreateFileA(
        path,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }

    LARGE_INTEGER size;
    if (!GetFileSizeEx(file, &size)
            || size.QuadPart <= 0
            || size.QuadPart > (LONGLONG)KBO_ROSTER_EXPORT_MAX_BYTES) {
        CloseHandle(file);
        return 0;
    }

    char* buffer = (char*)HeapAlloc(GetProcessHeap(), 0, (SIZE_T)size.QuadPart + 1u);
    if (buffer == NULL) {
        CloseHandle(file);
        return 0;
    }

    DWORD total = 0u;
    while (total < (DWORD)size.QuadPart) {
        DWORD chunk = (DWORD)size.QuadPart - total;
        DWORD read = 0u;
        if (!ReadFile(file, buffer + total, chunk, &read, NULL) || read == 0u) {
            HeapFree(GetProcessHeap(), 0, buffer);
            CloseHandle(file);
            return 0;
        }
        total += read;
    }
    CloseHandle(file);
    buffer[total] = '\0';
    *out = buffer;
    *out_len = (size_t)total;
    return 1;
}

static int kbo_roster_export_text_already_augmented(const char* data, size_t data_len)
{
    static const char needle[] = "kbo_injury_proneness_back";
    const size_t needle_len = sizeof(needle) - 1u;
    if (data == NULL || data_len < needle_len) {
        return 0;
    }
    for (size_t i = 0u; i + needle_len <= data_len; i++) {
        if (memcmp(data + i, needle, needle_len) == 0) {
            return 1;
        }
    }
    return 0;
}

static int kbo_roster_export_write_replace(const char* path, const char* data, size_t data_len)
{
    char temp_path[MAX_PATH] = {0};
    int written = snprintf(temp_path, sizeof(temp_path), "%s.kbo_tmp", path);
    if (written <= 0 || (size_t)written >= sizeof(temp_path)) {
        return 0;
    }

    HANDLE file = CreateFileA(
        temp_path,
        GENERIC_WRITE,
        0,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }

    DWORD total = 0u;
    while (total < data_len) {
        DWORD chunk = data_len - (size_t)total > 0x40000000u
            ? 0x40000000u
            : (DWORD)(data_len - (size_t)total);
        DWORD wrote = 0u;
        if (!WriteFile(file, data + total, chunk, &wrote, NULL) || wrote == 0u) {
            CloseHandle(file);
            DeleteFileA(temp_path);
            return 0;
        }
        total += wrote;
    }
    FlushFileBuffers(file);
    CloseHandle(file);

    if (!MoveFileExA(temp_path, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileA(temp_path);
        return 0;
    }
    return 1;
}

static int kbo_roster_export_transform_file(const char* path)
{
    char* input = NULL;
    size_t input_len = 0u;
    if (!kbo_roster_export_read_whole_file(path, &input, &input_len)) {
        return 0;
    }
    if (kbo_roster_export_text_already_augmented(input, input_len)) {
        HeapFree(GetProcessHeap(), 0, input);
        kbo_log_runtimef("KBO roster export watcher skipped already augmented path=%s", path);
        return 1;
    }

    char* pending = NULL;
    size_t pending_len = 0u;
    size_t pending_cap = 0u;
    char* output = NULL;
    size_t output_len = 0u;
    size_t output_cap = 0u;
    int built = kbo_roster_export_build_transformed(
        &pending,
        &pending_len,
        &pending_cap,
        input,
        input_len,
        1,
        &output,
        &output_len,
        &output_cap);
    HeapFree(GetProcessHeap(), 0, input);
    if (pending != NULL) {
        HeapFree(GetProcessHeap(), 0, pending);
    }
    if (!built || output == NULL || output_len == 0u) {
        if (output != NULL) {
            HeapFree(GetProcessHeap(), 0, output);
        }
        return 0;
    }

    int ok = kbo_roster_export_write_replace(path, output, output_len);
    HeapFree(GetProcessHeap(), 0, output);
    if (ok) {
        kbo_log_runtimef("KBO roster export watcher augmented path=%s bytes=%u", path, (unsigned)output_len);
    }
    return ok;
}

static void kbo_roster_export_poll_target(KboRosterExportWatchState* state, const char* path)
{
    if (state == NULL || path == NULL || path[0] == '\0') {
        return;
    }

    KboRosterExportFileSignature sig;
    if (!kbo_roster_export_read_signature(path, &sig) || !kbo_roster_export_signature_has_content(&sig)) {
        state->stable_polls = 0;
        return;
    }
    if (!kbo_roster_export_signature_equal(&state->seen, &sig)) {
        state->seen = sig;
        state->stable_polls = 0;
        return;
    }
    if (kbo_roster_export_signature_equal(&state->processed, &sig)) {
        return;
    }

    state->stable_polls++;
    if (state->stable_polls < KBO_ROSTER_EXPORT_STABLE_POLLS) {
        return;
    }

    if (kbo_roster_export_transform_file(path)) {
        KboRosterExportFileSignature updated;
        if (kbo_roster_export_read_signature(path, &updated)) {
            state->processed = updated;
            state->seen = updated;
        } else {
            state->processed = sig;
        }
    }
    state->stable_polls = 0;
}

static DWORD WINAPI kbo_roster_export_watcher_thread(LPVOID parameter)
{
    (void)parameter;
    kbo_log_runtime_line("KBO roster export watcher started");

    char last_save[MAX_PATH] = {0};
    KboRosterExportWatchState txt_state;
    KboRosterExportWatchState csv_state;
    memset(&txt_state, 0, sizeof(txt_state));
    memset(&csv_state, 0, sizeof(csv_state));

    while (kbo_runtime_threads_should_continue()) {
        char save_path[MAX_PATH] = {0};
        if (!kbo_get_current_save_path(save_path, sizeof(save_path))) {
            kbo_runtime_sleep_should_continue(KBO_ROSTER_EXPORT_WATCH_MS);
            continue;
        }
        if (strcmp(last_save, save_path) != 0) {
            snprintf(last_save, sizeof(last_save), "%s", save_path);
            memset(&txt_state, 0, sizeof(txt_state));
            memset(&csv_state, 0, sizeof(csv_state));
            kbo_log_runtimef("KBO roster export watcher save scope path=%s", last_save);
        }

        char txt_path[MAX_PATH] = {0};
        char csv_path[MAX_PATH] = {0};
        if (kbo_roster_export_path_for_save(last_save, "kbo_rosters.txt", txt_path, sizeof(txt_path))) {
            kbo_roster_export_poll_target(&txt_state, txt_path);
        }
        if (kbo_roster_export_path_for_save(last_save, "kbo_rosters.csv", csv_path, sizeof(csv_path))) {
            kbo_roster_export_poll_target(&csv_state, csv_path);
        }
        kbo_runtime_sleep_should_continue(KBO_ROSTER_EXPORT_WATCH_MS);
    }

    kbo_log_runtime_line("KBO roster export watcher stopped");
    return 0;
}

int start_kbo_roster_export_extra_columns_watcher(void)
{
    if (InterlockedCompareExchange(&g_kbo_roster_export_watcher_started, 1, 0) != 0) {
        return 1;
    }
    if (!kbo_start_runtime_thread(kbo_roster_export_watcher_thread, NULL, "roster export watcher")) {
        InterlockedExchange(&g_kbo_roster_export_watcher_started, 0);
        return 0;
    }
    return 1;
}
