#include "hotkey_window_runtime_window_internal.h"
#include "../../../core/core_flags/keys/runtime_flag_keys.generated.h"
#include "../../../core/dates/tick/current_date_tick_capture.h"

static PVOID g_kbo_hotkey_window_exception_handler = NULL;
static volatile LONG g_kbo_hotkey_window_crashed = 0;
static volatile LONG g_kbo_hotkey_window_deferred_refresh = 0;
static volatile LONG g_kbo_hotkey_window_fast_sim_defer_log_count = 0;
static LONG g_kbo_hotkey_window_last_date_sequence = 0;
static ULONGLONG g_kbo_hotkey_window_last_date_sequence_ms = 0ull;
static ULONGLONG g_kbo_hotkey_window_fast_sim_defer_until_ms = 0ull;

static int kbo_hotkey_window_auto_refresh_deferred_by_fast_sim(const char* source)
{
    enum {
        KBO_HUB_FAST_SIM_SEQUENCE_WINDOW_MS = 750u,
        KBO_HUB_FAST_SIM_MAX_OBSERVATION_GAP_MS = 3000u,
        KBO_HUB_FAST_SIM_REFRESH_QUIET_MS = 1500u
    };

    if (read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_DISABLE_KBO_F2_FAST_SIM_REFRESH_DEFER_FILE)) {
        return 0;
    }

    ULONGLONG now_ms = GetTickCount64();
    LONG sequence = InterlockedCompareExchange(
        &g_kbo_current_date_tick_event_published_sequence,
        0,
        0);
    LONG last_sequence = g_kbo_hotkey_window_last_date_sequence;
    ULONGLONG last_ms = g_kbo_hotkey_window_last_date_sequence_ms;

    if (sequence != last_sequence) {
        LONG delta = sequence - last_sequence;
        ULONGLONG elapsed_ms = last_ms != 0ull ? now_ms - last_ms : 0ull;
        if (last_ms != 0ull
                && delta > 0
                && elapsed_ms <= KBO_HUB_FAST_SIM_MAX_OBSERVATION_GAP_MS
                && (delta >= 3 || elapsed_ms <= KBO_HUB_FAST_SIM_SEQUENCE_WINDOW_MS)) {
            g_kbo_hotkey_window_fast_sim_defer_until_ms =
                now_ms + KBO_HUB_FAST_SIM_REFRESH_QUIET_MS;
        }
        g_kbo_hotkey_window_last_date_sequence = sequence;
        g_kbo_hotkey_window_last_date_sequence_ms = now_ms;
    }

    ULONGLONG defer_until = g_kbo_hotkey_window_fast_sim_defer_until_ms;
    if (defer_until != 0ull && now_ms < defer_until) {
        LONG slot = InterlockedIncrement(&g_kbo_hotkey_window_fast_sim_defer_log_count);
        if (slot <= 40) {
            kbo_log_runtimef(
                "KBO F2 hub auto refresh deferred during fast sim source=%s sequence=%ld quiet_ms=%llu",
                source != NULL ? source : "",
                (long)sequence,
                (unsigned long long)(defer_until - now_ms));
        }
        return 1;
    }

    return 0;
}

static void kbo_hotkey_window_defer_auto_refresh(const char* source)
{
    InterlockedExchange(&g_kbo_hotkey_window_deferred_refresh, 1);
    (void)source;
}

static int kbo_hotkey_window_try_flush_deferred_refresh(HWND hwnd)
{
    if (InterlockedCompareExchange(&g_kbo_hotkey_window_deferred_refresh, 0, 0) == 0) {
        return 0;
    }
    if (hwnd == NULL || !IsWindow(hwnd) || !IsWindowVisible(hwnd)) {
        return 0;
    }
    if (kbo_hotkey_window_auto_refresh_deferred_by_fast_sim("deferred_refresh")) {
        return 0;
    }

    InterlockedExchange(&g_kbo_hotkey_window_deferred_refresh, 0);
    kbo_independent_acquisition_ui_invalidate_offer_cache();
    kbo_refresh_hotkey_window_layout(hwnd);
    kbo_log_runtimef("KBO F2 hub deferred refresh flushed hwnd=%p", (void*)hwnd);
    return 1;
}

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

