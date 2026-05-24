#ifndef KBOFIX_SRC_HOTKEY_WINDOW_RUNTIME_WINDOW_INTERNAL_H_
#define KBOFIX_SRC_HOTKEY_WINDOW_RUNTIME_WINDOW_INTERNAL_H_

#include "../content/hotkey_window_runtime_content.h"
#include "../webview/hotkey_window_webview.h"

#define KBO_HUB_WINDOW_STYLE (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_THICKFRAME)
#define KBO_HUB_WINDOW_EX_STYLE (WS_EX_TOOLWINDOW | WS_EX_WINDOWEDGE)

BOOL CALLBACK kbo_enum_main_window_proc(HWND hwnd, LPARAM lparam);
HWND kbo_find_ootp_main_window(void);
int kbo_foreground_is_this_process(void);
void kbo_show_or_hide_hotkey_window(int requested_mode);
int kbo_queue_hotkey_window_toggle(int requested_mode);
int kbo_request_hotkey_window_refresh(const char* source);
LRESULT CALLBACK kbo_hotkey_keyboard_proc(int code, WPARAM wparam, LPARAM lparam);
void kbo_layout_hotkey_window(HWND hwnd);
void kbo_refresh_hotkey_window_layout(HWND hwnd);
void kbo_hub_get_work_area(HWND hwnd, RECT* out);
RECT kbo_hub_fixed_window_rect(HWND hwnd);
SIZE kbo_hub_min_track_size(void);
int kbo_hub_rect_width(const RECT* rect);
int kbo_hub_rect_height(const RECT* rect);
int kbo_hub_try_load_window_placement(HWND hwnd, RECT* out_rect);
void kbo_hub_save_window_placement(HWND hwnd);
void kbo_hub_apply_fixed_window_placement(HWND hwnd, int preserve_position);
void kbo_hotkey_window_install_exception_guard(void);
LRESULT CALLBACK kbo_hotkey_window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
DWORD WINAPI kbo_hotkey_window_thread(LPVOID parameter);
void start_kbo_hotkey_window_thread(HINSTANCE instance);

#endif
