#include "cbt_events.h"
#include "cbt_event_helpers.h"

#include <stdio.h>
#include <windows.h>

#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../core/core_flags/api/flags_api.h"
#include "../../core/dates/core_current_date.h"
#include "../../core/dates/tick/current_date_tick_capture.h"
#include "../../core/events/core_league_events.h"
#include "../../core/logging/core_log.h"
#include "../../custom_events/runtime/ledger/custom_event_ledger.h"
#include "../../custom_events/runtime/lookup/custom_event_lookup.h"
#include "../../custom_events/runtime/markers/custom_event_markers.h"
#include "../../custom_events/runtime/runner/custom_event_runner.h"
#include "../../custom_events/runtime/state/custom_event_state.h"
#include "../../fa_salary_snapshot/capture/salary_snapshot_write_capture.h"
#include "../../fa_salary_snapshot/paths/salary_snapshot_paths_dates.h"
#include "../../foreign/common/dates/foreign_waiver_date.h"
#include "../../hotkey_window/api/hotkey_window_refresh.h"
#include "../../runtime_memory/runtime_memory.h"
#include "../api/competitive_balance_tax.h"
#include "../audit/cbt_rule_audit.h"
#include "../exceptions/cbt_exceptions.h"
#include "../internal/cbt_internal.h"
#include "../records/cbt_records.h"
#include "../rules/cbt_rules.h"

static volatile LONG g_kbo_cbt_event_scheduler_started = 0;
static uint32_t g_kbo_cbt_schedule_ready_season = 0u;
static uint32_t g_kbo_cbt_schedule_ready_league_id = 0u;
static uint32_t g_kbo_cbt_schedule_ready_opening_day = 0u;
static uint32_t g_kbo_cbt_schedule_ready_deadline = 0u;
static uint32_t g_kbo_cbt_schedule_ready_announcement = 0u;
static uint32_t g_kbo_cbt_schedule_ready_checked_after_date = 0u;

