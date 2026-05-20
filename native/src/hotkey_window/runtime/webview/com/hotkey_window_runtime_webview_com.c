#include "../hotkey_window_webview.h"

typedef struct KboWebViewNavigationCompletedHandler {
    ICoreWebView2NavigationCompletedEventHandler iface;
    LONG ref;
} KboWebViewNavigationCompletedHandler;

typedef struct KboWebViewProcessFailedHandler {
    ICoreWebView2ProcessFailedEventHandler iface;
    LONG ref;
} KboWebViewProcessFailedHandler;

typedef struct KboWebViewEnvironmentOptions {
    ICoreWebView2EnvironmentOptions iface;
    LONG ref;
    WCHAR additional_browser_arguments[512];
    WCHAR language[32];
    WCHAR target_compatible_browser_version[64];
    BOOL allow_single_sign_on;
} KboWebViewEnvironmentOptions;

static HRESULT kbo_webview_options_alloc_string(LPCWSTR value, LPWSTR* out)
{
    if (out == NULL) {
        return E_POINTER;
    }
    *out = NULL;
    int length = value != NULL ? lstrlenW(value) : 0;
    LPWSTR copy = (LPWSTR)CoTaskMemAlloc(((SIZE_T)length + 1u) * sizeof(WCHAR));
    if (copy == NULL) {
        return E_OUTOFMEMORY;
    }
    if (length > 0) {
        memcpy(copy, value, (SIZE_T)length * sizeof(WCHAR));
    }
    copy[length] = L'\0';
    *out = copy;
    return S_OK;
}

static void kbo_webview_options_copy_string(WCHAR* out, size_t out_count, LPCWSTR value)
{
    if (out == NULL || out_count == 0u) {
        return;
    }
    out[0] = L'\0';
    if (value == NULL) {
        return;
    }
    size_t index = 0u;
    while (index + 1u < out_count && value[index] != L'\0') {
        out[index] = value[index];
        ++index;
    }
    out[index] = L'\0';
}

static HRESULT STDMETHODCALLTYPE kbo_webview_options_qi(
    ICoreWebView2EnvironmentOptions* This,
    REFIID riid,
    void** ppv)
{
    if (ppv == NULL) { return E_POINTER; }
    *ppv = NULL;
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_ICoreWebView2EnvironmentOptions)) {
        *ppv = This;
        ICoreWebView2EnvironmentOptions_AddRef(This);
        return S_OK;
    }
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE kbo_webview_options_addref(ICoreWebView2EnvironmentOptions* This)
{
    return (ULONG)InterlockedIncrement(&((KboWebViewEnvironmentOptions*)This)->ref);
}

static ULONG STDMETHODCALLTYPE kbo_webview_options_release(ICoreWebView2EnvironmentOptions* This)
{
    LONG value = InterlockedDecrement(&((KboWebViewEnvironmentOptions*)This)->ref);
    if (value < 1) { ((KboWebViewEnvironmentOptions*)This)->ref = 1; }
    return (ULONG)((KboWebViewEnvironmentOptions*)This)->ref;
}

static HRESULT STDMETHODCALLTYPE kbo_webview_options_get_additional_browser_arguments(
    ICoreWebView2EnvironmentOptions* This,
    LPWSTR* value)
{
    return kbo_webview_options_alloc_string(
        ((KboWebViewEnvironmentOptions*)This)->additional_browser_arguments,
        value);
}

