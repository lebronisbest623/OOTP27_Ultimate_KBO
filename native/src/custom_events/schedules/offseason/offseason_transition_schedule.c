#include "../../runtime/common/custom_events_common.h"
#include "offseason_transition_schedule.h"
#include <stdio.h>
#include <string.h>
#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/logging/core_log.h"
#include "../../../core/logging/rule_audit.h"
#include "../../../core/dates/core_current_date.h"
#include "../../../core/files/save_paths/core_save_paths.h"
#include "../../../core/dates/core_text_date.h"
#include "../../../core/season/phase/season_phase.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../allstar/allstar_league_context/allstar_league_context.h"
#include "../../../competitive_balance_tax/finance/cbt_cash_charge.h"
#include "../../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../../foreign/common/policy/foreign_waiver_policy.h"

static void kbo_audit_offseason_transition(
    const char* decision,
    const char* reason,
    const char* source,
    uint32_t today_yyyymmdd,
    uint32_t league_id,
    uint32_t league_year,
    uint8_t previous_phase,
    uint8_t phase,
    uint32_t anchor_yyyymmdd,
    uint32_t pending_anchor,
    int scheduled)
{
        do {
        KboLogFields audit_fields;
        kbo_log_fields_init(&audit_fields);
        kbo_log_field_u32(&audit_fields, "date", today_yyyymmdd);
        kbo_log_field_u32(&audit_fields, "league_id", league_id);
        kbo_log_field_u32(&audit_fields, "league_year", league_year);
        kbo_log_field_u32(&audit_fields, "previous_phase", (unsigned)previous_phase);
        kbo_log_field_u32(&audit_fields, "phase", (unsigned)phase);
        kbo_log_field_u32(&audit_fields, "anchor_date", anchor_yyyymmdd);
        kbo_log_field_u32(&audit_fields, "pending_anchor", pending_anchor);
        kbo_log_field_i32(&audit_fields, "scheduled", scheduled);
        kbo_rule_audit_emit_fields(
            "custom_event.offseason_transition",
            decision,
            reason,
            source,
            &audit_fields);
    } while (0);
}

int kbo_custom_event_phase_is_offseason(uint8_t phase)
{
    return kbo_season_phase_is_offseason(phase);
}

int kbo_custom_event_phase_can_enter_offseason(uint8_t phase)
{
    return kbo_season_phase_can_enter_offseason(phase);
}

int kbo_custom_event_date_allows_offseason_transition(uint32_t yyyymmdd)
{
    uint32_t month_day = yyyymmdd % 10000u;
    return month_day >= 1001u;
}

static int kbo_custom_event_date_gap_days(
    uint32_t older_yyyymmdd,
    uint32_t newer_yyyymmdd,
    uint32_t* out_days)
{
    if (out_days != NULL) {
        *out_days = 0u;
    }
    if (older_yyyymmdd == 0u || newer_yyyymmdd == 0u || older_yyyymmdd > newer_yyyymmdd) {
        return 0;
    }

    uint32_t older_serial = kbo_date_serial(
        older_yyyymmdd / 10000u,
        (older_yyyymmdd / 100u) % 100u,
        older_yyyymmdd % 100u);
    uint32_t newer_serial = kbo_date_serial(
        newer_yyyymmdd / 10000u,
        (newer_yyyymmdd / 100u) % 100u,
        newer_yyyymmdd % 100u);
    if (older_serial == 0u || newer_serial == 0u || older_serial > newer_serial) {
        return 0;
    }

    if (out_days != NULL) {
        *out_days = newer_serial - older_serial;
    }
    return 1;
}

static uint32_t kbo_custom_event_resolve_offseason_transition_anchor(
    uint32_t today_yyyymmdd,
    uint32_t previous_phase_date,
    const char* source)
{
    uint32_t offseason_starts = kbo_get_latest_offseason_starts_event(today_yyyymmdd);
    if (offseason_starts != 0u) {
        kbo_log_runtimef(
            "KBO custom event offseason transition anchor source=%s reason=offseason_starts_event today=%u anchor=%u",
            source != NULL ? source : "",
            today_yyyymmdd,
            offseason_starts);
        return offseason_starts;
    }

    uint32_t gap_days = 0u;
    if (previous_phase_date != 0u
            && previous_phase_date < today_yyyymmdd
            && kbo_custom_event_date_allows_offseason_transition(previous_phase_date)
            && kbo_custom_event_date_gap_days(previous_phase_date, today_yyyymmdd, &gap_days)
            && gap_days <= 1u) {
        kbo_log_runtimef(
            "KBO custom event offseason transition anchor source=%s reason=previous_phase_date today=%u anchor=%u gap_days=%u",
            source != NULL ? source : "",
            today_yyyymmdd,
            previous_phase_date,
            gap_days);
        return previous_phase_date;
    }

    return today_yyyymmdd;
}

