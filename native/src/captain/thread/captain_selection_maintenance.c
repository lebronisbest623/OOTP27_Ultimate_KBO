#include "../internal/captain_selection_internal.h"
#include "maintenance/captain_selection_maintenance_helpers.h"
#include "../../bootstrap/profiling/profiler.h"

int kbo_run_captain_selection_maintenance_for_date(uint32_t date, const char* source)
{
    KBO_PROFILE_BEGIN(profile_captain_selection_maintenance);
    if (!kbo_fix_enabled()) {
        KBO_PROFILE_END(profile_captain_selection_maintenance, "captain.maintenance.disabled");
        return 0;
    }
    if (!kbo_runtime_pause_for_save_if_needed(source != NULL ? source : "captain_selection_maintenance")) {
        KBO_PROFILE_END(profile_captain_selection_maintenance, "captain.maintenance.save_pause_abort");
        return -1;
    }

    if (date == 0u) {
        KBO_PROFILE_END(profile_captain_selection_maintenance, "captain.maintenance.no_date");
        return -1;
    }

    char save_path[MAX_PATH] = {0};
    if (!kbo_get_current_save_path(save_path, sizeof(save_path))) {
        KBO_PROFILE_END(profile_captain_selection_maintenance, "captain.maintenance.no_save");
        return -1;
    }

    uint32_t league_id = kbo_resolve_kbo_league_id();
    KboSeasonPhaseInfo phase_info;
    if (!kbo_season_phase_resolve(league_id, date, 0u, &phase_info)) {
        int result = kbo_captain_run_seed_startup_without_league_ptr(date, league_id, source);
        KBO_PROFILE_END(profile_captain_selection_maintenance, "captain.maintenance.no_league_ptr_seed_startup");
        return result;
    }

    uintptr_t league_ptr = phase_info.league_ptr;
    uint32_t league_season = phase_info.league_year;
    uint8_t phase = phase_info.effective_phase;
    uint32_t season = kbo_captain_effective_season(date, league_season);
    if (season < 1982u || season > 2200u) {
        KBO_PROFILE_END(profile_captain_selection_maintenance, "captain.maintenance.invalid_season");
        return 0;
    }

    int calendar_recovery = kbo_captain_calendar_season_recovery_active(date, league_season, phase);
    int calendar_preseason = kbo_captain_calendar_preseason_window_active(date, league_season, phase);
    int seed_startup = kbo_captain_seed_startup_window_active(date, season)
        && kbo_captain_seed_available_for_season(season, league_id);
    int preseason_first_day = kbo_captain_preseason_first_day_active(date, league_season, phase);
    int csv_exists = kbo_captain_selection_csv_exists(season);
    int initial_news_pending = csv_exists
        && !kbo_captain_initial_selection_news_exists(season, league_id);
    kbo_captain_log_phase_observed(
        source,
        date,
        league_id,
        league_ptr,
        league_season,
        season,
        phase,
        csv_exists,
        calendar_recovery,
        calendar_preseason);

    static uint32_t last_thread_date = 0u;
    static uint32_t last_thread_league_id = 0u;
    static uint32_t last_thread_league_season = 0u;
    static uint32_t last_thread_effective_season = 0u;
    static char last_thread_save_path[MAX_PATH] = {0};
    static uint8_t last_thread_phase = 0xffu;
    static int last_thread_csv_exists = -1;
    static int last_thread_calendar_recovery = -1;
    static int last_thread_calendar_preseason = -1;
    int source_is_captain_thread = source != NULL && strcmp(source, "captain_selection_thread") == 0;
    if (source_is_captain_thread
            && (last_thread_save_path[0] == '\0' || strcmp(last_thread_save_path, save_path) != 0)) {
        snprintf(last_thread_save_path, sizeof(last_thread_save_path), "%s", save_path);
        last_thread_date = 0u;
        last_thread_league_id = 0u;
        last_thread_league_season = 0u;
        last_thread_effective_season = 0u;
        last_thread_phase = 0xffu;
        last_thread_csv_exists = -1;
        last_thread_calendar_recovery = -1;
        last_thread_calendar_preseason = -1;
    }
    if (source_is_captain_thread
            && date == last_thread_date
            && league_id == last_thread_league_id
            && league_season == last_thread_league_season
            && season == last_thread_effective_season
            && phase == last_thread_phase
            && csv_exists == last_thread_csv_exists
            && calendar_recovery == last_thread_calendar_recovery
            && calendar_preseason == last_thread_calendar_preseason
            && !initial_news_pending) {
        KBO_PROFILE_END(profile_captain_selection_maintenance, "captain.maintenance.same_state");
        return 0;
    }
    if (source_is_captain_thread) {
        last_thread_date = date;
        last_thread_league_id = league_id;
        last_thread_league_season = league_season;
        last_thread_effective_season = season;
        last_thread_phase = phase;
        last_thread_csv_exists = csv_exists;
        last_thread_calendar_recovery = calendar_recovery;
        last_thread_calendar_preseason = calendar_preseason;
    }

    if (csv_exists) {
        int news_result = kbo_captain_emit_initial_selection_news_from_csv_or_defer(
            date,
            season,
            league_id,
            league_season,
            phase,
            csv_exists,
            calendar_recovery,
            calendar_preseason,
            seed_startup,
            preseason_first_day,
            source);
        if (news_result < 0) {
            KBO_PROFILE_END(profile_captain_selection_maintenance, "captain.maintenance.initial_news_deferred");
            return -1;
        }
    }

    if (seed_startup && !csv_exists) {
        kbo_captain_audit_maintenance(
            "write_missing_selection_csv",
            "seed_startup_missing_csv",
            source,
            date,
            season,
            league_id,
            league_season,
            phase,
            csv_exists,
            calendar_recovery,
            calendar_preseason,
            seed_startup,
            preseason_first_day);
        int result = kbo_captain_write_missing_csv_or_defer(
            date,
            season,
            league_id,
            phase,
            source != NULL ? source : "captain_seed_startup");
        KBO_PROFILE_END(profile_captain_selection_maintenance, "captain.maintenance.write_seed_startup");
        return result;
    }
    if (preseason_first_day && !csv_exists) {
        kbo_captain_audit_maintenance(
            "write_missing_selection_csv",
            "preseason_first_day_missing_csv",
            source,
            date,
            season,
            league_id,
            league_season,
            phase,
            csv_exists,
            calendar_recovery,
            calendar_preseason,
            seed_startup,
            preseason_first_day);
        int result = kbo_captain_write_missing_csv_or_defer(
            date,
            season,
            league_id,
            phase,
            source != NULL ? source : "captain_preseason_first_day");
        KBO_PROFILE_END(profile_captain_selection_maintenance, "captain.maintenance.write_preseason_first_day");
        return result;
    }
    if (phase == 3u) {
        if (!csv_exists) {
            kbo_captain_audit_maintenance(
                "skip",
                "regular_season_missing_csv_after_first_day",
                source,
                date,
                season,
                league_id,
                league_season,
                phase,
                csv_exists,
                calendar_recovery,
                calendar_preseason,
                seed_startup,
                preseason_first_day);
            KBO_PROFILE_END(profile_captain_selection_maintenance, "captain.maintenance.regular_missing_after_first_day");
            return 0;
        }
        kbo_captain_audit_maintenance(
            "inseason_repair",
            "regular_season_existing_csv",
            source,
            date,
            season,
            league_id,
            league_season,
            phase,
            csv_exists,
            calendar_recovery,
            calendar_preseason,
            seed_startup,
            preseason_first_day);
        int result = kbo_run_captain_inseason_repair_once(
            date,
            season,
            league_id,
            source != NULL ? source : "captain_inseason_thread");
        KBO_PROFILE_END(profile_captain_selection_maintenance, "captain.maintenance.inseason_repair_regular");
        return result;
    }
    if (calendar_recovery) {
        if (!csv_exists) {
            kbo_captain_audit_maintenance(
                "skip",
                "calendar_recovery_missing_csv",
                source,
                date,
                season,
                league_id,
                league_season,
                phase,
                csv_exists,
                calendar_recovery,
                calendar_preseason,
                seed_startup,
                preseason_first_day);
            KBO_PROFILE_END(profile_captain_selection_maintenance, "captain.maintenance.recovery_missing_csv");
            return 0;
        }
        kbo_captain_audit_maintenance(
            "inseason_repair",
            "calendar_year_recovery",
            source,
            date,
            season,
            league_id,
            league_season,
            phase,
            csv_exists,
            calendar_recovery,
            calendar_preseason,
            seed_startup,
            preseason_first_day);
        int result = kbo_run_captain_inseason_repair_once(
            date,
            season,
            league_id,
            source != NULL ? source : "captain_calendar_year_repair");
        KBO_PROFILE_END(profile_captain_selection_maintenance, "captain.maintenance.inseason_repair_recovery");
        return result;
    }
    kbo_captain_audit_maintenance(
        "skip",
        "no_trigger",
        source,
        date,
        season,
        league_id,
        league_season,
        phase,
        csv_exists,
        calendar_recovery,
        calendar_preseason,
        seed_startup,
        preseason_first_day);
    KBO_PROFILE_END(profile_captain_selection_maintenance, "captain.maintenance.no_trigger");
    return 0;
}

int kbo_run_captain_selection_maintenance_once(const char* source)
{
    uint32_t date = 0u;
    if (!kbo_captain_current_yyyymmdd(&date)) {
        return 0;
    }
    int result = kbo_run_captain_selection_maintenance_for_date(date, source);
    return result < 0 ? 0 : result;
}
