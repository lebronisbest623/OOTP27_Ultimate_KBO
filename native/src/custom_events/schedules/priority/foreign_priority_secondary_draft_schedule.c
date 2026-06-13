#include "../../runtime/common/custom_events_common.h"
#include "foreign_priority_event_schedule_internal.h"

#include <stdint.h>

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/events/core_league_events.h"
#include "../../../core/logging/core_log.h"
#include "../../../foreign/common/dates/foreign_waiver_date.h"

int kbo_foreign_priority_prepare_secondary_draft_schedule(
    const char* source,
    uint32_t offseason_starts_yyyymmdd,
    uint32_t anchor_date,
    uint32_t close_date,
    uint32_t fa_declaration_date,
    uint32_t intl_established_fa_date,
    const KboCustomEventSchedulePolicy* event_policy,
    KboForeignPriorityEventAudit* audit,
    int* required,
    uint32_t* season,
    uint32_t* protection_open_date,
    uint32_t* deadline_date,
    uint32_t* draft_date)
{
    uint32_t next_season = anchor_date / 10000u;
    int next_required = kbo_secondary_draft_is_odd_season(next_season);
    uint32_t next_open = 0u;
    uint32_t next_deadline = 0u;
    uint32_t next_draft = 0u;

    if (required != NULL) { *required = next_required; }
    if (season != NULL) { *season = next_season; }
    if (protection_open_date != NULL) { *protection_open_date = 0u; }
    if (deadline_date != NULL) { *deadline_date = 0u; }
    if (draft_date != NULL) { *draft_date = 0u; }

    if (next_required) {
        int32_t deadline_offset_days = event_policy->secondary_draft_offset_days
            - event_policy->secondary_draft_submission_deadline_days_before;
        if (deadline_offset_days < 0) {
            kbo_audit_foreign_priority_schedule("fail", "secondary_draft_date_invalid", source, audit);
            kbo_log_runtimef(
                "KBO custom event schedule skipped source=%s reason=secondary_draft_date_invalid season_end=%u offset_days=%d deadline_days_before=%d",
                source != NULL ? source : "",
                offseason_starts_yyyymmdd,
                event_policy->secondary_draft_offset_days,
                event_policy->secondary_draft_submission_deadline_days_before);
            return 0;
        }
        next_open = kbo_add_days_yyyymmdd(
            anchor_date,
            (uint32_t)event_policy->secondary_draft_protection_open_offset_days);
        next_deadline = kbo_add_days_yyyymmdd(anchor_date, (uint32_t)deadline_offset_days);
        next_draft = kbo_add_days_yyyymmdd(
            anchor_date,
            (uint32_t)event_policy->secondary_draft_offset_days);
    }
    if (protection_open_date != NULL) { *protection_open_date = next_open; }
    if (deadline_date != NULL) { *deadline_date = next_deadline; }
    if (draft_date != NULL) { *draft_date = next_draft; }

    if (close_date != 0u
            && fa_declaration_date != 0u
            && intl_established_fa_date != 0u
            && (!next_required
                || (next_open != 0u
                    && next_deadline != 0u
                    && next_draft != 0u
                    && next_open <= next_deadline
                    && next_deadline < next_draft))) {
        return 1;
    }
    kbo_audit_foreign_priority_schedule("fail", "derived_date_invalid", source, audit);
    kbo_log_runtimef(
        "KBO custom event schedule skipped source=%s reason=derived_date_invalid season_end=%u anchor=%u close=%u fa_declaration=%u intl_established_fa=%u secondary_open=%u secondary_deadline=%u secondary_draft=%u",
        source != NULL ? source : "",
        offseason_starts_yyyymmdd,
        anchor_date,
        close_date,
        fa_declaration_date,
        intl_established_fa_date,
        next_open,
        next_deadline,
        next_draft);
    return 0;
}

void kbo_foreign_priority_schedule_secondary_draft_event(
    uint32_t league_id,
    const char* source,
    int required,
    uint32_t season,
    uint32_t protection_open_date,
    uint32_t deadline_date,
    uint32_t draft_date,
    const char* title,
    KboSecondaryDraftWindow* window,
    int* exists,
    int* window_exists,
    int* created,
    int* pruned,
    int* registered_window,
    int* emitted_window_news)
{
    if (created != NULL) { *created = 0; }
    if (pruned != NULL) { *pruned = 0; }
    if (registered_window != NULL) { *registered_window = 0; }
    if (emitted_window_news != NULL) { *emitted_window_news = 0; }
    if (!required) {
        return;
    }

    int event_exists = exists != NULL ? *exists : 0;
    if (!event_exists) {
        int made = create_kbo_league_event(
            draft_date / 10000u,
            (draft_date / 100u) % 100u,
            draft_date % 100u,
            league_id,
            OOTP27_EVENT_TYPE_CUSTOM_EVENT,
            title,
            0,
            source != NULL ? source : g_kbo_default_event_source);
        if (created != NULL) {
            *created = made;
        }
        event_exists = made != 0;
    }

    event_exists = event_exists
        || kbo_custom_event_exists_by_kind_for_date(
            league_id,
            draft_date,
            KBO_CUSTOM_EVENT_KIND_SECONDARY_DRAFT);
    if (exists != NULL) {
        *exists = event_exists;
    }

    int pruned_count = kbo_prune_duplicate_custom_events_by_kind_for_date(
        league_id,
        draft_date,
        KBO_CUSTOM_EVENT_KIND_SECONDARY_DRAFT,
        source);
    if (pruned != NULL) {
        *pruned = pruned_count;
    }
    if (!event_exists || window == NULL) {
        return;
    }

    window->season = season;
    window->league_id = league_id;
    window->protection_open_yyyymmdd = protection_open_date;
    window->protection_deadline_yyyymmdd = deadline_date;
    window->draft_yyyymmdd = draft_date;

    int registered = kbo_secondary_draft_register_window(
        season,
        league_id,
        protection_open_date,
        deadline_date,
        draft_date,
        source);
    if (registered_window != NULL) {
        *registered_window = registered;
    }
    int loaded = registered || kbo_secondary_draft_load_window(season, window);
    if (window_exists != NULL) {
        *window_exists = loaded;
    }
    if (emitted_window_news != NULL) {
        *emitted_window_news = kbo_secondary_draft_emit_window_news(
            protection_open_date,
            window,
            source);
    }
}
