#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "../logging/core_log.h"
#include "kbo_optimizer.h"

static volatile LONG g_kbo_optimizer_result_temp_sequence = 0;

static ULONGLONG kbo_optimizer_file_write_time_ull(const WIN32_FIND_DATAA* data)
{
    if (data == NULL) {
        return 0u;
    }
    return (((ULONGLONG)data->ftLastWriteTime.dwHighDateTime) << 32)
        | (ULONGLONG)data->ftLastWriteTime.dwLowDateTime;
}

static ULONGLONG kbo_optimizer_path_write_time_ull(const char* path)
{
    if (path == NULL || path[0] == '\0') {
        return 0u;
    }
    WIN32_FIND_DATAA data;
    HANDLE find = FindFirstFileA(path, &data);
    if (find == INVALID_HANDLE_VALUE) {
        return 0u;
    }
    FindClose(find);
    return kbo_optimizer_file_write_time_ull(&data);
}

static ULONGLONG kbo_optimizer_newest_python_source_time(const char* module_path)
{
    if (module_path == NULL || module_path[0] == '\0') {
        return 0u;
    }

    char path[MAX_PATH * 3] = {0};
    snprintf(path, sizeof(path), "%stools\\kbo_optimizer.py", module_path);
    ULONGLONG newest = kbo_optimizer_path_write_time_ull(path);

    char pattern[MAX_PATH * 3] = {0};
    snprintf(pattern, sizeof(pattern), "%stools\\kbo_optimizer_lib\\*.py", module_path);
    WIN32_FIND_DATAA data;
    HANDLE find = FindFirstFileA(pattern, &data);
    if (find == INVALID_HANDLE_VALUE) {
        return newest;
    }
    do {
        if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0u) {
            ULONGLONG write_time = kbo_optimizer_file_write_time_ull(&data);
            if (write_time > newest) {
                newest = write_time;
            }
        }
    } while (FindNextFileA(find, &data));
    FindClose(find);
    return newest;
}

static int kbo_optimizer_make_temp_result_path(const char* result_path, char* out, size_t out_size)
{
    if (result_path == NULL || result_path[0] == '\0' || out == NULL || out_size == 0) {
        return 0;
    }
    LONG sequence = InterlockedIncrement(&g_kbo_optimizer_result_temp_sequence);
    int written = snprintf(
        out,
        out_size,
        "%s.tmp.%lu.%ld",
        result_path,
        (unsigned long)GetCurrentProcessId(),
        (long)sequence);
    return written > 0 && (size_t)written < out_size;
}

static int kbo_optimizer_get_tool_path(char* out, size_t out_size, int* out_is_python_script)
{
    if (out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';
    if (out_is_python_script != NULL) {
        *out_is_python_script = 0;
    }

    HMODULE module = NULL;
    if (!GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            (LPCSTR)&kbo_optimizer_get_tool_path,
            &module)) {
        return 0;
    }

    char module_path[MAX_PATH * 3] = {0};
    DWORD len = GetModuleFileNameA(module, module_path, (DWORD)sizeof(module_path));
    if (len == 0 || len >= sizeof(module_path)) {
        return 0;
    }
    char* slash = strrchr(module_path, '\\');
    if (slash == NULL) {
        return 0;
    }
    slash[1] = '\0';

    char exe_path[MAX_PATH * 3] = {0};
    char script_path[MAX_PATH * 3] = {0};
    snprintf(exe_path, sizeof(exe_path), "%stools\\kbo_optimizer.exe", module_path);
    snprintf(script_path, sizeof(script_path), "%stools\\kbo_optimizer.py", module_path);

    DWORD exe_attrs = GetFileAttributesA(exe_path);
    DWORD script_attrs = GetFileAttributesA(script_path);
    ULONGLONG exe_time = exe_attrs != INVALID_FILE_ATTRIBUTES
        ? kbo_optimizer_path_write_time_ull(exe_path)
        : 0u;
    ULONGLONG script_time = script_attrs != INVALID_FILE_ATTRIBUTES
        ? kbo_optimizer_newest_python_source_time(module_path)
        : 0u;

    if (script_attrs != INVALID_FILE_ATTRIBUTES
            && (exe_attrs == INVALID_FILE_ATTRIBUTES || script_time > exe_time)) {
        snprintf(out, out_size, "%s", script_path);
        if (out_is_python_script != NULL) {
            *out_is_python_script = 1;
        }
        return 1;
    }

    if (exe_attrs != INVALID_FILE_ATTRIBUTES) {
        snprintf(out, out_size, "%s", exe_path);
        return 1;
    }

    if (script_attrs != INVALID_FILE_ATTRIBUTES) {
        snprintf(out, out_size, "%s", script_path);
        if (out_is_python_script != NULL) {
            *out_is_python_script = 1;
        }
        return 1;
    }

    static volatile LONG missing_log_count = 0;
    if (InterlockedIncrement(&missing_log_count) <= 8) {
        kbo_log_runtimef(
            "KBO optimizer missing exe=%stools\\kbo_optimizer.exe script=%stools\\kbo_optimizer.py",
            module_path,
            module_path);
    }
    return 0;
}

