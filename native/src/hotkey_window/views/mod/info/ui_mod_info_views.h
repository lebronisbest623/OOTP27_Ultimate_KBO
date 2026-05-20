#ifndef KBO_HOTKEY_WINDOW_UI_MOD_INFO_VIEWS_H
#define KBO_HOTKEY_WINDOW_UI_MOD_INFO_VIEWS_H

#include "../../../support/text/buffer/ui_text_buffer.h"

#define KBO_HUB_MOD_SUBVIEW_README        0
#define KBO_HUB_MOD_SUBVIEW_LICENSE       1
#define KBO_HUB_MOD_SUBVIEW_CREDITS       2
#define KBO_HUB_MOD_SUBVIEW_CONTRIBUTIONS 3
#define KBO_HUB_MOD_SUBVIEW_SETTINGS      4
#define KBO_HUB_MOD_SUBVIEW_COUNT         5

enum {
    KBO_MOD_FLAG_USER = 0,
    KBO_MOD_FLAG_RECOVERY = 1,
    KBO_MOD_FLAG_DIAGNOSTIC = 2
};

typedef struct KboModRuntimeFlagSetting {
    const char* key;
    const char* label;
    int enabled_value;
    int default_enabled;
    const char* companion_enable_key;
    int category;
} KboModRuntimeFlagSetting;

const KboModRuntimeFlagSetting* kbo_find_mod_runtime_flag_setting(const char* key);
int kbo_get_mod_runtime_flag_enabled(const KboModRuntimeFlagSetting* setting);
int kbo_set_mod_runtime_flag_enabled(const KboModRuntimeFlagSetting* setting, int enabled);
void kbo_webview_append_mod_runtime_flag_row(KboWindowTextBuffer* buffer, const KboModRuntimeFlagSetting* setting);
void kbo_webview_append_mod_runtime_flag_group(
    KboWindowTextBuffer* buffer,
    int category,
    const char* title,
    const char* help_text);

void kbo_webview_begin_ootp_choice(KboWindowTextBuffer* buffer, const char* id, const char* current_label);
void kbo_webview_append_ootp_choice_option(
    KboWindowTextBuffer* buffer,
    const char* href,
    const char* label,
    int selected);
void kbo_webview_end_ootp_choice(KboWindowTextBuffer* buffer);

void kbo_webview_append_mod_info_view(KboWindowTextBuffer* buffer, int selected_mod_subview);
void kbo_webview_append_settings_view(KboWindowTextBuffer* buffer, int selected_settings_subview);

#endif
