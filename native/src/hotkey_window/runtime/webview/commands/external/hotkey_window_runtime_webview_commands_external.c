#include "../../hotkey_window_webview.h"

int kbo_webview_handle_external_or_foreign_command(const char* cmd, HWND hwnd)
{

    if (ascii_equals_ignore_case(cmd, "github")) {
        ShellExecuteA(
            hwnd,
            "open",
            "https://github.com/lebronisbest623/OOTP27_Ultimate_KBO",
            NULL,
            NULL,
            SW_SHOWNORMAL);
        return 1;
    }
    if (strncmp(cmd, "select/", 7) == 0) {
        uint32_t player_id = (uint32_t)strtoul(cmd + 7, NULL, 10);
        if (player_id != 0u) {
            g_kbo_hub_selected_foreign_player_id = player_id;
            kbo_log_runtimef("foreign rights webview: selected player=%u", player_id);
        }
        kbo_webview_navigate_current();
        return 1;
    }
    if (strncmp(cmd, "retain/", 7) == 0 || strncmp(cmd, "release/", 8) == 0) {
        int retain = strncmp(cmd, "retain/", 7) == 0;
        const char* id_text = retain ? cmd + 7 : cmd + 8;
        uint32_t player_id = (uint32_t)strtoul(id_text, NULL, 10);
        if (player_id != 0u) {
            g_kbo_hub_selected_foreign_player_id = player_id;
            if (kbo_webview_team_action_allowed(g_kbo_hub_selected_team_id, retain ? "hub_foreign_retain" : "hub_foreign_release")) {
                kbo_apply_foreign_rights_button(retain);
            }
        }
        kbo_webview_navigate_current();
        return 1;
    }
    return 0;
}
