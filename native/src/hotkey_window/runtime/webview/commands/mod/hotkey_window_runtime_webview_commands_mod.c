#include "../../hotkey_window_webview_internal.h"
#include "../../../../../perf_snapshot/perf_snapshot.h"

int kbo_webview_handle_mod_settings_command(const char* cmd)
{

    if (strncmp(cmd, "mod/settings/lang/", 18) == 0) {
        const char* lang = cmd + 18;
        if (ascii_equals_ignore_case(lang, "en") || ascii_equals_ignore_case(lang, "english")) {
            kbo_hub_set_language(KBO_HUB_LANG_EN);
            kbo_hub_save_language_setting();
        } else if (ascii_equals_ignore_case(lang, "ko") || ascii_equals_ignore_case(lang, "korean")) {
            kbo_hub_set_language(KBO_HUB_LANG_KO);
            kbo_hub_save_language_setting();
        }
        g_kbo_hub_selected_view = KBO_HUB_VIEW_MOD_INFO;
        g_kbo_hub_selected_mod_subview = KBO_HUB_MOD_SUBVIEW_SETTINGS;
        g_kbo_hub_open_dropdown = 0;
        kbo_webview_navigate_current();
        return 1;
    }
    if (strncmp(cmd, "mod/settings/custom-news-language/", 34) == 0) {
        const char* lang = cmd + 34;
        int language = ascii_equals_ignore_case(lang, "en") || ascii_equals_ignore_case(lang, "english")
            ? KBO_CUSTOM_NEWS_LANGUAGE_EN
            : KBO_CUSTOM_NEWS_LANGUAGE_KO;
        if (kbo_set_custom_news_language_setting(language)) {
            kbo_log_runtimef("mod settings webview: custom news language=%s", language == KBO_CUSTOM_NEWS_LANGUAGE_EN ? "en" : "ko");
        } else {
            kbo_log_runtimef("mod settings webview: failed to write custom news language=%s", language == KBO_CUSTOM_NEWS_LANGUAGE_EN ? "en" : "ko");
        }
        g_kbo_hub_selected_view = KBO_HUB_VIEW_MOD_INFO;
        g_kbo_hub_selected_mod_subview = KBO_HUB_MOD_SUBVIEW_SETTINGS;
        g_kbo_hub_open_dropdown = 0;
        kbo_webview_navigate_current();
        return 1;
    }
    if (strncmp(cmd, "mod/settings/profiler/", 22) == 0) {
        const char* value = cmd + 22;
        int enabled = ascii_equals_ignore_case(value, "on")
            || ascii_equals_ignore_case(value, "1")
            || ascii_equals_ignore_case(value, "true");
        if (kbo_set_profiler_enabled_setting(enabled)) {
            kbo_profiler_reset_enabled_cache();
            kbo_log_runtimef("mod settings webview: profiler enabled=%d", enabled ? 1 : 0);
        } else {
            kbo_log_runtimef("mod settings webview: failed to write profiler enabled=%d", enabled ? 1 : 0);
        }
        g_kbo_hub_selected_view = KBO_HUB_VIEW_MOD_INFO;
        g_kbo_hub_selected_mod_subview = KBO_HUB_MOD_SUBVIEW_SETTINGS;
        g_kbo_hub_open_dropdown = 0;
        kbo_webview_navigate_current();
        return 1;
    }
    if (strncmp(cmd, "mod/settings/", 13) == 0
            && !kbo_hub_current_mode_is_developer()) {
        kbo_log_runtimef("mod settings webview: blocked release-mode command=%s", cmd);
        g_kbo_hub_selected_view = KBO_HUB_VIEW_MOD_INFO;
        g_kbo_hub_selected_mod_subview = KBO_HUB_MOD_SUBVIEW_SETTINGS;
        g_kbo_hub_open_dropdown = 0;
        kbo_webview_navigate_current();
        return 1;
    }
    if (strcmp(cmd, "mod/settings/perf-snapshot/dump") == 0) {
        char snapshot_path[MAX_PATH] = {0};
        int ok = kbo_dump_perf_player_snapshot(
            "hub_perf_snapshot_dump",
            snapshot_path,
            sizeof(snapshot_path));
        kbo_log_runtimef(
            "mod settings webview: perf snapshot dump ok=%d path=%s",
            ok ? 1 : 0,
            snapshot_path);
        g_kbo_hub_selected_view = KBO_HUB_VIEW_MOD_INFO;
        g_kbo_hub_selected_mod_subview = KBO_HUB_MOD_SUBVIEW_SETTINGS;
        g_kbo_hub_open_dropdown = 0;
        kbo_webview_navigate_current();
        return 1;
    }
    if (strncmp(cmd, "mod/settings/ui-team-actions/", 29) == 0) {
        const char* value = cmd + 29;
        int allow_all = ascii_equals_ignore_case(value, "all")
            || ascii_equals_ignore_case(value, "on")
            || ascii_equals_ignore_case(value, "1")
            || ascii_equals_ignore_case(value, "true");
        if (kbo_set_allow_all_ui_team_actions_setting(allow_all)) {
            kbo_log_runtimef("mod settings webview: allow all UI team actions=%d", allow_all ? 1 : 0);
        } else {
            kbo_log_runtimef("mod settings webview: failed to write allow all UI team actions=%d", allow_all ? 1 : 0);
        }
        g_kbo_hub_selected_view = KBO_HUB_VIEW_MOD_INFO;
        g_kbo_hub_selected_mod_subview = KBO_HUB_MOD_SUBVIEW_SETTINGS;
        g_kbo_hub_open_dropdown = 0;
        kbo_webview_navigate_current();
        return 1;
    }
    if (strncmp(cmd, "mod/settings/flag/", 18) == 0) {
        const char* key = cmd + 18;
        const char* slash = strchr(key, '/');
        if (slash != NULL && slash > key) {
            char key_buffer[128] = {0};
            size_t key_len = (size_t)(slash - key);
            if (key_len >= sizeof(key_buffer)) {
                key_len = sizeof(key_buffer) - 1u;
            }
            memcpy(key_buffer, key, key_len);
            key_buffer[key_len] = '\0';

            const char* value = slash + 1;
            int enabled = ascii_equals_ignore_case(value, "on")
                || ascii_equals_ignore_case(value, "1")
                || ascii_equals_ignore_case(value, "true")
                || ascii_equals_ignore_case(value, "enabled");
            const KboModRuntimeFlagSetting* setting = kbo_find_mod_runtime_flag_setting(key_buffer);
            if (setting != NULL && kbo_set_mod_runtime_flag_enabled(setting, enabled)) {
                kbo_log_runtimef("mod settings webview: runtime flag %s enabled=%d", key_buffer, enabled ? 1 : 0);
            } else {
                kbo_log_runtimef("mod settings webview: failed to write runtime flag %s enabled=%d", key_buffer, enabled ? 1 : 0);
            }
        }
        g_kbo_hub_selected_view = KBO_HUB_VIEW_MOD_INFO;
        g_kbo_hub_selected_mod_subview = KBO_HUB_MOD_SUBVIEW_SETTINGS;
        g_kbo_hub_open_dropdown = 0;
        kbo_webview_navigate_current();
        return 1;
    }
    if (strncmp(cmd, "mod/", 4) == 0) {
        int subview = atoi(cmd + 4);
        if (subview >= 0 && subview < KBO_HUB_MOD_SUBVIEW_COUNT) {
            g_kbo_hub_selected_view = KBO_HUB_VIEW_MOD_INFO;
            g_kbo_hub_selected_mod_subview = subview;
            g_kbo_hub_open_dropdown = 0;
        }
        kbo_webview_navigate_current();
        return 1;
    }
    return 0;
}
