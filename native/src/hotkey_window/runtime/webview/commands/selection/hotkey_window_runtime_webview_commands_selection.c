#include "../../hotkey_window_webview_internal.h"

int kbo_webview_handle_selection_dropdown_command(const char* cmd, HWND hwnd)
{
    if (cmd == NULL) {
        return 0;
    }

    if (strncmp(cmd, "setleague/", 10) == 0) {
        uint32_t league_id = (uint32_t)strtoul(cmd + 10, NULL, 10);
        if (league_id != 0u) {
            g_kbo_hub_selected_league_id = league_id;
            g_kbo_hub_selected_team_id = 0;
            g_kbo_hub_open_dropdown = 0;
            kbo_hub_ensure_valid_selection();
        }
        kbo_webview_navigate_current();
        return 1;
    }
    if (strncmp(cmd, "setteam/", 8) == 0) {
        uint32_t team_id = (uint32_t)strtoul(cmd + 8, NULL, 10);
        if (team_id != 0u) {
            g_kbo_hub_selected_team_id = team_id;
            g_kbo_hub_open_dropdown = 0;
        }
        kbo_webview_navigate_current();
        return 1;
    }
    if (strcmp(cmd, "league/") == 0 || strcmp(cmd, "league") == 0) {
        (void)hwnd;
        g_kbo_hub_open_dropdown = g_kbo_hub_open_dropdown == 1 ? 0 : 1;
        kbo_webview_navigate_current();
        return 1;
    }
    if (strcmp(cmd, "team/") == 0 || strcmp(cmd, "team") == 0) {
        (void)hwnd;
        g_kbo_hub_open_dropdown = g_kbo_hub_open_dropdown == 2 ? 0 : 2;
        kbo_webview_navigate_current();
        return 1;
    }

    return 0;
}
