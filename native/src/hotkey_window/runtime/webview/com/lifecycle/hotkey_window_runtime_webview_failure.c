#include "../hotkey_window_runtime_webview_com_internal.h"

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
