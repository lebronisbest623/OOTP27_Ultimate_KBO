#include "hotkey_window_runtime_webview_com_internal.h"
#include "../../../../core/product/ootp_product.h"

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

int kbo_webview_is_failed(void)
{
    return InterlockedCompareExchange(&g_kbo_webview_failed, 0, 0) != 0;
}

void kbo_webview_mark_failed(const char* reason, HRESULT hr)
{
    if (InterlockedExchange(&g_kbo_webview_failed, 1) == 0) {
        kbo_log_runtimef(
            "WebView2 F2 hub disabled reason=%s hr=0x%08lx",
            reason != NULL ? reason : "unknown",
            (unsigned long)hr);
    }
    InterlockedExchange(&g_kbo_webview_ready, 0);
    HWND hwnd = g_kbo_hotkey_window;
    if (hwnd != NULL && IsWindow(hwnd)) {
        PostMessageA(hwnd, KBO_WM_WEBVIEW_DISABLE, 0, 0);
        InvalidateRect(hwnd, NULL, TRUE);
    } else {
        kbo_webview_shutdown_failed_surface();
    }
}

void kbo_webview_shutdown_failed_surface(void)
{
    InterlockedExchange(&g_kbo_webview_ready, 0);
    InterlockedExchange(&g_kbo_webview_starting, 0);
    kbo_destroy_webview_player_tooltip_popup();

    ICoreWebView2Controller* controller = g_kbo_webview_controller;
    ICoreWebView2* webview = g_kbo_webview;
    ICoreWebView2Environment* environment = g_kbo_webview_environment;
    g_kbo_webview_controller = NULL;
    g_kbo_webview = NULL;
    g_kbo_webview_environment = NULL;

    if (controller != NULL) {
        ICoreWebView2Controller_put_IsVisible(controller, FALSE);
        ICoreWebView2Controller_Close(controller);
    }
    if (webview != NULL) {
        ICoreWebView2_Release(webview);
    }
    if (controller != NULL) {
        ICoreWebView2Controller_Release(controller);
    }
    if (environment != NULL) {
        ICoreWebView2Environment_Release(environment);
    }
}

void kbo_webview_set_bounds(HWND hwnd)
{
    if (hwnd == NULL || g_kbo_webview_controller == NULL || kbo_webview_is_failed()) {
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

HRESULT STDMETHODCALLTYPE kbo_webview_controller_invoke(
    ICoreWebView2CreateCoreWebView2ControllerCompletedHandler* This,
    HRESULT errorCode,
    ICoreWebView2Controller* result)
{
    KboWebViewControllerHandler* handler = (KboWebViewControllerHandler*)This;
    if (FAILED(errorCode) || result == NULL) {
        kbo_log_runtimef("WebView2 controller create failed hr=0x%08lx", (unsigned long)errorCode);
        kbo_webview_mark_failed("controller_create_failed", errorCode);
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
        kbo_webview_register_navigation_handler(handler->hwnd);
        kbo_webview_register_diagnostic_handlers();
        kbo_webview_navigate_current();
    } else {
        kbo_log_runtimef("WebView2 core object unavailable hr=0x%08lx", (unsigned long)core_hr);
        kbo_webview_mark_failed("core_unavailable", core_hr);
        return S_OK;
    }
    if (kbo_webview_is_failed()) {
        return S_OK;
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
        kbo_webview_mark_failed("environment_create_failed", errorCode);
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
        kbo_webview_mark_failed("controller_request_failed", hr);
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
    if (kbo_webview_is_failed()) {
        return;
    }

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
        DWORD error = GetLastError();
        kbo_log_runtimef("WebView2Loader.dll load failed error=%lu", error);
        kbo_webview_mark_failed("loader_load_failed", HRESULT_FROM_WIN32(error));
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
        kbo_webview_mark_failed("create_environment_proc_missing", E_POINTER);
        return;
    }

    WCHAR user_data[MAX_PATH] = {0};
    WCHAR local[MAX_PATH] = {0};
    DWORD got = GetEnvironmentVariableW(L"LOCALAPPDATA", local, (DWORD)(sizeof(local) / sizeof(local[0])));
    if (got > 0 && got < (DWORD)(sizeof(local) / sizeof(local[0]))) {
        swprintf(
            user_data,
            sizeof(user_data) / sizeof(user_data[0]),
            L"%ls\\" KBO_PRODUCT_LOCAL_DATA_DIR_W L"\\WebView2",
            local);
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
    kbo_webview_copy_environment_options_arguments_utf8(browser_arguments_utf8, sizeof(browser_arguments_utf8));
    kbo_log_runtimef("WebView2 environment options additional_args=%s", browser_arguments_utf8);

    g_kbo_webview_env_handler.hwnd = hwnd;
    HRESULT hr = create_env(
        NULL,
        user_data[0] != L'\0' ? user_data : NULL,
        kbo_webview_environment_options_iface(),
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
        kbo_webview_mark_failed("environment_start_failed", hr);
    } else {
        kbo_log_runtime_line("WebView2 F2 rights UI starting");
    }
}