int kbo_optimizer_run_mode(
    const char* mode,
    const char* request_path,
    const char* result_path,
    DWORD timeout_ms)
{
    if (mode == NULL || mode[0] == '\0'
            || request_path == NULL || request_path[0] == '\0'
            || result_path == NULL || result_path[0] == '\0') {
        return 0;
    }

    char tool_path[MAX_PATH * 3] = {0};
    int is_python_script = 0;
    if (!kbo_optimizer_get_tool_path(tool_path, sizeof(tool_path), &is_python_script)) {
        return 0;
    }

    char temp_result_path[MAX_PATH * 3] = {0};
    if (!kbo_optimizer_make_temp_result_path(result_path, temp_result_path, sizeof(temp_result_path))) {
        kbo_log_runtimef("KBO optimizer temp result path unavailable mode=%s result=%s", mode, result_path);
        return 0;
    }

    DeleteFileA(result_path);
    DeleteFileA(temp_result_path);

    char command[MAX_PATH * 10] = {0};
    if (is_python_script) {
        snprintf(
            command,
            sizeof(command),
            "python \"%s\" --mode %s \"%s\" \"%s\"",
            tool_path,
            mode,
            request_path,
            temp_result_path);
    } else {
        snprintf(
            command,
            sizeof(command),
            "\"%s\" --mode %s \"%s\" \"%s\"",
            tool_path,
            mode,
            request_path,
            temp_result_path);
    }

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof(si));
    memset(&pi, 0, sizeof(pi));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    if (!CreateProcessA(NULL, command, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        static volatile LONG create_fail_count = 0;
        if (InterlockedIncrement(&create_fail_count) <= 8) {
            kbo_log_runtimef("KBO optimizer launch failed mode=%s gle=%lu tool=%s", mode, GetLastError(), tool_path);
        }
        return 0;
    }

    DWORD wait = WaitForSingleObject(pi.hProcess, timeout_ms != 0u ? timeout_ms : 8000u);
    DWORD exit_code = 1u;
    if (wait == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 1);
        WaitForSingleObject(pi.hProcess, 1000u);
        kbo_log_runtimef("KBO optimizer timed out mode=%s timeout_ms=%lu", mode, timeout_ms != 0u ? timeout_ms : 8000u);
    } else {
        GetExitCodeProcess(pi.hProcess, &exit_code);
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    int ok = wait != WAIT_TIMEOUT
        && exit_code == 0u
        && GetFileAttributesA(temp_result_path) != INVALID_FILE_ATTRIBUTES;
    if (ok) {
        DeleteFileA(result_path);
        if (!MoveFileExA(temp_result_path, result_path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED)) {
            kbo_log_runtimef(
                "KBO optimizer result promote failed mode=%s gle=%lu temp=%s result=%s",
                mode,
                GetLastError(),
                temp_result_path,
                result_path);
            ok = 0;
        }
    }
    if (!ok) {
        DeleteFileA(temp_result_path);
        DeleteFileA(result_path);
        static volatile LONG fail_count = 0;
        if (InterlockedIncrement(&fail_count) <= 8) {
            kbo_log_runtimef("KBO optimizer failed mode=%s exit=%lu result=%s", mode, exit_code, result_path);
        }
    }
    return ok;
}
