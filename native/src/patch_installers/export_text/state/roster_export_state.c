#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>
#include <string.h>

#include "../../../core/logging/core_log.h"
#include "../transform/roster_export_transform.h"
#include "roster_export_state.h"

#define KBO_ROSTER_EXPORT_TRACKED_HANDLES 16

typedef struct KboRosterExportHandleState {
    HANDLE handle;
    char path[MAX_PATH];
    char* pending;
    size_t pending_len;
    size_t pending_cap;
} KboRosterExportHandleState;

static SRWLOCK g_kbo_roster_export_lock = SRWLOCK_INIT;
static KboRosterExportHandleState g_kbo_roster_export_handles[KBO_ROSTER_EXPORT_TRACKED_HANDLES];
static volatile LONG g_kbo_roster_export_transforming = 0;
static KboRosterExportWriteFileFn g_kbo_original_WriteFile = NULL;

void kbo_roster_export_state_set_original_write_file(KboRosterExportWriteFileFn write_file)
{
    g_kbo_original_WriteFile = write_file;
}

static void kbo_roster_export_handle_state_free(KboRosterExportHandleState* state)
{
    if (state == NULL) {
        return;
    }
    if (state->pending != NULL) {
        HeapFree(GetProcessHeap(), 0, state->pending);
    }
    memset(state, 0, sizeof(*state));
}

void kbo_roster_export_register_handle(HANDLE handle, const char* path)
{
    if (handle == NULL || handle == INVALID_HANDLE_VALUE || path == NULL || path[0] == '\0') {
        return;
    }

    AcquireSRWLockExclusive(&g_kbo_roster_export_lock);
    int slot = -1;
    for (int i = 0; i < KBO_ROSTER_EXPORT_TRACKED_HANDLES; i++) {
        if (g_kbo_roster_export_handles[i].handle == handle) {
            slot = i;
            break;
        }
        if (slot < 0 && g_kbo_roster_export_handles[i].handle == NULL) {
            slot = i;
        }
    }
    if (slot >= 0) {
        kbo_roster_export_handle_state_free(&g_kbo_roster_export_handles[slot]);
        g_kbo_roster_export_handles[slot].handle = handle;
        snprintf(g_kbo_roster_export_handles[slot].path, sizeof(g_kbo_roster_export_handles[slot].path), "%s", path);
    }
    ReleaseSRWLockExclusive(&g_kbo_roster_export_lock);

    if (slot >= 0) {
        kbo_log_runtimef("KBO roster export extra columns tracking handle=%p path=%s", handle, path);
    } else {
        kbo_log_runtimef("KBO roster export extra columns handle table full path=%s", path);
    }
}

static int kbo_roster_export_find_handle_index_locked(HANDLE handle)
{
    for (int i = 0; i < KBO_ROSTER_EXPORT_TRACKED_HANDLES; i++) {
        if (g_kbo_roster_export_handles[i].handle == handle) {
            return i;
        }
    }
    return -1;
}

static BOOL kbo_roster_export_write_original(HANDLE file, const void* buffer, DWORD size, LPDWORD written)
{
    if (g_kbo_original_WriteFile == NULL) {
        SetLastError(ERROR_INVALID_FUNCTION);
        return FALSE;
    }

    InterlockedIncrement(&g_kbo_roster_export_transforming);
    BOOL ok = g_kbo_original_WriteFile(file, buffer, size, written, NULL);
    InterlockedDecrement(&g_kbo_roster_export_transforming);
    return ok;
}

