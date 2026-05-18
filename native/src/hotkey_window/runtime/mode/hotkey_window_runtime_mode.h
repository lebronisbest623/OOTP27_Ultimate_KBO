#ifndef KBOFIX_SRC_HOTKEY_WINDOW_RUNTIME_MODE_HOTKEY_WINDOW_RUNTIME_MODE_H_
#define KBOFIX_SRC_HOTKEY_WINDOW_RUNTIME_MODE_HOTKEY_WINDOW_RUNTIME_MODE_H_

#define KBO_HUB_MODE_RELEASE   0
#define KBO_HUB_MODE_DEVELOPER 1

int kbo_hub_current_mode_is_developer(void);
int kbo_hub_developer_mode_enabled(void);
int kbo_hub_requested_mode_available(int mode);
int kbo_hub_set_mode(int mode);
int kbo_hub_view_available_for_current_mode(int view);
int kbo_hub_mod_subview_available_for_current_mode(int subview);
const char* kbo_hub_mode_log_label(void);

#endif
