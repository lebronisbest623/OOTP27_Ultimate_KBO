#include "hotkey_window_runtime_window_internal.h"

#define KBO_HUB_WINDOW_STYLE (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_THICKFRAME)
#define KBO_HUB_WINDOW_EX_STYLE (WS_EX_TOOLWINDOW | WS_EX_WINDOWEDGE)
#define KBO_HUB_WINDOW_PLACEMENT_FILE "hub_window_placement.txt"

void kbo_layout_hotkey_window(HWND hwnd)
{
    if (hwnd == NULL) {
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
    if (InterlockedCompareExchange(&g_kbo_webview_ready, 0, 0) != 0) {
        kbo_webview_navigate_current_immediate();
    }
    InvalidateRect(hwnd, NULL, TRUE);
}

void kbo_hub_get_work_area(HWND hwnd, RECT* out)
{
    if (out == NULL) {
        return;
    }
    HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info;
    memset(&info, 0, sizeof(info));
    info.cbSize = sizeof(info);
    if (monitor != NULL && GetMonitorInfoA(monitor, &info)) {
        *out = info.rcWork;
        return;
    }
    SystemParametersInfoA(SPI_GETWORKAREA, 0, out, 0);
}

RECT kbo_hub_fixed_window_rect(HWND hwnd)
{
    RECT work = {0, 0, 0, 0};
    kbo_hub_get_work_area(hwnd, &work);
    int max_client_w = (work.right > work.left) ? (work.right - work.left - 48) : KBO_HUB_FIXED_CLIENT_WIDTH;
    int max_client_h = (work.bottom > work.top) ? (work.bottom - work.top - 72) : KBO_HUB_FIXED_CLIENT_HEIGHT;
    int client_w = KBO_HUB_FIXED_CLIENT_WIDTH;
    int client_h = KBO_HUB_FIXED_CLIENT_HEIGHT;
    if (max_client_w > 0 && client_w > max_client_w) { client_w = max_client_w; }
    if (max_client_h > 0 && client_h > max_client_h) { client_h = max_client_h; }
    if (client_w < KBO_HUB_MIN_CLIENT_WIDTH) { client_w = KBO_HUB_MIN_CLIENT_WIDTH; }
    if (client_h < KBO_HUB_MIN_CLIENT_HEIGHT) { client_h = KBO_HUB_MIN_CLIENT_HEIGHT; }
    RECT rect = {0, 0, client_w, client_h};
    AdjustWindowRectEx(&rect, KBO_HUB_WINDOW_STYLE, FALSE, KBO_HUB_WINDOW_EX_STYLE);
    return rect;
}

static SIZE kbo_hub_min_track_size(void)
{
    RECT rect = {0, 0, KBO_HUB_MIN_CLIENT_WIDTH, KBO_HUB_MIN_CLIENT_HEIGHT};
    AdjustWindowRectEx(&rect, KBO_HUB_WINDOW_STYLE, FALSE, KBO_HUB_WINDOW_EX_STYLE);
    SIZE size = {rect.right - rect.left, rect.bottom - rect.top};
    return size;
}

static int kbo_hub_rect_width(const RECT* rect)
{
    return rect != NULL ? (int)(rect->right - rect->left) : 0;
}

static int kbo_hub_rect_height(const RECT* rect)
{
    return rect != NULL ? (int)(rect->bottom - rect->top) : 0;
}

static void kbo_hub_clamp_rect_to_work_area(HWND hwnd, RECT* rect)
{
    if (rect == NULL) {
        return;
    }

    RECT work = {0, 0, 0, 0};
    HMONITOR monitor = !IsRectEmpty(rect) ? MonitorFromRect(rect, MONITOR_DEFAULTTONEAREST) : NULL;
    if (monitor != NULL) {
        MONITORINFO info;
        memset(&info, 0, sizeof(info));
        info.cbSize = sizeof(info);
        if (GetMonitorInfoA(monitor, &info)) {
            work = info.rcWork;
        }
    }
    if (work.right <= work.left || work.bottom <= work.top) {
        kbo_hub_get_work_area(hwnd, &work);
    }

    SIZE min_track = kbo_hub_min_track_size();
    int width = kbo_hub_rect_width(rect);
    int height = kbo_hub_rect_height(rect);
    int work_width = work.right - work.left;
    int work_height = work.bottom - work.top;

    if (width < min_track.cx) {
        width = min_track.cx;
    }
    if (height < min_track.cy) {
        height = min_track.cy;
    }
    if (work_width > 0 && width > work_width) {
        width = work_width;
    }
    if (work_height > 0 && height > work_height) {
        height = work_height;
    }

    if (rect->left + width > work.right) {
        rect->left = work.right - width;
    }
    if (rect->top + height > work.bottom) {
        rect->top = work.bottom - height;
    }
    if (rect->left < work.left) {
        rect->left = work.left;
    }
    if (rect->top < work.top) {
        rect->top = work.top;
    }

    rect->right = rect->left + width;
    rect->bottom = rect->top + height;
}

static int kbo_hub_read_window_placement_file(RECT* out_rect)
{
    if (out_rect == NULL) {
        return 0;
    }

    char path[MAX_PATH];
    if (!kbo_get_global_data_file(KBO_HUB_WINDOW_PLACEMENT_FILE, path, sizeof(path))) {
        kbo_log_runtimef("KBO F2 hub placement load skipped reason=path");
        return 0;
    }

    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        if (error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND) {
            kbo_log_runtimef("KBO F2 hub placement load failed open error=%lu path=%s", error, path);
        }
        return 0;
    }

    char buffer[128];
    DWORD bytes_read = 0;
    memset(buffer, 0, sizeof(buffer));
    if (!ReadFile(file, buffer, (DWORD)(sizeof(buffer) - 1), &bytes_read, NULL)) {
        DWORD error = GetLastError();
        CloseHandle(file);
        kbo_log_runtimef("KBO F2 hub placement load failed read error=%lu path=%s", error, path);
        return 0;
    }
    CloseHandle(file);
    buffer[bytes_read] = '\0';

    long left = 0;
    long top = 0;
    long right = 0;
    long bottom = 0;
    if (sscanf(buffer, "left=%ld top=%ld right=%ld bottom=%ld", &left, &top, &right, &bottom) != 4
            && sscanf(buffer, "%ld %ld %ld %ld", &left, &top, &right, &bottom) != 4) {
        kbo_log_runtimef("KBO F2 hub placement load ignored reason=parse path=%s", path);
        return 0;
    }

    out_rect->left = (LONG)left;
    out_rect->top = (LONG)top;
    out_rect->right = (LONG)right;
    out_rect->bottom = (LONG)bottom;
    if (kbo_hub_rect_width(out_rect) <= 0 || kbo_hub_rect_height(out_rect) <= 0) {
        kbo_log_runtimef("KBO F2 hub placement load ignored reason=invalid_rect path=%s", path);
        return 0;
    }

    kbo_log_runtimef("KBO F2 hub placement loaded rect=%ld,%ld,%ld,%ld path=%s",
        (long)out_rect->left, (long)out_rect->top, (long)out_rect->right, (long)out_rect->bottom, path);
    return 1;
}