int kbo_schedule_cbt_custom_events_for_date(uint32_t today, const char* source)
{
    uint32_t year = today / 10000u;
    if (today == 0u) {
        if (kbo_cbt_should_log_no_date()) {
            kbo_cbt_audit_event_schedule("skip", "ssot_date_unavailable", source, 0u, 0u, 0u, 0u, 0u, 0u, 0, 0, 0, 0, 0);
            kbo_log_runtimef("KBO CBT event schedule skipped source=%s reason=current_date_unavailable", source != NULL ? source : "");
        }
        return -1;
    }

    KboCbtRules rules;
    kbo_cbt_rules_load(&rules);
    if (!rules.enabled) {
        return 0;
    }

    uint32_t opening_day = 0u;
    if (!kbo_cbt_exception_resolve_opening_day(year, &opening_day)) {
        static uint32_t last_logged_no_opening_day = 0u;
        if (last_logged_no_opening_day != today) {
            last_logged_no_opening_day = today;
            kbo_log_runtimef(
                "KBO CBT event schedule skipped source=%s reason=opening_day_unavailable season=%u today=%u",
                source != NULL ? source : "",
                year,
                today);
            kbo_cbt_audit_event_schedule("skip", "opening_day_unavailable", source, year, today, 0u, 0u, 0u, 0u, 0, 0, 0, 0, 0);
        }
        return 0;
    }

    uint32_t deadline = kbo_add_days_yyyymmdd(opening_day, rules.exception_deadline_days_after_opening);
    uint32_t announcement = kbo_add_days_yyyymmdd(opening_day, rules.announcement_days_after_opening);
    if (deadline == 0u || announcement == 0u) {
        kbo_cbt_audit_event_schedule("skip", "date_window", source, year, today, 0u, opening_day, deadline, announcement, 0, 0, 0, 0, 0);
        return 0;
    }

    uint32_t league_id = kbo_resolve_kbo_league_id();
    if (league_id == 0u) {
        kbo_cbt_audit_event_schedule("skip", "league_id_unavailable", source, year, today, 0u, opening_day, deadline, announcement, 0, 0, 0, 0, 0);
        kbo_log_runtimef("KBO CBT event schedule skipped source=%s reason=league_id_unavailable season=%u", source != NULL ? source : "", year);
        return -1;
    }

    uint32_t first_due = deadline < announcement ? deadline : announcement;
    uint32_t last_due = deadline > announcement ? deadline : announcement;
    if (g_kbo_cbt_schedule_ready_season == year
            && g_kbo_cbt_schedule_ready_league_id == league_id
            && g_kbo_cbt_schedule_ready_opening_day == opening_day
            && g_kbo_cbt_schedule_ready_deadline == deadline
            && g_kbo_cbt_schedule_ready_announcement == announcement
            && first_due != 0u
            && today < first_due) {
        return 0;
    }
    if (g_kbo_cbt_schedule_ready_season == year
            && g_kbo_cbt_schedule_ready_league_id == league_id
            && g_kbo_cbt_schedule_ready_opening_day == opening_day
            && g_kbo_cbt_schedule_ready_deadline == deadline
            && g_kbo_cbt_schedule_ready_announcement == announcement
            && g_kbo_cbt_schedule_ready_checked_after_date != 0u
            && today >= g_kbo_cbt_schedule_ready_checked_after_date) {
        return 0;
    }

    int deadline_past = deadline < today;
    int announcement_past = announcement < today;
    char deadline_title[160] = {0};
    char announcement_title[160] = {0};
    if (!kbo_custom_event_title_for_kind(KBO_CUSTOM_EVENT_KIND_CBT_EXCEPTION_DEADLINE, deadline_title, sizeof(deadline_title))
            || !kbo_custom_event_title_for_kind(KBO_CUSTOM_EVENT_KIND_CBT_ANNOUNCEMENT, announcement_title, sizeof(announcement_title))) {
        kbo_log_runtimef(
            "KBO CBT event schedule skipped source=%s reason=title_unavailable season=%u",
            source != NULL ? source : "",
            year);
        kbo_cbt_audit_event_schedule("fail", "title_unavailable", source, year, today, league_id, opening_day, deadline, announcement, 0, 0, 0, 0, 0);
        return -1;
    }

    int pruned_deadline = kbo_prune_duplicate_custom_events_by_kind_for_date(
        league_id,
        deadline,
        KBO_CUSTOM_EVENT_KIND_CBT_EXCEPTION_DEADLINE,
        source);
    int pruned_announcement = kbo_prune_duplicate_custom_events_by_kind_for_date(
        league_id,
        announcement,
        KBO_CUSTOM_EVENT_KIND_CBT_ANNOUNCEMENT,
        source);

    int deadline_completed = kbo_custom_event_processed_marker_exists_for_kind(
            deadline,
            KBO_CUSTOM_EVENT_KIND_CBT_EXCEPTION_DEADLINE)
        || kbo_custom_event_ledger_completed(
            league_id,
            deadline,
            KBO_CUSTOM_EVENT_KIND_CBT_EXCEPTION_DEADLINE);
    int announcement_completed = kbo_custom_event_processed_marker_exists_for_kind(
            announcement,
            KBO_CUSTOM_EVENT_KIND_CBT_ANNOUNCEMENT)
        || kbo_custom_event_ledger_completed(
            league_id,
            announcement,
            KBO_CUSTOM_EVENT_KIND_CBT_ANNOUNCEMENT);

    int deadline_exists = deadline_past || deadline_completed || kbo_custom_event_exists_by_kind_for_date(
            league_id,
            deadline,
            KBO_CUSTOM_EVENT_KIND_CBT_EXCEPTION_DEADLINE);
    int announcement_exists = announcement_past || announcement_completed || kbo_custom_event_exists_by_kind_for_date(
            league_id,
            announcement,
            KBO_CUSTOM_EVENT_KIND_CBT_ANNOUNCEMENT);

    int direct_processed = 0;
    int direct_deadline_processed = 0;
    int direct_announcement_processed = 0;
    int direct_deferred = 0;
    int direct_result = kbo_process_due_cbt_custom_event(
        today,
        league_id,
        deadline,
        KBO_CUSTOM_EVENT_KIND_CBT_EXCEPTION_DEADLINE,
        deadline_title,
        source);
    if (direct_result > 0) {
        direct_processed = 1;
        direct_deadline_processed = 1;
        deadline_exists = 1;
    } else if (direct_result < 0) {
        direct_deferred = 1;
    }
    direct_result = kbo_process_due_cbt_custom_event(
        today,
        league_id,
        announcement,
        KBO_CUSTOM_EVENT_KIND_CBT_ANNOUNCEMENT,
        announcement_title,
        source);
    if (direct_result > 0) {
        direct_processed = 1;
        direct_announcement_processed = 1;
        announcement_exists = 1;
    } else if (direct_result < 0) {
        direct_deferred = 1;
    }

    int created_deadline = 0;
    if (!deadline_exists) {
        created_deadline = create_kbo_league_event(
            deadline / 10000u,
            (deadline / 100u) % 100u,
            deadline % 100u,
            league_id,
            OOTP27_EVENT_TYPE_CUSTOM_EVENT,
            deadline_title,
            0,
            source != NULL ? source : g_kbo_default_event_source);
    }

    int created_announcement = 0;
    if (!announcement_exists) {
        created_announcement = create_kbo_league_event(
            announcement / 10000u,
            (announcement / 100u) % 100u,
            announcement % 100u,
            league_id,
            OOTP27_EVENT_TYPE_CUSTOM_EVENT,
            announcement_title,
            0,
            source != NULL ? source : g_kbo_default_event_source);
    }

    deadline_exists = deadline_past || deadline_completed || direct_deadline_processed || created_deadline || kbo_custom_event_exists_by_kind_for_date(
            league_id,
            deadline,
            KBO_CUSTOM_EVENT_KIND_CBT_EXCEPTION_DEADLINE);
    announcement_exists = announcement_past || announcement_completed || direct_announcement_processed || created_announcement || kbo_custom_event_exists_by_kind_for_date(
            league_id,
            announcement,
            KBO_CUSTOM_EVENT_KIND_CBT_ANNOUNCEMENT);

    kbo_log_runtimef(
        "KBO CBT event schedule source=%s season=%u opening_day=%u deadline=%u announcement=%u created_deadline=%d created_announcement=%d direct_processed=%d direct_deferred=%d pruned_deadline=%d pruned_announcement=%d ready=%d",
        source != NULL ? source : "",
        year,
        opening_day,
        deadline,
        announcement,
        created_deadline,
        created_announcement,
        direct_processed,
        direct_deferred,
        pruned_deadline,
        pruned_announcement,
        deadline_exists && announcement_exists);
    kbo_cbt_audit_event_schedule(
        (deadline_exists && announcement_exists) ? "ready" : "fail",
        (created_deadline || created_announcement) ? "created_or_existing_events" : "existing_or_past_events",
        source,
        year,
        today,
        league_id,
        opening_day,
        deadline,
        announcement,
        created_deadline,
        created_announcement,
        pruned_deadline,
        pruned_announcement,
        deadline_exists && announcement_exists);
    int ready = deadline_exists && announcement_exists;
    if (ready && !direct_deferred) {
        g_kbo_cbt_schedule_ready_season = year;
        g_kbo_cbt_schedule_ready_league_id = league_id;
        g_kbo_cbt_schedule_ready_opening_day = opening_day;
        g_kbo_cbt_schedule_ready_deadline = deadline;
        g_kbo_cbt_schedule_ready_announcement = announcement;
        g_kbo_cbt_schedule_ready_checked_after_date = today >= last_due ? today : 0u;
    }
    if (direct_deferred) {
        return -1;
    }
    return ready
        ? (created_deadline || created_announcement || direct_processed)
        : -1;
}

