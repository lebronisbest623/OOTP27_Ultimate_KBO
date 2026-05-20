#include "../hotkey_window_webview.h"

#define KBO_PLAYER_TOOLTIP_CLASS_NAME "OOTPKBOPlayerTooltipWindow"
#define KBO_PLAYER_TOOLTIP_HTML_MAX   (128u * 1024u)
#define KBO_PLAYER_TOOLTIP_WIDTH      430
#define KBO_PLAYER_TOOLTIP_HEIGHT     252
#define KBO_PLAYER_TOOLTIP_MARGIN     8
#define KBO_PLAYER_TOOLTIP_WM_START   (WM_APP + 0x53au)

typedef struct KboPlayerTooltipControllerHandler {
    ICoreWebView2CreateCoreWebView2ControllerCompletedHandler iface;
    LONG ref;
} KboPlayerTooltipControllerHandler;

typedef struct KboPlayerTooltipNavHandler {
    ICoreWebView2NavigationStartingEventHandler iface;
    LONG ref;
} KboPlayerTooltipNavHandler;

static HWND g_kbo_player_tooltip_hwnd = NULL;
static ICoreWebView2Controller* g_kbo_player_tooltip_controller = NULL;
static ICoreWebView2* g_kbo_player_tooltip_webview = NULL;
static EventRegistrationToken g_kbo_player_tooltip_nav_token = {0};
static LONG g_kbo_player_tooltip_creating = 0;
static HWND g_kbo_player_tooltip_owner = NULL;
static int g_kbo_player_tooltip_plain_host = 0;
static POINT g_kbo_player_tooltip_anchor = {0, 0};
static int g_kbo_player_tooltip_width = KBO_PLAYER_TOOLTIP_WIDTH;
static int g_kbo_player_tooltip_height = KBO_PLAYER_TOOLTIP_HEIGHT;
static uint32_t g_kbo_player_tooltip_seq = 0u;
static char g_kbo_player_tooltip_asset_folder[MAX_PATH];
static char g_kbo_player_tooltip_pending_html[KBO_PLAYER_TOOLTIP_HTML_MAX];

static int kbo_tooltip_start_controller(void);
static int kbo_tooltip_schedule_controller_start(void);

static int kbo_tooltip_max_int(int a, int b)
{
    return a > b ? a : b;
}

static int kbo_tooltip_min_int(int a, int b)
{
    return a < b ? a : b;
}

static void kbo_tooltip_copy_wide_utf8(LPCWSTR value, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';
    if (value == NULL || value[0] == L'\0') {
        return;
    }
    WideCharToMultiByte(CP_UTF8, 0, value, -1, out, (int)out_size, NULL, NULL);
    out[out_size - 1u] = '\0';
}

static WCHAR* kbo_tooltip_alloc_wide_from_utf8(const char* text)
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

static void kbo_tooltip_apply_asset_mapping(void)
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

