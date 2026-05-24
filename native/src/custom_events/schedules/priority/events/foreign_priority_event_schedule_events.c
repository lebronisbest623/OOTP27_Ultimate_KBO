#include "../foreign_priority_event_schedule_internal.h"

#include <string.h>

#include "../../../runtime/common/custom_events_common.h"

#include "../../../../bootstrap/abi/ootp_offsets.h"
#include "../../../../core/events/core_league_events.h"
#include "../../../../core/logging/core_log.h"
#include "../../../runtime/catalog/custom_event_catalog.h"

static int kbo_foreign_priority_load_event_titles(
    int secondary_draft_year,
    char* open_title,
    size_t open_title_size,
    char* close_title,
    size_t close_title_size,
    char* fa_declaration_title,
    size_t fa_declaration_title_size,
    char* intl_established_fa_title,
    size_t intl_established_fa_title_size,
    char* military_title,
    size_t military_title_size,
    char* secondary_draft_title,
    size_t secondary_draft_title_size)
{
    return kbo_custom_event_title_for_kind(KBO_CUSTOM_EVENT_KIND_FOREIGN_PRIORITY_OPEN, open_title, open_title_size)
        && kbo_custom_event_title_for_kind(KBO_CUSTOM_EVENT_KIND_FOREIGN_PRIORITY_CLOSE, close_title, close_title_size)
        && kbo_custom_event_title_for_kind(KBO_CUSTOM_EVENT_KIND_FA_DECLARATION, fa_declaration_title, fa_declaration_title_size)
        && kbo_custom_event_title_for_kind(KBO_CUSTOM_EVENT_KIND_INTL_ESTABLISHED_FA, intl_established_fa_title, intl_established_fa_title_size)
        && kbo_custom_event_title_for_kind(KBO_CUSTOM_EVENT_KIND_MILITARY_SELECTION, military_title, military_title_size)
        && (!secondary_draft_year
            || kbo_custom_event_title_for_kind(
                KBO_CUSTOM_EVENT_KIND_SECONDARY_DRAFT,
                secondary_draft_title,
                secondary_draft_title_size));
}

