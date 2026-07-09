#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../core/logging/core_log.h"
#include "../../core/product/ootp_product.h"
#include "../common/patch_host.h"
#include "import_extra_columns/roster_import_extra_columns_direct.h"
#include "imports/roster_export_import_patch.h"
#include "roster_export_extra_columns.h"
#include "state/roster_export_state.h"
#include "watcher/roster_export_watcher.h"

typedef HANDLE (WINAPI *KboCreateFileAFn)(
    LPCSTR lpFileName,
    DWORD dwDesiredAccess,
    DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes,
    HANDLE hTemplateFile);

typedef HANDLE (WINAPI *KboCreateFileWFn)(
    LPCWSTR lpFileName,
    DWORD dwDesiredAccess,
    DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes,
    HANDLE hTemplateFile);

typedef HANDLE (WINAPI *KboCreateFile2Fn)(
    LPCWSTR lpFileName,
    DWORD dwDesiredAccess,
    DWORD dwShareMode,
    DWORD dwCreationDisposition,
    LPCVOID pCreateExParams);

typedef BOOL (WINAPI *KboCloseHandleFn)(HANDLE hObject);
typedef FILE* (__cdecl *KboFopenFn)(const char* file_name, const char* mode);
typedef int (__cdecl *KboFopenSFn)(FILE** stream, const char* file_name, const char* mode);
typedef FILE* (__cdecl *KboWfopenFn)(const wchar_t* file_name, const wchar_t* mode);
typedef int (__cdecl *KboWfopenSFn)(FILE** stream, const wchar_t* file_name, const wchar_t* mode);
typedef FILE* (__cdecl *KboStdFiopenAFn)(const char* file_name, int mode, int prot);
typedef FILE* (__cdecl *KboStdFiopenWFn)(const wchar_t* file_name, int mode, int prot);

typedef union KboRosterExportProcCast {
    FARPROC farproc;
    uintptr_t raw;
    KboCreateFileAFn create_file_a;
    KboCreateFileWFn create_file_w;
    KboCreateFile2Fn create_file2;
    KboRosterExportWriteFileFn write_file;
    KboCloseHandleFn close_handle;
    KboFopenFn fopen_fn;
    KboFopenSFn fopen_s_fn;
    KboWfopenFn wfopen_fn;
    KboWfopenSFn wfopen_s_fn;
    KboStdFiopenAFn std_fiopen_a;
    KboStdFiopenWFn std_fiopen_w;
} KboRosterExportProcCast;

static KboCreateFileAFn g_kbo_original_CreateFileA = NULL;
static KboCreateFileWFn g_kbo_original_CreateFileW = NULL;
static KboCreateFile2Fn g_kbo_original_CreateFile2 = NULL;
static KboCloseHandleFn g_kbo_original_CloseHandle = NULL;
static KboRosterExportWriteFileFn g_kbo_original_WriteFile = NULL;
static KboFopenFn g_kbo_original_fopen = NULL;
static KboFopenSFn g_kbo_original_fopen_s = NULL;
static KboWfopenFn g_kbo_original_wfopen = NULL;
static KboWfopenSFn g_kbo_original_wfopen_s = NULL;
static KboStdFiopenAFn g_kbo_original_std_fiopen_a = NULL;
static KboStdFiopenWFn g_kbo_original_std_fiopen_w = NULL;
static volatile LONG g_kbo_roster_export_patch_installed = 0;
static volatile LONG g_kbo_roster_export_import_export_logs = 0;
static volatile LONG g_kbo_roster_export_roster_open_logs = 0;
static volatile LONG g_kbo_roster_export_stdio_open_logs = 0;
static volatile LONG g_kbo_roster_export_stdio_stack_logs = 0;

static int kbo_roster_export_ascii_equal_ci(char a, char b)
{
    if (a >= 'A' && a <= 'Z') {
        a = (char)(a - 'A' + 'a');
    }
    if (b >= 'A' && b <= 'Z') {
        b = (char)(b - 'A' + 'a');
    }
    return a == b;
}

