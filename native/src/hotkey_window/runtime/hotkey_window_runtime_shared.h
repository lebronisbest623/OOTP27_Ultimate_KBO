#ifndef KBOFIX_SRC_HOTKEY_WINDOW_RUNTIME_SHARED_H_
#define KBOFIX_SRC_HOTKEY_WINDOW_RUNTIME_SHARED_H_

#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <tlhelp32.h>
#include <objbase.h>
#include <wincodec.h>
#include <shellapi.h>
#include <WebView2.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <intrin.h>

#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../bootstrap/abi/ootp_typedefs.h"
#include "../../bootstrap/profiling/profiler.h"
#include "../../build_verify/build_verify.h"
#include "../../core/files/atomic/core_atomic_file.h"
#include "../../core/dates/core_current_date.h"
#include "../../core/core_flags/api/flags_api.h"
#include "../../core/core_flags/localappdata/localappdata_reader.h"
#include "../../core/core_league_context_parts/event_manager/event_manager.h"
#include "../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../core/news/live/core_live_news.h"
#include "../../core/logging/core_log.h"
#include "../../core/files/save_paths/core_save_paths.h"
#include "../../core/dates/core_text_date.h"
#include "../../runtime_memory/runtime_memory.h"
#include "../../team/lookup/team_lookup.h"
#include "../../team/assignment/org_query/team_org_assignment_query.h"
#include "../../team/control/team_human_control.h"
#include "../../team/names/team_name_cache.h"
#include "../../team/names/team_string.h"
#include "../hotkey_window.h"
#include "../support/assets/paths/ui_asset_paths.h"
#include "../views/asian_games/ui_asian_games_view.h"
#include "../state/league_lookup/api/state_league_lookup.h"
#include "../state/team_vector/state_team_vector.h"
#include "../state/text/state_text_utils.h"
#include "../support/assets/names/support_names.h"
#include "../support/assets/logos/support_logos.h"
#include "../support/text/date/ui_date_format.h"
#include "../views/fa/ui_fa_views.h"
#include "../views/foreign/ui_foreign_rights_view.h"
#include "../support/assets/paths/ui_image_sources.h"
#include "../support/text/js/ui_js_string.h"
#include "../support/text/language/ui_language.h"
#include "../views/mod/info/ui_mod_info_views.h"
#include "../views/military/api/ui_military_view.h"
#include "../support/assets/nations/ui_nation_helpers.h"
#include "../support/roster/cells/ui_roster_cells.h"
#include "../views/mod/reputation/ui_reputation_view.h"
#include "../views/cbt/ui_cbt_view.h"
#include "../views/futures/ui_futures_league_view.h"
#include "../support/roster/sort/ui_roster_sort_script.h"
#include "../support/roster/table/ui_roster_table_css.h"
#include "../support/skin/ui_scrollbar_skin_css.h"
#include "../support/skin/ui_skin_metrics.h"
#include "../support/text/buffer/ui_text_buffer.h"
#include "../support/assets/names/ui_uniform_numbers.h"
#include "../support/skin_assets/bitmap_draw.h"
#include "../ui_html_helpers/position_helpers.h"
#include "mode/hotkey_window_runtime_mode.h"
#include "hotkey_window_domain_contract.h"