static BOOL kbo_roster_export_flush_pending_locked(KboRosterExportHandleState* state)
{
    if (state == NULL || state->handle == NULL || state->pending_len == 0u) {
        return TRUE;
    }

    char* transformed = NULL;
    size_t transformed_len = 0u;
    size_t transformed_cap = 0u;
    int built = kbo_roster_export_build_transformed(
        &state->pending,
        &state->pending_len,
        &state->pending_cap,
        NULL,
        0u,
        1,
        &transformed,
        &transformed_len,
        &transformed_cap);
    if (!built) {
        if (transformed != NULL) {
            HeapFree(GetProcessHeap(), 0, transformed);
        }
        return FALSE;
    }
    if (transformed_len == 0u) {
        if (transformed != NULL) {
            HeapFree(GetProcessHeap(), 0, transformed);
        }
        return TRUE;
    }

    DWORD written = 0u;
    BOOL ok = kbo_roster_export_write_original(state->handle, transformed, (DWORD)transformed_len, &written)
        && written == (DWORD)transformed_len;
    HeapFree(GetProcessHeap(), 0, transformed);
    return ok;
}

void kbo_roster_export_unregister_handle(HANDLE handle)
{
    if (handle == NULL || handle == INVALID_HANDLE_VALUE) {
        return;
    }

    AcquireSRWLockExclusive(&g_kbo_roster_export_lock);
    int index = kbo_roster_export_find_handle_index_locked(handle);
    if (index >= 0) {
        kbo_roster_export_flush_pending_locked(&g_kbo_roster_export_handles[index]);
        kbo_log_runtimef(
            "KBO roster export extra columns closing handle=%p path=%s",
            handle,
            g_kbo_roster_export_handles[index].path);
        kbo_roster_export_handle_state_free(&g_kbo_roster_export_handles[index]);
    }
    ReleaseSRWLockExclusive(&g_kbo_roster_export_lock);
}

BOOL WINAPI kbo_roster_export_WriteFile(
    HANDLE hFile,
    LPCVOID lpBuffer,
    DWORD nNumberOfBytesToWrite,
    LPDWORD lpNumberOfBytesWritten,
    LPOVERLAPPED lpOverlapped)
{
    if (InterlockedCompareExchange(&g_kbo_roster_export_transforming, 0, 0) != 0
            || lpOverlapped != NULL
            || lpBuffer == NULL
            || nNumberOfBytesToWrite == 0u) {
        return g_kbo_original_WriteFile(hFile, lpBuffer, nNumberOfBytesToWrite, lpNumberOfBytesWritten, lpOverlapped);
    }

    AcquireSRWLockExclusive(&g_kbo_roster_export_lock);
    int index = kbo_roster_export_find_handle_index_locked(hFile);
    if (index < 0) {
        ReleaseSRWLockExclusive(&g_kbo_roster_export_lock);
        return g_kbo_original_WriteFile(hFile, lpBuffer, nNumberOfBytesToWrite, lpNumberOfBytesWritten, lpOverlapped);
    }

    char* transformed = NULL;
    size_t transformed_len = 0u;
    size_t transformed_cap = 0u;
    int built = kbo_roster_export_build_transformed(
        &g_kbo_roster_export_handles[index].pending,
        &g_kbo_roster_export_handles[index].pending_len,
        &g_kbo_roster_export_handles[index].pending_cap,
        (const char*)lpBuffer,
        (size_t)nNumberOfBytesToWrite,
        0,
        &transformed,
        &transformed_len,
        &transformed_cap);
    ReleaseSRWLockExclusive(&g_kbo_roster_export_lock);

    if (!built) {
        if (transformed != NULL) {
            HeapFree(GetProcessHeap(), 0, transformed);
        }
        return g_kbo_original_WriteFile(hFile, lpBuffer, nNumberOfBytesToWrite, lpNumberOfBytesWritten, lpOverlapped);
    }

    BOOL ok = TRUE;
    DWORD transformed_written = 0u;
    if (transformed_len > 0u) {
        ok = kbo_roster_export_write_original(hFile, transformed, (DWORD)transformed_len, &transformed_written);
    }
    if (transformed != NULL) {
        HeapFree(GetProcessHeap(), 0, transformed);
    }

    if (ok) {
        if (lpNumberOfBytesWritten != NULL) {
            *lpNumberOfBytesWritten = nNumberOfBytesToWrite;
        }
        return TRUE;
    }
    return FALSE;
}
