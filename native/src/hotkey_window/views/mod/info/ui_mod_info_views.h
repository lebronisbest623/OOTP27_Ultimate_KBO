#ifndef KBO_HOTKEY_WINDOW_UI_MOD_INFO_VIEWS_H
#define KBO_HOTKEY_WINDOW_UI_MOD_INFO_VIEWS_H

#include "../../../support/text/buffer/ui_text_buffer.h"

#define KBO_HUB_MOD_SUBVIEW_README        0
#define KBO_HUB_MOD_SUBVIEW_LICENSE       1
#define KBO_HUB_MOD_SUBVIEW_CREDITS       2
#define KBO_HUB_MOD_SUBVIEW_CONTRIBUTIONS 3
#define KBO_HUB_MOD_SUBVIEW_SETTINGS      4
#define KBO_HUB_MOD_SUBVIEW_COUNT         5

typedef struct KboModRuntimeFlagSetting KboModRuntimeFlagSetting;

const KboModRuntimeFlagSetting* kbo_find_mod_runtime_flag_setting(const char* key);
int kbo_set_mod_runtime_flag_enabled(const KboModRuntimeFlagSetting* setting, int enabled);
void kbo_webview_append_mod_info_view(KboWindowTextBuffer* buffer, int selected_mod_subview);
void kbo_webview_append_settings_view(KboWindowTextBuffer* buffer, int selected_settings_subview);

#endif
