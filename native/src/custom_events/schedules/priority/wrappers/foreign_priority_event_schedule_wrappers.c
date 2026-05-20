#include "../foreign_priority_event_schedule_internal.h"

#include "../../../../core/logging/core_log.h"
#include "../../../../core/dates/tick/current_date_tick_capture.h"
#include "../../../../core/events/core_league_events.h"
#include "../../../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../../../custom_events/runtime/lookup/custom_event_lookup.h"
#include "../../../../foreign/common/dates/foreign_waiver_date.h"
#include "../../../../foreign/common/policy/foreign_waiver_policy.h"
#include "../../../../foreign/waiver_window/state/foreign_waiver_window_state.h"

int kbo_schedule_foreign_priority_custom_events_for_anchor(
    const char* source,
    uint32_t offseason_starts_yyyymmdd)
{
    uint32_t today = 0u;
    if (!kbo_current_date_tick_latest_published_date(&today) || today == 0u) {
        kbo_log_runtimef(
            "KBO custom event schedule skipped source=%s reason=ssot_date_unavailable",
            source != NULL ? source : "");
        return -1;
    }

    uint32_t league_id = kbo_get_foreign_waiver_league_id();
    if (league_id == 0u) {
        league_id = kbo_resolve_kbo_league_id();
    }
    return kbo_schedule_foreign_priority_custom_events_at_anchor(
        source,
        today,
        league_id,
        offseason_starts_yyyymmdd);
}

int kbo_schedule_foreign_priority_custom_events_for_anchor_on_date(
    const char* source,
    uint32_t today,
    uint32_t offseason_starts_yyyymmdd)
{
    uint32_t league_id = kbo_get_foreign_waiver_league_id();
    if (league_id == 0u) {
        league_id = kbo_resolve_kbo_league_id();
    }
    if (today == 0u) {
        kbo_log_runtimef(
            "KBO custom event schedule skipped source=%s reason=ssot_date_unavailable",
            source != NULL ? source : "");
        return -1;
    }
    return kbo_schedule_foreign_priority_custom_events_at_anchor(
        source,
        today,
        league_id,
        offseason_starts_yyyymmdd);
}

int kbo_schedule_foreign_priority_custom_events_for_date(
    const char* source,
    uint32_t today)
{
    uint32_t league_id = kbo_get_foreign_waiver_league_id();
    if (league_id == 0u) {
        league_id = kbo_resolve_kbo_league_id();
    }
    if (today == 0u) {
        kbo_log_runtimef(
            "KBO custom event schedule skipped source=%s reason=ssot_date_unavailable",
            source != NULL ? source : "");
        return -1;
    }
    uint32_t offseason_starts_yyyymmdd = kbo_get_latest_offseason_starts_event(today);
    if (offseason_starts_yyyymmdd == 0u) {
        offseason_starts_yyyymmdd = kbo_detect_offseason_anchor_by_league_year(league_id, today, source);
    }
    if (offseason_starts_yyyymmdd == 0u) {
        offseason_starts_yyyymmdd = kbo_recent_phase_transition_offseason_anchor(league_id, today);
        if (offseason_starts_yyyymmdd != 0u) {
            kbo_log_runtimef(
                "KBO custom event schedule fallback source=%s reason=recent_phase_transition_anchor today=%u season_end=%u",
                source != NULL ? source : "",
                today,
                offseason_starts_yyyymmdd);
        }
    }
    if (offseason_starts_yyyymmdd == 0u) {
        offseason_starts_yyyymmdd = kbo_recent_foreign_waiver_marker_anchor(today, source);
    }
    if (offseason_starts_yyyymmdd == 0u) {
        static uint32_t last_logged_no_event_today = 0u;
        if (last_logged_no_event_today != today) {
            last_logged_no_event_today = today;
            kbo_log_runtimef(
                "KBO custom event schedule skipped source=%s reason=no_offseason_starts_event today=%u",
                source != NULL ? source : "",
                today);
        }
        return -1;
    }

    return kbo_schedule_foreign_priority_custom_events_at_anchor(
        source,
        today,
        league_id,
        offseason_starts_yyyymmdd);
}

int kbo_schedule_foreign_priority_custom_events(const char* source)
{
    uint32_t today = 0u;
    if (!kbo_current_date_tick_latest_published_date(&today) || today == 0u) {
        kbo_log_runtimef(
            "KBO custom event schedule skipped source=%s reason=ssot_date_unavailable",
            source != NULL ? source : "");
        return -1;
    }
    return kbo_schedule_foreign_priority_custom_events_for_date(source, today);
}
