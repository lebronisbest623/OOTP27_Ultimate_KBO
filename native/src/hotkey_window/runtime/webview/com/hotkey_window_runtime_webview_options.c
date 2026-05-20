#include "hotkey_window_runtime_webview_com_internal.h"

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

void kbo_webview_copy_wide_utf8(LPCWSTR value, char* out, size_t out_size)
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

ICoreWebView2EnvironmentOptions* kbo_webview_environment_options_iface(void)
{
    return &g_kbo_webview_environment_options.iface;
}

void kbo_webview_copy_environment_options_arguments_utf8(char* out, size_t out_size)
{
    kbo_webview_copy_wide_utf8(
        g_kbo_webview_environment_options.additional_browser_arguments,
        out,
        out_size);
}