static int kbo_roster_export_ends_with_ci(const char* text, const char* suffix)
{
    if (text == NULL || suffix == NULL) {
        return 0;
    }
    size_t text_len = strlen(text);
    size_t suffix_len = strlen(suffix);
    if (suffix_len == 0u || text_len < suffix_len) {
        return 0;
    }

    const char* start = text + text_len - suffix_len;
    for (size_t i = 0u; i < suffix_len; i++) {
        char a = start[i] == '/' ? '\\' : start[i];
        char b = suffix[i] == '/' ? '\\' : suffix[i];
        if (!kbo_roster_export_ascii_equal_ci(a, b)) {
            return 0;
        }
    }
    return 1;
}

static int kbo_roster_export_contains_ci(const char* text, const char* needle)
{
    if (text == NULL || needle == NULL || needle[0] == '\0') {
        return 0;
    }
    size_t text_len = strlen(text);
    size_t needle_len = strlen(needle);
    if (text_len < needle_len) {
        return 0;
    }
    for (size_t i = 0u; i + needle_len <= text_len; i++) {
        int matched = 1;
        for (size_t j = 0u; j < needle_len; j++) {
            char a = text[i + j] == '/' ? '\\' : text[i + j];
            char b = needle[j] == '/' ? '\\' : needle[j];
            if (!kbo_roster_export_ascii_equal_ci(a, b)) {
                matched = 0;
                break;
            }
        }
        if (matched) {
            return 1;
        }
    }
    return 0;
}

static int kbo_roster_export_access_has_write(DWORD desired_access)
{
    return (desired_access & (GENERIC_WRITE | FILE_WRITE_DATA | FILE_APPEND_DATA)) != 0u;
}

static int kbo_roster_export_access_has_read(DWORD desired_access)
{
    return (desired_access & (GENERIC_READ | FILE_READ_DATA)) != 0u;
}

static int kbo_roster_export_path_is_roster_file(const char* path)
{
    if (path == NULL || path[0] == '\0') {
        return 0;
    }
    return kbo_roster_export_ends_with_ci(path, "\\import_export\\kbo_rosters.txt")
        || kbo_roster_export_ends_with_ci(path, "\\import_export\\kbo_rosters.csv")
        || kbo_roster_export_ends_with_ci(path, "\\import_export\\kbo_rosters.txt.tmp")
        || kbo_roster_export_ends_with_ci(path, "\\import_export\\kbo_rosters.csv.tmp")
        || kbo_roster_export_ends_with_ci(path, "\\import_export\\kbo_rosters.tmp");
}

static int kbo_roster_export_path_is_target(const char* path, DWORD desired_access)
{
    return kbo_roster_export_access_has_write(desired_access)
        && kbo_roster_export_path_is_roster_file(path);
}

static uintptr_t kbo_roster_export_address_to_exe_rva(const void* address)
{
    if (address == NULL) {
        return 0u;
    }
    HMODULE exe = GetModuleHandleA(NULL);
    uintptr_t base = (uintptr_t)exe;
    uintptr_t value = (uintptr_t)address;
    if (base == 0u || value < base) {
        return 0u;
    }
    uintptr_t rva = value - base;
    if (rva > 0x80000000ull) {
        return 0u;
    }
    return rva;
}

static int kbo_roster_export_stdio_mode_is_read(const char* mode)
{
    if (mode == NULL || mode[0] == '\0') {
        return 0;
    }
    return strchr(mode, 'r') != NULL
        && strchr(mode, 'w') == NULL
        && strchr(mode, 'a') == NULL;
}

static void kbo_roster_export_log_import_export_write_path(const char* path, DWORD desired_access)
{
    if (path == NULL
            || path[0] == '\0'
            || !kbo_roster_export_access_has_write(desired_access)
            || !kbo_roster_export_contains_ci(path, "\\import_export\\")) {
        return;
    }
    LONG seen = InterlockedIncrement(&g_kbo_roster_export_import_export_logs);
    if (seen <= 32) {
        kbo_log_runtimef(
            "KBO roster export extra columns observed import_export write access=0x%lx path=%s",
            desired_access,
            path);
    }
}

