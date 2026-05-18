#ifndef KBOFIX_SRC_HOTKEY_WINDOW_RUNTIME_WEBVIEW_HOTKEY_WINDOW_WEBVIEW_INTERNAL_H_
#define KBOFIX_SRC_HOTKEY_WINDOW_RUNTIME_WEBVIEW_HOTKEY_WINDOW_WEBVIEW_INTERNAL_H_

#include "../content/hotkey_window_runtime_content_internal.h"

typedef HRESULT (STDAPICALLTYPE *KboCreateCoreWebView2EnvironmentWithOptionsFn)(
    PCWSTR,
    PCWSTR,
    ICoreWebView2EnvironmentOptions*,
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler*);

typedef struct KboWebViewEnvHandler {
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler iface;
    LONG ref;
    HWND hwnd;
} KboWebViewEnvHandler;

typedef struct KboWebViewControllerHandler {
    ICoreWebView2CreateCoreWebView2ControllerCompletedHandler iface;
    LONG ref;
    HWND hwnd;
} KboWebViewControllerHandler;

typedef struct KboWebViewNavHandler {
    ICoreWebView2NavigationStartingEventHandler iface;
    LONG ref;
    HWND hwnd;
} KboWebViewNavHandler;

int kbo_webview_handle_external_or_foreign_command(const char* cmd, HWND hwnd);
int kbo_webview_handle_view_navigation_command(const char* cmd);
int kbo_webview_handle_mod_settings_command(const char* cmd);
int kbo_webview_handle_event_and_fa_command(const char* cmd);
int kbo_webview_handle_settings_command(const char* cmd);
int kbo_webview_handle_selection_dropdown_command(const char* cmd, HWND hwnd);
int kbo_webview_handle_player_hover_command(const char* cmd, HWND hwnd);
HRESULT STDMETHODCALLTYPE kbo_webview_nav_qi(ICoreWebView2NavigationStartingEventHandler* This, REFIID riid, void** ppv);
ULONG STDMETHODCALLTYPE kbo_webview_nav_addref(ICoreWebView2NavigationStartingEventHandler* This);
ULONG STDMETHODCALLTYPE kbo_webview_nav_release(ICoreWebView2NavigationStartingEventHandler* This);
int kbo_webview_handle_command_uri(const char* uri, HWND hwnd);
HRESULT STDMETHODCALLTYPE kbo_webview_nav_invoke(
    ICoreWebView2NavigationStartingEventHandler* This,
    ICoreWebView2* sender,
    ICoreWebView2NavigationStartingEventArgs* args);
HRESULT STDMETHODCALLTYPE kbo_webview_env_qi(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler* This, REFIID riid, void** ppv);
ULONG STDMETHODCALLTYPE kbo_webview_env_addref(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler* This);
ULONG STDMETHODCALLTYPE kbo_webview_env_release(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler* This);
HRESULT STDMETHODCALLTYPE kbo_webview_controller_qi(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler* This, REFIID riid, void** ppv);
ULONG STDMETHODCALLTYPE kbo_webview_controller_addref(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler* This);
ULONG STDMETHODCALLTYPE kbo_webview_controller_release(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler* This);
void kbo_webview_set_bounds(HWND hwnd);
void kbo_webview_apply_ootp_like_settings(void);
int kbo_show_webview_player_tooltip_popup(HWND owner, int screen_x, int screen_y, uint32_t hover_seq, const char* html);
void kbo_hide_webview_player_tooltip_popup(uint32_t hover_seq);
void kbo_destroy_webview_player_tooltip_popup(void);
HRESULT STDMETHODCALLTYPE kbo_webview_controller_invoke(
    ICoreWebView2CreateCoreWebView2ControllerCompletedHandler* This,
    HRESULT errorCode,
    ICoreWebView2Controller* result);
HRESULT STDMETHODCALLTYPE kbo_webview_env_invoke(
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler* This,
    HRESULT errorCode,
    ICoreWebView2Environment* result);
void kbo_start_webview_rights_ui(HWND hwnd);

#endif
