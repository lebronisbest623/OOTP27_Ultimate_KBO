#include "hotkey_window_runtime_window_internal.h"

LONG g_kbo_hotkey_window_started = 0;
HWND g_kbo_hotkey_window = NULL;
HWND g_kbo_hotkey_edit = NULL;
HWND g_kbo_foreign_list = NULL;
HWND g_kbo_foreign_keep_button = NULL;
HWND g_kbo_foreign_release_button = NULL;
ICoreWebView2Environment* g_kbo_webview_environment = NULL;
ICoreWebView2Controller* g_kbo_webview_controller = NULL;
ICoreWebView2* g_kbo_webview = NULL;
LONG g_kbo_webview_starting = 0;
LONG g_kbo_webview_ready = 0;
LONG g_kbo_webview_failed = 0;
DWORD g_kbo_hotkey_thread_id = 0;
HINSTANCE g_kbo_hotkey_instance = NULL;
HHOOK g_kbo_hotkey_keyboard_hook = NULL;
WNDPROC g_kbo_hotkey_edit_original_proc = NULL;
ULONGLONG g_kbo_hotkey_last_toggle_ms = 0;

HFONT g_kbo_hub_font_title = NULL;
HFONT g_kbo_hub_font_body = NULL;
HFONT g_kbo_hub_font_mono = NULL;
HFONT g_kbo_hub_font_small = NULL;
HBRUSH g_kbo_hub_brush_bg = NULL;
HBRUSH g_kbo_hub_brush_header = NULL;
HBRUSH g_kbo_hub_brush_panel = NULL;
HBRUSH g_kbo_hub_brush_panel_alt = NULL;
HBRUSH g_kbo_hub_brush_nav = NULL;
HBRUSH g_kbo_hub_brush_nav_active = NULL;
HBRUSH g_kbo_hub_brush_accent = NULL;

int g_kbo_hub_selected_view = 0;
int g_kbo_hub_selected_mod_subview = 0;
int g_kbo_hub_selected_foreign_subview = 0;
int g_kbo_hub_selected_agames_subview = 0;
int g_kbo_hub_selected_military_subview = 0;
int g_kbo_hub_selected_fa_subview = 0;
int g_kbo_hub_selected_fa_compensation_subview = 0;
int g_kbo_hub_selected_cbt_subview = 0;
int g_kbo_hub_selected_futures_subview = 0;
int g_kbo_hub_fa_market_filter = 0;
int g_kbo_hub_fa_market_position_filter = 0;
uint32_t g_kbo_hub_selected_fa_compensation_player_id = 0u;
uint32_t g_kbo_hub_selected_agames_roster_year = 0;
uint32_t g_kbo_hub_selected_military_results_year = 0;
RECT g_kbo_hub_refresh_rect = {0, 0, 0, 0};
RECT g_kbo_hub_language_rect = {0, 0, 0, 0};
RECT g_kbo_hub_github_rect = {0, 0, 0, 0};
RECT g_kbo_hub_lang_ko_rect = {0, 0, 0, 0};
RECT g_kbo_hub_lang_en_rect = {0, 0, 0, 0};
RECT g_kbo_hub_league_dropdown_rect = {0, 0, 0, 0};
RECT g_kbo_hub_team_dropdown_rect = {0, 0, 0, 0};
RECT g_kbo_hub_content_rect = {0, 0, 0, 0};
RECT g_kbo_hub_scrollbar_rect = {0, 0, 0, 0};
RECT g_kbo_hub_scrollbar_less_rect = {0, 0, 0, 0};
RECT g_kbo_hub_scrollbar_more_rect = {0, 0, 0, 0};
RECT g_kbo_hub_scrollbar_thumb_rect = {0, 0, 0, 0};
RECT g_kbo_hub_foreign_retain_rect = {0, 0, 0, 0};
RECT g_kbo_hub_foreign_skip_rect = {0, 0, 0, 0};
uint32_t g_kbo_hub_selected_league_id = 0;
uint32_t g_kbo_hub_selected_team_id = 0;
uint32_t g_kbo_hub_selected_foreign_player_id = 0;
int g_kbo_hub_open_dropdown = 0;