static void kbo_roster_export_log_roster_file_open_path(const char* path, DWORD desired_access)
{
    if (!kbo_roster_export_path_is_roster_file(path)
            || (!kbo_roster_export_access_has_read(desired_access)
                && !kbo_roster_export_access_has_write(desired_access))) {
        return;
    }

    LONG seen = InterlockedIncrement(&g_kbo_roster_export_roster_open_logs);
    if (seen <= 64) {
        kbo_log_runtimef(
            "KBO roster import/export observed roster file open access=0x%lx read=%d write=%d path=%s",
            desired_access,
            kbo_roster_export_access_has_read(desired_access),
            kbo_roster_export_access_has_write(desired_access),
            path);
    }
}

static void kbo_roster_export_log_stdio_open_path(
    const char* api_name,
    const char* path,
    const char* mode,
    int success,
    const void* caller)
{
    if (path == NULL || path[0] == '\0') {
        return;
    }
    if (!kbo_roster_export_contains_ci(path, "\\import_export\\")
            && !kbo_roster_export_contains_ci(path, "/import_export/")
            && !kbo_roster_export_contains_ci(path, "kbo_rosters")) {
        return;
    }

    LONG seen = InterlockedIncrement(&g_kbo_roster_export_stdio_open_logs);
    if (seen <= 128) {
        kbo_log_runtimef(
            "KBO roster import/export observed stdio open api=%s success=%d mode=%s caller=%p caller_rva=0x%llx path=%s",
            api_name != NULL ? api_name : "",
            success,
            mode != NULL ? mode : "",
            caller,
            (unsigned long long)kbo_roster_export_address_to_exe_rva(caller),
            path);
    }

    if (!success
            || !kbo_roster_export_path_is_roster_file(path)
            || !kbo_roster_export_stdio_mode_is_read(mode)) {
        return;
    }

    LONG stack_seen = InterlockedIncrement(&g_kbo_roster_export_stdio_stack_logs);
    if (stack_seen <= 16) {
        void* frames[12] = {0};
        USHORT frame_count = RtlCaptureStackBackTrace(0u, 12u, frames, NULL);
        char stack_text[768] = {0};
        size_t offset = 0u;
        for (USHORT i = 0u; i < frame_count && offset < sizeof(stack_text); i++) {
            int written = snprintf(
                stack_text + offset,
                sizeof(stack_text) - offset,
                " f%u=%p/rva=0x%llx",
                (unsigned)i,
                frames[i],
                (unsigned long long)kbo_roster_export_address_to_exe_rva(frames[i]));
            if (written <= 0) {
                break;
            }
            if ((size_t)written >= sizeof(stack_text) - offset) {
                offset = sizeof(stack_text) - 1u;
                break;
            }
            offset += (size_t)written;
        }
        kbo_log_runtimef(
            "KBO roster import/export stdio caller stack api=%s mode=%s caller=%p caller_rva=0x%llx frames=%u%s path=%s",
            api_name != NULL ? api_name : "",
            mode != NULL ? mode : "",
            caller,
            (unsigned long long)kbo_roster_export_address_to_exe_rva(caller),
            (unsigned)frame_count,
            stack_text,
            path);
    }
}

static int kbo_roster_export_wide_to_utf8(LPCWSTR in, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return 0;
    }
    out[0] = '\0';
    if (in == NULL || in[0] == L'\0') {
        return 0;
    }

    int written = WideCharToMultiByte(
        CP_UTF8,
        0,
        in,
        -1,
        out,
        (int)out_size,
        NULL,
        NULL);
    if (written <= 0) {
        out[0] = '\0';
        return 0;
    }
    out[out_size - 1u] = '\0';
    return 1;
}

