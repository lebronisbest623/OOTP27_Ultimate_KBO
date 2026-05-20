#ifndef KBOFIX_SRC_HOTKEY_WINDOW_RUNTIME_WEBVIEW_PLAYER_TOOLTIP_INTERNAL_H_
#define KBOFIX_SRC_HOTKEY_WINDOW_RUNTIME_WEBVIEW_PLAYER_TOOLTIP_INTERNAL_H_

#include "../hotkey_window_webview.h"

#define KBO_PLAYER_TOOLTIP_CLASS_NAME "OOTPKBOPlayerTooltipWindow"
#define KBO_PLAYER_TOOLTIP_HTML_MAX   (128u * 1024u)
#define KBO_PLAYER_TOOLTIP_WIDTH      430
#define KBO_PLAYER_TOOLTIP_HEIGHT     252
#define KBO_PLAYER_TOOLTIP_MARGIN     8
#define KBO_PLAYER_TOOLTIP_WM_START   (WM_APP + 0x53au)

extern HWND g_kbo_player_tooltip_hwnd;
extern ICoreWebView2Controller* g_kbo_player_tooltip_controller;
extern ICoreWebView2* g_kbo_player_tooltip_webview;
extern LONG g_kbo_player_tooltip_creating;
extern HWND g_kbo_player_tooltip_owner;
extern int g_kbo_player_tooltip_plain_host;
extern POINT g_kbo_player_tooltip_anchor;
extern int g_kbo_player_tooltip_width;
extern int g_kbo_player_tooltip_height;
extern uint32_t g_kbo_player_tooltip_seq;
extern char g_kbo_player_tooltip_asset_folder[MAX_PATH];
extern char g_kbo_player_tooltip_pending_html[];

WCHAR* kbo_tooltip_alloc_wide_from_utf8(const char* text);
void kbo_tooltip_apply_asset_mapping(void);
void kbo_tooltip_apply_bounds(int show);
int kbo_tooltip_ensure_window(HWND owner);
void kbo_tooltip_navigate_pending(void);
int kbo_tooltip_start_controller(void);
int kbo_tooltip_schedule_controller_start(void);

#endif