static int kbo_tooltip_parse_u32_segment(const char** cursor, uint32_t* out)
{
    if (cursor == NULL || *cursor == NULL || out == NULL) {
        return 0;
    }

    char* end = NULL;
    unsigned long value = strtoul(*cursor, &end, 10);
    if (end == *cursor || value > 0xfffffffful) {
        return 0;
    }

    *out = (uint32_t)value;
    *cursor = end;
    if (**cursor == '/') {
        *cursor += 1;
    }
    return 1;
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

static void kbo_tooltip_apply_bounds(int show)
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

static int kbo_tooltip_ensure_window(HWND owner)
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

static void kbo_tooltip_apply_settings(void)
{
    if (g_kbo_player_tooltip_webview != NULL) {
        ICoreWebView2Settings* settings = NULL;
        if (SUCCEEDED(ICoreWebView2_get_Settings(g_kbo_player_tooltip_webview, &settings)) && settings != NULL) {
            ICoreWebView2Settings_put_IsStatusBarEnabled(settings, FALSE);
            ICoreWebView2Settings_put_AreDefaultContextMenusEnabled(settings, FALSE);
            ICoreWebView2Settings_put_AreDevToolsEnabled(settings, FALSE);
            ICoreWebView2Settings_put_IsZoomControlEnabled(settings, FALSE);
            ICoreWebView2Settings_Release(settings);
        }
    }

    if (g_kbo_player_tooltip_controller != NULL) {
        ICoreWebView2Controller2* controller2 = NULL;
        if (SUCCEEDED(ICoreWebView2Controller_QueryInterface(
                g_kbo_player_tooltip_controller,
                &IID_ICoreWebView2Controller2,
                (void**)&controller2))
                && controller2 != NULL) {
            COREWEBVIEW2_COLOR transparent = {0, 0, 0, 0};
            ICoreWebView2Controller2_put_DefaultBackgroundColor(controller2, transparent);
            ICoreWebView2Controller2_Release(controller2);
        }
    }
}

static void kbo_tooltip_navigate_pending(void)
{
    if (g_kbo_player_tooltip_webview == NULL || g_kbo_player_tooltip_pending_html[0] == '\0') {
        return;
    }

    WCHAR* wide = kbo_tooltip_alloc_wide_from_utf8(g_kbo_player_tooltip_pending_html);
    if (wide == NULL) {
        return;
    }

    kbo_tooltip_apply_bounds(1);
    HRESULT hr = ICoreWebView2_NavigateToString(g_kbo_player_tooltip_webview, wide);
    HeapFree(GetProcessHeap(), 0, wide);
    if (FAILED(hr)) {
        ShowWindow(g_kbo_player_tooltip_hwnd, SW_HIDE);
        kbo_log_runtimef("KBO player tooltip popup navigation failed hr=0x%08lx", (unsigned long)hr);
    }
}

static int kbo_tooltip_handle_resize_uri(const char* uri)
{
    const char* prefix = "kbo-tooltip://resize/";
    size_t prefix_len = strlen(prefix);
    if (uri == NULL || strncmp(uri, prefix, prefix_len) != 0) {
        return 0;
    }

    const char* cursor = uri + prefix_len;
    uint32_t seq = 0u;
    uint32_t width = 0u;
    uint32_t height = 0u;
    if (!kbo_tooltip_parse_u32_segment(&cursor, &seq)
            || !kbo_tooltip_parse_u32_segment(&cursor, &width)
            || !kbo_tooltip_parse_u32_segment(&cursor, &height)) {
        return 1;
    }
    if (seq != 0u && seq != g_kbo_player_tooltip_seq) {
        return 1;
    }

    g_kbo_player_tooltip_width = (int)width;
    g_kbo_player_tooltip_height = (int)height;
    kbo_tooltip_apply_bounds(1);

    uint32_t player_id = 0u;
    uint32_t ratings = 0u;
    uint32_t bars = 0u;
    uint32_t fills = 0u;
    uint32_t scale = 0u;
    uint32_t first_pct = 0u;
    (void)kbo_tooltip_parse_u32_segment(&cursor, &player_id);
    (void)kbo_tooltip_parse_u32_segment(&cursor, &ratings);
    (void)kbo_tooltip_parse_u32_segment(&cursor, &bars);
    (void)kbo_tooltip_parse_u32_segment(&cursor, &fills);
    (void)kbo_tooltip_parse_u32_segment(&cursor, &scale);
    (void)kbo_tooltip_parse_u32_segment(&cursor, &first_pct);
    if (player_id != 0u) {
        kbo_log_runtimef(
            "KBO player tooltip popup resize seq=%u player=%u size=%ux%u ratings=%u bars=%u fills=%u scale=%u first_pct=%u",
            seq,
            player_id,
            width,
            height,
            ratings,
            bars,
            fills,
            scale,
            first_pct);
    }
    return 1;
}

static HRESULT STDMETHODCALLTYPE kbo_tooltip_nav_qi(
    ICoreWebView2NavigationStartingEventHandler* This,
    REFIID riid,
    void** ppv)
{
    if (ppv == NULL) { return E_POINTER; }
    *ppv = NULL;
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_ICoreWebView2NavigationStartingEventHandler)) {
        *ppv = This;
        ICoreWebView2NavigationStartingEventHandler_AddRef(This);
        return S_OK;
    }
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE kbo_tooltip_nav_addref(ICoreWebView2NavigationStartingEventHandler* This)
{
    return (ULONG)InterlockedIncrement(&((KboPlayerTooltipNavHandler*)This)->ref);
}