static FARPROC kbo_roster_export_get_proc_any(const char* proc_name, const char* const* module_names, size_t module_count)
{
    if (proc_name == NULL || module_names == NULL) {
        return NULL;
    }
    for (size_t i = 0u; i < module_count; i++) {
        const char* module_name = module_names[i];
        HMODULE module = GetModuleHandleA(module_name);
        if (module == NULL) {
            module = LoadLibraryA(module_name);
        }
        if (module == NULL) {
            continue;
        }
        FARPROC proc = GetProcAddress(module, proc_name);
        if (proc != NULL) {
            return proc;
        }
    }
    return NULL;
}

static HANDLE WINAPI kbo_roster_export_CreateFileA(
    LPCSTR lpFileName,
    DWORD dwDesiredAccess,
    DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes,
    HANDLE hTemplateFile)
{
    HANDLE handle = g_kbo_original_CreateFileA(
        lpFileName,
        dwDesiredAccess,
        dwShareMode,
        lpSecurityAttributes,
        dwCreationDisposition,
        dwFlagsAndAttributes,
        hTemplateFile);
    if (handle != INVALID_HANDLE_VALUE) {
        kbo_roster_export_log_import_export_write_path(lpFileName, dwDesiredAccess);
        kbo_roster_export_log_roster_file_open_path(lpFileName, dwDesiredAccess);
    }
    if (handle != INVALID_HANDLE_VALUE && kbo_roster_export_path_is_target(lpFileName, dwDesiredAccess)) {
        kbo_roster_export_register_handle(handle, lpFileName);
    }
    return handle;
}

static HANDLE WINAPI kbo_roster_export_CreateFileW(
    LPCWSTR lpFileName,
    DWORD dwDesiredAccess,
    DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes,
    HANDLE hTemplateFile)
{
    HANDLE handle = g_kbo_original_CreateFileW(
        lpFileName,
        dwDesiredAccess,
        dwShareMode,
        lpSecurityAttributes,
        dwCreationDisposition,
        dwFlagsAndAttributes,
        hTemplateFile);
    char path[MAX_PATH] = {0};
    if (handle != INVALID_HANDLE_VALUE && kbo_roster_export_wide_to_utf8(lpFileName, path, sizeof(path))) {
        kbo_roster_export_log_import_export_write_path(path, dwDesiredAccess);
        kbo_roster_export_log_roster_file_open_path(path, dwDesiredAccess);
    }
    if (handle != INVALID_HANDLE_VALUE
            && kbo_roster_export_path_is_target(path, dwDesiredAccess)) {
        kbo_roster_export_register_handle(handle, path);
    }
    return handle;
}

static HANDLE WINAPI kbo_roster_export_CreateFile2(
    LPCWSTR lpFileName,
    DWORD dwDesiredAccess,
    DWORD dwShareMode,
    DWORD dwCreationDisposition,
    LPCVOID pCreateExParams)
{
    HANDLE handle = g_kbo_original_CreateFile2(
        lpFileName,
        dwDesiredAccess,
        dwShareMode,
        dwCreationDisposition,
        pCreateExParams);
    char path[MAX_PATH] = {0};
    if (handle != INVALID_HANDLE_VALUE && kbo_roster_export_wide_to_utf8(lpFileName, path, sizeof(path))) {
        kbo_roster_export_log_import_export_write_path(path, dwDesiredAccess);
        kbo_roster_export_log_roster_file_open_path(path, dwDesiredAccess);
    }
    if (handle != INVALID_HANDLE_VALUE
            && kbo_roster_export_path_is_target(path, dwDesiredAccess)) {
        kbo_roster_export_register_handle(handle, path);
    }
    return handle;
}

static FILE* __cdecl kbo_roster_export_fopen(const char* file_name, const char* mode)
{
    void* caller = __builtin_return_address(0);
    FILE* stream = g_kbo_original_fopen(file_name, mode);
    kbo_roster_export_log_stdio_open_path("fopen", file_name, mode, stream != NULL, caller);
    if (stream != NULL) {
        kbo_roster_import_extra_columns_direct_maybe_apply_path(file_name, mode);
    }
    return stream;
}

