#include "hotkey_window_player_tooltip_internal.h"

HWND g_kbo_player_tooltip_hwnd = NULL;
ICoreWebView2Controller* g_kbo_player_tooltip_controller = NULL;
ICoreWebView2* g_kbo_player_tooltip_webview = NULL;
LONG g_kbo_player_tooltip_creating = 0;
HWND g_kbo_player_tooltip_owner = NULL;
int g_kbo_player_tooltip_plain_host = 0;
POINT g_kbo_player_tooltip_anchor = {0, 0};
int g_kbo_player_tooltip_width = KBO_PLAYER_TOOLTIP_WIDTH;
int g_kbo_player_tooltip_height = KBO_PLAYER_TOOLTIP_HEIGHT;
uint32_t g_kbo_player_tooltip_seq = 0u;
char g_kbo_player_tooltip_asset_folder[MAX_PATH];
char g_kbo_player_tooltip_pending_html[KBO_PLAYER_TOOLTIP_HTML_MAX];

static int kbo_tooltip_max_int(int a, int b)
{
    return a > b ? a : b;
}

static int kbo_tooltip_min_int(int a, int b)
{
    return a < b ? a : b;
}

WCHAR* kbo_tooltip_alloc_wide_from_utf8(const char* text)
{
    if (text == NULL || text[0] == '\0') {
        return NULL;
    }

    int wide_len = MultiByteToWideChar(CP_UTF8, 0, text, -1, NULL, 0);
    if (wide_len <= 0) {
        wide_len = MultiByteToWideChar(CP_ACP, 0, text, -1, NULL, 0);
        if (wide_len <= 0) {
            return NULL;
        }
    }

    WCHAR* wide = (WCHAR*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)wide_len * sizeof(WCHAR));
    if (wide == NULL) {
        return NULL;
    }

    int wrote = MultiByteToWideChar(CP_UTF8, 0, text, -1, wide, wide_len);
    if (wrote <= 0) {
        wrote = MultiByteToWideChar(CP_ACP, 0, text, -1, wide, wide_len);
    }
    if (wrote <= 0) {
        HeapFree(GetProcessHeap(), 0, wide);
        return NULL;
    }
    return wide;
}

void kbo_tooltip_apply_asset_mapping(void)
{
    if (g_kbo_player_tooltip_webview == NULL) {
        return;
    }

    ICoreWebView2_3* webview3 = NULL;
    HRESULT qi_hr = ICoreWebView2_QueryInterface(
        g_kbo_player_tooltip_webview,
        &IID_ICoreWebView2_3,
        (void**)&webview3);
    if (FAILED(qi_hr) || webview3 == NULL) {
        kbo_log_runtimef("KBO player tooltip asset mapping unavailable hr=0x%08lx", (unsigned long)qi_hr);
        return;
    }

    WCHAR* host_w = kbo_tooltip_alloc_wide_from_utf8(KBO_PLAYER_TOOLTIP_ASSET_HOST);
    if (host_w == NULL) {
        ICoreWebView2_3_Release(webview3);
        return;
    }

    HRESULT map_hr = S_OK;
    if (g_kbo_player_tooltip_asset_folder[0] == '\0') {
        map_hr = ICoreWebView2_3_ClearVirtualHostNameToFolderMapping(webview3, host_w);
    } else {
        WCHAR* folder_w = kbo_tooltip_alloc_wide_from_utf8(g_kbo_player_tooltip_asset_folder);
        if (folder_w == NULL) {
            HeapFree(GetProcessHeap(), 0, host_w);
            ICoreWebView2_3_Release(webview3);
            return;
        }
        map_hr = ICoreWebView2_3_SetVirtualHostNameToFolderMapping(
            webview3,
            host_w,
            folder_w,
            COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_ALLOW);
        HeapFree(GetProcessHeap(), 0, folder_w);
    }

    if (FAILED(map_hr)) {
        kbo_log_runtimef(
            "KBO player tooltip asset mapping failed hr=0x%08lx folder=%s",
            (unsigned long)map_hr,
            g_kbo_player_tooltip_asset_folder);
    }
    HeapFree(GetProcessHeap(), 0, host_w);
    ICoreWebView2_3_Release(webview3);
}

void kbo_set_webview_player_tooltip_asset_folder(const char* folder_path)
{
    char next[MAX_PATH] = {0};
    if (folder_path != NULL && folder_path[0] != '\0') {
        int written = snprintf(next, sizeof(next), "%s", folder_path);
        if (written <= 0 || (size_t)written >= sizeof(next)) {
            return;
        }
    }
    if (strcmp(g_kbo_player_tooltip_asset_folder, next) == 0) {
        return;
    }

    snprintf(g_kbo_player_tooltip_asset_folder, sizeof(g_kbo_player_tooltip_asset_folder), "%s", next);
    kbo_tooltip_apply_asset_mapping();
}

