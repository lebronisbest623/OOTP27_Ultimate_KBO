#include "hotkey_window_runtime_window_internal.h"
#include "../../../core/core_flags/keys/runtime_flag_keys.generated.h"
#include "../../../core/dates/tick/current_date_tick_capture.h"

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

void kbo_layout_hotkey_window(HWND hwnd)
{
    if (hwnd == NULL) {
        return;
    }
    if (kbo_webview_is_failed()) {
        return;
    }
    kbo_start_webview_rights_ui(hwnd);
    kbo_webview_set_bounds(hwnd);
}

void kbo_refresh_hotkey_window_layout(HWND hwnd)
{
    if (hwnd == NULL) {
        return;
    }
    kbo_layout_hotkey_window(hwnd);
    kbo_refresh_hotkey_window();
    if (!kbo_webview_is_failed() && InterlockedCompareExchange(&g_kbo_webview_ready, 0, 0) != 0) {
        kbo_webview_navigate_current_immediate();
    }
    InvalidateRect(hwnd, NULL, TRUE);
}

DWORD WINAPI kbo_hotkey_window_thread(LPVOID parameter)
{
    if (g_kbo_hotkey_window_exception_handler == NULL) {
        g_kbo_hotkey_window_exception_handler =
            AddVectoredExceptionHandler(1, kbo_hotkey_window_exception_guard);
        if (g_kbo_hotkey_window_exception_handler == NULL) {
            kbo_log_runtime_line("KBO F2 hub exception guard install failed");
        } else {
            kbo_log_runtime_line("KBO F2 hub exception guard installed");
        }
    }

    g_kbo_hotkey_instance = (HINSTANCE)parameter;
    HRESULT co_hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (FAILED(co_hr)) {
        kbo_log_runtimef("KBO F2 hub COM init failed hr=0x%08lx", (unsigned long)co_hr);
        kbo_webview_mark_failed("hotkey_thread_com_init_failed", co_hr);
    }
    kbo_hub_init_gdi_objects();

    WNDCLASSEXA wc;
    memset(&wc, 0, sizeof(wc));
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = kbo_hotkey_window_proc;
    wc.hInstance     = g_kbo_hotkey_instance;
    wc.hCursor       = LoadCursorA(NULL, IDC_ARROW);
    wc.hbrBackground = g_kbo_hub_brush_bg;
    wc.lpszClassName = "OOTPKBOHubWindow";

    ATOM klass = RegisterClassExA(&wc);
    if (klass == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        kbo_log_runtimef("KBO F2 hub class registration failed error=%lu", GetLastError());
        kbo_hub_delete_gdi_objects();
        InterlockedExchange(&g_kbo_hotkey_window_started, 0);
        g_kbo_hotkey_thread_id = 0;
        if (SUCCEEDED(co_hr)) {
            CoUninitialize();
        }
        return 0;
    }

    HWND owner = kbo_find_ootp_main_window();
    RECT fixed_rect = kbo_hub_fixed_window_rect(owner);
    RECT saved_rect = {0, 0, 0, 0};
    int initial_x = CW_USEDEFAULT;
    int initial_y = CW_USEDEFAULT;
    int initial_width = fixed_rect.right - fixed_rect.left;
    int initial_height = fixed_rect.bottom - fixed_rect.top;
    if (kbo_hub_try_load_window_placement(owner, &saved_rect)) {
        initial_x = saved_rect.left;
        initial_y = saved_rect.top;
        initial_width = kbo_hub_rect_width(&saved_rect);
        initial_height = kbo_hub_rect_height(&saved_rect);
    }
    HWND hwnd = CreateWindowExA(
        KBO_HUB_WINDOW_EX_STYLE,
        wc.lpszClassName,
        "Ultimate KBO",
        KBO_HUB_WINDOW_STYLE,
        initial_x, initial_y,
        initial_width,
        initial_height,
        owner, NULL, g_kbo_hotkey_instance, NULL);

    if (hwnd == NULL) {
        kbo_log_runtimef("KBO F2 hub window creation failed error=%lu", GetLastError());
        kbo_hub_delete_gdi_objects();
        InterlockedExchange(&g_kbo_hotkey_window_started, 0);
        g_kbo_hotkey_thread_id = 0;
        if (SUCCEEDED(co_hr)) {
            CoUninitialize();
        }
        return 0;
    }

    g_kbo_hotkey_window = hwnd;
    g_kbo_hotkey_keyboard_hook = SetWindowsHookExA(
        WH_KEYBOARD_LL, kbo_hotkey_keyboard_proc, g_kbo_hotkey_instance, 0);
    if (g_kbo_hotkey_keyboard_hook == NULL) {
        kbo_log_runtimef("KBO F2 hub keyboard hook failed error=%lu; falling back to polling", GetLastError());
    } else {
        kbo_log_runtimef("KBO F2 hub keyboard hook installed hook=%p", (void*)g_kbo_hotkey_keyboard_hook);
    }
    kbo_log_runtimef("KBO F2 hub ready hwnd=%p owner=%p", (void*)hwnd, (void*)owner);

    MSG message;
    while (GetMessageA(&message, NULL, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageA(&message);
    }

    if (SUCCEEDED(co_hr)) {
        CoUninitialize();
    }
    return 0;
}

void start_kbo_hotkey_window_thread(HINSTANCE instance)
{
    if (!kbo_fix_enabled()) {
        return;
    }

    if (InterlockedCompareExchange(&g_kbo_hotkey_window_started, 1, 0) != 0) {
        return;
    }

    HANDLE thread = CreateThread(NULL, 0, kbo_hotkey_window_thread, instance, 0, &g_kbo_hotkey_thread_id);
    if (thread == NULL) {
        InterlockedExchange(&g_kbo_hotkey_window_started, 0);
        kbo_log_runtimef("KBO F2 hub thread failed error=%lu", GetLastError());
        return;
    }

    kbo_register_runtime_thread(thread, "F2 hub window");
}