static int __cdecl kbo_roster_export_fopen_s(FILE** stream, const char* file_name, const char* mode)
{
    void* caller = __builtin_return_address(0);
    int result = g_kbo_original_fopen_s(stream, file_name, mode);
    kbo_roster_export_log_stdio_open_path(
        "fopen_s",
        file_name,
        mode,
        result == 0 && stream != NULL && *stream != NULL,
        caller);
    if (result == 0 && stream != NULL && *stream != NULL) {
        kbo_roster_import_extra_columns_direct_maybe_apply_path(file_name, mode);
    }
    return result;
}

static FILE* __cdecl kbo_roster_export_wfopen(const wchar_t* file_name, const wchar_t* mode)
{
    void* caller = __builtin_return_address(0);
    FILE* stream = g_kbo_original_wfopen(file_name, mode);
    char path[MAX_PATH] = {0};
    char mode_text[64] = {0};
    kbo_roster_export_wide_to_utf8(file_name, path, sizeof(path));
    kbo_roster_export_wide_to_utf8(mode, mode_text, sizeof(mode_text));
    kbo_roster_export_log_stdio_open_path("_wfopen", path, mode_text, stream != NULL, caller);
    if (stream != NULL) {
        kbo_roster_import_extra_columns_direct_maybe_apply_path(path, mode_text);
    }
    return stream;
}

static int __cdecl kbo_roster_export_wfopen_s(FILE** stream, const wchar_t* file_name, const wchar_t* mode)
{
    void* caller = __builtin_return_address(0);
    int result = g_kbo_original_wfopen_s(stream, file_name, mode);
    char path[MAX_PATH] = {0};
    char mode_text[64] = {0};
    kbo_roster_export_wide_to_utf8(file_name, path, sizeof(path));
    kbo_roster_export_wide_to_utf8(mode, mode_text, sizeof(mode_text));
    kbo_roster_export_log_stdio_open_path(
        "_wfopen_s",
        path,
        mode_text,
        result == 0 && stream != NULL && *stream != NULL,
        caller);
    if (result == 0 && stream != NULL && *stream != NULL) {
        kbo_roster_import_extra_columns_direct_maybe_apply_path(path, mode_text);
    }
    return result;
}

static FILE* __cdecl kbo_roster_export_std_fiopen_a(const char* file_name, int mode, int prot)
{
    void* caller = __builtin_return_address(0);
    FILE* stream = g_kbo_original_std_fiopen_a(file_name, mode, prot);
    char mode_text[32] = {0};
    snprintf(mode_text, sizeof(mode_text), "%d/%d", mode, prot);
    kbo_roster_export_log_stdio_open_path(
        "std::_FiopenA",
        file_name,
        mode_text,
        stream != NULL,
        caller);
    return stream;
}

static FILE* __cdecl kbo_roster_export_std_fiopen_w(const wchar_t* file_name, int mode, int prot)
{
    void* caller = __builtin_return_address(0);
    FILE* stream = g_kbo_original_std_fiopen_w(file_name, mode, prot);
    char path[MAX_PATH] = {0};
    char mode_text[32] = {0};
    kbo_roster_export_wide_to_utf8(file_name, path, sizeof(path));
    snprintf(mode_text, sizeof(mode_text), "%d/%d", mode, prot);
    kbo_roster_export_log_stdio_open_path(
        "std::_FiopenW",
        path,
        mode_text,
        stream != NULL,
        caller);
    return stream;
}

static BOOL WINAPI kbo_roster_export_CloseHandle(HANDLE hObject)
{
    kbo_roster_export_unregister_handle(hObject);
    return g_kbo_original_CloseHandle(hObject);
}