int kbo_custom_event_read_league_phase(
    uint32_t league_id,
    uintptr_t* out_league_ptr,
    uint32_t* out_league_year,
    uint8_t* out_phase,
    uint32_t* out_phase_year)
{
    if (out_league_ptr != NULL) { *out_league_ptr = 0; }
    if (out_league_year != NULL) { *out_league_year = 0; }
    if (out_phase != NULL) { *out_phase = 0xffu; }
    if (out_phase_year != NULL) { *out_phase_year = 0; }
    if (league_id == 0u) {
        return 0;
    }

    KboSeasonPhaseInfo phase_info;
    if (!kbo_season_phase_read_raw(league_id, &phase_info)) {
        return 0;
    }
    if (out_league_ptr != NULL) { *out_league_ptr = phase_info.league_ptr; }
    if (out_league_year != NULL) { *out_league_year = phase_info.league_year; }
    if (out_phase != NULL) { *out_phase = phase_info.raw_phase; }
    if (out_phase_year != NULL) { *out_phase_year = phase_info.raw_phase_year; }
    return 1;
}

int kbo_custom_event_schedule_pending_offseason_transition(
    uint32_t today_yyyymmdd,
    const char* source)
{
    if (g_kbo_custom_event_pending_offseason_transition_anchor == 0u) {
        return 0;
    }
    if (g_kbo_custom_event_last_offseason_transition_anchor
            == g_kbo_custom_event_pending_offseason_transition_anchor) {
        kbo_audit_offseason_transition(
            "skip",
            "pending_anchor_already_scheduled",
            source,
            today_yyyymmdd,
            0u,
            0u,
            255u,
            255u,
            g_kbo_custom_event_pending_offseason_transition_anchor,
            g_kbo_custom_event_pending_offseason_transition_anchor,
            0);
        g_kbo_custom_event_pending_offseason_transition_anchor = 0u;
        return 0;
    }
    if (today_yyyymmdd < g_kbo_custom_event_pending_offseason_transition_anchor) {
        kbo_audit_offseason_transition(
            "skip",
            "pending_anchor_in_future",
            source,
            today_yyyymmdd,
            0u,
            0u,
            255u,
            255u,
            g_kbo_custom_event_pending_offseason_transition_anchor,
            g_kbo_custom_event_pending_offseason_transition_anchor,
            0);
        return 0;
    }

    int scheduled = kbo_schedule_foreign_priority_custom_events_for_anchor_on_date(
        source,
        today_yyyymmdd,
        g_kbo_custom_event_pending_offseason_transition_anchor);
    kbo_log_runtimef(
        "KBO custom event offseason transition schedule source=%s today=%u anchor=%u result=%d",
        source != NULL ? source : "",
        today_yyyymmdd,
        g_kbo_custom_event_pending_offseason_transition_anchor,
        scheduled);
    kbo_audit_offseason_transition(
        scheduled >= 0 ? "schedule_foreign_priority" : "fail",
        scheduled >= 0 ? "pending_anchor_ready" : "pending_anchor_schedule_failed",
        source,
        today_yyyymmdd,
        0u,
        0u,
        255u,
        255u,
        g_kbo_custom_event_pending_offseason_transition_anchor,
        g_kbo_custom_event_pending_offseason_transition_anchor,
        scheduled);
    if (scheduled >= 0) {
        g_kbo_custom_event_last_offseason_transition_anchor =
            g_kbo_custom_event_pending_offseason_transition_anchor;
        g_kbo_custom_event_pending_offseason_transition_anchor = 0u;
        return 1;
    }
    return 0;
}

