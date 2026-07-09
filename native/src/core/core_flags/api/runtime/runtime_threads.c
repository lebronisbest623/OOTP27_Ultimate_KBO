#include "../flags_api.h"

#include "../../localappdata/localappdata_reader.h"
#include "../../../files/save_paths/core_save_paths.h"
#include "../../../files/save_paths/platform/core_path_io.h"
#include "../../../logging/core_log.h"
#include "../../../product/ootp_product.h"
#include "../../../sync/spin_lock.h"

#include <stdio.h>
#include <string.h>
#include <windows.h>

static volatile LONG g_kbo_runtime_threads_stop_requested = 0;
static KboSpinLock g_kbo_runtime_thread_registry_lock = KBO_SPIN_LOCK_INIT;

#define KBO_RUNTIME_THREAD_MAX 64
#define KBO_RUNTIME_SAVE_STARTED_STALE_MINUTES 15u

typedef struct KboRuntimeThreadEntry {
    HANDLE handle;
    char label[64];
} KboRuntimeThreadEntry;

static KboRuntimeThreadEntry g_kbo_runtime_threads[KBO_RUNTIME_THREAD_MAX];

static int kbo_runtime_get_file_write_time(const char* path, FILETIME* out_time)
{
    if (out_time != NULL) {
        memset(out_time, 0, sizeof(*out_time));
    }
    if (path == NULL || path[0] == '\0' || out_time == NULL) {
        return 0;
    }
    WIN32_FILE_ATTRIBUTE_DATA data;
    if (!kbo_get_file_attributes_ex_utf8(path, &data)
            || (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0u) {
        return 0;
    }
    *out_time = data.ftLastWriteTime;
    return 1;
}

static ULONGLONG kbo_runtime_filetime_u64(FILETIME time)
{
    ULARGE_INTEGER value;
    value.LowPart = time.dwLowDateTime;
    value.HighPart = time.dwHighDateTime;
    return value.QuadPart;
}

static int kbo_runtime_save_in_progress_uncached(void)
{
    char save_path[KBO_UTF8_PATH_BYTES] = {0};
    if (!kbo_get_current_save_path(save_path, sizeof(save_path))) {
        return 0;
    }

    char started_path[KBO_UTF8_PATH_BYTES] = {0};
    char completed_path[KBO_UTF8_PATH_BYTES] = {0};
    if (!kbo_get_ootp_save_started_file_path(save_path, started_path, sizeof(started_path))
            || !kbo_get_ootp_save_completed_file_path(save_path, completed_path, sizeof(completed_path))) {
        return 0;
    }

    FILETIME started_time = {0};
    if (!kbo_runtime_get_file_write_time(started_path, &started_time)) {
        return 0;
    }

    FILETIME now_time = {0};
    GetSystemTimeAsFileTime(&now_time);
    ULONGLONG started = kbo_runtime_filetime_u64(started_time);
    ULONGLONG now = kbo_runtime_filetime_u64(now_time);
    FILETIME process_created = {0};
    FILETIME ignored_exit = {0};
    FILETIME ignored_kernel = {0};
    FILETIME ignored_user = {0};
    if (GetProcessTimes(
            GetCurrentProcess(),
            &process_created,
            &ignored_exit,
            &ignored_kernel,
            &ignored_user)
            && started < kbo_runtime_filetime_u64(process_created)) {
        return 0;
    }
    const ULONGLONG stale_save_started_100ns =
        (ULONGLONG)KBO_RUNTIME_SAVE_STARTED_STALE_MINUTES * 60ull * 1000ull * 10000ull;
    if (now > started && now - started > stale_save_started_100ns) {
        return 0;
    }

    FILETIME completed_time = {0};
    if (!kbo_runtime_get_file_write_time(completed_path, &completed_time)) {
        return 1;
    }

    return kbo_runtime_filetime_u64(completed_time) < started;
}

/* The uncached check costs two filesystem metadata queries and is called from
 * per-offer hooks and 50ms-pulse background loops. Only the negative result is
 * cached, and only briefly: "save in progress" answers are always re-verified,
 * so a save is never considered finished early. The save-start marker is
 * written by OOTP itself and only becomes visible with filesystem latency, so
 * a 100ms-stale negative stays within the detection slack that already exists. */
#define KBO_RUNTIME_SAVE_CHECK_NEGATIVE_CACHE_MS 100ull

int kbo_runtime_save_in_progress(void)
{
    static volatile LONG64 s_last_negative_tick = 0;

    ULONGLONG now = GetTickCount64();
    LONG64 last_negative = InterlockedCompareExchange64(&s_last_negative_tick, 0, 0);
    if (last_negative != 0
            && now >= (ULONGLONG)last_negative
            && now - (ULONGLONG)last_negative < KBO_RUNTIME_SAVE_CHECK_NEGATIVE_CACHE_MS) {
        return 0;
    }

    int in_progress = kbo_runtime_save_in_progress_uncached();
    if (in_progress) {
        InterlockedExchange64(&s_last_negative_tick, 0);
    } else {
        InterlockedExchange64(&s_last_negative_tick, (LONG64)now);
    }
    return in_progress;
}

int kbo_runtime_pause_for_save_if_needed(const char* label)
{
    const char* thread_label = label != NULL ? label : "runtime";
    int logged = 0;
    while (kbo_runtime_threads_should_continue() && kbo_runtime_save_in_progress()) {
        if (!logged) {
            kbo_log_runtimef("KBO runtime paused for save label=\"%s\"", thread_label);
            logged = 1;
        }
        Sleep(500);
    }
    if (logged) {
        kbo_log_runtimef("KBO runtime resumed after save label=\"%s\"", thread_label);
    }
    return kbo_runtime_threads_should_continue();
}

static int kbo_should_log_runtime_thread_registration(void)
{
    static LONG runtime_registration_log_count = 0;
    return InterlockedIncrement(&runtime_registration_log_count) <= 512;
}

static void kbo_lock_runtime_thread_registry(void)
{
    kbo_spin_lock(&g_kbo_runtime_thread_registry_lock);
}

static void kbo_unlock_runtime_thread_registry(void)
{
    kbo_spin_unlock(&g_kbo_runtime_thread_registry_lock);
}

void kbo_request_runtime_threads_stop(void)
{
    InterlockedExchange(&g_kbo_runtime_threads_stop_requested, 1);
}

void kbo_register_runtime_thread(HANDLE thread, const char* label)
{
    const char* thread_label = label != NULL ? label : "runtime thread";
    if (thread == NULL) {
        return;
    }

    kbo_lock_runtime_thread_registry();
    for (int i = 0; i < KBO_RUNTIME_THREAD_MAX; ++i) {
        if (g_kbo_runtime_threads[i].handle != NULL
                && WaitForSingleObject(g_kbo_runtime_threads[i].handle, 0) == WAIT_OBJECT_0) {
            CloseHandle(g_kbo_runtime_threads[i].handle);
            g_kbo_runtime_threads[i].handle = NULL;
            g_kbo_runtime_threads[i].label[0] = '\0';
        }
    }

    for (int i = 0; i < KBO_RUNTIME_THREAD_MAX; ++i) {
        if (g_kbo_runtime_threads[i].handle == NULL) {
            g_kbo_runtime_threads[i].handle = thread;
            snprintf(g_kbo_runtime_threads[i].label, sizeof(g_kbo_runtime_threads[i].label), "%s", thread_label);
            kbo_unlock_runtime_thread_registry();
            if (kbo_should_log_runtime_thread_registration()) {
                kbo_log_runtimef("registered KBO runtime thread label=\"%s\" handle=%p", thread_label, thread);
            }
            return;
        }
    }
    kbo_unlock_runtime_thread_registry();

    kbo_log_runtimef("KBO runtime thread registry full; closing untracked thread label=\"%s\" handle=%p", thread_label, thread);
    CloseHandle(thread);
}

int kbo_start_runtime_thread(LPTHREAD_START_ROUTINE start, LPVOID parameter, const char* label)
{
    const char* thread_label = label != NULL ? label : "runtime thread";
    if (start == NULL) {
        kbo_log_runtimef("KBO runtime thread start skipped label=\"%s\" reason=no_start_proc", thread_label);
        return 0;
    }

    HANDLE thread = CreateThread(NULL, 0, start, parameter, 0, NULL);
    if (thread == NULL) {
        kbo_log_runtimef(
            "KBO runtime thread start failed label=\"%s\" error=%lu",
            thread_label,
            (unsigned long)GetLastError());
        return 0;
    }

    kbo_register_runtime_thread(thread, thread_label);
    return 1;
}

void kbo_shutdown_runtime_threads(uint32_t timeout_ms)
{
    HANDLE handles[KBO_RUNTIME_THREAD_MAX];
    char labels[KBO_RUNTIME_THREAD_MAX][64];
    DWORD count = 0;
    DWORD deadline = GetTickCount() + timeout_ms;

    kbo_request_runtime_threads_stop();

    kbo_lock_runtime_thread_registry();
    for (int i = 0; i < KBO_RUNTIME_THREAD_MAX; ++i) {
        if (g_kbo_runtime_threads[i].handle != NULL) {
            handles[count] = g_kbo_runtime_threads[i].handle;
            snprintf(labels[count], sizeof(labels[count]), "%s", g_kbo_runtime_threads[i].label);
            g_kbo_runtime_threads[i].handle = NULL;
            g_kbo_runtime_threads[i].label[0] = '\0';
            ++count;
        }
    }
    kbo_unlock_runtime_thread_registry();

    if (count == 0) {
        kbo_log_runtime_line("KBO runtime shutdown: no registered threads");
        return;
    }

    kbo_log_runtimef("KBO runtime shutdown: waiting for %lu thread(s)", (unsigned long)count);
    for (DWORD i = 0; i < count; ++i) {
        if (GetThreadId(handles[i]) == GetCurrentThreadId()) {
            kbo_log_runtimef("KBO runtime shutdown: skipping current thread label=\"%s\"", labels[i]);
            CloseHandle(handles[i]);
            continue;
        }

        DWORD now = GetTickCount();
        DWORD remaining = now >= deadline ? 0u : deadline - now;
        DWORD wait = WaitForSingleObject(handles[i], remaining);
        if (wait == WAIT_OBJECT_0) {
            kbo_log_runtimef("KBO runtime shutdown: joined label=\"%s\"", labels[i]);
        } else {
            kbo_log_runtimef(
                "KBO runtime shutdown: join timeout/failure label=\"%s\" wait=0x%08lX",
                labels[i],
                (unsigned long)wait);
        }
        CloseHandle(handles[i]);
    }
}

int kbo_runtime_threads_should_continue(void)
{
    if (InterlockedCompareExchange(&g_kbo_runtime_threads_stop_requested, 0, 0) != 0) {
        return 0;
    }

    int configured = 1;
    if (kbo_read_localappdata_json_flag_value("enable_kbo_fix", &configured)
            && !configured) {
        return 0;
    }
    return 1;
}

int kbo_runtime_sleep_should_continue(uint32_t total_ms)
{
    uint32_t elapsed = 0;
    while (elapsed < total_ms) {
        if (!kbo_runtime_threads_should_continue()) {
            return 0;
        }

        uint32_t step = total_ms - elapsed;
        if (step > 250u) {
            step = 250u;
        }
        Sleep(step);
        elapsed += step;
    }
    return kbo_runtime_pause_for_save_if_needed("runtime sleep");
}
