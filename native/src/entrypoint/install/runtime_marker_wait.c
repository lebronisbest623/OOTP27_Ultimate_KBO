#include "../entrypoint_internal.h"
#include "../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../core/dates/core_current_date.h"
#include "../../core/dates/core_text_date.h"
#include "../../custom_events/asian_games_lifecycle/maintenance/asian_games_lifecycle_maintenance.h"
#include "../../custom_events/asian_games/schedule/asian_games_schedule.h"
#include "../../custom_events/schedules/independent/independent_team_acquisition_schedule.h"
#include "../../custom_events/schedules/priority/foreign_priority_event_schedule.h"
#include "../../core/dates/constants/kbo_date_constants.h"
#include "../../core/core_flags/keys/runtime_flag_keys.generated.h"

static int kbo_runtime_marker_wait_year_matches(uint32_t date_year, uint32_t observed_year)
{
    if (date_year < KBO_TEXT_DATE_YEAR_MIN || date_year > KBO_SIM_YEAR_MAX
            || observed_year < KBO_TEXT_DATE_YEAR_MIN || observed_year > KBO_SIM_YEAR_MAX) {
        return 1;
    }

    uint32_t min_year = date_year > 0u ? date_year - 1u : 0u;
    uint32_t max_year = date_year + 1u;
    return observed_year >= min_year && observed_year <= max_year;
}

static int kbo_runtime_marker_wait_date_matches_save_state(
    uint32_t date,
    uint32_t* out_observed_date,
    uint32_t* out_league_id,
    uint32_t* out_league_year,
    const char** out_reason)
{
    if (out_observed_date != NULL) { *out_observed_date = 0u; }
    if (out_league_id != NULL) { *out_league_id = 0u; }
    if (out_league_year != NULL) { *out_league_year = 0u; }
    if (out_reason != NULL) { *out_reason = "ok"; }

    if (!kbo_yyyymmdd_valid(date)) {
        if (out_reason != NULL) { *out_reason = "invalid_date"; }
        return 0;
    }

    uint32_t date_year = date / 10000u;
    uint32_t observed_date = 0u;
    if (kbo_get_current_yyyymmdd(&observed_date) && kbo_yyyymmdd_valid(observed_date)) {
        if (out_observed_date != NULL) {
            *out_observed_date = observed_date;
        }
        if (!kbo_runtime_marker_wait_year_matches(date_year, observed_date / 10000u)) {
            if (out_reason != NULL) { *out_reason = "current_date_year_mismatch"; }
            return 0;
        }
    }

    uint32_t league_id = kbo_get_foreign_waiver_league_id();
    if (league_id == 0u) {
        league_id = kbo_resolve_kbo_league_id();
    }
    if (out_league_id != NULL) {
        *out_league_id = league_id;
    }
    if (league_id == 0u) {
        return 1;
    }

    uint32_t league_year = kbo_find_league_year_from_id(league_id);
    if (out_league_year != NULL) {
        *out_league_year = league_year;
    }
    if (!kbo_runtime_marker_wait_year_matches(date_year, league_year)) {
        if (out_reason != NULL) { *out_reason = "league_year_mismatch"; }
        return 0;
    }

    return 1;
}

static void kbo_prime_startup_custom_event_schedules(uint32_t today)
{
    if (today == 0u) {
        return;
    }

    const char* source = "runtime_marker_wait_startup";
    int foreign = kbo_schedule_foreign_priority_custom_events_for_date(source, today);
    int asian = kbo_schedule_asian_games_custom_events_for_date(today, source);
    int asian_hold = kbo_maintain_asian_games_restricted_players(today, source);
    int cbt = kbo_schedule_cbt_custom_events_for_date(today, source);
    int independent = kbo_schedule_independent_team_acquisition_custom_events_for_date(today, source);

    kbo_log_runtimef(
        "KBO startup custom event schedules primed source=%s today=%u foreign=%d asian=%d asian_hold=%d cbt=%d independent=%d",
        source,
        today,
        foreign,
        asian,
        asian_hold,
        cbt,
        independent);
}

static volatile LONG64 g_kbo_runtime_marker_guard_started_filetime = 0;

