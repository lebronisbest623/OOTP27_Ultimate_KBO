#include "../../runtime/common/custom_events_common.h"
#include "handlers.h"
#include <stdio.h>
#include <string.h>
#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/logging/core_log.h"
#include "../../../core/dates/core_current_date.h"
#include "../../../core/dates/tick/current_date_tick_capture.h"
#include "../../../core/files/save_paths/core_save_paths.h"
#include "../../../core/dates/core_text_date.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../emit/emit.h"

int kbo_handle_asian_games_selection_event(uint32_t event_yyyymmdd, const char* source)
{
    kbo_clear_asian_games_roster_if_save_changed(source);
    if (event_yyyymmdd == 0u || g_kbo_asian_games_last_selection_fired_date == event_yyyymmdd) {
        return 0;
    }
    int selected = kbo_select_asian_games_roster(event_yyyymmdd, source);
    if (selected <= 0) {
        kbo_log_runtimef(
            "KBO Asian Games selection deferred source=%s date=%u selected=%d",
            source != NULL ? source : "",
            event_yyyymmdd,
            selected);
        return 0;
    }
    g_kbo_asian_games_last_selection_fired_date = event_yyyymmdd;
    uint32_t news_yyyymmdd = kbo_asian_games_effective_action_date(event_yyyymmdd);
    kbo_emit_asian_games_news(
        news_yyyymmdd,
        "asian_games.selection",
        source);
    kbo_log_runtimef(
        "KBO Asian Games selection reached source=%s event_date=%u news_date=%u selected=%d",
        source != NULL ? source : "",
        event_yyyymmdd,
        news_yyyymmdd,
        selected);
    return 1;
}

int kbo_handle_asian_games_departure_event(uint32_t event_yyyymmdd, const char* source)
{
    kbo_clear_asian_games_roster_if_save_changed(source);
    if (event_yyyymmdd == 0u || g_kbo_asian_games_last_departure_fired_date == event_yyyymmdd) {
        return 0;
    }
    uint32_t action_yyyymmdd = kbo_asian_games_effective_action_date(event_yyyymmdd);
    int departed = kbo_asian_games_depart_selected_players(action_yyyymmdd, source);
    if (departed <= 0) {
        uint32_t current_yyyymmdd = 0u;
        if (kbo_current_date_tick_latest_published_date(&current_yyyymmdd)
                && (current_yyyymmdd / 10000u) > (event_yyyymmdd / 10000u)) {
            kbo_log_runtimef(
                "KBO Asian Games departure closed stale source=%s date=%u current=%u departed=%d",
                source != NULL ? source : "",
                event_yyyymmdd,
                current_yyyymmdd,
                departed);
            return 1;
        }
        kbo_log_runtimef(
            "KBO Asian Games departure deferred source=%s date=%u departed=%d",
            source != NULL ? source : "",
            event_yyyymmdd,
            departed);
        return 0;
    }
    g_kbo_asian_games_last_departure_fired_date = event_yyyymmdd;
    kbo_emit_asian_games_news(
        action_yyyymmdd,
        "asian_games.departure",
        source);
    kbo_log_runtimef(
        "KBO Asian Games departure reached source=%s event_date=%u action_date=%u departed=%d",
        source != NULL ? source : "",
        event_yyyymmdd,
        action_yyyymmdd,
        departed);
    return 1;
}

int kbo_handle_asian_games_final_event(uint32_t event_yyyymmdd, const char* source)
{
    kbo_clear_asian_games_roster_if_save_changed(source);
    if (event_yyyymmdd == 0u || g_kbo_asian_games_last_final_fired_date == event_yyyymmdd) {
        return 0;
    }
    uint32_t action_yyyymmdd = kbo_asian_games_effective_action_date(event_yyyymmdd);
    int returned = kbo_asian_games_finalize_selected_players(action_yyyymmdd, source);
    int already_finalized = returned <= 0 ? kbo_asian_games_roster_already_finalized(source) : 0;
    if (returned <= 0 && !already_finalized) {
        uint32_t current_yyyymmdd = 0u;
        if (kbo_current_date_tick_latest_published_date(&current_yyyymmdd)
                && (current_yyyymmdd / 10000u) > (event_yyyymmdd / 10000u)) {
            kbo_log_runtimef(
                "KBO Asian Games final closed stale source=%s date=%u current=%u returned=%d",
                source != NULL ? source : "",
                event_yyyymmdd,
                current_yyyymmdd,
                returned);
            return 1;
        }
        kbo_log_runtimef(
            "KBO Asian Games final deferred source=%s date=%u returned=%d",
            source != NULL ? source : "",
            event_yyyymmdd,
            returned);
        return 0;
    }
    g_kbo_asian_games_last_final_fired_date = event_yyyymmdd;
    const char* template_prefix = g_kbo_asian_games_result == KBO_ASIAN_GAMES_RESULT_NO_GOLD
        ? "asian_games.final.failure"
        : "asian_games.final";
    kbo_emit_asian_games_news(
        action_yyyymmdd,
        template_prefix,
        source);
    kbo_log_runtimef(
        "KBO Asian Games final reached source=%s event_date=%u action_date=%u returned=%d already_finalized=%d result=%u template=%s",
        source != NULL ? source : "",
        event_yyyymmdd,
        action_yyyymmdd,
        returned,
        already_finalized,
        (uint32_t)g_kbo_asian_games_result,
        template_prefix);
    return 1;
}