#define KBO_WM_TOGGLE_SERVICE_MONITOR (WM_APP + 0x4b0u)
#define KBO_WM_REFRESH_HUB            (WM_APP + 0x4b1u)
#define KBO_WM_SHOW_HUB_CONTENT       (WM_APP + 0x4b2u)
#define KBO_WM_WEBVIEW_DISABLE        (WM_APP + 0x4b3u)
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
#define KBO_HUB_SETTINGS_SUBVIEW_LEAGUE   0
#define KBO_HUB_SETTINGS_SUBVIEW_COUNT    1
extern LONG g_kbo_hotkey_window_started;
extern HWND g_kbo_hotkey_window;
extern HWND g_kbo_hotkey_edit;
extern HWND g_kbo_foreign_list;
extern HWND g_kbo_foreign_keep_button;
extern HWND g_kbo_foreign_release_button;
extern ICoreWebView2Environment* g_kbo_webview_environment;
extern ICoreWebView2Controller* g_kbo_webview_controller;
extern ICoreWebView2* g_kbo_webview;
extern LONG g_kbo_webview_starting;
extern LONG g_kbo_webview_ready;
extern LONG g_kbo_webview_failed;
extern DWORD g_kbo_hotkey_thread_id;
extern HINSTANCE g_kbo_hotkey_instance;
extern HHOOK g_kbo_hotkey_keyboard_hook;
extern WNDPROC g_kbo_hotkey_edit_original_proc;
extern ULONGLONG g_kbo_hotkey_last_toggle_ms;
extern HFONT g_kbo_hub_font_title;
extern HFONT g_kbo_hub_font_body;
extern HFONT g_kbo_hub_font_mono;
extern HFONT g_kbo_hub_font_small;
extern HBRUSH g_kbo_hub_brush_bg;
extern HBRUSH g_kbo_hub_brush_header;
extern HBRUSH g_kbo_hub_brush_panel;
extern HBRUSH g_kbo_hub_brush_panel_alt;
extern HBRUSH g_kbo_hub_brush_nav;
extern HBRUSH g_kbo_hub_brush_nav_active;
extern HBRUSH g_kbo_hub_brush_accent;
extern int g_kbo_hub_selected_view;
extern int g_kbo_hub_selected_mod_subview;
extern int g_kbo_hub_selected_foreign_subview;
extern int g_kbo_hub_selected_agames_subview;
extern int g_kbo_hub_selected_military_subview;
extern int g_kbo_hub_selected_fa_subview;
extern int g_kbo_hub_selected_fa_compensation_subview;
extern int g_kbo_hub_selected_cbt_subview;
extern int g_kbo_hub_selected_futures_subview;
extern int g_kbo_hub_selected_settings_subview;
extern int g_kbo_hub_mode;
extern int g_kbo_hub_fa_market_filter;
extern int g_kbo_hub_fa_market_position_filter;
extern uint32_t g_kbo_hub_selected_fa_compensation_player_id;
extern uint32_t g_kbo_hub_selected_agames_roster_year;
extern uint32_t g_kbo_hub_selected_military_results_year;
extern RECT g_kbo_hub_refresh_rect;
extern RECT g_kbo_hub_language_rect;
extern RECT g_kbo_hub_github_rect;
extern RECT g_kbo_hub_lang_ko_rect;
extern RECT g_kbo_hub_lang_en_rect;
extern RECT g_kbo_hub_league_dropdown_rect;
extern RECT g_kbo_hub_team_dropdown_rect;
extern RECT g_kbo_hub_content_rect;
extern RECT g_kbo_hub_scrollbar_rect;
extern RECT g_kbo_hub_scrollbar_less_rect;
extern RECT g_kbo_hub_scrollbar_more_rect;
extern RECT g_kbo_hub_scrollbar_thumb_rect;
extern RECT g_kbo_hub_foreign_retain_rect;
extern RECT g_kbo_hub_foreign_skip_rect;
extern uint32_t g_kbo_hub_selected_league_id;
extern uint32_t g_kbo_hub_selected_team_id;
extern uint32_t g_kbo_hub_selected_foreign_player_id;
extern int g_kbo_hub_open_dropdown;
extern LONG g_kbo_hub_skin_assets_loaded;
extern HBITMAP g_kbo_hub_asset_github;
extern HBITMAP g_kbo_hub_asset_menu_arrow;
extern HBITMAP g_kbo_hub_asset_sb_bar_top;
extern HBITMAP g_kbo_hub_asset_sb_bar_mid;
extern HBITMAP g_kbo_hub_asset_sb_bar_bottom;
extern HBITMAP g_kbo_hub_asset_sb_less;
extern HBITMAP g_kbo_hub_asset_sb_more;
extern HBITMAP g_kbo_hub_asset_sb_slider_top;
extern HBITMAP g_kbo_hub_asset_sb_slider_mid;
extern HBITMAP g_kbo_hub_asset_sb_slider_bottom;
extern const COLORREF KBO_HUB_COLOR_BG;
extern const COLORREF KBO_HUB_COLOR_HEADER;
extern const COLORREF KBO_HUB_COLOR_NAV;
extern const COLORREF KBO_HUB_COLOR_NAV_ACTIVE;
extern const COLORREF KBO_HUB_COLOR_PANEL;
extern const COLORREF KBO_HUB_COLOR_PANEL_ALT;
extern const COLORREF KBO_HUB_COLOR_TEXT;
extern const COLORREF KBO_HUB_COLOR_MUTED;
extern const COLORREF KBO_HUB_COLOR_ACCENT;

#endif