static ULONG STDMETHODCALLTYPE kbo_tooltip_nav_release(ICoreWebView2NavigationStartingEventHandler* This)
{
    LONG value = InterlockedDecrement(&((KboPlayerTooltipNavHandler*)This)->ref);
    if (value < 1) { ((KboPlayerTooltipNavHandler*)This)->ref = 1; }
    return (ULONG)((KboPlayerTooltipNavHandler*)This)->ref;
}

static HRESULT STDMETHODCALLTYPE kbo_tooltip_nav_invoke(
    ICoreWebView2NavigationStartingEventHandler* This,
    ICoreWebView2* sender,
    ICoreWebView2NavigationStartingEventArgs* args)
{
    (void)This;
    (void)sender;
    LPWSTR uri_w = NULL;
    if (args == NULL || FAILED(ICoreWebView2NavigationStartingEventArgs_get_Uri(args, &uri_w)) || uri_w == NULL) {
        return S_OK;
    }
    char uri[512] = {0};
    kbo_tooltip_copy_wide_utf8(uri_w, uri, sizeof(uri));
    CoTaskMemFree(uri_w);
    if (kbo_tooltip_handle_resize_uri(uri)) {
        ICoreWebView2NavigationStartingEventArgs_put_Cancel(args, TRUE);
    }
    return S_OK;
}

static ICoreWebView2NavigationStartingEventHandlerVtbl g_kbo_tooltip_nav_vtbl = {
    kbo_tooltip_nav_qi,
    kbo_tooltip_nav_addref,
    kbo_tooltip_nav_release,
    kbo_tooltip_nav_invoke
};

static KboPlayerTooltipNavHandler g_kbo_tooltip_nav_handler = {
    { &g_kbo_tooltip_nav_vtbl },
    1
};

static HRESULT STDMETHODCALLTYPE kbo_tooltip_controller_qi(
    ICoreWebView2CreateCoreWebView2ControllerCompletedHandler* This,
    REFIID riid,
    void** ppv)
{
    if (ppv == NULL) { return E_POINTER; }
    *ppv = NULL;
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_ICoreWebView2CreateCoreWebView2ControllerCompletedHandler)) {
        *ppv = This;
        ICoreWebView2CreateCoreWebView2ControllerCompletedHandler_AddRef(This);
        return S_OK;
    }
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE kbo_tooltip_controller_addref(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler* This)
{
    return (ULONG)InterlockedIncrement(&((KboPlayerTooltipControllerHandler*)This)->ref);
}

static ULONG STDMETHODCALLTYPE kbo_tooltip_controller_release(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler* This)
{
    LONG value = InterlockedDecrement(&((KboPlayerTooltipControllerHandler*)This)->ref);
    if (value < 1) { ((KboPlayerTooltipControllerHandler*)This)->ref = 1; }
    return (ULONG)((KboPlayerTooltipControllerHandler*)This)->ref;
}

static HRESULT STDMETHODCALLTYPE kbo_tooltip_controller_invoke(
    ICoreWebView2CreateCoreWebView2ControllerCompletedHandler* This,
    HRESULT errorCode,
    ICoreWebView2Controller* result)
{
    (void)This;
    InterlockedExchange(&g_kbo_player_tooltip_creating, 0);
    if (FAILED(errorCode) || result == NULL) {
        kbo_log_runtimef("KBO player tooltip WebView controller create failed hr=0x%08lx", (unsigned long)errorCode);
        if (!g_kbo_player_tooltip_plain_host && g_kbo_player_tooltip_hwnd != NULL) {
            g_kbo_player_tooltip_plain_host = 1;
            DestroyWindow(g_kbo_player_tooltip_hwnd);
            g_kbo_player_tooltip_hwnd = NULL;
            if (kbo_tooltip_ensure_window(g_kbo_player_tooltip_owner)) {
                kbo_log_runtime_line("KBO player tooltip retrying WebView controller with plain host window");
                (void)kbo_tooltip_schedule_controller_start();
            }
        } else if (g_kbo_player_tooltip_hwnd != NULL) {
            ShowWindow(g_kbo_player_tooltip_hwnd, SW_HIDE);
        }
        return S_OK;
    }

    g_kbo_player_tooltip_controller = result;
    ICoreWebView2Controller_AddRef(g_kbo_player_tooltip_controller);
    HRESULT core_hr = ICoreWebView2Controller_get_CoreWebView2(
        g_kbo_player_tooltip_controller,
        &g_kbo_player_tooltip_webview);
    if (g_kbo_player_tooltip_webview != NULL) {
        kbo_tooltip_apply_settings();
        HRESULT nav_hr = ICoreWebView2_add_NavigationStarting(
            g_kbo_player_tooltip_webview,
            &g_kbo_tooltip_nav_handler.iface,
            &g_kbo_player_tooltip_nav_token);
        kbo_tooltip_apply_asset_mapping();
        kbo_log_runtimef(
            "KBO player tooltip WebView ready hwnd=%p controller=%p core=%p hr_core=0x%08lx hr_nav=0x%08lx",
            (void*)g_kbo_player_tooltip_hwnd,
            (void*)g_kbo_player_tooltip_controller,
            (void*)g_kbo_player_tooltip_webview,
            (unsigned long)core_hr,
            (unsigned long)nav_hr);
        kbo_tooltip_navigate_pending();
    } else {
        kbo_log_runtimef("KBO player tooltip WebView core unavailable hr=0x%08lx", (unsigned long)core_hr);
    }
    return S_OK;
}

