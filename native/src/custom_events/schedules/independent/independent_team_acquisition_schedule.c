#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "independent_team_acquisition_schedule.h"
#include "independent_team_acquisition_schedule_module.h"

#include <stdint.h>

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../../core/dates/tick/current_date_tick_capture.h"
#include "../../../core/events/core_league_events.h"
#include "../../../core/logging/core_log.h"
#include "../../../foreign/common/dates/foreign_waiver_date.h"
#include "../../../foreign/common/policy/foreign_waiver_policy.h"
#include "../../../team/classification/team_classification.h"
#include "../../runtime/catalog/custom_event_catalog.h"
#include "../../runtime/ledger/custom_event_ledger.h"
#include "../../runtime/lookup/custom_event_lookup.h"
#include "../../runtime/markers/custom_event_markers.h"
#include "../../runtime/names/custom_event_names.h"
#include "../../runtime/runner/custom_event_runner.h"
#include "../../runtime/state/custom_event_state.h"
#include "../../../team/independent_acquisition/window/independent_acquisition_window.h"

static uint32_t g_kbo_independent_acquisition_schedule_ready_year = 0u;
static uint32_t g_kbo_independent_acquisition_schedule_ready_event_league_id = 0u;
static uint32_t g_kbo_independent_acquisition_schedule_ready_first_open_date = 0u;

static int kbo_process_due_independent_team_acquisition_open_event(
    uint32_t today,
    uint32_t league_id,
    uint32_t open_date,
    const char* title,
    const char* source)
{
    if (today == 0u || open_date == 0u || today < open_date) {
        return 0;
    }
    int completed = kbo_custom_event_processed_marker_exists_for_kind(
            open_date,
            KBO_CUSTOM_EVENT_KIND_INDEPENDENT_TEAM_ACQUISITION_OPEN)
        || kbo_custom_event_ledger_completed(
            league_id,
            open_date,
            KBO_CUSTOM_EVENT_KIND_INDEPENDENT_TEAM_ACQUISITION_OPEN);
    if (completed
            && kbo_independent_team_acquisition_completion_valid(league_id, open_date)) {
        return 0;
    }
    if (completed) {
        kbo_log_runtimef(
            "KBO independent futures acquisition stale completion ignored source=%s event_date=%u today=%u",
            source != NULL ? source : "",
            open_date,
            today);
    }

    int result = kbo_run_custom_event_by_kind(
        0,
        league_id,
        open_date,
        KBO_CUSTOM_EVENT_KIND_INDEPENDENT_TEAM_ACQUISITION_OPEN,
        title,
        source);
    if (result > 0) {
        if (result == KBO_CUSTOM_EVENT_RUN_ALREADY_COMPLETED) {
            return 0;
        }
        kbo_log_runtimef(
            "KBO independent futures acquisition due event handled source=%s event_date=%u today=%u result=%d",
            source != NULL ? source : "",
            open_date,
            today,
            result);
        return 1;
    }

    kbo_log_runtimef(
        "KBO independent futures acquisition due event deferred source=%s event_date=%u today=%u result=%d",
        source != NULL ? source : "",
        open_date,
        today,
        result);
    return -1;
}