static RECT kbo_tooltip_rect_for_anchor(int width, int height)
{
    RECT work = {0, 0, 0, 0};
    HMONITOR monitor = MonitorFromPoint(g_kbo_player_tooltip_anchor, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info;
    memset(&info, 0, sizeof(info));
    info.cbSize = sizeof(info);
    if (monitor != NULL && GetMonitorInfoA(monitor, &info)) {
        work = info.rcWork;
    } else {
        work.left = 0;
        work.top = 0;
        work.right = GetSystemMetrics(SM_CXSCREEN);
        work.bottom = GetSystemMetrics(SM_CYSCREEN);
    }

    int max_width = kbo_tooltip_max_int(120, (work.right - work.left) - (KBO_PLAYER_TOOLTIP_MARGIN * 2));
    int max_height = kbo_tooltip_max_int(80, (work.bottom - work.top) - (KBO_PLAYER_TOOLTIP_MARGIN * 2));
    width = kbo_tooltip_min_int(kbo_tooltip_max_int(width, 120), max_width);
    height = kbo_tooltip_min_int(kbo_tooltip_max_int(height, 42), max_height);

    int x = g_kbo_player_tooltip_anchor.x;
    int y = g_kbo_player_tooltip_anchor.y;
    if (x + width > work.right - KBO_PLAYER_TOOLTIP_MARGIN) {
        x = work.right - width - KBO_PLAYER_TOOLTIP_MARGIN;
    }
    int available_below = (work.bottom - KBO_PLAYER_TOOLTIP_MARGIN) - y;
    if (available_below < height) {
        if (available_below >= 42) {
            height = available_below;
        } else {
            y = work.bottom - height - KBO_PLAYER_TOOLTIP_MARGIN;
        }
    }
    x = kbo_tooltip_max_int(work.left + KBO_PLAYER_TOOLTIP_MARGIN, x);
    y = kbo_tooltip_max_int(work.top + KBO_PLAYER_TOOLTIP_MARGIN, y);

    RECT rect = {x, y, x + width, y + height};
    return rect;
}

void kbo_tooltip_apply_bounds(int show)
{
    if (g_kbo_player_tooltip_hwnd == NULL) {
        return;
    }

    RECT rect = kbo_tooltip_rect_for_anchor(g_kbo_player_tooltip_width, g_kbo_player_tooltip_height);
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    RECT bounds = {0, 0, width, height};
    if (g_kbo_player_tooltip_controller != NULL) {
        ICoreWebView2Controller_put_Bounds(g_kbo_player_tooltip_controller, bounds);
        ICoreWebView2Controller_put_IsVisible(g_kbo_player_tooltip_controller, TRUE);
    }

    UINT flags = SWP_NOACTIVATE;
    if (show) {
        flags |= SWP_SHOWWINDOW;
    } else {
        flags |= SWP_NOZORDER;
    }
    SetWindowPos(
        g_kbo_player_tooltip_hwnd,
        show ? HWND_TOP : NULL,
        rect.left,
        rect.top,
        width,
        height,
        flags);
}

static LRESULT CALLBACK kbo_player_tooltip_window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    (void)wparam;
    (void)lparam;
    switch (message) {
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;
    case WM_NCHITTEST:
        return HTTRANSPARENT;
    case WM_CLOSE:
        ShowWindow(hwnd, SW_HIDE);
        return 0;
    case KBO_PLAYER_TOOLTIP_WM_START:
        (void)kbo_tooltip_start_controller();
        return 0;
    case WM_DESTROY:
        if (hwnd == g_kbo_player_tooltip_hwnd) {
            g_kbo_player_tooltip_hwnd = NULL;
            InterlockedExchange(&g_kbo_player_tooltip_creating, 0);
        }
        return 0;
    }
    return DefWindowProcA(hwnd, message, wparam, lparam);
}

