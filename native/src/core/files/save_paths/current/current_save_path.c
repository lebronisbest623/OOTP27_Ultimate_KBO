/* Current-save path resolution. */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../core_save_paths.h"
#include "../core_save_paths_internal.h"
#include "../../../product/ootp_product.h"
#include "../../../../bootstrap/abi/ootp_offsets.h"
#include "../../../../runtime_memory/runtime_memory.h"
#include "../../../../team/names/team_string.h"
#include "../../../logging/core_log.h"

typedef LONG (WINAPI* KboNtQuerySystemInformationFn)(ULONG, PVOID, ULONG, PULONG);

#define KBO_SYSTEM_HANDLE_TABLE_ENTRY_ARRAY_OFFSET 16u
#define KBO_SYSTEM_HANDLE_TABLE_ENTRY_SIZE 40u
#define KBO_SYSTEM_HANDLE_TABLE_ENTRY_PID_OFFSET 8u
#define KBO_SYSTEM_HANDLE_TABLE_ENTRY_HANDLE_OFFSET 16u
#define KBO_SYSTEM_EXTENDED_HANDLE_INFORMATION_CLASS 64u
#define KBO_SYSTEM_HANDLE_QUERY_INITIAL_BYTES 0x10000u
#define KBO_NTSTATUS_INFO_LENGTH_MISMATCH ((LONG)0xC0000004u)
#define KBO_CURRENT_SAVE_HANDLE_PROBE_MIN_INTERVAL_MS 2000ull

static char g_kbo_cached_current_save_path[KBO_UTF8_PATH_BYTES] = {0};
static volatile LONG64 g_kbo_last_current_save_handle_probe_tick = 0;
static volatile LONG g_kbo_current_save_handle_probe_active = 0;
static char g_kbo_last_logged_current_save_path[KBO_UTF8_PATH_BYTES] = {0};
static char g_kbo_last_logged_current_save_source[32] = {0};

static void kbo_cache_current_save_path(const char* path)
{
    if (path == NULL || !kbo_path_looks_like_absolute_save_path(path)) {
        return;
    }
    snprintf(g_kbo_cached_current_save_path, sizeof(g_kbo_cached_current_save_path), "%s", path);
}

static int kbo_get_cached_current_save_path(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0 || g_kbo_cached_current_save_path[0] == '\0') {
        return 0;
    }
    if (!kbo_path_looks_like_absolute_save_path(g_kbo_cached_current_save_path)) {
        g_kbo_cached_current_save_path[0] = '\0';
        return 0;
    }
    snprintf(out, out_size, "%s", g_kbo_cached_current_save_path);
    return out[0] != '\0';
}

static int kbo_begin_current_save_handle_probe(void)
{
    ULONGLONG now = GetTickCount64();
    LONG64 last_tick = InterlockedCompareExchange64(&g_kbo_last_current_save_handle_probe_tick, 0, 0);
    if (last_tick > 0
            && now >= (ULONGLONG)last_tick
            && now - (ULONGLONG)last_tick < KBO_CURRENT_SAVE_HANDLE_PROBE_MIN_INTERVAL_MS) {
        return 0;
    }

    if (InterlockedCompareExchange(&g_kbo_current_save_handle_probe_active, 1, 0) != 0) {
        return 0;
    }

    now = GetTickCount64();
    last_tick = InterlockedCompareExchange64(&g_kbo_last_current_save_handle_probe_tick, 0, 0);
    if (last_tick > 0
            && now >= (ULONGLONG)last_tick
            && now - (ULONGLONG)last_tick < KBO_CURRENT_SAVE_HANDLE_PROBE_MIN_INTERVAL_MS) {
        InterlockedExchange(&g_kbo_current_save_handle_probe_active, 0);
        return 0;
    }

    InterlockedExchange64(&g_kbo_last_current_save_handle_probe_tick, (LONG64)now);
    return 1;
}

static void kbo_end_current_save_handle_probe(void)
{
    InterlockedExchange(&g_kbo_current_save_handle_probe_active, 0);
}