int kbo_schedule_cbt_custom_events(const char* source)
{
    uint32_t today = 0u;
    if (!kbo_current_date_tick_latest_published_date(&today)) {
        if (kbo_cbt_should_log_no_date()) {
            kbo_cbt_audit_event_schedule("skip", "ssot_date_unavailable", source, 0u, 0u, 0u, 0u, 0u, 0u, 0, 0, 0, 0, 0);
            kbo_log_runtimef("KBO CBT event schedule skipped source=%s reason=ssot_date_unavailable", source != NULL ? source : "");
        }
        return -1;
    }
    return kbo_schedule_cbt_custom_events_for_date(today, source);
}

static DWORD WINAPI kbo_cbt_event_scheduler_thread(LPVOID parameter)
{
    (void)parameter;
    kbo_log_runtime_line(
        "KBO CBT event scheduler retired: custom_event_monitor sync owns CBT scheduling");
    InterlockedExchange(&g_kbo_cbt_event_scheduler_started, 0);
    kbo_log_runtime_line("KBO CBT event scheduler stopped");
    return 0;
}

void start_kbo_cbt_event_scheduler_thread(void)
{
    if (!kbo_fix_enabled()) {
        kbo_log_runtime_line("KBO CBT event scheduler skipped reason=fix_disabled");
        return;
    }
    if (InterlockedCompareExchange(&g_kbo_cbt_event_scheduler_started, 1, 0) != 0) {
        return;
    }
    if (!kbo_start_runtime_thread(kbo_cbt_event_scheduler_thread, NULL, "CBT event scheduler")) {
        InterlockedExchange(&g_kbo_cbt_event_scheduler_started, 0);
    }
}