int kbo_schedule_independent_team_acquisition_custom_events_for_date(
    uint32_t today,
    const char* source)
{
    if (today == 0u) {
        kbo_log_runtimef(
            "KBO independent futures acquisition schedule skipped source=%s reason=current_date_unavailable",
            source != NULL ? source : "");
        return -1;
    }

    KboIndependentFuturesTeamLeague leagues[16];
    int seed_rows = 0;
    int unresolved_rows = 0;
    int league_count = kbo_collect_independent_futures_team_leagues(
        leagues,
        (int)(sizeof(leagues) / sizeof(leagues[0])),
        &seed_rows,
        &unresolved_rows);
    if (seed_rows <= 0) {
        if (!kbo_team_classification_seed_source_available()) {
            static uint32_t last_logged_seed_missing_date = 0u;
            if (last_logged_seed_missing_date != today) {
                last_logged_seed_missing_date = today;
                kbo_log_runtimef(
                    "KBO independent futures acquisition schedule skipped source=%s reason=team_classification_seed_unavailable today=%u",
                    source != NULL ? source : "",
                    today);
            }
            return 0;
        }
        return 0;
    }
    if (league_count <= 0) {
        static uint32_t last_logged_unresolved_date = 0u;
        if (last_logged_unresolved_date != today) {
            last_logged_unresolved_date = today;
            kbo_log_runtimef(
                "KBO independent futures acquisition schedule skipped source=%s reason=seeded_team_unresolved today=%u seed_rows=%d unresolved=%d",
                source != NULL ? source : "",
                today,
                seed_rows,
                unresolved_rows);
        }
        return 0;
    }

    uint32_t event_league_id = kbo_get_foreign_waiver_league_id();
    if (event_league_id == 0u) {
        event_league_id = kbo_resolve_kbo_league_id();
    }
    if (event_league_id == 0u) {
        kbo_log_runtimef(
            "KBO independent futures acquisition schedule skipped source=%s reason=event_league_id_unavailable today=%u seed_rows=%d leagues=%d",
            source != NULL ? source : "",
            today,
            seed_rows,
            league_count);
        return -1;
    }
    uint32_t schedule_year = today / 10000u;
    uint32_t existing_open_date = kbo_independent_team_acquisition_window_open_date_for_date(today);
    if (existing_open_date != 0u && today >= existing_open_date) {
        static uint32_t last_logged_existing_window_date = 0u;
        if (last_logged_existing_window_date != today) {
            last_logged_existing_window_date = today;
            kbo_log_runtimef(
                "KBO independent futures acquisition schedule skipped source=%s reason=window_already_open today=%u open=%u",
                source != NULL ? source : "",
                today,
                existing_open_date);
        }
        return 0;
    }
    if (g_kbo_independent_acquisition_schedule_ready_year == schedule_year
            && g_kbo_independent_acquisition_schedule_ready_event_league_id == event_league_id
            && g_kbo_independent_acquisition_schedule_ready_first_open_date != 0u
            && today < g_kbo_independent_acquisition_schedule_ready_first_open_date) {
        return 0;
    }

    char title[160] = {0};
    if (!kbo_custom_event_title_for_kind(
            KBO_CUSTOM_EVENT_KIND_INDEPENDENT_TEAM_ACQUISITION_OPEN,
            title,
            sizeof(title))) {
        kbo_log_runtimef(
            "KBO independent futures acquisition schedule skipped source=%s reason=title_unavailable today=%u",
            source != NULL ? source : "",
            today);
        return -1;
    }

    const KboCustomEventSchedulePolicy* policy = kbo_custom_event_schedule_policy();
    uint32_t offset_months = policy->independent_team_acquisition_open_offset_months >= 0
        ? (uint32_t)policy->independent_team_acquisition_open_offset_months
        : 2u;

    int created = 0;
    int direct_processed = 0;
    int direct_deferred = 0;
    int ready = 0;
    int failed = 0;
    int hard_failed = 0;
    uint32_t first_open_date = 0u;
    for (int i = 0; i < league_count; i++) {
        uint32_t anchor_league_id = leagues[i].league_id;
        uint32_t season_start = 0u;
        const char* start_source = "";
        if (!kbo_independent_team_acquisition_resolve_start_date(
                anchor_league_id,
                event_league_id,
                today,
                &season_start,
                &start_source)) {
            failed++;
            kbo_log_runtimef(
                "KBO independent futures acquisition schedule waiting source=%s reason=season_start_unavailable today=%u event_league_id=%u anchor_league_id=%u team=%u csv=%s",
                source != NULL ? source : "",
                today,
                event_league_id,
                anchor_league_id,
                leagues[i].team_id,
                leagues[i].team_csv_id);
            continue;
        }

        uint32_t open_date = kbo_independent_team_acquisition_add_months(season_start, offset_months);
        if (open_date == 0u) {
            failed++;
            kbo_log_runtimef(
                "KBO independent futures acquisition schedule skipped source=%s reason=derived_date_invalid today=%u event_league_id=%u anchor_league_id=%u start=%u offset_months=%u",
                source != NULL ? source : "",
                today,
                event_league_id,
                anchor_league_id,
                season_start,
                offset_months);
            hard_failed = 1;
            continue;
        }
        if (first_open_date == 0u || open_date < first_open_date) {
            first_open_date = open_date;
        }

        int direct_result = kbo_process_due_independent_team_acquisition_open_event(
            today,
            event_league_id,
            open_date,
            title,
            source);
        if (direct_result > 0) {
            direct_processed = 1;
        } else if (direct_result < 0) {
            direct_deferred = 1;
        }

        int exists = kbo_custom_event_exists_by_kind_for_date(
            event_league_id,
            open_date,
            KBO_CUSTOM_EVENT_KIND_INDEPENDENT_TEAM_ACQUISITION_OPEN);
        int created_event = 0;
        if (!exists) {
            created_event = create_kbo_league_event(
                open_date / 10000u,
                (open_date / 100u) % 100u,
                open_date % 100u,
                event_league_id,
                OOTP27_EVENT_TYPE_CUSTOM_EVENT,
                title,
                0,
                source != NULL ? source : g_kbo_default_event_source);
        }

        exists = exists
            || created_event
            || kbo_custom_event_exists_by_kind_for_date(
                event_league_id,
                open_date,
                KBO_CUSTOM_EVENT_KIND_INDEPENDENT_TEAM_ACQUISITION_OPEN);
        if (!exists) {
            hard_failed = 1;
            failed++;
        } else {
            ready++;
        }
        if (created_event) {
            created++;
        }

        kbo_log_runtimef(
            "KBO independent futures acquisition schedule source=%s today=%u event_league_id=%u anchor_league_id=%u team=%u csv=%s start=%u start_source=%s open=%u offset_months=%u created=%d ready=%d",
            source != NULL ? source : "",
            today,
            event_league_id,
            anchor_league_id,
            leagues[i].team_id,
            leagues[i].team_csv_id,
            season_start,
            start_source != NULL ? start_source : "",
            open_date,
            offset_months,
            created_event,
            exists);
    }

    if (ready <= 0 && failed > 0) {
        if (today % 10000u >= 501u) {
            kbo_log_runtimef(
                "KBO independent futures acquisition schedule deferred source=%s reason=futures_memory_start_required today=%u year=%u failed=%d hard_failed=%d",
                source != NULL ? source : "",
                today,
                schedule_year,
                failed,
                hard_failed);
            return -1;
        }
        return hard_failed ? -1 : 0;
    }
    if (direct_deferred) {
        return -1;
    }
    if (ready > 0 && failed == 0 && first_open_date != 0u) {
        g_kbo_independent_acquisition_schedule_ready_year = schedule_year;
        g_kbo_independent_acquisition_schedule_ready_event_league_id = event_league_id;
        g_kbo_independent_acquisition_schedule_ready_first_open_date = first_open_date;
    }
    return created || direct_processed;
}

int kbo_schedule_independent_team_acquisition_custom_events(const char* source)
{
    uint32_t today = 0u;
    if (!kbo_current_date_tick_latest_published_date(&today) || today == 0u) {
        kbo_log_runtimef(
            "KBO independent futures acquisition schedule skipped source=%s reason=ssot_date_unavailable",
            source != NULL ? source : "");
        return -1;
    }
    return kbo_schedule_independent_team_acquisition_custom_events_for_date(today, source);
}
