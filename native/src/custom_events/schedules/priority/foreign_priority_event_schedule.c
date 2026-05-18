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
#include "../../../foreign/common/dates/foreign_waiver_date.h"
#include "../../../foreign/common/policy/foreign_player_policy.h"
#include "../../../foreign/common/policy/foreign_waiver_policy.h"
#include "../../../foreign/waiver_window/state/foreign_waiver_window_state.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../runtime/catalog/custom_event_catalog.h"

typedef struct KboForeignPriorityReadyCache {
    uint32_t anchor_date;
    uint32_t league_id;
    uintptr_t event_manager;
    uintptr_t event_vector;
    int32_t event_count;
    DWORD tick;
    uint8_t valid;
} KboForeignPriorityReadyCache;

static KboForeignPriorityReadyCache g_kbo_foreign_priority_ready_cache = {0};

static int kbo_foreign_priority_event_state(
    uintptr_t* out_event_manager,
    uintptr_t* out_event_vector,
    int32_t* out_event_count)
{
    if (out_event_manager != NULL) { *out_event_manager = 0; }
    if (out_event_vector != NULL) { *out_event_vector = 0; }
    if (out_event_count != NULL) { *out_event_count = 0; }

    uintptr_t event_manager = get_kbo_league_event_manager();
    if (event_manager == 0
            || !memory_range_readable((void*)event_manager, OOTP27_EVENT_MANAGER_EVENT_COUNT_OFFSET + sizeof(int32_t))) {
        return 0;
    }

    uintptr_t event_vector = *(uintptr_t*)(event_manager + OOTP27_EVENT_MANAGER_EVENT_VECTOR_OFFSET);
    int32_t event_count = *(int32_t*)(event_manager + OOTP27_EVENT_MANAGER_EVENT_COUNT_OFFSET);
    if (event_vector == 0 || event_count < 0 || event_count > 20000
            || !memory_range_readable((void*)event_vector, (SIZE_T)event_count * sizeof(uintptr_t))) {
        return 0;
    }

    if (out_event_manager != NULL) { *out_event_manager = event_manager; }
    if (out_event_vector != NULL) { *out_event_vector = event_vector; }
    if (out_event_count != NULL) { *out_event_count = event_count; }
    return 1;
}

static int kbo_foreign_priority_ready_cache_hit(uint32_t anchor_date, uint32_t league_id)
{
    KboForeignPriorityReadyCache cached = g_kbo_foreign_priority_ready_cache;
    if (!cached.valid
            || cached.anchor_date != anchor_date
            || cached.league_id != league_id
            || cached.tick == 0u
            || GetTickCount() - cached.tick > 5000u) {
        return 0;
    }

    uintptr_t event_manager = 0;
    uintptr_t event_vector = 0;
    int32_t event_count = 0;
    if (!kbo_foreign_priority_event_state(&event_manager, &event_vector, &event_count)) {
        return 0;
    }
    return cached.event_manager == event_manager
        && cached.event_vector == event_vector
        && cached.event_count == event_count;
}

static void kbo_foreign_priority_ready_cache_store(uint32_t anchor_date, uint32_t league_id)
{
    uintptr_t event_manager = 0;
    uintptr_t event_vector = 0;
    int32_t event_count = 0;
    if (!kbo_foreign_priority_event_state(&event_manager, &event_vector, &event_count)) {
        g_kbo_foreign_priority_ready_cache.valid = 0u;
        return;
    }
    g_kbo_foreign_priority_ready_cache = (KboForeignPriorityReadyCache){
        .anchor_date = anchor_date,
        .league_id = league_id,
        .event_manager = event_manager,
        .event_vector = event_vector,
        .event_count = event_count,
        .tick = GetTickCount(),
        .valid = 1u,
    };
}