LRESULT CALLBACK kbo_hotkey_window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message) {
    case WM_CREATE:
        kbo_hub_load_language_setting();
        kbo_hub_init_gdi_objects();
        kbo_hub_ensure_valid_selection();
        SetTimer(hwnd, 1u, 80u, NULL);
        kbo_hub_apply_fixed_window_placement(hwnd, 1);
        kbo_layout_hotkey_window(hwnd);
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        if (hdc != NULL) {
            RECT client;
            GetClientRect(hwnd, &client);
            FillRect(hdc, &client, g_kbo_hub_brush_bg);
            if (InterlockedCompareExchange(&g_kbo_webview_ready, 0, 0) == 0) {
                RECT text_rect = {22, 22, client.right - 22, 70};
                const char* status_text = InterlockedCompareExchange(&g_kbo_webview_failed, 0, 0) != 0
                    ? "KBO FRONT OFFICE HTML UI FAILED - CHECK runtime.ndjson"
                    : "KBO FRONT OFFICE HTML UI LOADING...";
                kbo_hub_draw_text(hdc, status_text, text_rect,
                    RGB(245, 241, 231), g_kbo_hub_font_title, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            }
        }
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_SIZE:
        if (wparam != SIZE_MINIMIZED) {
            kbo_hide_webview_player_tooltip_popup(0u);
            kbo_layout_hotkey_window(hwnd);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;

    case WM_EXITSIZEMOVE:
        kbo_hub_save_window_placement(hwnd);
        return 0;

    case WM_GETMINMAXINFO: {
        MINMAXINFO* info = (MINMAXINFO*)lparam;
        if (info != NULL) {
            SIZE min_track = kbo_hub_min_track_size();
            info->ptMinTrackSize.x = min_track.cx;
            info->ptMinTrackSize.y = min_track.cy;
        }
        return 0;
    }

    case WM_COMMAND:
        break;

    case WM_LBUTTONDOWN: {
        POINT point;
        point.x = (int)(short)LOWORD(lparam);
        point.y = (int)(short)HIWORD(lparam);
        if (PtInRect(&g_kbo_hub_league_dropdown_rect, point)) {
            kbo_hub_show_league_dropdown(hwnd);
            kbo_refresh_hotkey_window_layout(hwnd);
            return 0;
        }
        if (PtInRect(&g_kbo_hub_team_dropdown_rect, point)) {
            kbo_hub_show_team_dropdown(hwnd);
            kbo_refresh_hotkey_window_layout(hwnd);
            return 0;
        }
        if (PtInRect(&g_kbo_hub_lang_ko_rect, point)) {
            kbo_hub_set_language(KBO_HUB_LANG_KO);
            kbo_hub_save_language_setting();
            kbo_refresh_hotkey_window();
            InvalidateRect(hwnd, NULL, TRUE);
            return 0;
        }
        if (PtInRect(&g_kbo_hub_lang_en_rect, point)) {
            kbo_hub_set_language(KBO_HUB_LANG_EN);
            kbo_hub_save_language_setting();
            kbo_refresh_hotkey_window();
            InvalidateRect(hwnd, NULL, TRUE);
            return 0;
        }
        if (PtInRect(&g_kbo_hub_scrollbar_less_rect, point)) {
            if (g_kbo_hotkey_edit != NULL) {
                SendMessageA(g_kbo_hotkey_edit, EM_LINESCROLL, 0, -1);
                InvalidateRect(hwnd, &g_kbo_hub_scrollbar_rect, FALSE);
            }
            return 0;
        }
        if (PtInRect(&g_kbo_hub_scrollbar_more_rect, point)) {
            if (g_kbo_hotkey_edit != NULL) {
                SendMessageA(g_kbo_hotkey_edit, EM_LINESCROLL, 0, 1);
                InvalidateRect(hwnd, &g_kbo_hub_scrollbar_rect, FALSE);
            }
            return 0;
        }
        if (PtInRect(&g_kbo_hub_scrollbar_rect, point)) {
            if (g_kbo_hotkey_edit != NULL) {
                int direction = point.y < g_kbo_hub_scrollbar_thumb_rect.top
                    ? -kbo_hub_estimate_visible_edit_lines()
                    :  kbo_hub_estimate_visible_edit_lines();
                SendMessageA(g_kbo_hotkey_edit, EM_LINESCROLL, 0, direction);
                InvalidateRect(hwnd, &g_kbo_hub_scrollbar_rect, FALSE);
            }
            return 0;
        }
        RECT wnd_rect;
        GetClientRect(hwnd, &wnd_rect);
        int wnd_w = wnd_rect.right - wnd_rect.left;
        int wnd_h = wnd_rect.bottom - wnd_rect.top;
        for (int i = 0; i < KBO_HUB_NAV_COUNT; i++) {
            RECT item = kbo_hub_nav_item_rect(i, wnd_w, wnd_h);
            if (PtInRect(&item, point)) {
                g_kbo_hub_selected_view = i;
                kbo_refresh_hotkey_window_layout(hwnd);
                return 0;
            }
        }
        break;
    }

    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT: {
        HDC hdc = (HDC)wparam;
        SetTextColor(hdc, KBO_HUB_COLOR_TEXT);
        SetBkColor(hdc, KBO_HUB_COLOR_PANEL_ALT);
        return (LRESULT)g_kbo_hub_brush_panel_alt;
    }

    case WM_TIMER:
        if (wparam == 1u && !kbo_runtime_threads_should_continue()) {
            kbo_hub_save_window_placement(hwnd);
            DestroyWindow(hwnd);
            return 0;
        }
        if (wparam == 1u
                && (GetAsyncKeyState(VK_F2) & 1) != 0
                && kbo_foreground_is_this_process()) {
            kbo_queue_hotkey_window_toggle(KBO_HUB_MODE_RELEASE);
            return 0;
        }
        if (wparam == 1u
                && (GetAsyncKeyState(VK_F3) & 1) != 0
                && kbo_foreground_is_this_process()) {
            kbo_queue_hotkey_window_toggle(KBO_HUB_MODE_DEVELOPER);
            return 0;
        }
        if (wparam == 1u
                && (GetAsyncKeyState(VK_F5) & 1) != 0
                && g_kbo_hotkey_window != NULL
                && IsWindowVisible(g_kbo_hotkey_window)
                && kbo_foreground_is_this_process()) {
            kbo_independent_acquisition_ui_invalidate_offer_cache();
            kbo_refresh_hotkey_window();
            InvalidateRect(hwnd, NULL, TRUE);
            return 0;
        }
        if (wparam == 1u && g_kbo_hotkey_window != NULL && IsWindowVisible(g_kbo_hotkey_window)) {
            static uintptr_t s_last_db_ptr = 0;
            static ULONGLONG s_last_scrollbar_invalidate_ms = 0ull;
            uintptr_t cur_db = get_ootp_cached_global_database();
            if (kbo_hotkey_window_auto_refresh_deferred_by_fast_sim("timer")) {
                if (cur_db != s_last_db_ptr) {
                    s_last_db_ptr = cur_db;
                    kbo_hotkey_window_defer_auto_refresh("timer_db_changed");
                }
                return 0;
            }
            if (kbo_hotkey_window_try_flush_deferred_refresh(hwnd)) {
                return 0;
            }
            if (cur_db != s_last_db_ptr) {
                s_last_db_ptr = cur_db;
                kbo_independent_acquisition_ui_invalidate_offer_cache();
                kbo_refresh_hotkey_window();
                InvalidateRect(hwnd, NULL, TRUE);
            } else {
                ULONGLONG now_ms = GetTickCount64();
                if (now_ms - s_last_scrollbar_invalidate_ms >= 250ull) {
                    s_last_scrollbar_invalidate_ms = now_ms;
                    InvalidateRect(hwnd, &g_kbo_hub_scrollbar_rect, FALSE);
                }
            }
        }
        break;

    case KBO_WM_TOGGLE_SERVICE_MONITOR:
        kbo_show_or_hide_hotkey_window((int)wparam);
        return 0;

    case KBO_WM_REFRESH_HUB:
        if (kbo_hotkey_window_auto_refresh_deferred_by_fast_sim("refresh_request")) {
            kbo_hotkey_window_defer_auto_refresh("refresh_request");
            return 0;
        }
        kbo_independent_acquisition_ui_invalidate_offer_cache();
        kbo_refresh_hotkey_window_layout(hwnd);
        kbo_log_runtimef("KBO F2 hub refreshed by request hwnd=%p", (void*)hwnd);
        return 0;

    case KBO_WM_WEBVIEW_DISABLE:
        kbo_webview_shutdown_failed_surface();
        InvalidateRect(hwnd, NULL, TRUE);
        return 0;

    case KBO_WM_SHOW_HUB_CONTENT:
        if (kbo_webview_is_failed()) {
            kbo_webview_shutdown_failed_surface();
            InvalidateRect(hwnd, NULL, TRUE);
            return 0;
        }
        if (IsWindowVisible(hwnd)) {
            kbo_refresh_hotkey_window_layout(hwnd);
            kbo_log_runtimef("KBO F2 hub content loaded after loading screen hwnd=%p", (void*)hwnd);
        }
        return 0;

    case WM_CLOSE:
        kbo_hide_webview_player_tooltip_popup(0u);
        kbo_hub_save_window_placement(hwnd);
        ShowWindow(hwnd, SW_HIDE);
        return 0;

    case WM_DESTROY:
        kbo_destroy_webview_player_tooltip_popup();
        KillTimer(hwnd, 1u);
        if (g_kbo_hotkey_keyboard_hook != NULL) {
            UnhookWindowsHookEx(g_kbo_hotkey_keyboard_hook);
            g_kbo_hotkey_keyboard_hook = NULL;
        }
        g_kbo_hotkey_window         = NULL;
        g_kbo_hotkey_edit           = NULL;
        g_kbo_foreign_list          = NULL;
        g_kbo_foreign_keep_button   = NULL;
        g_kbo_foreign_release_button = NULL;
        g_kbo_hotkey_edit_original_proc = NULL;
        kbo_hub_delete_gdi_objects();
        InterlockedExchange(&g_kbo_hotkey_window_started, 0);
        g_kbo_hotkey_thread_id = 0;
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcA(hwnd, message, wparam, lparam);
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

