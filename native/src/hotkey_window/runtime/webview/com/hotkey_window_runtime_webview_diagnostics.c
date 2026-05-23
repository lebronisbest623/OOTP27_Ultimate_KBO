#include "hotkey_window_runtime_webview_com_internal.h"

typedef struct KboWebViewNavigationCompletedHandler {
    ICoreWebView2NavigationCompletedEventHandler iface;
    LONG ref;
} KboWebViewNavigationCompletedHandler;

typedef struct KboWebViewProcessFailedHandler {
    ICoreWebView2ProcessFailedEventHandler iface;
    LONG ref;
} KboWebViewProcessFailedHandler;

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
    if (!is_success || kbo_hub_current_mode_is_developer()) {
        kbo_log_runtimef(
            "WebView2 navigation completed success=%d web_error=%d navigation_id=%llu hr_success=0x%08lx hr_error=0x%08lx hr_id=0x%08lx",
            is_success ? 1 : 0,
            (int)web_error,
            (unsigned long long)navigation_id,
            (unsigned long)success_hr,
            (unsigned long)error_hr,
            (unsigned long)id_hr);
    }
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
    kbo_webview_mark_failed("webview_process_failed", HRESULT_FROM_WIN32((DWORD)exit_code));
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

void kbo_webview_register_diagnostic_handlers(void)
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