static uintptr_t kbo_roster_export_get_hook_proc(const char* proc_name)
{
    KboRosterExportProcCast cast;
    cast.raw = 0u;
    if (strcmp(proc_name, "CreateFileA") == 0) {
        cast.create_file_a = &kbo_roster_export_CreateFileA;
        return cast.raw;
    }
    if (strcmp(proc_name, "CreateFileW") == 0) {
        cast.create_file_w = &kbo_roster_export_CreateFileW;
        return cast.raw;
    }
    if (strcmp(proc_name, "CreateFile2") == 0 && g_kbo_original_CreateFile2 != NULL) {
        cast.create_file2 = &kbo_roster_export_CreateFile2;
        return cast.raw;
    }
    if (strcmp(proc_name, "WriteFile") == 0) {
        cast.write_file = &kbo_roster_export_WriteFile;
        return cast.raw;
    }
    if (strcmp(proc_name, "CloseHandle") == 0) {
        cast.close_handle = &kbo_roster_export_CloseHandle;
        return cast.raw;
    }
    if (strcmp(proc_name, "fopen") == 0 && g_kbo_original_fopen != NULL) {
        cast.fopen_fn = &kbo_roster_export_fopen;
        return cast.raw;
    }
    if (strcmp(proc_name, "fopen_s") == 0 && g_kbo_original_fopen_s != NULL) {
        cast.fopen_s_fn = &kbo_roster_export_fopen_s;
        return cast.raw;
    }
    if (strcmp(proc_name, "_wfopen") == 0 && g_kbo_original_wfopen != NULL) {
        cast.wfopen_fn = &kbo_roster_export_wfopen;
        return cast.raw;
    }
    if (strcmp(proc_name, "_wfopen_s") == 0 && g_kbo_original_wfopen_s != NULL) {
        cast.wfopen_s_fn = &kbo_roster_export_wfopen_s;
        return cast.raw;
    }
    if (strcmp(proc_name, "?_Fiopen@std@@YAPEAU_iobuf@@PEBDHH@Z") == 0
            && g_kbo_original_std_fiopen_a != NULL) {
        cast.std_fiopen_a = &kbo_roster_export_std_fiopen_a;
        return cast.raw;
    }
    if (strcmp(proc_name, "?_Fiopen@std@@YAPEAU_iobuf@@PEB_WHH@Z") == 0
            && g_kbo_original_std_fiopen_w != NULL) {
        cast.std_fiopen_w = &kbo_roster_export_std_fiopen_w;
        return cast.raw;
    }
    return 0u;
}

