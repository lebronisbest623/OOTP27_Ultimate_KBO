#include "hotkey_window_player_tooltip_internal.h"

typedef struct KboPlayerTooltipControllerHandler {
    ICoreWebView2CreateCoreWebView2ControllerCompletedHandler iface;
    LONG ref;
} KboPlayerTooltipControllerHandler;

typedef struct KboPlayerTooltipNavHandler {
    ICoreWebView2NavigationStartingEventHandler iface;
    LONG ref;
} KboPlayerTooltipNavHandler;

static EventRegistrationToken g_kbo_player_tooltip_nav_token = {0};

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

void kbo_tooltip_navigate_pending(void)
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

int kbo_tooltip_start_controller(void)
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

int kbo_tooltip_schedule_controller_start(void)
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
