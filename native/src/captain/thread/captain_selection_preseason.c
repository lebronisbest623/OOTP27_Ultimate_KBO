#include "../internal/captain_selection_internal.h"
#include "../audit/captain_rule_audit.h"
#include "../../core/dates/tick/current_date_tick_capture.h"
#include "../../runtime_memory/runtime_memory.h"

int kbo_captain_current_yyyymmdd(uint32_t* out_date)
{
    if (out_date != NULL) {
        *out_date = 0u;
    }

    uint32_t date = 0u;
    if (!kbo_current_date_tick_latest_published_date(&date)) {
        return 0;
    }
    if (out_date != NULL) {
        *out_date = date;
    }
    return 1;
}

int kbo_captain_find_row_index_by_team(
    const KboCaptainSelectionRow* rows,
    int row_count,
    uint32_t team_id)
{
    if (rows == NULL || row_count <= 0 || team_id == 0u) {
        return -1;
    }
    for (int i = 0; i < row_count; i++) {
        if (rows[i].team_id == team_id) {
            return i;
        }
    }
    return -1;
}

static uint8_t* kbo_captain_find_player_by_id(uint32_t player_id, int* out_vector_available)
{
    if (kbo_runtime_save_in_progress()) {
        return NULL;
    }
    if (out_vector_available != NULL) {
        *out_vector_available = 0;
    }
    if (player_id == 0u) {
        return NULL;
    }

    uintptr_t player_vector = 0u;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        return NULL;
    }
    if (player_vector == 0u || player_count <= 0 || player_count > KBO_RUNTIME_MAX_PLAYER_VECTOR_COUNT
            || !memory_range_readable((void*)player_vector, (SIZE_T)player_count * sizeof(uintptr_t))) {
        return NULL;
    }
    if (out_vector_available != NULL) {
        *out_vector_available = 1;
    }

    for (int32_t i = 0; i < player_count; i++) {
        if ((i & 127) == 0 && kbo_runtime_save_in_progress()) {
            return NULL;
        }
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)
                || !memory_range_readable((void*)player_ptr, OOTP27_PLAYER_SCAN_BYTES)) {
            continue;
        }
        uint8_t* player = (uint8_t*)player_ptr;
        if (*(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET) == player_id) {
            return player;
        }
    }
    return NULL;
}

int kbo_captain_existing_row_still_with_team(const KboCaptainSelectionRow* row)
{
    if (row == NULL || row->team_id == 0u || row->player_id == 0u) {
        return 0;
    }

    int vector_available = 0;
    uint8_t* player = kbo_captain_find_player_by_id(row->player_id, &vector_available);
    if (!vector_available) {
        return 1;
    }
    if (player == NULL) {
        return 0;
    }
    if (player[OOTP27_PLAYER_RETIRED_FLAG_OFFSET] != 0u) {
        return 0;
    }
    if (kbo_player_current_assignment_matches_team_or_affiliate(player, row->team_id)) {
        return 1;
    }
    if (player[OOTP27_PLAYER_INJURY_ACTIVE_OFFSET] != 0u
            && (row->current_team_id == row->team_id || row->active_team_id == row->team_id)) {
        return 1;
    }
    return 0;
}

static int kbo_captain_row_is_resolved_empty_exhibition_team(const KboCaptainSelectionRow* row)
{
    if (row == NULL
            || row->team_id == 0u
            || row->player_id != 0u
            || strcmp(row->reason, "no_eligible_candidate") != 0) {
        return 0;
    }
    return kbo_captain_team_is_exhibition(row->team_id);
}

