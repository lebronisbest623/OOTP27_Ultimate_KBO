#include "../info/ui_mod_info_views.h"
#include "runtime_flags.generated.h"

#include <stdio.h>
#include <string.h>

#include "../../../../core/core_flags/localappdata/localappdata_reader.h"

const KboModRuntimeFlagSetting* kbo_find_mod_runtime_flag_setting(const char* key)
{
    if (key == NULL || key[0] == '\0') {
        return NULL;
    }
    for (size_t i = 0; i < KBO_MOD_RUNTIME_FLAG_SETTINGS_COUNT; i++) {
        if (strcmp(KBO_MOD_RUNTIME_FLAG_SETTINGS[i].key, key) == 0) {
            return &KBO_MOD_RUNTIME_FLAG_SETTINGS[i];
        }
    }
    return NULL;
}

int kbo_get_mod_runtime_flag_enabled(const KboModRuntimeFlagSetting* setting)
{
    if (setting == NULL || setting->key == NULL || setting->key[0] == '\0') {
        return 0;
    }
    int raw_value = setting->enabled_value ? 0 : 1;
    if (!kbo_read_localappdata_json_flag_value(setting->key, &raw_value)) {
        return setting->default_enabled ? 1 : 0;
    }
    return raw_value == setting->enabled_value ? 1 : 0;
}

int kbo_set_mod_runtime_flag_enabled(const KboModRuntimeFlagSetting* setting, int enabled)
{
    if (setting == NULL || setting->key == NULL || setting->key[0] == '\0') {
        return 0;
    }
    int raw_value = enabled ? setting->enabled_value : (setting->enabled_value ? 0 : 1);
    int ok = kbo_write_localappdata_json_int_value(setting->key, raw_value);
    if (ok && enabled && setting->companion_enable_key != NULL && setting->companion_enable_key[0] != '\0') {
        ok = kbo_write_localappdata_json_int_value(setting->companion_enable_key, 1);
    }
    return ok;
}

void kbo_webview_append_mod_runtime_flag_row(KboWindowTextBuffer* buffer, const KboModRuntimeFlagSetting* setting)
{
    if (buffer == NULL || setting == NULL || setting->key == NULL || setting->label == NULL) {
        return;
    }
    int enabled = kbo_get_mod_runtime_flag_enabled(setting);
    kbo_window_text_appendf(
        buffer,
        "<div class='settingRow flagSettingRow'><label class='settingLabel' for='flag_%s'>",
        setting->key);
    kbo_html_append_escaped(buffer, setting->label);
    char id[128] = {0};
    char href_on[192] = {0};
    char href_off[192] = {0};
    snprintf(id, sizeof(id), "flag_%s", setting->key);
    snprintf(href_on, sizeof(href_on), "kbo://mod/settings/flag/%s/on", setting->key);
    snprintf(href_off, sizeof(href_off), "kbo://mod/settings/flag/%s/off", setting->key);
    kbo_window_text_appendf(buffer, "</label>");
    kbo_webview_begin_ootp_choice(buffer, id, enabled ? "켬" : "끔");
    kbo_webview_append_ootp_choice_option(buffer, href_on, "켬", enabled);
    kbo_webview_append_ootp_choice_option(buffer, href_off, "끔", !enabled);
    kbo_webview_end_ootp_choice(buffer);
    kbo_window_text_appendf(buffer, "</div>");
}

void kbo_webview_append_mod_runtime_flag_group(
    KboWindowTextBuffer* buffer,
    int category,
    const char* title,
    const char* help_text)
{
    if (buffer == NULL || title == NULL) {
        return;
    }
    kbo_window_text_appendf(buffer, "<div class='flagGroup'><h3>");
    kbo_html_append_escaped(buffer, title);
    kbo_window_text_appendf(buffer, "</h3>");
    if (help_text != NULL && help_text[0] != '\0') {
        kbo_window_text_appendf(buffer, "<p>");
        kbo_html_append_escaped(buffer, help_text);
        kbo_window_text_appendf(buffer, "</p>");
    }
    for (size_t i = 0; i < KBO_MOD_RUNTIME_FLAG_SETTINGS_COUNT; i++) {
        if (KBO_MOD_RUNTIME_FLAG_SETTINGS[i].category == category) {
            kbo_webview_append_mod_runtime_flag_row(buffer, &KBO_MOD_RUNTIME_FLAG_SETTINGS[i]);
        }
    }
    kbo_window_text_appendf(buffer, "</div>");
}

