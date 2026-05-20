#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../src/custom_events/asian_games_news/handlers/handlers.h"
#include "../src/custom_events/runtime/state/custom_event_state.h"
#include "../src/custom_events/asian_games/state/asian_games_state.h"

static char g_test_save_path[MAX_PATH] = {0};
static int g_test_clear_calls = 0;
static int g_test_select_calls = 0;
static int g_test_emit_calls = 0;
static int g_test_select_result = 24;

int kbo_get_current_save_path(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u || g_test_save_path[0] == '\0') {
        return 0;
    }
    snprintf(out, out_size, "%s", g_test_save_path);
    return 1;
}

void kbo_clear_asian_games_roster_if_save_changed(const char* source)
{
    (void)source;
    g_test_clear_calls++;
}

int kbo_select_asian_games_roster(uint32_t event_yyyymmdd, const char* source)
{
    (void)event_yyyymmdd;
    (void)source;
    g_test_select_calls++;
    return g_test_select_result;
}

uint32_t kbo_asian_games_effective_action_date(uint32_t event_yyyymmdd)
{
    return event_yyyymmdd;
}

int kbo_emit_asian_games_news(uint32_t event_yyyymmdd, const char* template_prefix, const char* source)
{
    (void)event_yyyymmdd;
    (void)template_prefix;
    (void)source;
    g_test_emit_calls++;
    return 1;
}

int kbo_current_date_tick_latest_published_date(uint32_t* out_yyyymmdd)
{
    if (out_yyyymmdd != NULL) {
        *out_yyyymmdd = 0u;
    }
    return 0;
}

int kbo_asian_games_depart_selected_players(uint32_t event_yyyymmdd, const char* source)
{
    (void)event_yyyymmdd;
    (void)source;
    return 0;
}

int kbo_asian_games_finalize_selected_players(uint32_t event_yyyymmdd, const char* source)
{
    (void)event_yyyymmdd;
    (void)source;
    return 0;
}

int kbo_asian_games_roster_already_finalized(const char* source)
{
    (void)source;
    return 0;
}

void kbo_log_runtime_line_at(const char* file, int line, const char* message)
{
    (void)file;
    (void)line;
    (void)message;
}

void kbo_log_runtimef_at(const char* file, int line, const char* format, ...)
{
    (void)file;
    (void)line;
    (void)format;
}

static void reset_test_state(void)
{
    snprintf(g_test_save_path, sizeof(g_test_save_path), "C:\\test\\saved_games\\SaveA.lg");
    g_test_clear_calls = 0;
    g_test_select_calls = 0;
    g_test_emit_calls = 0;
    g_test_select_result = 24;
    g_kbo_asian_games_last_selection_fired_date = 0u;
    g_kbo_asian_games_last_departure_fired_date = 0u;
    g_kbo_asian_games_last_final_fired_date = 0u;
    g_kbo_asian_games_result = KBO_ASIAN_GAMES_RESULT_UNKNOWN;
}

static void test_asian_games_handler_resets_fire_dates_when_save_changes(void)
{
    reset_test_state();

    assert(kbo_handle_asian_games_selection_event(20260805u, "test") == 1);
    assert(g_kbo_asian_games_last_selection_fired_date == 20260805u);
    assert(g_test_select_calls == 1);
    assert(g_test_emit_calls == 1);

    assert(kbo_handle_asian_games_selection_event(20260805u, "test") == 0);
    assert(g_test_select_calls == 1);
    assert(g_test_emit_calls == 1);

    g_kbo_asian_games_last_departure_fired_date = 20260919u;
    g_kbo_asian_games_last_final_fired_date = 20261004u;
    snprintf(g_test_save_path, sizeof(g_test_save_path), "C:\\test\\saved_games\\SaveB.lg");

    assert(kbo_handle_asian_games_selection_event(20260805u, "test") == 1);
    assert(g_kbo_asian_games_last_selection_fired_date == 20260805u);
    assert(g_kbo_asian_games_last_departure_fired_date == 0u);
    assert(g_kbo_asian_games_last_final_fired_date == 0u);
    assert(g_test_select_calls == 2);
    assert(g_test_emit_calls == 2);

    printf("test_asian_games_handler_resets_fire_dates_when_save_changes: PASS\n");
}

int main(void)
{
    test_asian_games_handler_resets_fire_dates_when_save_changes();
    printf("All Asian Games handler save-context tests passed.\n");
    return 0;
}
