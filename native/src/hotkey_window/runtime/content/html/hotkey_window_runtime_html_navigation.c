#include "../hotkey_window_runtime_content.h"

static volatile LONG g_kbo_webview_navigate_current_pending = 0;
static volatile LONG g_kbo_webview_navigation_inflight = 0;
static volatile LONG g_kbo_webview_navigation_failure_streak = 0;
static volatile LONG g_kbo_webview_navigation_cancel_streak = 0;
static ICoreWebView2* g_kbo_webview_last_html_target = NULL;
static uint64_t g_kbo_webview_last_html_hash = 0u;
static int g_kbo_webview_last_html_chars = 0;

static uint64_t kbo_webview_hash_html(const WCHAR* html, int chars)
{
    uint64_t hash = 1469598103934665603ull;
    if (html == NULL || chars <= 0) {
        return hash;
    }

    for (int i = 0; i < chars; i++) {
        uint16_t value = (uint16_t)html[i];
        hash ^= (uint64_t)(value & 0xffu);
        hash *= 1099511628211ull;
        hash ^= (uint64_t)(value >> 8);
        hash *= 1099511628211ull;
    }
    return hash;
}

void kbo_webview_note_navigation_completed(BOOL is_success, COREWEBVIEW2_WEB_ERROR_STATUS web_error)
{
    InterlockedExchange(&g_kbo_webview_navigation_inflight, 0);
    if (is_success) {
        InterlockedExchange(&g_kbo_webview_navigation_failure_streak, 0);
        InterlockedExchange(&g_kbo_webview_navigation_cancel_streak, 0);
    } else {
        LONG failure_streak = InterlockedIncrement(&g_kbo_webview_navigation_failure_streak);
        if (web_error == COREWEBVIEW2_WEB_ERROR_STATUS_OPERATION_CANCELED) {
            LONG cancel_streak = InterlockedIncrement(&g_kbo_webview_navigation_cancel_streak);
            if (cancel_streak >= 16) {
                kbo_webview_mark_failed("navigation_cancel_loop", E_ABORT);
                return;
            }
        } else {
            InterlockedExchange(&g_kbo_webview_navigation_cancel_streak, 0);
            if (failure_streak >= 3
                    || web_error == COREWEBVIEW2_WEB_ERROR_STATUS_CONNECTION_ABORTED
                    || web_error == COREWEBVIEW2_WEB_ERROR_STATUS_DISCONNECTED
                    || web_error == COREWEBVIEW2_WEB_ERROR_STATUS_UNEXPECTED_ERROR) {
                kbo_webview_mark_failed("navigation_failed", E_FAIL);
                return;
            }
        }
    }

    HWND hwnd = g_kbo_hotkey_window;
    if (!kbo_webview_is_failed()
            && InterlockedExchange(&g_kbo_webview_navigate_current_pending, 0) != 0
            && hwnd != NULL
            && IsWindow(hwnd)
            && IsWindowVisible(hwnd)) {
        PostMessageA(hwnd, KBO_WM_SHOW_HUB_CONTENT, 0, 0);
    }
}

void kbo_webview_navigate_current_immediate(void)
{
    if (kbo_webview_is_failed()) {
        kbo_profiler_record_us("webview.navigate_current.skipped_failed", 0);
        return;
    }
    if (g_kbo_webview == NULL) {
        kbo_log_runtime_line("WebView2 navigate_current_immediate skipped reason=core_unavailable");
        return;
    }
    if (InterlockedCompareExchange(&g_kbo_webview_navigation_inflight, 0, 0) != 0) {
        InterlockedExchange(&g_kbo_webview_navigate_current_pending, 1);
        kbo_profiler_record_us("webview.navigate_current.coalesced_inflight", 0);
        return;
    }
    InterlockedExchange(&g_kbo_webview_navigate_current_pending, 0);
    KBO_PROFILE_BEGIN(profile_webview_navigate);
    WCHAR* html = kbo_build_webview_hub_html();
    if (html != NULL) {
        int wide_chars = lstrlenW(html);
        uint64_t html_hash = kbo_webview_hash_html(html, wide_chars);
        if (g_kbo_webview_last_html_target == g_kbo_webview
                && g_kbo_webview_last_html_chars == wide_chars
                && g_kbo_webview_last_html_hash == html_hash) {
            kbo_profiler_record_us("webview.navigate_current.unchanged", 0);
            if (kbo_hub_current_mode_is_developer()) {
                kbo_log_runtimef(
                    "WebView2 NavigateToString current skipped reason=unchanged view=%d mod=%d wide_chars=%d",
                    g_kbo_hub_selected_view,
                    g_kbo_hub_selected_mod_subview,
                    wide_chars);
            }
            HeapFree(GetProcessHeap(), 0, html);
            KBO_PROFILE_END(profile_webview_navigate, "webview.navigate_current");
            return;
        }

        HRESULT hr = ICoreWebView2_NavigateToString(g_kbo_webview, html);
        if (SUCCEEDED(hr)) {
            InterlockedExchange(&g_kbo_webview_navigation_inflight, 1);
            g_kbo_webview_last_html_target = g_kbo_webview;
            g_kbo_webview_last_html_hash = html_hash;
            g_kbo_webview_last_html_chars = wide_chars;
        } else {
            kbo_webview_mark_failed("navigate_to_string_failed", hr);
        }
        if (FAILED(hr) || kbo_hub_current_mode_is_developer()) {
            kbo_log_runtimef(
                "WebView2 NavigateToString current view=%d mod=%d wide_chars=%d hr=0x%08lx",
                g_kbo_hub_selected_view,
                g_kbo_hub_selected_mod_subview,
                wide_chars,
                (unsigned long)hr);
        }
        HeapFree(GetProcessHeap(), 0, html);
    } else {
        kbo_log_runtimef(
            "WebView2 NavigateToString current skipped reason=html_build_failed view=%d mod=%d",
            g_kbo_hub_selected_view,
            g_kbo_hub_selected_mod_subview);
    }
    KBO_PROFILE_END(profile_webview_navigate, "webview.navigate_current");
}