int kbo_handle_cbt_deadline_event(uint32_t event_yyyymmdd, const char* source)
{
    uint32_t season = event_yyyymmdd / 10000u;
    if (!kbo_cbt_ensure_salary_snapshot_rows(
            season,
            event_yyyymmdd,
            "cbt_deadline_snapshot_ensure")) {
        kbo_log_runtimef(
            "KBO CBT exception designation deadline deferred source=%s date=%u season=%u reason=salary_snapshot_unavailable",
            source != NULL ? source : "",
            event_yyyymmdd,
            season);
        return 0;
    }
    kbo_cbt_exception_auto_designate_missing(season, "cbt_deadline_event");
    kbo_cbt_audit_event_handler("apply_exception_designations", "deadline_event", source, event_yyyymmdd, season);
    kbo_log_runtimef(
        "KBO CBT exception designation deadline reached source=%s date=%u",
        source != NULL ? source : "",
        event_yyyymmdd);
    kbo_request_hotkey_window_refresh("cbt_exception_deadline");
    return 1;
}

int kbo_handle_cbt_announcement_event(uint32_t event_yyyymmdd, const char* source)
{
    uint32_t season = event_yyyymmdd / 10000u;
    uint32_t news_yyyymmdd = event_yyyymmdd;
    if (!kbo_cbt_ensure_salary_snapshot_rows(
            season,
            event_yyyymmdd,
            "cbt_announcement_snapshot_ensure")) {
        kbo_log_runtimef(
            "KBO CBT announcement event deferred source=%s event_date=%u season=%u reason=salary_snapshot_unavailable",
            source != NULL ? source : "",
            event_yyyymmdd,
            season);
        return 0;
    }
    kbo_cbt_exception_auto_designate_missing(season, "cbt_announcement_event");
    kbo_cbt_audit_event_handler("process_tax", "announcement_event", source, event_yyyymmdd, season);
    kbo_log_runtimef(
        "KBO CBT announcement event reached source=%s event_date=%u news_date=%u season=%u",
        source != NULL ? source : "",
        event_yyyymmdd,
        news_yyyymmdd,
        season);
    return kbo_process_competitive_balance_tax_for_date(season, news_yyyymmdd, "cbt_announcement_event");
}
