#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define KBOFIX_SRC_HOTKEY_WINDOW_RUNTIME_WEBVIEW_HOTKEY_WINDOW_WEBVIEW_INTERNAL_H_

uint32_t g_kbo_hub_selected_league_id = 0u;
uint32_t g_kbo_hub_selected_team_id = 0u;
int g_kbo_hub_open_dropdown = 0;

static int g_navigate_current_count = 0;
static int g_ensure_valid_selection_count = 0;

void kbo_webview_navigate_current(void)
{
    g_navigate_current_count++;
}

void kbo_hub_ensure_valid_selection(void)
{
    g_ensure_valid_selection_count++;
}

void kbo_log_runtimef(const char* format, ...)
{
    (void)format;
}

int kbo_webview_handle_external_or_foreign_command(const char* cmd, HWND hwnd)
{
    (void)cmd;
    (void)hwnd;
    return 0;
}

int kbo_webview_handle_view_navigation_command(const char* cmd)
{
    (void)cmd;
    return 0;
}

int kbo_webview_handle_mod_settings_command(const char* cmd)
{
    (void)cmd;
    return 0;
}

int kbo_webview_handle_event_and_fa_command(const char* cmd)
{
    (void)cmd;
    return 0;
}

int kbo_webview_handle_settings_command(const char* cmd)
{
    (void)cmd;
    return 0;
}

int kbo_webview_handle_player_hover_command(const char* cmd, HWND hwnd)
{
    (void)cmd;
    (void)hwnd;
    return 0;
}

#include "../src/hotkey_window/runtime/webview/commands/selection/hotkey_window_runtime_webview_commands_selection.c"
#include "../src/hotkey_window/runtime/webview/commands/router/hotkey_window_runtime_webview_commands.c"

static void reset_state(void)
{
    g_kbo_hub_selected_league_id = 0u;
    g_kbo_hub_selected_team_id = 0u;
    g_kbo_hub_open_dropdown = 0;
    g_navigate_current_count = 0;
    g_ensure_valid_selection_count = 0;
}

static void test_unknown_commands_are_not_handled(void)
{
    reset_state();

    assert(!kbo_webview_handle_command_uri(NULL, NULL));
    assert(!kbo_webview_handle_command_uri("https://example.test", NULL));
    assert(!kbo_webview_handle_selection_dropdown_command(NULL, NULL));
    assert(!kbo_webview_handle_selection_dropdown_command("unknown", NULL));
    assert(!kbo_webview_handle_command_uri("kbo://unknown", NULL));
    assert(kbo_webview_handle_command_uri("kbo://render/ready/app=1280x720", NULL));
    assert(g_navigate_current_count == 0);
    assert(g_ensure_valid_selection_count == 0);

    printf("test_unknown_commands_are_not_handled: PASS\n");
}

static void test_selection_commands_are_handled(void)
{
    reset_state();

    assert(kbo_webview_handle_command_uri("kbo://setleague/42", NULL));
    assert(g_kbo_hub_selected_league_id == 42u);
    assert(g_kbo_hub_selected_team_id == 0u);
    assert(g_kbo_hub_open_dropdown == 0);
    assert(g_navigate_current_count == 1);
    assert(g_ensure_valid_selection_count == 1);

    assert(kbo_webview_handle_command_uri("kbo://setteam/77", NULL));
    assert(g_kbo_hub_selected_team_id == 77u);
    assert(g_navigate_current_count == 2);

    assert(kbo_webview_handle_command_uri("kbo://league", NULL));
    assert(g_kbo_hub_open_dropdown == 1);
    assert(g_navigate_current_count == 3);

    assert(kbo_webview_handle_command_uri("kbo://team/", NULL));
    assert(g_kbo_hub_open_dropdown == 2);
    assert(g_navigate_current_count == 4);

    printf("test_selection_commands_are_handled: PASS\n");
}

int main(void)
{
    test_unknown_commands_are_not_handled();
    test_selection_commands_are_handled();
    printf("All webview command router tests passed.\n");
    return 0;
}
