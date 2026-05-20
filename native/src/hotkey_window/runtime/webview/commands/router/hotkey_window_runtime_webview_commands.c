#include "../../hotkey_window_webview.h"

int kbo_webview_handle_command_uri(const char* uri, HWND hwnd)
{
    if (uri == NULL || strncmp(uri, "kbo://", 6) != 0) {
        return 0;
    }
    const char* cmd = uri + 6;

    if (strncmp(cmd, "render/", 7) == 0) {
        kbo_log_runtimef("WebView2 render diagnostic %s", cmd + 7);
        return 1;
    }
    if (kbo_webview_handle_external_or_foreign_command(cmd, hwnd)) { return 1; }
    if (kbo_webview_handle_view_navigation_command(cmd)) { return 1; }
    if (kbo_webview_handle_mod_settings_command(cmd)) { return 1; }
    if (kbo_webview_handle_event_and_fa_command(cmd)) { return 1; }
    if (kbo_webview_handle_settings_command(cmd)) { return 1; }
    if (kbo_webview_handle_player_hover_command(cmd, hwnd)) { return 1; }
    return kbo_webview_handle_selection_dropdown_command(cmd, hwnd) ? 1 : 0;
}