int kbo_captain_current_rows_need_inseason_repair(
    uint32_t league_id,
    const KboCaptainSelectionRow* current_rows,
    int current_count,
    uint32_t* out_team_ids,
    int* out_team_count,
    int* out_missing_count,
    int* out_departed_count)
{
    if (out_team_count != NULL) { *out_team_count = 0; }
    if (out_missing_count != NULL) { *out_missing_count = 0; }
    if (out_departed_count != NULL) { *out_departed_count = 0; }
    if (out_team_ids == NULL || league_id == 0u) {
        return 0;
    }

    int scanned_teams = 0;
    int unreadable_teams = 0;
    int team_count = collect_kbo_league_team_ids(
        league_id,
        out_team_ids,
        KBO_CAPTAIN_MAX_TEAMS,
        &scanned_teams,
        &unreadable_teams);
    if (team_count <= 0) {
        return 0;
    }
    int exhibition_count = 0;
    team_count = kbo_captain_filter_regular_team_ids(
        out_team_ids,
        team_count,
        KBO_CAPTAIN_MAX_TEAMS,
        &exhibition_count);
    if (team_count <= 0) {
        return 0;
    }
    if (out_team_count != NULL) {
        *out_team_count = team_count;
    }

    int missing_count = 0;
    int departed_count = 0;
    for (int i = 0; i < team_count; i++) {
        int index = kbo_captain_find_row_index_by_team(current_rows, current_count, out_team_ids[i]);
        if (index < 0) {
            missing_count++;
            continue;
        }
        if (kbo_captain_row_is_resolved_empty_exhibition_team(&current_rows[index])) {
            continue;
        }
        if (!kbo_captain_existing_row_still_with_team(&current_rows[index])) {
            departed_count++;
        }
    }
    if (out_missing_count != NULL) { *out_missing_count = missing_count; }
    if (out_departed_count != NULL) { *out_departed_count = departed_count; }
    return missing_count > 0 || departed_count > 0;
}

int kbo_captain_write_initial_selection(
    uint32_t date,
    uint32_t season,
    uint32_t league_id,
    const char* source)
{
    if (!kbo_runtime_pause_for_save_if_needed(source != NULL ? source : "captain_initial_selection")) {
        return 0;
    }

    KboCaptainSelectionRow rows[KBO_CAPTAIN_MAX_TEAMS];
    memset(rows, 0, sizeof(rows));
    int selected_count = 0;
    int row_count = kbo_captain_select_for_preseason(
        date,
        season,
        league_id,
        rows,
        KBO_CAPTAIN_MAX_TEAMS,
        &selected_count);
    if (row_count <= 0 || selected_count <= 0) {
        kbo_captain_audit_preseason_selection(
            "skip", "no_selection", source, date, season, league_id, row_count, selected_count);
        kbo_log_runtimef(
            "KBO captain selection skipped source=%s date=%u season=%u league_id=%u reason=no_selection rows=%d selected=%d",
            source != NULL ? source : "",
            date,
            season,
            league_id,
            row_count,
            selected_count);
        return 0;
    }

    char csv_path[MAX_PATH] = {0};
    int wrote = kbo_captain_write_selection_csv(
        rows,
        row_count,
        source != NULL ? source : "captain_selection",
        csv_path,
        sizeof(csv_path));
    if (wrote) {
        kbo_log_runtimef(
            "KBO captain selection written source=%s date=%u season=%u league_id=%u rows=%d selected=%d csv=%s",
            source != NULL ? source : "",
            date,
            season,
            league_id,
            row_count,
            selected_count,
            csv_path);
        kbo_captain_audit_preseason_selection(
            "write_selection_csv", "selected_captains", source, date, season, league_id, row_count, selected_count);
        kbo_emit_captain_initial_selection_news(
            date,
            season,
            league_id,
            rows,
            row_count,
            source != NULL ? source : "captain_selection");
    }
    return wrote;
}