static void kbo_roster_export_init_originals(void)
{
    static const char* const crt_modules[] = {
        "ucrtbase.dll",
        "api-ms-win-crt-stdio-l1-1-0.dll"
    };
    static const char* const msvcp_modules[] = {
        "MSVCP140.dll"
    };

    HMODULE kernel32 = GetModuleHandleA("kernel32.dll");
    if (kernel32 == NULL) {
        kernel32 = LoadLibraryA("kernel32.dll");
    }
    if (kernel32 == NULL) {
        return;
    }

    KboRosterExportProcCast cast;
    if (g_kbo_original_CreateFileA == NULL) {
        cast.farproc = GetProcAddress(kernel32, "CreateFileA");
        g_kbo_original_CreateFileA = cast.create_file_a;
    }
    if (g_kbo_original_CreateFileW == NULL) {
        cast.farproc = GetProcAddress(kernel32, "CreateFileW");
        g_kbo_original_CreateFileW = cast.create_file_w;
    }
    if (g_kbo_original_CreateFile2 == NULL) {
        cast.farproc = GetProcAddress(kernel32, "CreateFile2");
        g_kbo_original_CreateFile2 = cast.create_file2;
    }
    if (g_kbo_original_CloseHandle == NULL) {
        cast.farproc = GetProcAddress(kernel32, "CloseHandle");
        g_kbo_original_CloseHandle = cast.close_handle;
    }
    if (g_kbo_original_WriteFile == NULL) {
        cast.farproc = GetProcAddress(kernel32, "WriteFile");
        g_kbo_original_WriteFile = cast.write_file;
    }
    if (g_kbo_original_fopen == NULL) {
        cast.farproc = kbo_roster_export_get_proc_any("fopen", crt_modules, sizeof(crt_modules) / sizeof(crt_modules[0]));
        g_kbo_original_fopen = cast.fopen_fn;
    }
    if (g_kbo_original_fopen_s == NULL) {
        cast.farproc = kbo_roster_export_get_proc_any("fopen_s", crt_modules, sizeof(crt_modules) / sizeof(crt_modules[0]));
        g_kbo_original_fopen_s = cast.fopen_s_fn;
    }
    if (g_kbo_original_wfopen == NULL) {
        cast.farproc = kbo_roster_export_get_proc_any("_wfopen", crt_modules, sizeof(crt_modules) / sizeof(crt_modules[0]));
        g_kbo_original_wfopen = cast.wfopen_fn;
    }
    if (g_kbo_original_wfopen_s == NULL) {
        cast.farproc = kbo_roster_export_get_proc_any("_wfopen_s", crt_modules, sizeof(crt_modules) / sizeof(crt_modules[0]));
        g_kbo_original_wfopen_s = cast.wfopen_s_fn;
    }
    if (g_kbo_original_std_fiopen_a == NULL) {
        cast.farproc = kbo_roster_export_get_proc_any(
            "?_Fiopen@std@@YAPEAU_iobuf@@PEBDHH@Z",
            msvcp_modules,
            sizeof(msvcp_modules) / sizeof(msvcp_modules[0]));
        g_kbo_original_std_fiopen_a = cast.std_fiopen_a;
    }
    if (g_kbo_original_std_fiopen_w == NULL) {
        cast.farproc = kbo_roster_export_get_proc_any(
            "?_Fiopen@std@@YAPEAU_iobuf@@PEB_WHH@Z",
            msvcp_modules,
            sizeof(msvcp_modules) / sizeof(msvcp_modules[0]));
        g_kbo_original_std_fiopen_w = cast.std_fiopen_w;
    }
    kbo_roster_export_state_set_original_write_file(g_kbo_original_WriteFile);
}

int install_kbo_roster_export_extra_columns_patch(void)
{
    if (InterlockedCompareExchange(&g_kbo_roster_export_patch_installed, 1, 0) != 0) {
        return 1;
    }

    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        kbo_log_runtime_line("GetModuleHandleA(NULL) failed for KBO roster export extra columns patch");
        InterlockedExchange(&g_kbo_roster_export_patch_installed, 0);
        return 0;
    }

    char host[MAX_PATH] = {0};
    GetModuleFileNameA(exe, host, (DWORD)sizeof(host));
    if (!kbo_patch_host_matches_product(host)) {
        kbo_log_runtimef("host is not " KBO_OOTP_EXECUTABLE_NAME ", skipping KBO roster export extra columns patch host=%s", host);
        return 0;
    }

    kbo_roster_export_init_originals();
    if (g_kbo_original_CreateFileA == NULL
            || g_kbo_original_CreateFileW == NULL
            || g_kbo_original_CloseHandle == NULL
            || g_kbo_original_WriteFile == NULL) {
        kbo_log_runtime_line("KBO roster export extra columns patch skipped: WinAPI originals unresolved");
        InterlockedExchange(&g_kbo_roster_export_patch_installed, 0);
        return 0;
    }

    int watcher_started = start_kbo_roster_export_extra_columns_watcher();
    int patched = kbo_roster_export_patch_module_imports(exe, kbo_roster_export_get_hook_proc);
    kbo_log_runtimef(
        "installed KBO roster export extra columns host IAT patch slots=%d watcher=%d",
        patched,
        watcher_started);
    if (patched <= 0 && !watcher_started) {
        InterlockedExchange(&g_kbo_roster_export_patch_installed, 0);
    }
    return patched > 0 || watcher_started;
}