static LONG64 kbo_filetime_to_i64(FILETIME time)
{
    ULARGE_INTEGER value;
    value.LowPart = time.dwLowDateTime;
    value.HighPart = time.dwHighDateTime;
    return (LONG64)value.QuadPart;
}

DWORD WINAPI kbo_full_runtime_marker_wait_thread(LPVOID parameter)
{
    HINSTANCE instance = (HINSTANCE)parameter;
    FILETIME guard_started_time = {0};
    GetSystemTimeAsFileTime(&guard_started_time);
    InterlockedExchange64(
        &g_kbo_runtime_marker_guard_started_filetime,
        kbo_filetime_to_i64(guard_started_time));
    kbo_log_runtime_line("KBO full runtime marker guard thread started");

    int early_amateur_team_add_guard_installed = 0;
    KboCurrentDateTickConsumer date_consumer = {0};
    kbo_current_date_tick_consumer_init(
        &date_consumer,
        "runtime_marker_wait",
        KBO_CURRENT_DATE_TICK_CONSUMER_EMIT_CURRENT_ON_SAVE_ENTER);
    const KboRuntimeTuningPolicy* tuning = kbo_runtime_tuning_policy();
    for (int attempt = 1; attempt <= tuning->runtime_marker_wait_attempts; attempt++) {
        int log_detail = kbo_runtime_tuning_runtime_marker_log_attempt(attempt);
        if (kbo_current_save_has_required_roster_marker("runtime_marker_wait", log_detail)) {
            if (read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_ENABLE_KBO_CBT_SERVICE_TIME_PROBE_FILE)) {
                kbo_cbt_service_time_probe_once();
            }

            if (!early_amateur_team_add_guard_installed
                    && !read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_DISABLE_AMATEUR_ASSIGNMENT_REROUTE_FILE)
                    && !read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_DISABLE_KBO_MILITARY_TEAM_ADD_GUARD_PATCH_FILE)) {
                kbo_log_runtime_line("KBO early amateur team-add guard installing after roster marker");
                early_amateur_team_add_guard_installed = install_kbo_military_team_add_guard_patch();
            }

            KboCurrentDateTickWork date_work = {0};
            int date_rejected = 0;
            if (kbo_current_date_tick_consumer_next(&date_consumer, &date_work)) {
                uint32_t observed_date = 0u;
                uint32_t league_id = 0u;
                uint32_t league_year = 0u;
                const char* stale_reason = "unknown";
                if (!kbo_runtime_marker_wait_date_matches_save_state(
                        date_work.date,
                        &observed_date,
                        &league_id,
                        &league_year,
                        &stale_reason)) {
                    kbo_log_runtimef(
                        "KBO full runtime marker guard rejected stale date source=runtime_marker_wait date=%u event=%u site=0x%x seq=%u observed_date=%u league_id=%u league_year=%u reason=%s",
                        date_work.date,
                        date_work.event_date,
                        date_work.site_rva,
                        date_work.sequence,
                        observed_date,
                        league_id,
                        league_year,
                        stale_reason);
                    kbo_current_date_tick_force_resync("runtime_marker_wait", stale_reason);
                    kbo_current_date_tick_consumer_skip_to_latest(&date_consumer);
                    date_rejected = 1;
                } else {
                    kbo_current_date_tick_consumer_mark_processed(&date_consumer);
                    InterlockedExchange(&g_kbo_runtime_date_stable_ready, 1);
                    kbo_prime_startup_custom_event_schedules(date_work.date);
                    kbo_log_runtimef(
                        "KBO full runtime marker guard ready source=runtime_marker_wait date=%u observed_date=%u league_id=%u league_year=%u",
                        date_work.date,
                        observed_date,
                        league_id,
                        league_year);
                    install_kbo_full_runtime_after_roster_marker(instance);
                    return 0;
                }
            }

            if (log_detail) {
                kbo_log_runtimef(
                    "KBO full runtime marker guard waiting source=runtime_marker_wait reason=%s",
                    date_rejected ? "current_date_stale_resync" : "current_date_hook_unavailable");
            }
        }
        Sleep((DWORD)tuning->runtime_marker_wait_sleep_ms);
    }

    kbo_log_runtime_line("KBO full runtime marker guard gave up: required roster marker was not found");
    return 0;
}
