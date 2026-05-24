#include "../hotkey_window_runtime_window_internal.h"

static PVOID g_kbo_hotkey_window_exception_handler = NULL;
static volatile LONG g_kbo_hotkey_window_crashed = 0;

static LONG CALLBACK kbo_hotkey_window_exception_guard(EXCEPTION_POINTERS* exception_info)
{
    if (GetCurrentThreadId() != g_kbo_hotkey_thread_id) {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    DWORD code = 0u;
    PVOID address = NULL;
    if (exception_info != NULL && exception_info->ExceptionRecord != NULL) {
        code = exception_info->ExceptionRecord->ExceptionCode;
        address = exception_info->ExceptionRecord->ExceptionAddress;
    }

    switch (code) {
    case EXCEPTION_ACCESS_VIOLATION:
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
    case EXCEPTION_DATATYPE_MISALIGNMENT:
    case EXCEPTION_FLT_DENORMAL_OPERAND:
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:
    case EXCEPTION_FLT_INEXACT_RESULT:
    case EXCEPTION_FLT_INVALID_OPERATION:
    case EXCEPTION_FLT_OVERFLOW:
    case EXCEPTION_FLT_STACK_CHECK:
    case EXCEPTION_FLT_UNDERFLOW:
    case EXCEPTION_ILLEGAL_INSTRUCTION:
    case EXCEPTION_IN_PAGE_ERROR:
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
    case EXCEPTION_INT_OVERFLOW:
    case EXCEPTION_INVALID_DISPOSITION:
    case EXCEPTION_NONCONTINUABLE_EXCEPTION:
    case EXCEPTION_PRIV_INSTRUCTION:
    case EXCEPTION_STACK_OVERFLOW:
        break;
    default:
        return EXCEPTION_CONTINUE_SEARCH;
    }

    if (InterlockedExchange(&g_kbo_hotkey_window_crashed, 1) == 0) {
        kbo_log_runtimef(
            "KBO F2 hub isolated crash: disabling hub thread to keep OOTP alive code=0x%08lx address=%p",
            (unsigned long)code,
            address);
    }

    InterlockedExchange(&g_kbo_webview_failed, 1);
    InterlockedExchange(&g_kbo_webview_ready, 0);
    InterlockedExchange(&g_kbo_webview_starting, 0);
    InterlockedExchange(&g_kbo_hotkey_window_started, 0);
    g_kbo_hotkey_window = NULL;
    g_kbo_hotkey_thread_id = 0;
    ExitThread(0);
    return EXCEPTION_CONTINUE_EXECUTION;
}

void kbo_hotkey_window_install_exception_guard(void)
{
    if (g_kbo_hotkey_window_exception_handler != NULL) {
        return;
    }

    g_kbo_hotkey_window_exception_handler =
        AddVectoredExceptionHandler(1, kbo_hotkey_window_exception_guard);
    if (g_kbo_hotkey_window_exception_handler == NULL) {
        kbo_log_runtime_line("KBO F2 hub exception guard install failed");
    } else {
        kbo_log_runtime_line("KBO F2 hub exception guard installed");
    }
}