int kbo_captain_write_missing_selection_csv(
    uint32_t date,
    uint32_t season,
    uint32_t league_id,
    uint8_t phase,
    const char* source)
{
    if (kbo_captain_selection_csv_exists(season)) {
        kbo_captain_audit_preseason_bootstrap(
            "skip", "csv_already_exists", source, date, season, league_id, phase);
        return 0;
    }

    LONG last_attempt = InterlockedCompareExchange(&g_kbo_captain_last_attempted_season, 0, 0);
    if (last_attempt == (LONG)season) {
        kbo_captain_audit_preseason_bootstrap(
            "skip", "season_attempt_in_progress", source, date, season, league_id, phase);
        return 0;
    }
    InterlockedExchange(&g_kbo_captain_last_attempted_season, (LONG)season);

    kbo_log_runtimef(
        "KBO captain selection bootstrap source=%s date=%u season=%u league_id=%u phase=%u reason=missing_csv",
        source != NULL ? source : "",
        date,
        season,
        league_id,
        (unsigned)phase);
    kbo_captain_audit_preseason_bootstrap(
        "write_missing_selection_csv", "missing_csv", source, date, season, league_id, phase);
    int wrote = kbo_captain_write_initial_selection(
        date,
        season,
        league_id,
        source != NULL ? source : "captain_missing_csv_bootstrap");
    InterlockedExchange(&g_kbo_captain_last_attempted_season, 0);
    return wrote;
}

int kbo_captain_run_seed_startup_without_league_ptr(
    uint32_t date,
    uint32_t league_id,
    const char* source)
{
    uint32_t season = date / 10000u;
    int startup_window = kbo_captain_seed_startup_window_active(date, season);
    if (!startup_window) {
        return 0;
    }
    if (league_id == 0u) {
        kbo_log_runtimef(
            "KBO captain seed startup deferred source=%s date=%u season=%u reason=league_id_unavailable",
            source != NULL ? source : "",
            date,
            season);
        return -1;
    }
    int seed_available = kbo_captain_seed_available_for_season(season, league_id);
    if (!seed_available) {
        return 0;
    }

    int csv_exists = kbo_captain_selection_csv_exists(season);
    static uint32_t last_logged_date = 0u;
    static uint32_t last_logged_season = 0u;
    static uint32_t last_logged_league_id = 0u;
    static int last_logged_csv_exists = -1;
    if (date != last_logged_date
            || season != last_logged_season
            || league_id != last_logged_league_id
            || csv_exists != last_logged_csv_exists) {
        kbo_log_runtimef(
            "KBO captain seed startup fallback source=%s date=%u season=%u league_id=%u csv_exists=%d reason=league_ptr_unavailable",
            source != NULL ? source : "",
            date,
            season,
            league_id,
            csv_exists);
        last_logged_date = date;
        last_logged_season = season;
        last_logged_league_id = league_id;
        last_logged_csv_exists = csv_exists;
    }

    if (!csv_exists) {
        kbo_captain_audit_seed_startup(
            "write_missing_selection_csv", "league_ptr_unavailable", source, date, season, league_id,
            startup_window, seed_available, csv_exists, 0);
        int result = kbo_captain_write_missing_selection_csv(
            date,
            season,
            league_id,
            0u,
            source != NULL ? source : "captain_seed_startup_no_league_ptr");
        return result > 0 || kbo_captain_selection_csv_exists(season) ? result : -1;
    }

    if (kbo_captain_initial_selection_news_exists(season, league_id)) {
        return 0;
    }

    KboCaptainSelectionRow summary_rows[KBO_CAPTAIN_MAX_TEAMS];
    memset(summary_rows, 0, sizeof(summary_rows));
    int summary_count = kbo_captain_load_selection_csv(season, summary_rows, KBO_CAPTAIN_MAX_TEAMS);
    if (summary_count <= 0) {
        kbo_captain_audit_seed_startup(
            "skip", "summary_rows_unavailable", source, date, season, league_id,
            startup_window, seed_available, csv_exists, summary_count);
        return -1;
    }
    kbo_captain_audit_seed_startup(
        "emit_initial_selection_news", "league_ptr_unavailable_existing_csv", source, date, season, league_id,
        startup_window, seed_available, csv_exists, summary_count);
    int result = kbo_emit_captain_initial_selection_news(
        date,
        season,
        league_id,
        summary_rows,
        summary_count,
        source != NULL ? source : "captain_seed_startup_summary_no_league_ptr");
    return result > 0 || kbo_captain_initial_selection_news_exists(season, league_id) ? result : -1;
}