static int kbo_hub_try_load_window_placement(HWND hwnd, RECT* out_rect)
{
    if (!kbo_hub_read_window_placement_file(out_rect)) {
        return 0;
    }
    kbo_hub_clamp_rect_to_work_area(hwnd, out_rect);
    return 1;
}

void kbo_hub_save_window_placement(HWND hwnd)
{
    if (hwnd == NULL || IsIconic(hwnd)) {
        return;
    }

    char path[MAX_PATH];
    if (!kbo_get_global_data_file(KBO_HUB_WINDOW_PLACEMENT_FILE, path, sizeof(path))) {
        kbo_log_runtimef("KBO F2 hub placement save skipped reason=path");
        return;
    }

    WINDOWPLACEMENT placement;
    memset(&placement, 0, sizeof(placement));
    placement.length = sizeof(placement);

    RECT rect = {0, 0, 0, 0};
    if (GetWindowPlacement(hwnd, &placement)) {
        rect = placement.rcNormalPosition;
    } else if (!GetWindowRect(hwnd, &rect)) {
        kbo_log_runtimef("KBO F2 hub placement save failed rect error=%lu path=%s", GetLastError(), path);
        return;
    }

    if (kbo_hub_rect_width(&rect) <= 0 || kbo_hub_rect_height(&rect) <= 0) {
        kbo_log_runtimef("KBO F2 hub placement save skipped reason=invalid_rect");
        return;
    }

    kbo_hub_clamp_rect_to_work_area(hwnd, &rect);

    char buffer[128];
    int length = snprintf(buffer, sizeof(buffer), "left=%ld top=%ld right=%ld bottom=%ld\r\n",
        (long)rect.left, (long)rect.top, (long)rect.right, (long)rect.bottom);
    if (length <= 0 || length >= (int)sizeof(buffer)) {
        kbo_log_runtimef("KBO F2 hub placement save skipped reason=overflow");
        return;
    }

    HANDLE file = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        kbo_log_runtimef("KBO F2 hub placement save failed open error=%lu path=%s", GetLastError(), path);
        return;
    }

    DWORD bytes_written = 0;
    if (!WriteFile(file, buffer, (DWORD)length, &bytes_written, NULL) || bytes_written != (DWORD)length) {
        DWORD error = GetLastError();
        CloseHandle(file);
        kbo_log_runtimef("KBO F2 hub placement save failed write error=%lu path=%s", error, path);
        return;
    }
    CloseHandle(file);

    kbo_log_runtimef("KBO F2 hub placement saved rect=%ld,%ld,%ld,%ld path=%s",
        (long)rect.left, (long)rect.top, (long)rect.right, (long)rect.bottom, path);
}

