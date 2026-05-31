#ifndef KBOFIX_SRC_HOTKEY_WINDOW_RUNTIME_WEBVIEW_COM_INTERNAL_H_
#define KBOFIX_SRC_HOTKEY_WINDOW_RUNTIME_WEBVIEW_COM_INTERNAL_H_

#include "../hotkey_window_webview.h"

void kbo_webview_copy_wide_utf8(LPCWSTR value, char* out, size_t out_size);
ICoreWebView2EnvironmentOptions* kbo_webview_environment_options_iface(void);
void kbo_webview_copy_environment_options_arguments_utf8(char* out, size_t out_size);
void kbo_webview_register_diagnostic_handlers(void);
void kbo_webview_register_navigation_handler(HWND hwnd);

#endif
