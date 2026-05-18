#ifndef KBO_HOTKEY_WINDOW_UI_MOD_INFO_VIEWS_INTERNAL_H
#define KBO_HOTKEY_WINDOW_UI_MOD_INFO_VIEWS_INTERNAL_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "../../../../build_verify/build_verify.h"
#include "../../../../core/core_flags/api/flags_api.h"
#include "../../../../core/core_flags/localappdata/localappdata_reader.h"
#include "../../../support/assets/paths/ui_asset_paths.h"
#include "../../../support/assets/paths/ui_image_sources.h"
#include "../../../support/text/language/ui_language.h"
#include "ui_mod_info_views.h"
#include "../../../support/text/buffer/ui_text_buffer.h"

enum {
    KBO_MOD_FLAG_USER = 0,
    KBO_MOD_FLAG_RECOVERY = 1,
    KBO_MOD_FLAG_DIAGNOSTIC = 2
};

struct KboModRuntimeFlagSetting {
    const char* key;
    const char* label;
    int enabled_value;
    int default_enabled;
    const char* companion_enable_key;
    int category;
};

void kbo_webview_append_mod_runtime_flag_group(KboWindowTextBuffer* buffer, int category, const char* title, const char* help_text);

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
void kbo_webview_append_mod_tester_credits(KboWindowTextBuffer* buffer);
void kbo_webview_append_mod_info_view(KboWindowTextBuffer* buffer, int selected_mod_subview);
void kbo_webview_append_settings_view(KboWindowTextBuffer* buffer, int selected_settings_subview);

#endif
