#include "../../runtime/common/custom_events_common.h"
#include "../audit/foreign_priority_event_audit.h"
#include "foreign_priority_event_schedule.h"
#include "foreign_priority_event_schedule_internal.h"

#include <stdio.h>
#include <windows.h>

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/core_league_context_parts/event_manager/event_manager.h"
#include "../../../core/logging/core_log.h"
#include "../../../core/dates/core_text_date.h"
#include "../../../core/dates/tick/current_date_tick_capture.h"
#include "../../../core/events/core_league_events.h"
#include "../../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../../competitive_balance_tax/finance/cbt_cash_charge.h"
#include "../../../foreign/common/dates/foreign_waiver_date.h"
#include "../../../foreign/common/policy/foreign_player_policy.h"
#include "../../../foreign/common/policy/foreign_waiver_policy.h"
#include "../../../foreign/injury/api/foreign_injury.h"
#include "../../../foreign/waiver_window/state/foreign_waiver_window_state.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../runtime/catalog/custom_event_catalog.h"
#include "../../secondary_draft/secondary_draft.h"

static volatile LONG g_kbo_foreign_priority_schedule_running = 0;

#define KBO_FOREIGN_PRIORITY_SCHEDULE_RETURN(value) \
    do { \
        InterlockedExchange(&g_kbo_foreign_priority_schedule_running, 0); \
        return (value); \
    } while (0)