static HRESULT STDMETHODCALLTYPE kbo_webview_options_put_additional_browser_arguments(
    ICoreWebView2EnvironmentOptions* This,
    LPCWSTR value)
{
    kbo_webview_options_copy_string(
        ((KboWebViewEnvironmentOptions*)This)->additional_browser_arguments,
        sizeof(((KboWebViewEnvironmentOptions*)This)->additional_browser_arguments) / sizeof(WCHAR),
        value);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE kbo_webview_options_get_language(
    ICoreWebView2EnvironmentOptions* This,
    LPWSTR* value)
{
    return kbo_webview_options_alloc_string(((KboWebViewEnvironmentOptions*)This)->language, value);
}

static HRESULT STDMETHODCALLTYPE kbo_webview_options_put_language(
    ICoreWebView2EnvironmentOptions* This,
    LPCWSTR value)
{
    kbo_webview_options_copy_string(
        ((KboWebViewEnvironmentOptions*)This)->language,
        sizeof(((KboWebViewEnvironmentOptions*)This)->language) / sizeof(WCHAR),
        value);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE kbo_webview_options_get_target_compatible_browser_version(
    ICoreWebView2EnvironmentOptions* This,
    LPWSTR* value)
{
    return kbo_webview_options_alloc_string(
        ((KboWebViewEnvironmentOptions*)This)->target_compatible_browser_version,
        value);
}

static HRESULT STDMETHODCALLTYPE kbo_webview_options_put_target_compatible_browser_version(
    ICoreWebView2EnvironmentOptions* This,
    LPCWSTR value)
{
    kbo_webview_options_copy_string(
        ((KboWebViewEnvironmentOptions*)This)->target_compatible_browser_version,
        sizeof(((KboWebViewEnvironmentOptions*)This)->target_compatible_browser_version) / sizeof(WCHAR),
        value);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE kbo_webview_options_get_allow_sso(
    ICoreWebView2EnvironmentOptions* This,
    BOOL* allow)
{
    if (allow == NULL) {
        return E_POINTER;
    }
    *allow = ((KboWebViewEnvironmentOptions*)This)->allow_single_sign_on;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE kbo_webview_options_put_allow_sso(
    ICoreWebView2EnvironmentOptions* This,
    BOOL allow)
{
    ((KboWebViewEnvironmentOptions*)This)->allow_single_sign_on = allow ? TRUE : FALSE;
    return S_OK;
}

static ICoreWebView2EnvironmentOptionsVtbl g_kbo_webview_options_vtbl = {
    kbo_webview_options_qi,
    kbo_webview_options_addref,
    kbo_webview_options_release,
    kbo_webview_options_get_additional_browser_arguments,
    kbo_webview_options_put_additional_browser_arguments,
    kbo_webview_options_get_language,
    kbo_webview_options_put_language,
    kbo_webview_options_get_target_compatible_browser_version,
    kbo_webview_options_put_target_compatible_browser_version,
    kbo_webview_options_get_allow_sso,
    kbo_webview_options_put_allow_sso
};

static KboWebViewEnvironmentOptions g_kbo_webview_environment_options = {
    { &g_kbo_webview_options_vtbl },
    1,
    L"--disable-gpu",
    L"",
    L"",
    FALSE
};

static void kbo_webview_copy_wide_utf8(LPCWSTR value, char* out, size_t out_size)
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

HRESULT STDMETHODCALLTYPE kbo_webview_nav_invoke(
    ICoreWebView2NavigationStartingEventHandler* This,
    ICoreWebView2* sender,
    ICoreWebView2NavigationStartingEventArgs* args)
{
    (void)sender;
    KboWebViewNavHandler* handler = (KboWebViewNavHandler*)This;
    LPWSTR uri_w = NULL;
    if (args == NULL || FAILED(ICoreWebView2NavigationStartingEventArgs_get_Uri(args, &uri_w)) || uri_w == NULL) {
        return S_OK;
    }
    int needed = WideCharToMultiByte(CP_UTF8, 0, uri_w, -1, NULL, 0, NULL, NULL);
    char uri[512] = {0};
    if (needed > 0) {
        WideCharToMultiByte(CP_UTF8, 0, uri_w, -1, uri, sizeof(uri), NULL, NULL);
    }
    CoTaskMemFree(uri_w);
    if (kbo_webview_handle_command_uri(uri, handler->hwnd)) {
        ICoreWebView2NavigationStartingEventArgs_put_Cancel(args, TRUE);
    }
    return S_OK;
}

static ICoreWebView2NavigationStartingEventHandlerVtbl g_kbo_webview_nav_vtbl = {
    kbo_webview_nav_qi,
    kbo_webview_nav_addref,
    kbo_webview_nav_release,
    kbo_webview_nav_invoke
};

static KboWebViewNavHandler g_kbo_webview_nav_handler = {
    { &g_kbo_webview_nav_vtbl },
    1,
    NULL
};

static EventRegistrationToken g_kbo_webview_nav_token = {0};
static EventRegistrationToken g_kbo_webview_nav_completed_token = {0};
static EventRegistrationToken g_kbo_webview_process_failed_token = {0};

static HRESULT STDMETHODCALLTYPE kbo_webview_nav_completed_qi(
    ICoreWebView2NavigationCompletedEventHandler* This,
    REFIID riid,
    void** ppv)
{
    if (ppv == NULL) { return E_POINTER; }
    *ppv = NULL;
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_ICoreWebView2NavigationCompletedEventHandler)) {
        *ppv = This;
        ICoreWebView2NavigationCompletedEventHandler_AddRef(This);
        return S_OK;
    }
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE kbo_webview_nav_completed_addref(ICoreWebView2NavigationCompletedEventHandler* This)
{
    return (ULONG)InterlockedIncrement(&((KboWebViewNavigationCompletedHandler*)This)->ref);
}

static ULONG STDMETHODCALLTYPE kbo_webview_nav_completed_release(ICoreWebView2NavigationCompletedEventHandler* This)
{
    LONG value = InterlockedDecrement(&((KboWebViewNavigationCompletedHandler*)This)->ref);
    if (value < 1) { ((KboWebViewNavigationCompletedHandler*)This)->ref = 1; }
    return (ULONG)((KboWebViewNavigationCompletedHandler*)This)->ref;
}

static HRESULT STDMETHODCALLTYPE kbo_webview_nav_completed_invoke(
    ICoreWebView2NavigationCompletedEventHandler* This,
    ICoreWebView2* sender,
    ICoreWebView2NavigationCompletedEventArgs* args)
{
    (void)This;
    (void)sender;
    BOOL is_success = FALSE;
    COREWEBVIEW2_WEB_ERROR_STATUS web_error = (COREWEBVIEW2_WEB_ERROR_STATUS)0;
    UINT64 navigation_id = 0u;
    HRESULT success_hr = args != NULL
        ? ICoreWebView2NavigationCompletedEventArgs_get_IsSuccess(args, &is_success)
        : E_POINTER;
    HRESULT error_hr = args != NULL
        ? ICoreWebView2NavigationCompletedEventArgs_get_WebErrorStatus(args, &web_error)
        : E_POINTER;
    HRESULT id_hr = args != NULL
        ? ICoreWebView2NavigationCompletedEventArgs_get_NavigationId(args, &navigation_id)
        : E_POINTER;
    kbo_log_runtimef(
        "WebView2 navigation completed success=%d web_error=%d navigation_id=%llu hr_success=0x%08lx hr_error=0x%08lx hr_id=0x%08lx",
        is_success ? 1 : 0,
        (int)web_error,
        (unsigned long long)navigation_id,
        (unsigned long)success_hr,
        (unsigned long)error_hr,
        (unsigned long)id_hr);
    return S_OK;
}

static ICoreWebView2NavigationCompletedEventHandlerVtbl g_kbo_webview_nav_completed_vtbl = {
    kbo_webview_nav_completed_qi,
    kbo_webview_nav_completed_addref,
    kbo_webview_nav_completed_release,
    kbo_webview_nav_completed_invoke
};

static KboWebViewNavigationCompletedHandler g_kbo_webview_nav_completed_handler = {
    { &g_kbo_webview_nav_completed_vtbl },
    1
};

static HRESULT STDMETHODCALLTYPE kbo_webview_process_failed_qi(
    ICoreWebView2ProcessFailedEventHandler* This,
    REFIID riid,
    void** ppv)
{
    if (ppv == NULL) { return E_POINTER; }
    *ppv = NULL;
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_ICoreWebView2ProcessFailedEventHandler)) {
        *ppv = This;
        ICoreWebView2ProcessFailedEventHandler_AddRef(This);
        return S_OK;
    }
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE kbo_webview_process_failed_addref(ICoreWebView2ProcessFailedEventHandler* This)
{
    return (ULONG)InterlockedIncrement(&((KboWebViewProcessFailedHandler*)This)->ref);
}

static ULONG STDMETHODCALLTYPE kbo_webview_process_failed_release(ICoreWebView2ProcessFailedEventHandler* This)
{
    LONG value = InterlockedDecrement(&((KboWebViewProcessFailedHandler*)This)->ref);
    if (value < 1) { ((KboWebViewProcessFailedHandler*)This)->ref = 1; }
    return (ULONG)((KboWebViewProcessFailedHandler*)This)->ref;
}

static HRESULT STDMETHODCALLTYPE kbo_webview_process_failed_invoke(
    ICoreWebView2ProcessFailedEventHandler* This,
    ICoreWebView2* sender,
    ICoreWebView2ProcessFailedEventArgs* args)
{
    (void)This;
    (void)sender;
    COREWEBVIEW2_PROCESS_FAILED_KIND kind = (COREWEBVIEW2_PROCESS_FAILED_KIND)0;
    HRESULT kind_hr = args != NULL
        ? ICoreWebView2ProcessFailedEventArgs_get_ProcessFailedKind(args, &kind)
        : E_POINTER;
    COREWEBVIEW2_PROCESS_FAILED_REASON reason = (COREWEBVIEW2_PROCESS_FAILED_REASON)0;
    INT32 exit_code = 0;
    char description[256] = {0};
    char module_path[MAX_PATH] = {0};
    HRESULT reason_hr = E_NOINTERFACE;
    HRESULT exit_hr = E_NOINTERFACE;
    HRESULT description_hr = E_NOINTERFACE;
    HRESULT module_hr = E_NOINTERFACE;

    ICoreWebView2ProcessFailedEventArgs2* args2 = NULL;
    if (args != NULL && SUCCEEDED(ICoreWebView2ProcessFailedEventArgs_QueryInterface(
            args, &IID_ICoreWebView2ProcessFailedEventArgs2, (void**)&args2)) && args2 != NULL) {
        reason_hr = ICoreWebView2ProcessFailedEventArgs2_get_Reason(args2, &reason);
        exit_hr = ICoreWebView2ProcessFailedEventArgs2_get_ExitCode(args2, &exit_code);
        LPWSTR description_w = NULL;
        description_hr = ICoreWebView2ProcessFailedEventArgs2_get_ProcessDescription(args2, &description_w);
        if (SUCCEEDED(description_hr) && description_w != NULL) {
            kbo_webview_copy_wide_utf8(description_w, description, sizeof(description));
            CoTaskMemFree(description_w);
        }
        ICoreWebView2ProcessFailedEventArgs2_Release(args2);
    }

    ICoreWebView2ProcessFailedEventArgs3* args3 = NULL;
    if (args != NULL && SUCCEEDED(ICoreWebView2ProcessFailedEventArgs_QueryInterface(
            args, &IID_ICoreWebView2ProcessFailedEventArgs3, (void**)&args3)) && args3 != NULL) {
        LPWSTR module_w = NULL;
        module_hr = ICoreWebView2ProcessFailedEventArgs3_get_FailureSourceModulePath(args3, &module_w);
        if (SUCCEEDED(module_hr) && module_w != NULL) {
            kbo_webview_copy_wide_utf8(module_w, module_path, sizeof(module_path));
            CoTaskMemFree(module_w);
        }
        ICoreWebView2ProcessFailedEventArgs3_Release(args3);
    }

    kbo_log_runtimef(
        "WebView2 process failed kind=%d reason=%d exit=%d description=%s module=%s hr_kind=0x%08lx hr_reason=0x%08lx hr_exit=0x%08lx hr_description=0x%08lx hr_module=0x%08lx",
        (int)kind,
        (int)reason,
        (int)exit_code,
        description,
        module_path,
        (unsigned long)kind_hr,
        (unsigned long)reason_hr,
        (unsigned long)exit_hr,
        (unsigned long)description_hr,
        (unsigned long)module_hr);
    return S_OK;
}

static ICoreWebView2ProcessFailedEventHandlerVtbl g_kbo_webview_process_failed_vtbl = {
    kbo_webview_process_failed_qi,
    kbo_webview_process_failed_addref,
    kbo_webview_process_failed_release,
    kbo_webview_process_failed_invoke
};

static KboWebViewProcessFailedHandler g_kbo_webview_process_failed_handler = {
    { &g_kbo_webview_process_failed_vtbl },
    1
};

HRESULT STDMETHODCALLTYPE kbo_webview_env_qi(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler* This, REFIID riid, void** ppv)
{
    if (ppv == NULL) { return E_POINTER; }
    *ppv = NULL;
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler)) {
        *ppv = This;
        ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler_AddRef(This);
        return S_OK;
    }
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE kbo_webview_env_addref(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler* This)
{
    return (ULONG)InterlockedIncrement(&((KboWebViewEnvHandler*)This)->ref);
}

ULONG STDMETHODCALLTYPE kbo_webview_env_release(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler* This)
{
    LONG value = InterlockedDecrement(&((KboWebViewEnvHandler*)This)->ref);
    if (value < 1) { ((KboWebViewEnvHandler*)This)->ref = 1; }
    return (ULONG)((KboWebViewEnvHandler*)This)->ref;
}

HRESULT STDMETHODCALLTYPE kbo_webview_controller_qi(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler* This, REFIID riid, void** ppv)
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

ULONG STDMETHODCALLTYPE kbo_webview_controller_addref(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler* This)
{
    return (ULONG)InterlockedIncrement(&((KboWebViewControllerHandler*)This)->ref);
}

ULONG STDMETHODCALLTYPE kbo_webview_controller_release(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler* This)
{
    LONG value = InterlockedDecrement(&((KboWebViewControllerHandler*)This)->ref);
    if (value < 1) { ((KboWebViewControllerHandler*)This)->ref = 1; }
    return (ULONG)((KboWebViewControllerHandler*)This)->ref;
}

void kbo_webview_set_bounds(HWND hwnd)
{
    if (hwnd == NULL || g_kbo_webview_controller == NULL) {
        return;
    }
    RECT client;
    GetClientRect(hwnd, &client);
    RECT bounds = {0, 0, client.right, client.bottom};
    int width = client.right - client.left;
    int height = client.bottom - client.top;
    HRESULT bounds_hr = ICoreWebView2Controller_put_Bounds(g_kbo_webview_controller, bounds);
    HRESULT visible_hr = ICoreWebView2Controller_put_IsVisible(g_kbo_webview_controller, TRUE);
    static int s_last_width = -1;
    static int s_last_height = -1;
    if (width != s_last_width || height != s_last_height || FAILED(bounds_hr) || FAILED(visible_hr)) {
        s_last_width = width;
        s_last_height = height;
        kbo_log_runtimef(
            "WebView2 bounds updated hwnd=%p client=%dx%d hr_bounds=0x%08lx hr_visible=0x%08lx",
            (void*)hwnd,
            width,
            height,
            (unsigned long)bounds_hr,
            (unsigned long)visible_hr);
    }
}

void kbo_webview_apply_ootp_like_settings(void)
{
    if (g_kbo_webview == NULL) {
        return;
    }

    ICoreWebView2Settings* settings = NULL;
    if (FAILED(ICoreWebView2_get_Settings(g_kbo_webview, &settings)) || settings == NULL) {
        kbo_log_runtime_line("WebView2 settings unavailable");
        return;
    }

    HRESULT status_hr = ICoreWebView2Settings_put_IsStatusBarEnabled(settings, FALSE);
    HRESULT context_hr = ICoreWebView2Settings_put_AreDefaultContextMenusEnabled(settings, FALSE);
    HRESULT devtools_hr = ICoreWebView2Settings_put_AreDevToolsEnabled(settings, FALSE);
    HRESULT zoom_hr = ICoreWebView2Settings_put_IsZoomControlEnabled(settings, FALSE);
    ICoreWebView2Settings_Release(settings);
    kbo_log_runtimef(
        "WebView2 settings applied hr_status=0x%08lx hr_context=0x%08lx hr_devtools=0x%08lx hr_zoom=0x%08lx",
        (unsigned long)status_hr,
        (unsigned long)context_hr,
        (unsigned long)devtools_hr,
        (unsigned long)zoom_hr);
}

static void kbo_webview_register_diagnostic_handlers(void)
{
    if (g_kbo_webview == NULL) {
        return;
    }
    HRESULT nav_completed_hr = ICoreWebView2_add_NavigationCompleted(
        g_kbo_webview,
        &g_kbo_webview_nav_completed_handler.iface,
        &g_kbo_webview_nav_completed_token);
    HRESULT process_failed_hr = ICoreWebView2_add_ProcessFailed(
        g_kbo_webview,
        &g_kbo_webview_process_failed_handler.iface,
        &g_kbo_webview_process_failed_token);
    kbo_log_runtimef(
        "WebView2 diagnostic handlers registered hr_navigation_completed=0x%08lx token_navigation_completed=%lld hr_process_failed=0x%08lx token_process_failed=%lld",
        (unsigned long)nav_completed_hr,
        (long long)g_kbo_webview_nav_completed_token.value,
        (unsigned long)process_failed_hr,
        (long long)g_kbo_webview_process_failed_token.value);
}

HRESULT STDMETHODCALLTYPE kbo_webview_controller_invoke(
    ICoreWebView2CreateCoreWebView2ControllerCompletedHandler* This,
    HRESULT errorCode,
    ICoreWebView2Controller* result)
{
    KboWebViewControllerHandler* handler = (KboWebViewControllerHandler*)This;
    if (FAILED(errorCode) || result == NULL) {
        kbo_log_runtimef("WebView2 controller create failed hr=0x%08lx", (unsigned long)errorCode);
        InterlockedExchange(&g_kbo_webview_failed, 1);
        return S_OK;
    }

    g_kbo_webview_controller = result;
    ICoreWebView2Controller_AddRef(g_kbo_webview_controller);
    HRESULT core_hr = ICoreWebView2Controller_get_CoreWebView2(g_kbo_webview_controller, &g_kbo_webview);
    kbo_log_runtimef(
        "WebView2 controller created hwnd=%p controller=%p core=%p hr_core=0x%08lx",
        (void*)handler->hwnd,
        (void*)g_kbo_webview_controller,
        (void*)g_kbo_webview,
        (unsigned long)core_hr);
    if (g_kbo_webview != NULL) {
        kbo_webview_apply_ootp_like_settings();
        g_kbo_webview_nav_handler.hwnd = handler->hwnd;
        HRESULT nav_hr = ICoreWebView2_add_NavigationStarting(g_kbo_webview, &g_kbo_webview_nav_handler.iface, &g_kbo_webview_nav_token);
        kbo_log_runtimef(
            "WebView2 navigation-starting handler registered hr=0x%08lx token=%lld",
            (unsigned long)nav_hr,
            (long long)g_kbo_webview_nav_token.value);
        kbo_webview_register_diagnostic_handlers();
        kbo_webview_navigate_current();
    } else {
        kbo_log_runtimef("WebView2 core object unavailable hr=0x%08lx", (unsigned long)core_hr);
    }
    InterlockedExchange(&g_kbo_webview_ready, 1);
    kbo_webview_set_bounds(handler->hwnd);
    kbo_log_runtime_line("WebView2 F2 rights UI ready");
    return S_OK;
}

static ICoreWebView2CreateCoreWebView2ControllerCompletedHandlerVtbl g_kbo_webview_controller_vtbl = {
    kbo_webview_controller_qi,
    kbo_webview_controller_addref,
    kbo_webview_controller_release,
    kbo_webview_controller_invoke
};

static KboWebViewControllerHandler g_kbo_webview_controller_handler = {
    { &g_kbo_webview_controller_vtbl },
    1,
    NULL
};

HRESULT STDMETHODCALLTYPE kbo_webview_env_invoke(
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler* This,
    HRESULT errorCode,
    ICoreWebView2Environment* result)
{
    KboWebViewEnvHandler* handler = (KboWebViewEnvHandler*)This;
    if (FAILED(errorCode) || result == NULL) {
        kbo_log_runtimef("WebView2 environment create failed hr=0x%08lx", (unsigned long)errorCode);
        InterlockedExchange(&g_kbo_webview_failed, 1);
        return S_OK;
    }

    LPWSTR version_w = NULL;
    char version[128] = {0};
    HRESULT version_hr = ICoreWebView2Environment_get_BrowserVersionString(result, &version_w);
    if (SUCCEEDED(version_hr) && version_w != NULL) {
        kbo_webview_copy_wide_utf8(version_w, version, sizeof(version));
        CoTaskMemFree(version_w);
    }
    kbo_log_runtimef(
        "WebView2 environment ready hwnd=%p env=%p browser_version=%s hr_version=0x%08lx",
        (void*)handler->hwnd,
        (void*)result,
        version,
        (unsigned long)version_hr);

    if (g_kbo_webview_environment == NULL) {
        g_kbo_webview_environment = result;
        ICoreWebView2Environment_AddRef(g_kbo_webview_environment);
    }

    g_kbo_webview_controller_handler.hwnd = handler->hwnd;
    HRESULT hr = ICoreWebView2Environment_CreateCoreWebView2Controller(
        result,
        handler->hwnd,
        &g_kbo_webview_controller_handler.iface);
    if (FAILED(hr)) {
        kbo_log_runtimef("WebView2 CreateCoreWebView2Controller failed hr=0x%08lx", (unsigned long)hr);
        InterlockedExchange(&g_kbo_webview_failed, 1);
    } else {
        kbo_log_runtimef("WebView2 controller creation requested hwnd=%p hr=0x%08lx", (void*)handler->hwnd, (unsigned long)hr);
    }
    return S_OK;
}

static ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandlerVtbl g_kbo_webview_env_vtbl = {
    kbo_webview_env_qi,
    kbo_webview_env_addref,
    kbo_webview_env_release,
    kbo_webview_env_invoke
};

static KboWebViewEnvHandler g_kbo_webview_env_handler = {
    { &g_kbo_webview_env_vtbl },
    1,
    NULL
};

void kbo_start_webview_rights_ui(HWND hwnd)
{
    if (hwnd == NULL || InterlockedCompareExchange(&g_kbo_webview_starting, 1, 0) != 0) {
        kbo_webview_set_bounds(hwnd);
        return;
    }

    HMODULE self_module = NULL;
    char loader_path[MAX_PATH] = {0};
    HMODULE loader = NULL;
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            (LPCSTR)&kbo_start_webview_rights_ui, &self_module)
            && GetModuleFileNameA(self_module, loader_path, sizeof(loader_path)) > 0) {
        char* slash = strrchr(loader_path, '\\');
        if (slash != NULL) {
            slash[1] = '\0';
            strncat(loader_path, "WebView2Loader.dll", sizeof(loader_path) - strlen(loader_path) - 1);
            loader = LoadLibraryA(loader_path);
        }
    }
    if (loader == NULL) {
        loader = LoadLibraryA("WebView2Loader.dll");
    }
    if (loader == NULL) {
        kbo_log_runtimef("WebView2Loader.dll load failed error=%lu", GetLastError());
        InterlockedExchange(&g_kbo_webview_failed, 1);
        return;
    }
    char actual_loader_path[MAX_PATH] = {0};
    GetModuleFileNameA(loader, actual_loader_path, sizeof(actual_loader_path));
    kbo_log_runtimef(
        "WebView2 loader loaded module=%p path=%s",
        (void*)loader,
        actual_loader_path[0] != '\0' ? actual_loader_path : loader_path);

    union {
        FARPROC proc;
        KboCreateCoreWebView2EnvironmentWithOptionsFn fn;
    } create_env_lookup;
    create_env_lookup.proc = GetProcAddress(loader, "CreateCoreWebView2EnvironmentWithOptions");
    KboCreateCoreWebView2EnvironmentWithOptionsFn create_env = create_env_lookup.fn;
    if (create_env == NULL) {
        kbo_log_runtime_line("WebView2 CreateCoreWebView2EnvironmentWithOptions missing");
        InterlockedExchange(&g_kbo_webview_failed, 1);
        return;
    }

    WCHAR user_data[MAX_PATH] = {0};
    WCHAR local[MAX_PATH] = {0};
    DWORD got = GetEnvironmentVariableW(L"LOCALAPPDATA", local, (DWORD)(sizeof(local) / sizeof(local[0])));
    if (got > 0 && got < (DWORD)(sizeof(local) / sizeof(local[0]))) {
        swprintf(user_data, sizeof(user_data) / sizeof(user_data[0]), L"%ls\\OOTP-KBO\\WebView2", local);
        CreateDirectoryW(user_data, NULL);
    }
    char user_data_utf8[MAX_PATH] = {0};
    kbo_webview_copy_wide_utf8(user_data, user_data_utf8, sizeof(user_data_utf8));
    kbo_log_runtimef(
        "WebView2 environment start requested hwnd=%p user_data=%s localappdata_chars=%lu",
        (void*)hwnd,
        user_data_utf8,
        (unsigned long)got);
    char browser_arguments_utf8[512] = {0};
    kbo_webview_copy_wide_utf8(
        g_kbo_webview_environment_options.additional_browser_arguments,
        browser_arguments_utf8,
        sizeof(browser_arguments_utf8));
    kbo_log_runtimef("WebView2 environment options additional_args=%s", browser_arguments_utf8);

    g_kbo_webview_env_handler.hwnd = hwnd;
    HRESULT hr = create_env(
        NULL,
        user_data[0] != L'\0' ? user_data : NULL,
        &g_kbo_webview_environment_options.iface,
        &g_kbo_webview_env_handler.iface);
    if (FAILED(hr)) {
        kbo_log_runtimef(
            "WebView2 environment start with rendering options failed hr=0x%08lx; retrying without options",
            (unsigned long)hr);
        hr = create_env(
            NULL,
            user_data[0] != L'\0' ? user_data : NULL,
            NULL,
            &g_kbo_webview_env_handler.iface);
    }
    if (FAILED(hr)) {
        kbo_log_runtimef("WebView2 environment start failed after fallback hr=0x%08lx", (unsigned long)hr);
        InterlockedExchange(&g_kbo_webview_failed, 1);
    } else {
        kbo_log_runtime_line("WebView2 F2 rights UI starting");
    }
}