int kbo_custom_event_monitor_check_offseason_transition(
    uint32_t today_yyyymmdd,
    const char* source)
{
    if (today_yyyymmdd == 0u) {
        kbo_audit_offseason_transition(
            "skip",
            "date_unavailable",
            source,
            today_yyyymmdd,
            0u,
            0u,
            255u,
            255u,
            0u,
            g_kbo_custom_event_pending_offseason_transition_anchor,
            0);
        return 0;
    }
    if (!kbo_custom_event_date_allows_offseason_transition(today_yyyymmdd)) {
        return kbo_custom_event_schedule_pending_offseason_transition(today_yyyymmdd, source);
    }

    uint32_t league_id = kbo_resolve_kbo_league_id();

    uint32_t known_offseason_start = kbo_get_latest_offseason_starts_event(today_yyyymmdd);
    KboSeasonPhaseInfo phase_info;
    if (!kbo_season_phase_resolve(
            league_id,
            today_yyyymmdd,
            known_offseason_start,
            &phase_info)) {
        kbo_audit_offseason_transition(
            "defer",
            "league_phase_unavailable",
            source,
            today_yyyymmdd,
            league_id,
            0u,
            255u,
            255u,
            0u,
            g_kbo_custom_event_pending_offseason_transition_anchor,
            0);
        return kbo_custom_event_schedule_pending_offseason_transition(today_yyyymmdd, source);
    }
    uintptr_t league_ptr = phase_info.league_ptr;
    uint32_t league_year = phase_info.league_year;
    uint32_t phase_year = phase_info.raw_phase_year;
    uint8_t phase = phase_info.effective_phase;
    uint8_t raw_phase = phase_info.raw_phase;

    int same_league = g_kbo_custom_event_last_phase_league_id == league_id
        && g_kbo_custom_event_last_phase_league_ptr == league_ptr;
    int had_previous = same_league && g_kbo_custom_event_last_seen_league_phase <= 4u;
    int transitioned_to_offseason = had_previous
        && kbo_custom_event_phase_can_enter_offseason(g_kbo_custom_event_last_seen_league_phase)
        && kbo_custom_event_phase_is_offseason(phase);
    transitioned_to_offseason = transitioned_to_offseason
        || kbo_season_phase_info_has_today_offseason_transition_capture(&phase_info, today_yyyymmdd);

    if (!same_league
            || g_kbo_custom_event_last_phase_date != today_yyyymmdd
            || g_kbo_custom_event_last_phase_league_year != league_year
            || g_kbo_custom_event_last_seen_league_phase != phase
            || g_kbo_custom_event_last_phase_year != phase_year) {
        kbo_log_runtimef(
            "KBO custom event phase observed source=%s league=%p league_id=%u date=%u league_year=%u previous_phase=%u raw_phase=%u phase=%u label=%s phase_year=%u opening_day=%u offseason_start=%u corrected=%d",
            source != NULL ? source : "",
            (void*)league_ptr,
            league_id,
            today_yyyymmdd,
            league_year,
            had_previous ? (unsigned)g_kbo_custom_event_last_seen_league_phase : 255u,
            (unsigned)raw_phase,
            (unsigned)phase,
            kbo_season_phase_label(phase),
            phase_year,
            phase_info.opening_day,
            known_offseason_start,
            phase_info.corrected);
    }

    if (transitioned_to_offseason) {
        if (kbo_custom_event_date_allows_offseason_transition(today_yyyymmdd)) {
            uint32_t transition_anchor = kbo_custom_event_resolve_offseason_transition_anchor(
                today_yyyymmdd,
                g_kbo_custom_event_last_phase_date,
                source);
            if (g_kbo_custom_event_last_offseason_transition_anchor != transition_anchor) {
                g_kbo_custom_event_pending_offseason_transition_anchor = transition_anchor;
                kbo_audit_offseason_transition(
                    "queue_schedule",
                    "phase_transition_to_offseason",
                    source,
                    today_yyyymmdd,
                    league_id,
                    league_year,
                    g_kbo_custom_event_last_seen_league_phase,
                    phase,
                    transition_anchor,
                    g_kbo_custom_event_pending_offseason_transition_anchor,
                    0);
                kbo_log_runtimef(
                    "KBO custom event offseason transition detected source=%s league=%p league_id=%u date=%u anchor=%u league_year=%u previous_phase=%u phase=%u",
                    source != NULL ? source : "",
                    (void*)league_ptr,
                    league_id,
                    today_yyyymmdd,
                    transition_anchor,
                    league_year,
                    (unsigned)g_kbo_custom_event_last_seen_league_phase,
                    (unsigned)phase);
                kbo_cbt_apply_offseason_cash_charges(
                    league_year,
                    transition_anchor,
                    source != NULL ? source : "offseason_transition");
            } else {
                kbo_audit_offseason_transition(
                    "skip",
                    "anchor_already_scheduled",
                    source,
                    today_yyyymmdd,
                    league_id,
                    league_year,
                    g_kbo_custom_event_last_seen_league_phase,
                    phase,
                    transition_anchor,
                    g_kbo_custom_event_pending_offseason_transition_anchor,
                    0);
            }
        } else {
            kbo_log_runtimef(
                "KBO custom event offseason transition ignored source=%s reason=date_window date=%u previous_phase=%u phase=%u",
                source != NULL ? source : "",
                today_yyyymmdd,
                (unsigned)g_kbo_custom_event_last_seen_league_phase,
                (unsigned)phase);
            kbo_audit_offseason_transition(
                "skip",
                "date_window",
                source,
                today_yyyymmdd,
                league_id,
                league_year,
                g_kbo_custom_event_last_seen_league_phase,
                phase,
                0u,
                g_kbo_custom_event_pending_offseason_transition_anchor,
                0);
        }
    }

    g_kbo_custom_event_last_phase_league_id = league_id;
    g_kbo_custom_event_last_phase_league_ptr = league_ptr;
    g_kbo_custom_event_last_phase_date = today_yyyymmdd;
    g_kbo_custom_event_last_phase_league_year = league_year;
    g_kbo_custom_event_last_phase_year = phase_year;
    g_kbo_custom_event_last_seen_league_phase = phase;

    return kbo_custom_event_schedule_pending_offseason_transition(today_yyyymmdd, source);
}