int kbo_schedule_foreign_priority_custom_events_at_anchor(
    const char* source,
    uint32_t today,
    uint32_t league_id,
    uint32_t offseason_starts_yyyymmdd)
{
    if (InterlockedCompareExchange(&g_kbo_foreign_priority_schedule_running, 1, 0) != 0) {
        kbo_log_runtimef(
            "KBO custom event schedule skipped source=%s reason=foreign_priority_schedule_already_running today=%u season_end=%u",
            source != NULL ? source : "",
            today,
            offseason_starts_yyyymmdd);
        return 0;
    }

    KboForeignPriorityEventAudit audit = {0};
    audit.today = today;
    audit.league_id = league_id;
    audit.anchor_date = offseason_starts_yyyymmdd;

    if (league_id == 0u) {
        kbo_audit_foreign_priority_schedule("skip", "league_id_unavailable", source, &audit);
        kbo_log_runtimef(
            "KBO custom event schedule skipped source=%s reason=league_id_unavailable today=%u season_end=%u",
            source != NULL ? source : "",
            today,
            offseason_starts_yyyymmdd);
        KBO_FOREIGN_PRIORITY_SCHEDULE_RETURN(-1);
    }
    if (today == 0u || offseason_starts_yyyymmdd == 0u) {
        kbo_audit_foreign_priority_schedule("skip", "anchor_unavailable", source, &audit);
        kbo_log_runtimef(
            "KBO custom event schedule skipped source=%s reason=anchor_unavailable today=%u season_end=%u",
            source != NULL ? source : "",
            today,
            offseason_starts_yyyymmdd);
        KBO_FOREIGN_PRIORITY_SCHEDULE_RETURN(-1);
    }

    if (offseason_starts_yyyymmdd > today) {
        kbo_audit_foreign_priority_schedule("skip", "offseason_starts_in_future", source, &audit);
        kbo_log_runtimef(
            "KBO custom event schedule skipped source=%s reason=offseason_starts_in_future season_end=%u today=%u",
            source != NULL ? source : "",
            offseason_starts_yyyymmdd,
            today);
        KBO_FOREIGN_PRIORITY_SCHEDULE_RETURN(0);
    }

    uint32_t anchor_date = offseason_starts_yyyymmdd;
    uint32_t open_date = anchor_date;
    uint32_t close_date = kbo_add_days_yyyymmdd(
        anchor_date,
        (uint32_t)kbo_foreign_player_policy()->waiver_window_days);
    const KboCustomEventSchedulePolicy* event_policy = kbo_custom_event_schedule_policy();
    uint32_t fa_declaration_date = kbo_add_days_yyyymmdd(
        anchor_date,
        (uint32_t)event_policy->foreign_priority_fa_declaration_offset_days);
    uint32_t intl_established_fa_date = kbo_add_days_yyyymmdd(
        anchor_date,
        (uint32_t)event_policy->foreign_priority_intl_established_fa_offset_days);
    uint32_t military_selection_date = kbo_custom_event_add_months_yyyymmdd(
        anchor_date,
        (uint32_t)event_policy->foreign_priority_military_selection_offset_months);
    int secondary_draft_year = ((anchor_date / 10000u) % 2u) == 1u;
    int32_t secondary_draft_deadline_offset_days =
        event_policy->secondary_draft_offset_days
        - event_policy->secondary_draft_submission_deadline_days_before;
    int secondary_draft_window_offsets_valid = !secondary_draft_year
        || (secondary_draft_deadline_offset_days >= event_policy->secondary_draft_protection_open_offset_days
            && secondary_draft_deadline_offset_days < event_policy->secondary_draft_offset_days);
    uint32_t secondary_draft_protection_open_date =
        secondary_draft_year && secondary_draft_window_offsets_valid
        ? kbo_add_days_yyyymmdd(
            anchor_date,
            (uint32_t)event_policy->secondary_draft_protection_open_offset_days)
        : 0u;
    uint32_t secondary_draft_protection_deadline_date =
        secondary_draft_year && secondary_draft_window_offsets_valid
        ? kbo_add_days_yyyymmdd(anchor_date, (uint32_t)secondary_draft_deadline_offset_days)
        : 0u;
    uint32_t secondary_draft_date = secondary_draft_year
        ? kbo_add_days_yyyymmdd(anchor_date, (uint32_t)event_policy->secondary_draft_offset_days)
        : 0u;
    audit.anchor_date = anchor_date;
    audit.open_date = open_date;
    audit.close_date = close_date;
    audit.fa_declaration_date = fa_declaration_date;
    audit.intl_established_fa_date = intl_established_fa_date;
    audit.military_selection_date = military_selection_date;
    if (today == offseason_starts_yyyymmdd) {
        uint32_t charge_season = offseason_starts_yyyymmdd / 10000u;
        int cbt_cash_charges = kbo_cbt_apply_offseason_cash_charges(
            charge_season,
            offseason_starts_yyyymmdd,
            source != NULL ? source : "foreign_priority_schedule");
        if (cbt_cash_charges > 0) {
            kbo_log_runtimef(
                "KBO custom event schedule applied CBT cash charges at offseason start source=%s season=%u date=%u applied=%d",
                source != NULL ? source : "",
                charge_season,
                offseason_starts_yyyymmdd,
                cbt_cash_charges);
        }
    }
    if (close_date == 0u
            || fa_declaration_date == 0u
            || intl_established_fa_date == 0u
            || (secondary_draft_year
                && (!secondary_draft_window_offsets_valid
                    || secondary_draft_protection_open_date == 0u
                    || secondary_draft_protection_deadline_date == 0u
                    || secondary_draft_date == 0u))) {
        kbo_audit_foreign_priority_schedule("fail", "derived_date_invalid", source, &audit);
        kbo_log_runtimef(
            "KBO custom event schedule skipped source=%s reason=derived_date_invalid season_end=%u anchor=%u close=%u fa_declaration=%u intl_established_fa=%u secondary_draft_open=%u secondary_draft_deadline=%u secondary_draft=%u",
            source != NULL ? source : "",
            offseason_starts_yyyymmdd,
            anchor_date,
            close_date,
            fa_declaration_date,
            intl_established_fa_date,
            secondary_draft_protection_open_date,
            secondary_draft_protection_deadline_date,
            secondary_draft_date);
        KBO_FOREIGN_PRIORITY_SCHEDULE_RETURN(-1);
    }

    int registered_secondary_draft_window = 1;
    KboSecondaryDraftWindow secondary_draft_window;
    memset(&secondary_draft_window, 0, sizeof(secondary_draft_window));
    if (secondary_draft_year) {
        secondary_draft_window.season = anchor_date / 10000u;
        secondary_draft_window.league_id = league_id;
        secondary_draft_window.protection_open_yyyymmdd = secondary_draft_protection_open_date;
        secondary_draft_window.protection_deadline_yyyymmdd = secondary_draft_protection_deadline_date;
        secondary_draft_window.draft_yyyymmdd = secondary_draft_date;
        registered_secondary_draft_window = kbo_secondary_draft_register_window(
            secondary_draft_window.season,
            secondary_draft_window.league_id,
            secondary_draft_window.protection_open_yyyymmdd,
            secondary_draft_window.protection_deadline_yyyymmdd,
            secondary_draft_window.draft_yyyymmdd,
            source != NULL ? source : "foreign_priority_schedule");
        if (!registered_secondary_draft_window) {
            kbo_audit_foreign_priority_schedule("fail", "secondary_draft_window_register_failed", source, &audit);
            kbo_log_runtimef(
                "KBO custom event schedule skipped source=%s reason=secondary_draft_window_register_failed season_end=%u secondary_draft_open=%u secondary_draft_deadline=%u secondary_draft=%u",
                source != NULL ? source : "",
                offseason_starts_yyyymmdd,
                secondary_draft_protection_open_date,
                secondary_draft_protection_deadline_date,
                secondary_draft_date);
            KBO_FOREIGN_PRIORITY_SCHEDULE_RETURN(-1);
        }
    }

    if (kbo_foreign_priority_ready_cache_hit(offseason_starts_yyyymmdd, league_id)) {
        KBO_FOREIGN_PRIORITY_SCHEDULE_RETURN(0);
    }

    KboForeignPriorityScheduleDates dates = {
        anchor_date,
        open_date,
        close_date,
        fa_declaration_date,
        intl_established_fa_date,
        military_selection_date,
        secondary_draft_protection_open_date,
        secondary_draft_protection_deadline_date,
        secondary_draft_date,
        secondary_draft_year
    };
    KboForeignPriorityScheduleResult schedule_result = {0};
    int schedule_status = kbo_foreign_priority_schedule_events(
        league_id,
        today,
        offseason_starts_yyyymmdd,
        &dates,
        &secondary_draft_window,
        registered_secondary_draft_window,
        source,
        &audit,
        &schedule_result);
    if (schedule_result.already_scheduled) {
        KBO_FOREIGN_PRIORITY_SCHEDULE_RETURN(0);
    }
    if (schedule_status < 0) {
        KBO_FOREIGN_PRIORITY_SCHEDULE_RETURN(-1);
    }

    kbo_log_runtimef(
        "KBO custom event schedule source=%s season_end=%u anchor=%u open=%u close=%u fa_declaration=%u intl_established_fa=%u military=%u secondary_draft_open=%u secondary_draft_deadline=%u secondary_draft=%u created_open=%d created_close=%d created_fa_declaration=%d created_intl_established_fa=%d pruned_old_intl_established_fa=%d created_military=%d created_secondary_draft=%d registered_secondary_draft_window=%d secondary_draft_window_news=%d ready=%d",
        source != NULL ? source : "",
        offseason_starts_yyyymmdd,
        anchor_date,
        open_date,
        close_date,
        fa_declaration_date,
        intl_established_fa_date,
        military_selection_date,
        secondary_draft_protection_open_date,
        secondary_draft_protection_deadline_date,
        secondary_draft_date,
        schedule_result.created_open,
        schedule_result.created_close,
        schedule_result.created_fa_declaration,
        schedule_result.created_intl_established_fa,
        schedule_result.pruned_old_intl_established_fa,
        schedule_result.created_military,
        schedule_result.created_secondary_draft,
        registered_secondary_draft_window,
        schedule_result.secondary_draft_window_news,
        schedule_result.ready);

    if (!schedule_result.ready) {
        kbo_audit_foreign_priority_schedule("fail", "events_not_ready", source, &audit);
        KBO_FOREIGN_PRIORITY_SCHEDULE_RETURN(-1);
    }
    if (today == offseason_starts_yyyymmdd) {
        int closed_foreign_injury_replacements =
            kbo_foreign_injury_reset_open_replacements_for_offseason(
                offseason_starts_yyyymmdd,
                source != NULL ? source : "foreign_priority_schedule");
        if (closed_foreign_injury_replacements > 0) {
            kbo_log_runtimef(
                "KBO custom event schedule closed foreign injury replacements at offseason start source=%s date=%u closed=%d",
                source != NULL ? source : "",
                offseason_starts_yyyymmdd,
                closed_foreign_injury_replacements);
        }
    }
    g_kbo_foreign_priority_last_scheduled_date = offseason_starts_yyyymmdd;
    kbo_foreign_priority_ready_cache_store(offseason_starts_yyyymmdd, league_id);
    int changed = schedule_result.changed;
    kbo_audit_foreign_priority_schedule(changed ? "schedule" : "ready", "created_or_existing_events", source, &audit);
    KBO_FOREIGN_PRIORITY_SCHEDULE_RETURN(changed);
}

#undef KBO_FOREIGN_PRIORITY_SCHEDULE_RETURN