int kbo_schedule_foreign_priority_custom_events_at_anchor(
    const char* source,
    uint32_t today,
    uint32_t league_id,
    uint32_t offseason_starts_yyyymmdd)
{
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
        return -1;
    }
    if (today == 0u || offseason_starts_yyyymmdd == 0u) {
        kbo_audit_foreign_priority_schedule("skip", "anchor_unavailable", source, &audit);
        kbo_log_runtimef(
            "KBO custom event schedule skipped source=%s reason=anchor_unavailable today=%u season_end=%u",
            source != NULL ? source : "",
            today,
            offseason_starts_yyyymmdd);
        return -1;
    }

    if (offseason_starts_yyyymmdd > today) {
        kbo_audit_foreign_priority_schedule("skip", "offseason_starts_in_future", source, &audit);
        kbo_log_runtimef(
            "KBO custom event schedule skipped source=%s reason=offseason_starts_in_future season_end=%u today=%u",
            source != NULL ? source : "",
            offseason_starts_yyyymmdd,
            today);
        return 0;
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
    audit.anchor_date = anchor_date;
    audit.open_date = open_date;
    audit.close_date = close_date;
    audit.fa_declaration_date = fa_declaration_date;
    audit.intl_established_fa_date = intl_established_fa_date;
    audit.military_selection_date = military_selection_date;
    if (close_date == 0u || fa_declaration_date == 0u || intl_established_fa_date == 0u) {
        kbo_audit_foreign_priority_schedule("fail", "derived_date_invalid", source, &audit);
        kbo_log_runtimef(
            "KBO custom event schedule skipped source=%s reason=derived_date_invalid season_end=%u anchor=%u close=%u fa_declaration=%u intl_established_fa=%u",
            source != NULL ? source : "",
            offseason_starts_yyyymmdd,
            anchor_date,
            close_date,
            fa_declaration_date,
            intl_established_fa_date);
        return -1;
    }

    if (kbo_foreign_priority_ready_cache_hit(offseason_starts_yyyymmdd, league_id)) {
        return 0;
    }

    char open_title[160] = {0};
    char close_title[160] = {0};
    char fa_declaration_title[160] = {0};
    char intl_established_fa_title[160] = {0};
    char military_title[160] = {0};
    if (!kbo_custom_event_title_for_kind(KBO_CUSTOM_EVENT_KIND_FOREIGN_PRIORITY_OPEN, open_title, sizeof(open_title))
            || !kbo_custom_event_title_for_kind(KBO_CUSTOM_EVENT_KIND_FOREIGN_PRIORITY_CLOSE, close_title, sizeof(close_title))
            || !kbo_custom_event_title_for_kind(KBO_CUSTOM_EVENT_KIND_FA_DECLARATION, fa_declaration_title, sizeof(fa_declaration_title))
            || !kbo_custom_event_title_for_kind(KBO_CUSTOM_EVENT_KIND_INTL_ESTABLISHED_FA, intl_established_fa_title, sizeof(intl_established_fa_title))
            || !kbo_custom_event_title_for_kind(KBO_CUSTOM_EVENT_KIND_MILITARY_SELECTION, military_title, sizeof(military_title))) {
        kbo_audit_foreign_priority_schedule("fail", "title_unavailable", source, &audit);
        kbo_log_runtimef(
            "KBO custom event schedule skipped source=%s reason=title_unavailable season_end=%u",
            source != NULL ? source : "",
            offseason_starts_yyyymmdd);
        return -1;
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
    if (g_kbo_foreign_priority_last_scheduled_date == offseason_starts_yyyymmdd
            && open_exists
            && close_exists
            && fa_declaration_exists
            && intl_established_fa_exists
            && military_exists
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
        return 0;
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
    audit.created_open = created_open;
    audit.created_close = created_close;
    audit.created_fa_declaration = created_fa_declaration;
    audit.created_intl_established_fa = created_intl_established_fa;
    audit.created_military = created_military;
    audit.ready = open_exists && close_exists && fa_declaration_exists && intl_established_fa_exists && military_exists;

    kbo_log_runtimef(
        "KBO custom event schedule source=%s season_end=%u anchor=%u open=%u close=%u fa_declaration=%u intl_established_fa=%u military=%u created_open=%d created_close=%d created_fa_declaration=%d created_intl_established_fa=%d pruned_old_intl_established_fa=%d created_military=%d ready=%d",
        source != NULL ? source : "",
        offseason_starts_yyyymmdd,
        anchor_date,
        open_date,
        close_date,
        fa_declaration_date,
        intl_established_fa_date,
        military_selection_date,
        created_open,
        created_close,
        created_fa_declaration,
        created_intl_established_fa,
        pruned_old_intl_established_fa,
        created_military,
        open_exists && close_exists && fa_declaration_exists && intl_established_fa_exists && military_exists);

    if (!(open_exists && close_exists && fa_declaration_exists && intl_established_fa_exists && military_exists)) {
        kbo_audit_foreign_priority_schedule("fail", "events_not_ready", source, &audit);
        return -1;
    }
    g_kbo_foreign_priority_last_scheduled_date = offseason_starts_yyyymmdd;
    kbo_foreign_priority_ready_cache_store(offseason_starts_yyyymmdd, league_id);
    int changed = created_open
        || created_close
        || created_fa_declaration
        || created_intl_established_fa
        || pruned_old_intl_established_fa
        || created_military;
    kbo_audit_foreign_priority_schedule(changed ? "schedule" : "ready", "created_or_existing_events", source, &audit);
    return changed;
}

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

