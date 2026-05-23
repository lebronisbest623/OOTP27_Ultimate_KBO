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

    char open_title[160] = {0};
    char close_title[160] = {0};
    char fa_declaration_title[160] = {0};
    char intl_established_fa_title[160] = {0};
    char military_title[160] = {0};
    char secondary_draft_title[160] = {0};
    if (!kbo_custom_event_title_for_kind(KBO_CUSTOM_EVENT_KIND_FOREIGN_PRIORITY_OPEN, open_title, sizeof(open_title))
            || !kbo_custom_event_title_for_kind(KBO_CUSTOM_EVENT_KIND_FOREIGN_PRIORITY_CLOSE, close_title, sizeof(close_title))
            || !kbo_custom_event_title_for_kind(KBO_CUSTOM_EVENT_KIND_FA_DECLARATION, fa_declaration_title, sizeof(fa_declaration_title))
            || !kbo_custom_event_title_for_kind(KBO_CUSTOM_EVENT_KIND_INTL_ESTABLISHED_FA, intl_established_fa_title, sizeof(intl_established_fa_title))
            || !kbo_custom_event_title_for_kind(KBO_CUSTOM_EVENT_KIND_MILITARY_SELECTION, military_title, sizeof(military_title))
            || (secondary_draft_year
                && !kbo_custom_event_title_for_kind(
                    KBO_CUSTOM_EVENT_KIND_SECONDARY_DRAFT,
                    secondary_draft_title,
                    sizeof(secondary_draft_title)))) {
        kbo_audit_foreign_priority_schedule("fail", "title_unavailable", source, &audit);
        kbo_log_runtimef(
            "KBO custom event schedule skipped source=%s reason=title_unavailable season_end=%u",
            source != NULL ? source : "",
            offseason_starts_yyyymmdd);
        KBO_FOREIGN_PRIORITY_SCHEDULE_RETURN(-1);
    }

    int open_exists = kbo_custom_event_exists_for_date(
        league_id,
        open_date,
        1);
    int close_exists = kbo_custom_event_exists_for_date(
        league_id,
        close_date,
        0);
    int fa_declaration_exists = kbo_custom_event_exists_by_kind_for_date(
        league_id,
        fa_declaration_date,
        KBO_CUSTOM_EVENT_KIND_FA_DECLARATION);
    int pruned_old_intl_established_fa = 0;
    if (intl_established_fa_date != fa_declaration_date) {
        pruned_old_intl_established_fa = kbo_delete_custom_events_by_kind_for_date(
            league_id,
            fa_declaration_date,
            KBO_CUSTOM_EVENT_KIND_INTL_ESTABLISHED_FA,
            source);
    }
    int intl_established_fa_exists = kbo_custom_event_exists_by_kind_for_date(
        league_id,
        intl_established_fa_date,
        KBO_CUSTOM_EVENT_KIND_INTL_ESTABLISHED_FA);
    audit.pruned_old_intl_established_fa = pruned_old_intl_established_fa;
    int military_exists = military_selection_date == 0u
        || kbo_custom_event_exists_by_kind_for_date(
            league_id,
            military_selection_date,
            KBO_CUSTOM_EVENT_KIND_MILITARY_SELECTION);
    int secondary_draft_exists = !secondary_draft_year
        || kbo_custom_event_exists_by_kind_for_date(
            league_id,
            secondary_draft_date,
            KBO_CUSTOM_EVENT_KIND_SECONDARY_DRAFT);
    if (g_kbo_foreign_priority_last_scheduled_date == offseason_starts_yyyymmdd
            && open_exists
            && close_exists
            && fa_declaration_exists
            && intl_established_fa_exists
            && military_exists
            && secondary_draft_exists
            && pruned_old_intl_established_fa == 0) {
        static uint32_t last_logged_already_scheduled = 0u;
        if (last_logged_already_scheduled != offseason_starts_yyyymmdd) {
            last_logged_already_scheduled = offseason_starts_yyyymmdd;
            kbo_log_runtimef(
                "KBO custom event schedule skipped source=%s reason=already_scheduled_for_season_end today=%u season_end=%u",
                source != NULL ? source : "",
                today,
                offseason_starts_yyyymmdd);
            audit.ready = 1;
            kbo_audit_foreign_priority_schedule("skip", "already_scheduled", source, &audit);
        }
        KBO_FOREIGN_PRIORITY_SCHEDULE_RETURN(0);
    }

    int created_open = 0;
    if (!open_exists) {
        created_open = create_kbo_league_event(
            open_date / 10000u,
            (open_date / 100u) % 100u,
            open_date % 100u,
            league_id,
            OOTP27_EVENT_TYPE_CUSTOM_EVENT,
            open_title,
            0,
            source != NULL ? source : g_kbo_default_event_source);
    }

    int created_close = 0;
    if (!close_exists) {
        created_close = create_kbo_league_event(
            close_date / 10000u,
            (close_date / 100u) % 100u,
            close_date % 100u,
            league_id,
            OOTP27_EVENT_TYPE_CUSTOM_EVENT,
            close_title,
            0,
            source != NULL ? source : g_kbo_default_event_source);
    }

    int created_fa_declaration = 0;
    if (!fa_declaration_exists) {
        created_fa_declaration = create_kbo_league_event(
            fa_declaration_date / 10000u,
            (fa_declaration_date / 100u) % 100u,
            fa_declaration_date % 100u,
            league_id,
            OOTP27_EVENT_TYPE_CUSTOM_EVENT,
            fa_declaration_title,
            0,
            source != NULL ? source : g_kbo_default_event_source);
    }

    int created_intl_established_fa = 0;
    if (!intl_established_fa_exists) {
        created_intl_established_fa = create_kbo_league_event(
            intl_established_fa_date / 10000u,
            (intl_established_fa_date / 100u) % 100u,
            intl_established_fa_date % 100u,
            league_id,
            OOTP27_EVENT_TYPE_CUSTOM_EVENT,
            intl_established_fa_title,
            0,
            source != NULL ? source : g_kbo_default_event_source);
    }

    int created_military = 0;
    if (military_selection_date != 0u
            && !military_exists) {
        created_military = create_kbo_league_event(
            military_selection_date / 10000u,
            (military_selection_date / 100u) % 100u,
            military_selection_date % 100u,
            league_id,
            OOTP27_EVENT_TYPE_CUSTOM_EVENT,
            military_title,
            0,
            source != NULL ? source : g_kbo_default_event_source);
        if (created_military) {
            g_kbo_military_selection_last_scheduled_date = offseason_starts_yyyymmdd;
        }
    }

    int created_secondary_draft = 0;
    if (secondary_draft_year
            && secondary_draft_date != 0u
            && !secondary_draft_exists) {
        created_secondary_draft = create_kbo_league_event(
            secondary_draft_date / 10000u,
            (secondary_draft_date / 100u) % 100u,
            secondary_draft_date % 100u,
            league_id,
            OOTP27_EVENT_TYPE_CUSTOM_EVENT,
            secondary_draft_title,
            0,
            source != NULL ? source : g_kbo_default_event_source);
    }

    open_exists = open_exists || created_open || kbo_custom_event_exists_for_date(
        league_id,
        open_date,
        1);
    close_exists = close_exists || created_close || kbo_custom_event_exists_for_date(
        league_id,
        close_date,
        0);
    fa_declaration_exists = fa_declaration_exists
        || created_fa_declaration
        || kbo_custom_event_exists_by_kind_for_date(
            league_id,
            fa_declaration_date,
            KBO_CUSTOM_EVENT_KIND_FA_DECLARATION);
    intl_established_fa_exists = intl_established_fa_exists
        || created_intl_established_fa
        || kbo_custom_event_exists_by_kind_for_date(
            league_id,
            intl_established_fa_date,
            KBO_CUSTOM_EVENT_KIND_INTL_ESTABLISHED_FA);
    kbo_prune_duplicate_custom_events_by_kind_for_date(
        league_id,
        fa_declaration_date,
        KBO_CUSTOM_EVENT_KIND_FA_DECLARATION,
        source);
    kbo_prune_duplicate_custom_events_by_kind_for_date(
        league_id,
        intl_established_fa_date,
        KBO_CUSTOM_EVENT_KIND_INTL_ESTABLISHED_FA,
        source);
    military_exists = military_selection_date == 0u
        || military_exists
        || created_military
        || kbo_custom_event_exists_by_kind_for_date(
            league_id,
            military_selection_date,
            KBO_CUSTOM_EVENT_KIND_MILITARY_SELECTION);
    secondary_draft_exists = !secondary_draft_year
        || secondary_draft_exists
        || created_secondary_draft
        || kbo_custom_event_exists_by_kind_for_date(
            league_id,
            secondary_draft_date,
            KBO_CUSTOM_EVENT_KIND_SECONDARY_DRAFT);
    if (secondary_draft_year && secondary_draft_date != 0u) {
        kbo_prune_duplicate_custom_events_by_kind_for_date(
            league_id,
            secondary_draft_date,
            KBO_CUSTOM_EVENT_KIND_SECONDARY_DRAFT,
            source);
    }
    int secondary_draft_window_news = 0;
    if (secondary_draft_year && registered_secondary_draft_window) {
        secondary_draft_window_news = kbo_secondary_draft_emit_window_news(
            today != 0u ? today : anchor_date,
            &secondary_draft_window,
            source != NULL ? source : "foreign_priority_schedule");
    }
    audit.created_open = created_open;
    audit.created_close = created_close;
    audit.created_fa_declaration = created_fa_declaration;
    audit.created_intl_established_fa = created_intl_established_fa;
    audit.created_military = created_military;
    audit.ready = open_exists
        && close_exists
        && fa_declaration_exists
        && intl_established_fa_exists
        && military_exists
        && secondary_draft_exists;

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
        created_open,
        created_close,
        created_fa_declaration,
        created_intl_established_fa,
        pruned_old_intl_established_fa,
        created_military,
        created_secondary_draft,
        registered_secondary_draft_window,
        secondary_draft_window_news,
        open_exists
            && close_exists
            && fa_declaration_exists
            && intl_established_fa_exists
            && military_exists
            && secondary_draft_exists);

    if (!(open_exists
            && close_exists
            && fa_declaration_exists
            && intl_established_fa_exists
            && military_exists
            && secondary_draft_exists)) {
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
    int changed = created_open
        || created_close
        || created_fa_declaration
        || created_intl_established_fa
        || pruned_old_intl_established_fa
        || created_military
        || created_secondary_draft
        || secondary_draft_window_news;
    kbo_audit_foreign_priority_schedule(changed ? "schedule" : "ready", "created_or_existing_events", source, &audit);
    KBO_FOREIGN_PRIORITY_SCHEDULE_RETURN(changed);
}

#undef KBO_FOREIGN_PRIORITY_SCHEDULE_RETURN