void kbo_webview_navigate_current(void)
{
    if (kbo_webview_is_failed()) {
        kbo_profiler_record_us("webview.navigate_current.skipped_failed", 0);
        return;
    }

    HWND hwnd = g_kbo_hotkey_window;
    if (g_kbo_webview == NULL || hwnd == NULL || !IsWindow(hwnd) || !IsWindowVisible(hwnd)) {
        kbo_webview_navigate_current_immediate();
        return;
    }

    if (InterlockedCompareExchange(&g_kbo_webview_navigate_current_pending, 1, 0) != 0) {
        kbo_profiler_record_us("webview.navigate_current.coalesced", 0);
        if (kbo_hub_current_mode_is_developer()) {
            kbo_log_runtimef(
                "WebView2 navigate_current coalesced view=%d mod=%d",
                g_kbo_hub_selected_view,
                g_kbo_hub_selected_mod_subview);
        }
        return;
    }
    if (kbo_hub_current_mode_is_developer()) {
        kbo_log_runtimef(
            "WebView2 navigate_current scheduled hwnd=%p view=%d mod=%d",
            (void*)hwnd,
            g_kbo_hub_selected_view,
            g_kbo_hub_selected_mod_subview);
    }
    PostMessageA(hwnd, KBO_WM_SHOW_HUB_CONTENT, 0, 0);
}

void kbo_webview_navigate_loading(void)
{
    if (g_kbo_webview == NULL || kbo_webview_is_failed()) {
        return;
    }

    static const WCHAR loading_html[] =
        L"<!doctype html><html><head><meta charset='utf-8'><style>"
        L"*{box-sizing:border-box;-webkit-user-select:none;user-select:none}"
        L"html,body{height:100%;margin:0;overflow:hidden}"
        L"body{background:#111;color:#f2f2f2;font-family:'Malgun Gothic',sans-serif}"
        L".app{height:100%;display:flex;align-items:center;justify-content:center;background:linear-gradient(135deg,#111 0%,#171717 58%,#0b0b0b 100%)}"
        L".box{display:flex;align-items:center;gap:14px;padding:18px 22px;border:1px solid #303030;border-radius:5px;background:#181818;box-shadow:0 10px 26px rgba(0,0,0,.46)}"
        L".spinner{width:28px;height:28px;border:3px solid #383838;border-top-color:#de6d1f;border-radius:50%;animation:spin .8s linear infinite;flex:none}"
        L".title{font-size:16px;font-weight:900;line-height:1.15;text-transform:uppercase}"
        L".sub{margin-top:4px;color:#aaa;font-size:12px;font-weight:700}"
        L"@keyframes spin{to{transform:rotate(360deg)}}"
        L"</style></head><body><div class='app'><div class='box'>"
        L"<div class='spinner'></div><div><div class='title'>Ultimate KBO Loading</div>"
        L"<div class='sub'>Preparing F2 hub data...</div></div>"
        L"</div></div></body></html>";

    HRESULT hr = ICoreWebView2_NavigateToString(g_kbo_webview, loading_html);
    if (FAILED(hr)) {
        kbo_log_runtimef("WebView2 NavigateToString loading failed hr=0x%08lx", (unsigned long)hr);
        kbo_webview_mark_failed("navigate_loading_failed", hr);
    } else {
        InterlockedExchange(&g_kbo_webview_navigation_inflight, 1);
    }
}