int kbo_foreign_priority_schedule_events(
    uint32_t league_id,
    uint32_t today,
    uint32_t offseason_starts_yyyymmdd,
    const KboForeignPriorityScheduleDates* dates,
    const KboSecondaryDraftWindow* secondary_draft_window,
    int registered_secondary_draft_window,
    const char* source,
    KboForeignPriorityEventAudit* audit,
    KboForeignPriorityScheduleResult* out)
{
    if (dates == NULL || audit == NULL || out == NULL) {
        return -1;
    }
    memset(out, 0, sizeof(*out));

    char open_title[160] = {0};
    char close_title[160] = {0};
    char fa_declaration_title[160] = {0};
    char intl_established_fa_title[160] = {0};
    char military_title[160] = {0};
    char secondary_draft_title[160] = {0};
    if (!kbo_foreign_priority_load_event_titles(
            dates->secondary_draft_year,
            open_title,
            sizeof(open_title),
            close_title,
            sizeof(close_title),
            fa_declaration_title,
            sizeof(fa_declaration_title),
            intl_established_fa_title,
            sizeof(intl_established_fa_title),
            military_title,
            sizeof(military_title),
            secondary_draft_title,
            sizeof(secondary_draft_title))) {
        kbo_audit_foreign_priority_schedule("fail", "title_unavailable", source, audit);
        kbo_log_runtimef(
            "KBO custom event schedule skipped source=%s reason=title_unavailable season_end=%u",
            source != NULL ? source : "",
            offseason_starts_yyyymmdd);
        return -1;
    }

    int open_exists = kbo_custom_event_exists_for_date(league_id, dates->open_date, 1);
    int close_exists = kbo_custom_event_exists_for_date(league_id, dates->close_date, 0);
    int fa_declaration_exists = kbo_custom_event_exists_by_kind_for_date(
        league_id,
        dates->fa_declaration_date,
        KBO_CUSTOM_EVENT_KIND_FA_DECLARATION);
    int pruned_old_intl_established_fa = 0;
    if (dates->intl_established_fa_date != dates->fa_declaration_date) {
        pruned_old_intl_established_fa = kbo_delete_custom_events_by_kind_for_date(
            league_id,
            dates->fa_declaration_date,
            KBO_CUSTOM_EVENT_KIND_INTL_ESTABLISHED_FA,
            source);
    }
    int intl_established_fa_exists = kbo_custom_event_exists_by_kind_for_date(
        league_id,
        dates->intl_established_fa_date,
        KBO_CUSTOM_EVENT_KIND_INTL_ESTABLISHED_FA);
    audit->pruned_old_intl_established_fa = pruned_old_intl_established_fa;
    int military_exists = dates->military_selection_date == 0u
        || kbo_custom_event_exists_by_kind_for_date(
            league_id,
            dates->military_selection_date,
            KBO_CUSTOM_EVENT_KIND_MILITARY_SELECTION);
    int secondary_draft_exists = !dates->secondary_draft_year
        || kbo_custom_event_exists_by_kind_for_date(
            league_id,
            dates->secondary_draft_date,
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
            audit->ready = 1;
            kbo_audit_foreign_priority_schedule("skip", "already_scheduled", source, audit);
        }
        out->ready = 1;
        out->already_scheduled = 1;
        return 1;
    }

    if (!open_exists) {
        out->created_open = create_kbo_league_event(
            dates->open_date / 10000u,
            (dates->open_date / 100u) % 100u,
            dates->open_date % 100u,
            league_id,
            OOTP27_EVENT_TYPE_CUSTOM_EVENT,
            open_title,
            0,
            source != NULL ? source : g_kbo_default_event_source);
    }
    if (!close_exists) {
        out->created_close = create_kbo_league_event(
            dates->close_date / 10000u,
            (dates->close_date / 100u) % 100u,
            dates->close_date % 100u,
            league_id,
            OOTP27_EVENT_TYPE_CUSTOM_EVENT,
            close_title,
            0,
            source != NULL ? source : g_kbo_default_event_source);
    }
    if (!fa_declaration_exists) {
        out->created_fa_declaration = create_kbo_league_event(
            dates->fa_declaration_date / 10000u,
            (dates->fa_declaration_date / 100u) % 100u,
            dates->fa_declaration_date % 100u,
            league_id,
            OOTP27_EVENT_TYPE_CUSTOM_EVENT,
            fa_declaration_title,
            0,
            source != NULL ? source : g_kbo_default_event_source);
    }
    if (!intl_established_fa_exists) {
        out->created_intl_established_fa = create_kbo_league_event(
            dates->intl_established_fa_date / 10000u,
            (dates->intl_established_fa_date / 100u) % 100u,
            dates->intl_established_fa_date % 100u,
            league_id,
            OOTP27_EVENT_TYPE_CUSTOM_EVENT,
            intl_established_fa_title,
            0,
            source != NULL ? source : g_kbo_default_event_source);
    }
    if (dates->military_selection_date != 0u && !military_exists) {
        out->created_military = create_kbo_league_event(
            dates->military_selection_date / 10000u,
            (dates->military_selection_date / 100u) % 100u,
            dates->military_selection_date % 100u,
            league_id,
            OOTP27_EVENT_TYPE_CUSTOM_EVENT,
            military_title,
            0,
            source != NULL ? source : g_kbo_default_event_source);
        if (out->created_military) {
            g_kbo_military_selection_last_scheduled_date = offseason_starts_yyyymmdd;
        }
    }
    if (dates->secondary_draft_year && dates->secondary_draft_date != 0u && !secondary_draft_exists) {
        out->created_secondary_draft = create_kbo_league_event(
            dates->secondary_draft_date / 10000u,
            (dates->secondary_draft_date / 100u) % 100u,
            dates->secondary_draft_date % 100u,
            league_id,
            OOTP27_EVENT_TYPE_CUSTOM_EVENT,
            secondary_draft_title,
            0,
            source != NULL ? source : g_kbo_default_event_source);
    }

    open_exists = open_exists || out->created_open || kbo_custom_event_exists_for_date(league_id, dates->open_date, 1);
    close_exists = close_exists || out->created_close || kbo_custom_event_exists_for_date(league_id, dates->close_date, 0);
    fa_declaration_exists = fa_declaration_exists
        || out->created_fa_declaration
        || kbo_custom_event_exists_by_kind_for_date(league_id, dates->fa_declaration_date, KBO_CUSTOM_EVENT_KIND_FA_DECLARATION);
    intl_established_fa_exists = intl_established_fa_exists
        || out->created_intl_established_fa
        || kbo_custom_event_exists_by_kind_for_date(league_id, dates->intl_established_fa_date, KBO_CUSTOM_EVENT_KIND_INTL_ESTABLISHED_FA);
    kbo_prune_duplicate_custom_events_by_kind_for_date(
        league_id,
        dates->fa_declaration_date,
        KBO_CUSTOM_EVENT_KIND_FA_DECLARATION,
        source);
    kbo_prune_duplicate_custom_events_by_kind_for_date(
        league_id,
        dates->intl_established_fa_date,
        KBO_CUSTOM_EVENT_KIND_INTL_ESTABLISHED_FA,
        source);
    military_exists = dates->military_selection_date == 0u
        || military_exists
        || out->created_military
        || kbo_custom_event_exists_by_kind_for_date(league_id, dates->military_selection_date, KBO_CUSTOM_EVENT_KIND_MILITARY_SELECTION);
    secondary_draft_exists = !dates->secondary_draft_year
        || secondary_draft_exists
        || out->created_secondary_draft
        || kbo_custom_event_exists_by_kind_for_date(league_id, dates->secondary_draft_date, KBO_CUSTOM_EVENT_KIND_SECONDARY_DRAFT);
    if (dates->secondary_draft_year && dates->secondary_draft_date != 0u) {
        kbo_prune_duplicate_custom_events_by_kind_for_date(
            league_id,
            dates->secondary_draft_date,
            KBO_CUSTOM_EVENT_KIND_SECONDARY_DRAFT,
            source);
    }
    if (dates->secondary_draft_year && registered_secondary_draft_window && secondary_draft_window != NULL) {
        out->secondary_draft_window_news = kbo_secondary_draft_emit_window_news(
            today != 0u ? today : dates->anchor_date,
            secondary_draft_window,
            source != NULL ? source : "foreign_priority_schedule");
    }

    out->pruned_old_intl_established_fa = pruned_old_intl_established_fa;
    out->ready = open_exists
        && close_exists
        && fa_declaration_exists
        && intl_established_fa_exists
        && military_exists
        && secondary_draft_exists;
    out->changed = out->created_open
        || out->created_close
        || out->created_fa_declaration
        || out->created_intl_established_fa
        || out->pruned_old_intl_established_fa
        || out->created_military
        || out->created_secondary_draft
        || out->secondary_draft_window_news;

    audit->created_open = out->created_open;
    audit->created_close = out->created_close;
    audit->created_fa_declaration = out->created_fa_declaration;
    audit->created_intl_established_fa = out->created_intl_established_fa;
    audit->created_military = out->created_military;
    audit->ready = out->ready;
    return out->ready ? 1 : 0;
}