static void kbo_log_current_save_path_resolved(const char* source, const char* path, const char* detail)
{
    if (source == NULL || source[0] == '\0' || path == NULL || path[0] == '\0') {
        return;
    }
    if (strcmp(g_kbo_last_logged_current_save_path, path) == 0
            && strcmp(g_kbo_last_logged_current_save_source, source) == 0) {
        return;
    }
    snprintf(g_kbo_last_logged_current_save_path, sizeof(g_kbo_last_logged_current_save_path), "%s", path);
    snprintf(g_kbo_last_logged_current_save_source, sizeof(g_kbo_last_logged_current_save_source), "%s", source);
    if (detail != NULL && detail[0] != '\0') {
        kbo_log_runtimef("KBO save path resolved by %s path=%s source=%s", source, path, detail);
    } else {
        kbo_log_runtimef("KBO save path resolved by %s path=%s", source, path);
    }
}

static int kbo_extract_lg_save_path_from_file_path(const char* path, char* out, size_t out_size)
{
    if (path == NULL || out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';

    const char* normalized = path;
    if (strncmp(normalized, "\\\\?\\", 4) == 0) {
        normalized += 4;
    }

    const char* marker = NULL;
    for (const char* p = normalized; *p != '\0'; p++) {
        if ((p[0] == '.' || p[0] == '.')
                && (p[1] == 'l' || p[1] == 'L')
                && (p[2] == 'g' || p[2] == 'G')
                && (p[3] == '\0' || p[3] == '\\' || p[3] == '/')) {
            marker = p;
            break;
        }
    }
    if (marker == NULL) {
        return 0;
    }

    size_t len = (size_t)(marker - normalized) + 3u;
    if (len == 0 || len >= out_size) {
        return 0;
    }
    memcpy(out, normalized, len);
    out[len] = '\0';
    return kbo_path_looks_like_absolute_save_path(out);
}

static int kbo_get_current_save_path_from_own_file_handles(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';

    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    if (ntdll == NULL) {
        return 0;
    }
    KboNtQuerySystemInformationFn nt_query =
        (KboNtQuerySystemInformationFn)GetProcAddress(ntdll, "NtQuerySystemInformation");
    if (nt_query == NULL) {
        return 0;
    }

    ULONG buffer_size = KBO_SYSTEM_HANDLE_QUERY_INITIAL_BYTES;
    BYTE* buffer = NULL;
    LONG status = 0;
    ULONG returned = 0;
    for (int attempt = 0; attempt < 8; attempt++) {
        buffer = (BYTE*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, buffer_size);
        if (buffer == NULL) {
            return 0;
        }
        status = nt_query(KBO_SYSTEM_EXTENDED_HANDLE_INFORMATION_CLASS, buffer, buffer_size, &returned);
        if (status == 0) {
            break;
        }
        HeapFree(GetProcessHeap(), 0, buffer);
        buffer = NULL;
        if (status != KBO_NTSTATUS_INFO_LENGTH_MISMATCH) {
            return 0;
        }
        buffer_size = returned > buffer_size ? returned + KBO_SYSTEM_HANDLE_QUERY_INITIAL_BYTES : buffer_size * 2u;
    }
    if (buffer == NULL || status != 0) {
        if (buffer != NULL) {
            HeapFree(GetProcessHeap(), 0, buffer);
        }
        return 0;
    }

    DWORD pid = GetCurrentProcessId();
    ULONGLONG handle_count = *(ULONGLONG*)buffer;
    BYTE* entries = buffer + KBO_SYSTEM_HANDLE_TABLE_ENTRY_ARRAY_OFFSET;
    const SIZE_T entry_size = KBO_SYSTEM_HANDLE_TABLE_ENTRY_SIZE;
    int found = 0;

    for (ULONGLONG i = 0; i < handle_count; i++) {
        BYTE* entry = entries + (SIZE_T)i * entry_size;
        DWORD owner_pid = (DWORD)(*(ULONGLONG*)(entry + KBO_SYSTEM_HANDLE_TABLE_ENTRY_PID_OFFSET));
        if (owner_pid != pid) {
            continue;
        }

        HANDLE handle = (HANDLE)(uintptr_t)(*(ULONGLONG*)(entry + KBO_SYSTEM_HANDLE_TABLE_ENTRY_HANDLE_OFFSET));
        if (handle == NULL || handle == INVALID_HANDLE_VALUE) {
            continue;
        }
        if (GetFileType(handle) != FILE_TYPE_DISK) {
            continue;
        }

        WCHAR path_w[KBO_WIDE_PATH_CHARS] = {0};
        DWORD len = GetFinalPathNameByHandleW(handle, path_w, KBO_WIDE_PATH_CHARS, FILE_NAME_NORMALIZED);
        if (len == 0 || len >= KBO_WIDE_PATH_CHARS) {
            continue;
        }

        char path[KBO_UTF8_PATH_BYTES] = {0};
        if (!kbo_wide_to_utf8_path(path_w, path, sizeof(path))) {
            continue;
        }

        char save_path[KBO_UTF8_PATH_BYTES] = {0};
        if (kbo_extract_lg_save_path_from_file_path(path, save_path, sizeof(save_path))) {
            snprintf(out, out_size, "%s", save_path);
            kbo_log_current_save_path_resolved("own file handle", out, path);
            found = 1;
            break;
        }
    }

    HeapFree(GetProcessHeap(), 0, buffer);
    return found;
}

static int kbo_get_current_save_path_from_ootp_global(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';

    uintptr_t global = get_ootp_global_database();
    if (global != 0 && memory_range_readable((void*)(global + OOTP27_MESSAGE_SAVE_PATH_OFFSET), 0x20)) {
        copy_ootp_string_object_text((uint8_t*)global, OOTP27_MESSAGE_SAVE_PATH_OFFSET, out, out_size);
    }

    if (!kbo_path_looks_like_absolute_save_path(out)) {
        out[0] = '\0';
        return 0;
    }

    kbo_log_current_save_path_resolved("OOTP global", out, NULL);
    return 1;
}

static int kbo_get_current_save_path_from_launcher_cache_file(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';

    char local_app_data[KBO_UTF8_PATH_BYTES] = {0};
    if (!kbo_get_localappdata_utf8(local_app_data, sizeof(local_app_data))) {
        return 0;
    }

    char cache_path[KBO_UTF8_PATH_BYTES] = {0};
    char cache_file_name[64] = {0};
    snprintf(
        cache_file_name,
        sizeof(cache_file_name),
        KBO_PRODUCT_CURRENT_SAVE_PATH_FILE_FORMAT,
        (unsigned long)GetCurrentProcessId());
    snprintf(
        cache_path,
        sizeof(cache_path),
        "%s\\" KBO_PRODUCT_LOCAL_DATA_DIR "\\%s",
        local_app_data,
        cache_file_name);
    HANDLE file = kbo_create_file_read_utf8(cache_path);
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }

    DWORD read = 0;
    char cached[KBO_UTF8_PATH_BYTES] = {0};
    int found = 0;
    if (ReadFile(file, cached, (DWORD)sizeof(cached) - 1u, &read, NULL) && read > 0) {
        cached[read] = '\0';
        for (DWORD i = 0; i < read; i++) {
            if (cached[i] == '\r' || cached[i] == '\n') {
                cached[i] = '\0';
                break;
            }
        }
        if (kbo_path_looks_like_absolute_save_path(cached)) {
            snprintf(out, out_size, "%s", cached);
            found = out[0] != '\0';
        }
    }
    CloseHandle(file);

    if (found) {
        kbo_log_current_save_path_resolved("launcher cache", out, NULL);
    }
    return found;
}

int kbo_get_current_save_path(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';

    if (kbo_get_current_save_path_from_ootp_global(out, out_size)) {
        kbo_cache_current_save_path(out);
        return 1;
    }

    if (kbo_get_current_save_path_from_launcher_cache_file(out, out_size)) {
        kbo_cache_current_save_path(out);
        return 1;
    }

    if (kbo_get_cached_current_save_path(out, out_size)) {
        return 1;
    }

    /* File-handle discovery is a last resort. OOTP can keep handles to old
     * saves open after a save switch, so it must not override launcher/global
     * evidence for the current process. */
    if (kbo_begin_current_save_handle_probe()) {
        int found = kbo_get_current_save_path_from_own_file_handles(out, out_size);
        kbo_end_current_save_handle_probe();
        if (found) {
            kbo_cache_current_save_path(out);
            return 1;
        }
    }

    return 0;
}
