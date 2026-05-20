#ifndef KBOFIX_SRC_HOTKEY_WINDOW_RUNTIME_WEBVIEW_PLAYER_TOOLTIP_HOTKEY_WINDOW_PLAYER_TOOLTIP_H_
#define KBOFIX_SRC_HOTKEY_WINDOW_RUNTIME_WEBVIEW_PLAYER_TOOLTIP_HOTKEY_WINDOW_PLAYER_TOOLTIP_H_

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>

#define KBO_PLAYER_TOOLTIP_ASSET_HOST "ootp-kbo-player-tooltip.local"

void kbo_set_webview_player_tooltip_asset_folder(const char* folder_path);
int kbo_show_webview_player_tooltip_popup(HWND owner, int screen_x, int screen_y, uint32_t hover_seq, const char* html);
void kbo_hide_webview_player_tooltip_popup(uint32_t hover_seq);
void kbo_destroy_webview_player_tooltip_popup(void);

#endif