void kbo_hub_apply_fixed_window_placement(HWND hwnd, int preserve_position)
{
    if (hwnd == NULL) {
        return;
    }

    RECT rect = {0, 0, 0, 0};
    if (!kbo_hub_try_load_window_placement(hwnd, &rect)) {
        rect = kbo_hub_fixed_window_rect(hwnd);
        if (preserve_position) {
            RECT current = {0, 0, 0, 0};
            if (GetWindowRect(hwnd, &current)) {
                int width = kbo_hub_rect_width(&rect);
                int height = kbo_hub_rect_height(&rect);
                rect.left = current.left;
                rect.top = current.top;
                rect.right = rect.left + width;
                rect.bottom = rect.top + height;
            }
        } else {
            RECT work = {0, 0, 0, 0};
            kbo_hub_get_work_area(hwnd, &work);
            int width = kbo_hub_rect_width(&rect);
            int height = kbo_hub_rect_height(&rect);
            if (work.right > work.left && work.bottom > work.top) {
                rect.left = work.left + ((work.right - work.left) - width) / 2;
                rect.top = work.top + ((work.bottom - work.top) - height) / 2;
                rect.right = rect.left + width;
                rect.bottom = rect.top + height;
            }
        }
        kbo_hub_clamp_rect_to_work_area(hwnd, &rect);
    }

    SetWindowPos(
        hwnd,
        NULL,
        rect.left,
        rect.top,
        kbo_hub_rect_width(&rect),
        kbo_hub_rect_height(&rect),
        SWP_NOZORDER | SWP_NOACTIVATE);
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
        kbo_layout_hotkey_window(hwnd);
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
            uintptr_t cur_db = get_ootp_cached_global_database();
            if (cur_db != s_last_db_ptr) {
                s_last_db_ptr = cur_db;
                kbo_independent_acquisition_ui_invalidate_offer_cache();
                kbo_refresh_hotkey_window();
                InvalidateRect(hwnd, NULL, TRUE);
            } else {
                InvalidateRect(hwnd, &g_kbo_hub_scrollbar_rect, FALSE);
            }
        }
        break;

    case KBO_WM_TOGGLE_SERVICE_MONITOR:
        kbo_show_or_hide_hotkey_window((int)wparam);
        return 0;

    case KBO_WM_REFRESH_HUB:
        kbo_independent_acquisition_ui_invalidate_offer_cache();
        kbo_refresh_hotkey_window_layout(hwnd);
        kbo_log_runtimef("KBO F2 hub refreshed by request hwnd=%p", (void*)hwnd);
        return 0;

    case KBO_WM_SHOW_HUB_CONTENT:
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
        return 0;
    }

    return DefWindowProcA(hwnd, message, wparam, lparam);
}

DWORD WINAPI kbo_hotkey_window_thread(LPVOID parameter)
{
    g_kbo_hotkey_instance = (HINSTANCE)parameter;
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
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

    CoUninitialize();
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

