#include "../internal/captain_selection_internal.h"
#include "../../bootstrap/profiling/profiler.h"
#include "../../core/dates/tick/current_date_tick_capture.h"
#include "../../core/runtime_tuning/runtime_tuning_policy.h"
#include "../../core/dates/constants/kbo_date_constants.h"

int kbo_run_captain_preseason_selection_once(const char* source)
{
    if (!kbo_fix_enabled()) {
        return 0;
    }
    if (!kbo_runtime_pause_for_save_if_needed(source != NULL ? source : "captain_preseason_selection")) {
        return 0;
    }

    uint32_t date = 0;
    if (!kbo_captain_current_yyyymmdd(&date)) {
        return 0;
    }

    char save_path[MAX_PATH] = {0};
    if (!kbo_get_current_save_path(save_path, sizeof(save_path))) {
        return 0;
    }

    uint32_t league_id = kbo_resolve_kbo_league_id();
    KboSeasonPhaseInfo phase_info;
    if (!kbo_season_phase_resolve(league_id, date, 0u, &phase_info)) {
        return kbo_captain_run_seed_startup_without_league_ptr(date, league_id, source);
    }

    uint32_t league_season = phase_info.league_year;
    uint8_t phase = phase_info.effective_phase;
    uint32_t season = kbo_captain_effective_season(date, league_season);
    int preseason_first_day = kbo_captain_preseason_first_day_active(date, league_season, phase);
    int seed_startup = kbo_captain_seed_startup_window_active(date, season)
        && kbo_captain_seed_available_for_season(season, league_id);
    if (season < KBO_SEASON_YEAR_MIN || season > KBO_SIM_YEAR_MAX || (!preseason_first_day && !seed_startup)) {
        return 0;
    }

    return kbo_captain_write_missing_selection_csv(
        date,
        season,
        league_id,
        phase,
        source != NULL
            ? source
            : (seed_startup
                ? "captain_seed_startup"
                : "captain_preseason_first_day"));
}

DWORD WINAPI kbo_captain_preseason_selection_thread(LPVOID parameter)
{
    (void)parameter;
    kbo_log_runtime_line("KBO captain selection maintenance thread started");

    KboCurrentDateTickConsumer consumer = {0};
    kbo_current_date_tick_consumer_init(
        &consumer,
        "captain_selection_thread",
        KBO_CURRENT_DATE_TICK_CONSUMER_EMIT_CURRENT_ON_SAVE_ENTER);

    while (kbo_runtime_threads_should_continue()) {
        if (!kbo_runtime_pause_for_save_if_needed("captain_selection_thread")) {
            break;
        }

        KboCurrentDateTickWork work = {0};
        while (kbo_current_date_tick_consumer_next(&consumer, &work)) {
            const char* source = work.site_rva == KBO_CURRENT_DATE_TICK_SAVE_ENTER_SITE_RVA
                ? "captain_selection_background_save_enter"
                : "captain_selection_background_post_advance";
            int result = kbo_run_captain_selection_maintenance_for_date(work.date, source);
            if (result < 0 || kbo_runtime_save_in_progress()) {
                break;
            }
            kbo_current_date_tick_consumer_mark_processed(&consumer);
        }

        if (!kbo_runtime_sleep_should_continue((uint32_t)kbo_runtime_tuning_policy()->captain_selection_thread_sleep_ms)) {
            break;
        }
    }

    InterlockedExchange(&g_kbo_captain_preseason_thread_started, 0);
    kbo_log_runtime_line("KBO captain selection maintenance thread stopped");
    return 0;
}

void start_kbo_captain_preseason_selection_thread(void)
{
    if (InterlockedCompareExchange(&g_kbo_captain_preseason_thread_started, 1, 0) != 0) {
        return;
    }

    if (!kbo_start_runtime_thread(kbo_captain_preseason_selection_thread, NULL, "captain selection maintenance")) {
        InterlockedExchange(&g_kbo_captain_preseason_thread_started, 0);
    }
}