static int kbo_tooltip_register_window_class(void)
{
    HINSTANCE instance = g_kbo_hotkey_instance != NULL ? g_kbo_hotkey_instance : GetModuleHandleA(NULL);
    WNDCLASSEXA wc;
    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = kbo_player_tooltip_window_proc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorA(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = KBO_PLAYER_TOOLTIP_CLASS_NAME;
    ATOM klass = RegisterClassExA(&wc);
    if (klass == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        kbo_log_runtimef("KBO player tooltip popup class registration failed error=%lu", GetLastError());
        return 0;
    }
    return 1;
}

int kbo_tooltip_ensure_window(HWND owner)
{
    if (g_kbo_player_tooltip_hwnd != NULL && IsWindow(g_kbo_player_tooltip_hwnd)) {
        if (owner != NULL) {
            SetWindowLongPtrA(g_kbo_player_tooltip_hwnd, GWLP_HWNDPARENT, (LONG_PTR)owner);
        }
        return 1;
    }
    if (!kbo_tooltip_register_window_class()) {
        return 0;
    }

    HINSTANCE instance = g_kbo_hotkey_instance != NULL ? g_kbo_hotkey_instance : GetModuleHandleA(NULL);
    RECT rect = kbo_tooltip_rect_for_anchor(KBO_PLAYER_TOOLTIP_WIDTH, KBO_PLAYER_TOOLTIP_HEIGHT);
    DWORD ex_style = WS_EX_TOOLWINDOW;
    if (!g_kbo_player_tooltip_plain_host) {
        ex_style |= WS_EX_NOACTIVATE;
    }
    g_kbo_player_tooltip_hwnd = CreateWindowExA(
        ex_style,
        KBO_PLAYER_TOOLTIP_CLASS_NAME,
        "",
        WS_POPUP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        rect.left,
        rect.top,
        rect.right - rect.left,
        rect.bottom - rect.top,
        owner,
        NULL,
        instance,
        NULL);
    if (g_kbo_player_tooltip_hwnd == NULL) {
        kbo_log_runtimef("KBO player tooltip popup window creation failed error=%lu", GetLastError());
        return 0;
    }
    kbo_log_runtimef(
        "KBO player tooltip popup window ready hwnd=%p owner=%p ex=0x%08lx plain=%d",
        (void*)g_kbo_player_tooltip_hwnd,
        (void*)owner,
        (unsigned long)ex_style,
        g_kbo_player_tooltip_plain_host);
    return 1;
}

int kbo_show_webview_player_tooltip_popup(HWND owner, int screen_x, int screen_y, uint32_t hover_seq, const char* html)
{
    if (html == NULL || html[0] == '\0') {
        return 0;
    }

    g_kbo_player_tooltip_owner = owner;
    g_kbo_player_tooltip_anchor.x = screen_x;
    g_kbo_player_tooltip_anchor.y = screen_y;
    g_kbo_player_tooltip_width = KBO_PLAYER_TOOLTIP_WIDTH;
    g_kbo_player_tooltip_height = KBO_PLAYER_TOOLTIP_HEIGHT;
    g_kbo_player_tooltip_seq = hover_seq;
    int written = snprintf(g_kbo_player_tooltip_pending_html, sizeof(g_kbo_player_tooltip_pending_html), "%s", html);
    if (written <= 0 || (size_t)written >= sizeof(g_kbo_player_tooltip_pending_html)) {
        g_kbo_player_tooltip_pending_html[0] = '\0';
        kbo_log_runtimef("KBO player tooltip popup skipped reason=html_too_large bytes=%d", written);
        return 0;
    }

    if (!kbo_tooltip_ensure_window(owner)) {
        return 0;
    }
    kbo_tooltip_apply_bounds(0);

    if (g_kbo_player_tooltip_webview != NULL) {
        kbo_tooltip_navigate_pending();
        return 1;
    }

    return kbo_tooltip_schedule_controller_start();
}

void kbo_hide_webview_player_tooltip_popup(uint32_t hover_seq)
{
    if (hover_seq != 0u && hover_seq != g_kbo_player_tooltip_seq) {
        return;
    }
    g_kbo_player_tooltip_pending_html[0] = '\0';
    if (g_kbo_player_tooltip_hwnd != NULL) {
        ShowWindow(g_kbo_player_tooltip_hwnd, SW_HIDE);
    }
}

void kbo_destroy_webview_player_tooltip_popup(void)
{
    g_kbo_player_tooltip_pending_html[0] = '\0';
    g_kbo_player_tooltip_owner = NULL;
    g_kbo_player_tooltip_plain_host = 0;
    InterlockedExchange(&g_kbo_player_tooltip_creating, 0);
    if (g_kbo_player_tooltip_webview != NULL) {
        ICoreWebView2_Release(g_kbo_player_tooltip_webview);
        g_kbo_player_tooltip_webview = NULL;
    }
    if (g_kbo_player_tooltip_controller != NULL) {
        ICoreWebView2Controller_Close(g_kbo_player_tooltip_controller);
        ICoreWebView2Controller_Release(g_kbo_player_tooltip_controller);
        g_kbo_player_tooltip_controller = NULL;
    }
    if (g_kbo_player_tooltip_hwnd != NULL) {
        DestroyWindow(g_kbo_player_tooltip_hwnd);
        g_kbo_player_tooltip_hwnd = NULL;
    }
}
