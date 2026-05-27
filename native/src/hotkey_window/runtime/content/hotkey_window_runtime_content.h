#ifndef KBOFIX_SRC_HOTKEY_WINDOW_RUNTIME_CONTENT_HOTKEY_WINDOW_RUNTIME_CONTENT_H_
#define KBOFIX_SRC_HOTKEY_WINDOW_RUNTIME_CONTENT_HOTKEY_WINDOW_RUNTIME_CONTENT_H_

#include "../hotkey_window_runtime_shared.h"

#define KBO_HUB_DROPDOWN_MAX_LEAGUES 256
#define KBO_HUB_DROPDOWN_MAX_TEAMS 512
#define KBO_HUB_LEAGUE_MENU_COMMAND_BASE 1000u
#define KBO_HUB_TEAM_MENU_COMMAND_BASE 2000u

const char* kbo_hub_nav_label(int index);
const char* kbo_hub_foreign_subnav_label(int index);
const char* kbo_hub_agames_subnav_label(int index);
const char* kbo_hub_fa_subnav_label(int index);
const char* kbo_hub_military_subnav_label(int index);
const char* kbo_hub_mod_subnav_label(int index);
const char* kbo_hub_cbt_subnav_label(int index);
const char* kbo_hub_futures_subnav_label(int index);
const char* kbo_hub_settings_subnav_label(int index);
void kbo_hub_ensure_valid_selection(void);
void kbo_refresh_hotkey_window(void);
void kbo_layout_hotkey_window(HWND hwnd);
void kbo_hub_apply_fixed_window_placement(HWND hwnd, int preserve_position);
void kbo_hub_save_window_placement(HWND hwnd);

const char* kbo_hub_foreign_slot_code_for_player(uint8_t* player);
void kbo_hub_delete_bitmap(HBITMAP* bitmap);
HBITMAP kbo_hub_load_png_hbitmap_wic(const char* path);
void kbo_hub_load_skin_assets(void);
void kbo_hub_delete_skin_assets(void);
int kbo_hub_estimate_visible_edit_lines(void);
void kbo_hub_update_scrollbar_geometry(void);
void kbo_hub_draw_ootp_scrollbar(HDC hdc);
void kbo_hub_init_gdi_objects(void);
void kbo_hub_delete_gdi_objects(void);
int kbo_hub_argb_team_color_to_hex(uint32_t argb, char* out, size_t out_size);
int kbo_hub_copy_team_bar_colors(uint32_t team_id, char* primary, size_t primary_size, char* secondary, size_t secondary_size);
int kbo_hub_count_service_players(uint32_t service_team_id, int* out_due_60, int* out_due_now);
void kbo_build_overview_hub_text(char* out, size_t out_size);
void kbo_build_military_service_window_text(char* out, size_t out_size);
void kbo_build_foreign_rights_window_text(char* out, size_t out_size);
void kbo_build_foreign_injury_replacement_hub_text(char* out, size_t out_size);
void kbo_build_mod_info_hub_text(char* out, size_t out_size);
void kbo_build_foreign_policy_hub_text(char* out, size_t out_size);
void kbo_build_asian_games_hub_text(char* out, size_t out_size);
void kbo_build_fa_cases_hub_text(char* out, size_t out_size);
void kbo_build_fa_compensation_hub_text(char* out, size_t out_size);
void kbo_build_settings_hub_text(char* out, size_t out_size);
void kbo_build_hub_window_text(char* out, size_t out_size);
RECT kbo_hub_nav_item_rect(int index, int width, int height);
const char* kbo_hub_current_view_title(void);
const char* kbo_hub_current_view_subtitle(void);
int kbo_hub_league_keeps_allstar_teams(uint32_t league_id);
int kbo_hub_team_name_is_allstar(const char* name);
int kbo_hub_team_hidden_from_dropdown(uint8_t* team);
POINT kbo_hub_dropdown_anchor_point(HWND hwnd, const RECT* rect);
void kbo_hub_show_league_dropdown(HWND hwnd);
void kbo_hub_show_team_dropdown(HWND hwnd);
int kbo_hub_foreign_rights_ui_selected(void);
uint32_t kbo_parse_foreign_candidate_player_id_from_line(const char* line);
int kbo_select_foreign_candidate_from_edit_click(HWND edit, LPARAM lparam);
LRESULT CALLBACK kbo_hotkey_edit_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
void kbo_refresh_foreign_rights_controls(void);
void kbo_apply_foreign_rights_button(int retain);
void kbo_webview_append_league_dropdown(KboWindowTextBuffer* buffer, uint32_t current_year);
void kbo_webview_append_team_dropdown(KboWindowTextBuffer* buffer, uint32_t current_year);
void kbo_webview_append_asian_quota_view(KboWindowTextBuffer* buffer);
void kbo_webview_append_fallback_text_view(KboWindowTextBuffer* buffer);
void kbo_webview_append_selected_view(KboWindowTextBuffer* buffer, uint32_t current_year, const char* window_status);
int kbo_hub_selected_league_is_amateur_reputation_league(void);
int kbo_hub_selected_league_is_kbo(void);
int kbo_hub_view_available_for_selected_league(int view);
void kbo_webview_append_main_tabs(KboWindowTextBuffer* buffer);
void kbo_webview_append_sub_tabs(KboWindowTextBuffer* buffer);
int kbo_webview_current_view_has_sub_tabs(void);
WCHAR* kbo_build_webview_hub_html(void);
WCHAR* kbo_build_webview_hub_update_script(void);
int kbo_webview_is_failed(void);
void kbo_webview_mark_failed(const char* reason, HRESULT hr);
void kbo_webview_shutdown_failed_surface(void);
void kbo_webview_note_navigation_completed(BOOL is_success, COREWEBVIEW2_WEB_ERROR_STATUS web_error);
void kbo_webview_navigate_loading(void);
void kbo_webview_navigate_current(void);
void kbo_webview_navigate_current_immediate(void);
int kbo_webview_team_action_allowed(uint32_t team_id, const char* source);

#endif