static ICoreWebView2CreateCoreWebView2ControllerCompletedHandlerVtbl g_kbo_tooltip_controller_vtbl = {
    kbo_tooltip_controller_qi,
    kbo_tooltip_controller_addref,
    kbo_tooltip_controller_release,
    kbo_tooltip_controller_invoke
};

static KboPlayerTooltipControllerHandler g_kbo_tooltip_controller_handler = {
    { &g_kbo_tooltip_controller_vtbl },
    1
};

static int kbo_tooltip_start_controller(void)
{
    if (g_kbo_webview_environment == NULL || g_kbo_player_tooltip_hwnd == NULL) {
        return 0;
    }
    if (g_kbo_player_tooltip_pending_html[0] == '\0') {
        return 0;
    }
    if (g_kbo_player_tooltip_controller != NULL) {
        return 1;
    }
    if (InterlockedCompareExchange(&g_kbo_player_tooltip_creating, 1, 0) != 0) {
        return 1;
    }

    kbo_tooltip_apply_bounds(1);
    HRESULT hr = ICoreWebView2Environment_CreateCoreWebView2Controller(
        g_kbo_webview_environment,
        g_kbo_player_tooltip_hwnd,
        &g_kbo_tooltip_controller_handler.iface);
    if (FAILED(hr)) {
        InterlockedExchange(&g_kbo_player_tooltip_creating, 0);
        ShowWindow(g_kbo_player_tooltip_hwnd, SW_HIDE);
        kbo_log_runtimef("KBO player tooltip WebView controller start failed hr=0x%08lx", (unsigned long)hr);
        return 0;
    }
    kbo_log_runtimef(
        "KBO player tooltip WebView controller creation requested hwnd=%p hr=0x%08lx visible=%d",
        (void*)g_kbo_player_tooltip_hwnd,
        (unsigned long)hr,
        IsWindowVisible(g_kbo_player_tooltip_hwnd) ? 1 : 0);
    return 1;
}

static int kbo_tooltip_schedule_controller_start(void)
{
    if (g_kbo_player_tooltip_hwnd == NULL || !IsWindow(g_kbo_player_tooltip_hwnd)) {
        return 0;
    }
    if (g_kbo_player_tooltip_controller != NULL || g_kbo_player_tooltip_webview != NULL) {
        return 1;
    }
    if (InterlockedCompareExchange(&g_kbo_player_tooltip_creating, 0, 0) != 0) {
        return 1;
    }

    if (!PostMessageA(g_kbo_player_tooltip_hwnd, KBO_PLAYER_TOOLTIP_WM_START, 0, 0)) {
        DWORD error = GetLastError();
        kbo_log_runtimef("KBO player tooltip WebView controller start post failed error=%lu", error);
        return kbo_tooltip_start_controller();
    }
    kbo_log_runtimef("KBO player tooltip WebView controller start posted hwnd=%p", (void*)g_kbo_player_tooltip_hwnd);
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
