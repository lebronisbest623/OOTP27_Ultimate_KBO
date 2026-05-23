#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "custom_event_catalog.h"

#include <stdio.h>

#include "../../../core/core_flags/localappdata/localappdata_reader.h"
#include "../../../core/policy/core_policy.h"

#define KBO_CUSTOM_EVENT_CATALOG_FILE "custom_event_catalog.json"
#define KBO_CUSTOM_EVENT_TITLE_KEY_MAX 128u

static INIT_ONCE g_kbo_custom_event_schedule_policy_once = INIT_ONCE_STATIC_INIT;
static KboCustomEventSchedulePolicy g_kbo_custom_event_schedule_policy;

static const char* kbo_custom_event_catalog_key_for_kind(KboCustomEventKind kind)
{
    switch (kind) {
    case KBO_CUSTOM_EVENT_KIND_FOREIGN_PRIORITY_OPEN:
        return "event.foreign_priority_open.title_key";
    case KBO_CUSTOM_EVENT_KIND_FOREIGN_PRIORITY_CLOSE:
        return "event.foreign_priority_close.title_key";
    case KBO_CUSTOM_EVENT_KIND_MILITARY_SELECTION:
        return "event.military_selection.title_key";
    case KBO_CUSTOM_EVENT_KIND_ASIAN_GAMES_SELECTION:
        return "event.asian_games_selection.title_key";
    case KBO_CUSTOM_EVENT_KIND_ASIAN_GAMES_DEPARTURE:
        return "event.asian_games_departure.title_key";
    case KBO_CUSTOM_EVENT_KIND_ASIAN_GAMES_FINAL:
        return "event.asian_games_final.title_key";
    case KBO_CUSTOM_EVENT_KIND_CBT_EXCEPTION_DEADLINE:
        return "event.cbt_exception_deadline.title_key";
    case KBO_CUSTOM_EVENT_KIND_CBT_ANNOUNCEMENT:
        return "event.cbt_announcement.title_key";
    case KBO_CUSTOM_EVENT_KIND_FA_DECLARATION:
        return "event.fa_declaration.title_key";
    case KBO_CUSTOM_EVENT_KIND_INTL_ESTABLISHED_FA:
        return "event.intl_established_fa.title_key";
    case KBO_CUSTOM_EVENT_KIND_INDEPENDENT_TEAM_ACQUISITION_OPEN:
        return "event.independent_team_acquisition_open.title_key";
    case KBO_CUSTOM_EVENT_KIND_SECONDARY_DRAFT:
        return "event.secondary_draft.title_key";
    default:
        return NULL;
    }
}

static const char* kbo_custom_event_default_title_key(KboCustomEventKind kind)
{
    switch (kind) {
    case KBO_CUSTOM_EVENT_KIND_FOREIGN_PRIORITY_OPEN:
        return "custom_event.foreign_priority.open.title";
    case KBO_CUSTOM_EVENT_KIND_FOREIGN_PRIORITY_CLOSE:
        return "custom_event.foreign_priority.close.title";
    case KBO_CUSTOM_EVENT_KIND_MILITARY_SELECTION:
        return "custom_event.military.selection.title";
    case KBO_CUSTOM_EVENT_KIND_ASIAN_GAMES_SELECTION:
        return "custom_event.asian_games.selection.title";
    case KBO_CUSTOM_EVENT_KIND_ASIAN_GAMES_DEPARTURE:
        return "custom_event.asian_games.departure.title";
    case KBO_CUSTOM_EVENT_KIND_ASIAN_GAMES_FINAL:
        return "custom_event.asian_games.final.title";
    case KBO_CUSTOM_EVENT_KIND_CBT_EXCEPTION_DEADLINE:
        return "custom_event.cbt.exception_deadline.title";
    case KBO_CUSTOM_EVENT_KIND_CBT_ANNOUNCEMENT:
        return "custom_event.cbt.announcement.title";
    case KBO_CUSTOM_EVENT_KIND_FA_DECLARATION:
        return "custom_event.fa.declaration.title";
    case KBO_CUSTOM_EVENT_KIND_INTL_ESTABLISHED_FA:
        return "custom_event.intl_established_fa.title";
    case KBO_CUSTOM_EVENT_KIND_INDEPENDENT_TEAM_ACQUISITION_OPEN:
        return "independent_acquisition.open.title";
    case KBO_CUSTOM_EVENT_KIND_SECONDARY_DRAFT:
        return "custom_event.secondary_draft.title";
    default:
        return NULL;
    }
}

int kbo_custom_event_catalog_title_key(
    KboCustomEventKind kind,
    char* out,
    size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return 0;
    }
    out[0] = '\0';

    const char* catalog_key = kbo_custom_event_catalog_key_for_kind(kind);
    if (catalog_key == NULL) {
        return 0;
    }

    if (kbo_read_localappdata_named_json_string_value(
            KBO_CUSTOM_EVENT_CATALOG_FILE,
            catalog_key,
            out,
            out_size)) {
        return out[0] != '\0';
    }

    const char* fallback = kbo_custom_event_default_title_key(kind);
    if (fallback == NULL) {
        return 0;
    }
    snprintf(out, out_size, "%s", fallback);
    return out[0] != '\0';
}

static BOOL CALLBACK kbo_custom_event_schedule_policy_init_once(
    PINIT_ONCE init_once,
    PVOID parameter,
    PVOID* context)
{
    (void)init_once;
    (void)parameter;
    (void)context;

    KboCustomEventSchedulePolicy* p = &g_kbo_custom_event_schedule_policy;
    p->foreign_priority_fa_declaration_offset_days = kbo_read_clamped_policy_int(
        KBO_CUSTOM_EVENT_CATALOG_FILE,
        "schedule.foreign_priority.fa_declaration_offset_days",
        7,
        0,
        365);
    p->foreign_priority_intl_established_fa_offset_days = kbo_read_clamped_policy_int(
        KBO_CUSTOM_EVENT_CATALOG_FILE,
        "schedule.foreign_priority.intl_established_fa_offset_days",
        21,
        0,
        365);
    p->foreign_priority_military_selection_offset_months = kbo_read_clamped_policy_int(
        KBO_CUSTOM_EVENT_CATALOG_FILE,
        "schedule.foreign_priority.military_selection_offset_months",
        1,
        0,
        24);
    p->independent_team_acquisition_open_offset_months = kbo_read_clamped_policy_int(
        KBO_CUSTOM_EVENT_CATALOG_FILE,
        "schedule.independent_team_acquisition.open_offset_months",
        2,
        0,
        24);
    p->secondary_draft_protection_open_offset_days = kbo_read_clamped_policy_int(
        KBO_CUSTOM_EVENT_CATALOG_FILE,
        "schedule.secondary_draft.protection_open_offset_days",
        7,
        0,
        120);
    p->secondary_draft_submission_deadline_days_before = kbo_read_clamped_policy_int(
        KBO_CUSTOM_EVENT_CATALOG_FILE,
        "schedule.secondary_draft.submission_deadline_days_before",
        10,
        1,
        30);
    p->secondary_draft_offset_days = kbo_read_clamped_policy_int(
        KBO_CUSTOM_EVENT_CATALOG_FILE,
        "schedule.secondary_draft.offset_days",
        24,
        0,
        120);
    return TRUE;
}

const KboCustomEventSchedulePolicy* kbo_custom_event_schedule_policy(void)
{
    InitOnceExecuteOnce(
        &g_kbo_custom_event_schedule_policy_once,
        kbo_custom_event_schedule_policy_init_once,
        NULL,
        NULL);
    return &g_kbo_custom_event_schedule_policy;
}