LONG g_kbo_hub_skin_assets_loaded = 0;
HBITMAP g_kbo_hub_asset_github = NULL;
HBITMAP g_kbo_hub_asset_menu_arrow = NULL;
HBITMAP g_kbo_hub_asset_sb_bar_top = NULL;
HBITMAP g_kbo_hub_asset_sb_bar_mid = NULL;
HBITMAP g_kbo_hub_asset_sb_bar_bottom = NULL;
HBITMAP g_kbo_hub_asset_sb_less = NULL;
HBITMAP g_kbo_hub_asset_sb_more = NULL;
HBITMAP g_kbo_hub_asset_sb_slider_top = NULL;
HBITMAP g_kbo_hub_asset_sb_slider_mid = NULL;
HBITMAP g_kbo_hub_asset_sb_slider_bottom = NULL;

#define KBO_WM_TOGGLE_SERVICE_MONITOR (WM_APP + 0x4b0u)
#define KBO_HUB_CONTROL_EDIT            3
#define KBO_HUB_CONTROL_FOREIGN_LIST    41
#define KBO_HUB_CONTROL_FOREIGN_KEEP    42
#define KBO_HUB_CONTROL_FOREIGN_RELEASE 43
#define KBO_HUB_FIXED_CLIENT_WIDTH      1280
#define KBO_HUB_FIXED_CLIENT_HEIGHT     720
#define KBO_HUB_MIN_CLIENT_WIDTH        960
#define KBO_HUB_MIN_CLIENT_HEIGHT       560

#define KBO_HUB_VIEW_MOD_INFO    0
#define KBO_HUB_VIEW_MILITARY    1
#define KBO_HUB_VIEW_FOREIGN_RIGHTS 2
#define KBO_HUB_VIEW_ASIAN_QUOTA 3
#define KBO_HUB_VIEW_ASIAN_GAMES 4
#define KBO_HUB_VIEW_UPCOMING_FA 5
#define KBO_HUB_VIEW_FA_CASES   6
#define KBO_HUB_VIEW_SETTINGS    7
#define KBO_HUB_VIEW_REPUTATION  8
#define KBO_HUB_VIEW_CBT         9
#define KBO_HUB_VIEW_FUTURES_LEAGUE 10
#define KBO_HUB_NAV_COUNT        11
#define KBO_HUB_FOREIGN_SUBVIEW_ROSTER 0
#define KBO_HUB_FOREIGN_SUBVIEW_RIGHTS 1
#define KBO_HUB_FOREIGN_SUBVIEW_COUNT  2
#define KBO_HUB_MOD_SUBVIEW_README        0
#define KBO_HUB_MOD_SUBVIEW_LICENSE       1
#define KBO_HUB_MOD_SUBVIEW_CREDITS       2
#define KBO_HUB_MOD_SUBVIEW_CONTRIBUTIONS 3
#define KBO_HUB_MOD_SUBVIEW_SETTINGS      4
#define KBO_HUB_MOD_SUBVIEW_COUNT         5

const COLORREF KBO_HUB_COLOR_BG         = RGB(0x0A, 0x0A, 0x0A);
const COLORREF KBO_HUB_COLOR_HEADER      = RGB(0x1D, 0x55, 0x6C);
const COLORREF KBO_HUB_COLOR_NAV         = RGB(0x14, 0x14, 0x14);
const COLORREF KBO_HUB_COLOR_NAV_ACTIVE  = RGB(0x1D, 0x55, 0x6C);
const COLORREF KBO_HUB_COLOR_PANEL       = RGB(0x16, 0x16, 0x16);
const COLORREF KBO_HUB_COLOR_PANEL_ALT   = RGB(0x22, 0x22, 0x22);
const COLORREF KBO_HUB_COLOR_TEXT        = RGB(0xFC, 0xFC, 0xFC);
const COLORREF KBO_HUB_COLOR_MUTED       = RGB(0x8F, 0x8F, 0x8F);
const COLORREF KBO_HUB_COLOR_ACCENT      = RGB(0xDE, 0x6D, 0x1F);
#include "../../state/team_vector/state_team_vector.h"

