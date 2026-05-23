#include "../internal/captain_selection_internal.h"
#include "captain_selection_policy.h"
#include "score/captain_selection_score.h"
#include "../../player_team_history/player_team_seasons.h"
#include "../../core/dates/constants/kbo_date_constants.h"
#include "../../runtime_memory/runtime_memory.h"

static uintptr_t* kbo_captain_copy_player_vector_snapshot(
    uintptr_t player_vector,
    int32_t player_count,
    const char** out_failure_reason)
{
    if (out_failure_reason != NULL) {
        *out_failure_reason = "unknown";
    }
    if (player_vector == 0u || player_count <= 0 || player_count > KBO_RUNTIME_MAX_PLAYER_VECTOR_COUNT) {
        if (out_failure_reason != NULL) { *out_failure_reason = "invalid_vector"; }
        return NULL;
    }
    if ((SIZE_T)player_count > ((SIZE_T)-1 / sizeof(uintptr_t))) {
        if (out_failure_reason != NULL) { *out_failure_reason = "count_overflow"; }
        return NULL;
    }

    SIZE_T bytes = (SIZE_T)player_count * sizeof(uintptr_t);
    if (!memory_range_readable((void*)player_vector, bytes)) {
        if (out_failure_reason != NULL) { *out_failure_reason = "unreadable_vector"; }
        return NULL;
    }

    uintptr_t* snapshot = (uintptr_t*)HeapAlloc(GetProcessHeap(), 0, bytes);
    if (snapshot == NULL) {
        if (out_failure_reason != NULL) { *out_failure_reason = "alloc_failed"; }
        return NULL;
    }

    SIZE_T bytes_read = 0;
    if (!ReadProcessMemory(
            GetCurrentProcess(),
            (LPCVOID)player_vector,
            snapshot,
            bytes,
            &bytes_read)
            || bytes_read != bytes) {
        HeapFree(GetProcessHeap(), 0, snapshot);
        if (out_failure_reason != NULL) { *out_failure_reason = "copy_failed"; }
        return NULL;
    }

    if (out_failure_reason != NULL) {
        *out_failure_reason = NULL;
    }
    return snapshot;
}

static int32_t kbo_captain_player_salary_for_season(uint8_t* player, uint32_t season)
{
    if (player == NULL
            || !memory_range_readable(player + OOTP27_PLAYER_CONTRACT_START_YEAR_OFFSET, sizeof(int32_t))
            || !memory_range_readable(
                player + OOTP27_PLAYER_CONTRACT_SALARY_Y1_OFFSET,
                OOTP27_PLAYER_CONTRACT_SALARY_YEARS * sizeof(int32_t))) {
        return 0;
    }

    int32_t years[OOTP27_PLAYER_CONTRACT_SALARY_YEARS] = {0};
    for (uint32_t i = 0; i < OOTP27_PLAYER_CONTRACT_SALARY_YEARS; i++) {
        years[i] = *(int32_t*)(player + OOTP27_PLAYER_CONTRACT_SALARY_Y1_OFFSET + (i * sizeof(int32_t)));
    }

    int32_t start_year = *(int32_t*)(player + OOTP27_PLAYER_CONTRACT_START_YEAR_OFFSET);
    if (start_year >= (int32_t)KBO_SEASON_YEAR_MIN
            && start_year <= (int32_t)KBO_SIM_YEAR_MAX
            && season >= (uint32_t)start_year) {
        uint32_t index = season - (uint32_t)start_year;
        if (index < OOTP27_PLAYER_CONTRACT_SALARY_YEARS && years[index] > 0) {
            return years[index];
        }
    }

    for (uint32_t i = 0; i < OOTP27_PLAYER_CONTRACT_SALARY_YEARS; i++) {
        if (years[i] > 0) {
            return years[i];
        }
    }
    return 0;
}

static void kbo_captain_copy_team_name(uint32_t team_id, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';

    uint8_t* team = find_kbo_team_by_numeric_id_any_league(team_id, 0);
    if (team == NULL || !memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        snprintf(out, out_size, "Team #%u", team_id);
        return;
    }

    char city[64] = {0};
    char nick[64] = {0};
    copy_ootp_string_object_text(team, OOTP27_KBO_TEAM_CITY_STRING_OFFSET, city, sizeof(city));
    copy_ootp_string_object_text(team, OOTP27_KBO_TEAM_NICKNAME_STRING_OFFSET, nick, sizeof(nick));
    if (city[0] != '\0' && nick[0] != '\0') {
        snprintf(out, out_size, "%s %s", city, nick);
    } else if (nick[0] != '\0') {
        snprintf(out, out_size, "%s", nick);
    } else if (city[0] != '\0') {
        snprintf(out, out_size, "%s", city);
    } else {
        snprintf(out, out_size, "Team #%u", team_id);
    }
}

static int kbo_captain_team_index(uint32_t team_id, const uint32_t* team_ids, int team_count)
{
    for (int i = 0; i < team_count; i++) {
        if (team_ids[i] == team_id) {
            return i;
        }
    }
    return -1;
}

static int kbo_captain_player_name_is_generic_id(const char* name)
{
    if (name == NULL || strncmp(name, "Player #", 8u) != 0) {
        return 0;
    }
    const char* p = name + 8u;
    if (*p < '0' || *p > '9') {
        return 0;
    }
    for (; *p != '\0'; p++) {
        if (*p < '0' || *p > '9') {
            return 0;
        }
    }
    return 1;
}

static void kbo_captain_fill_player_row(
    KboCaptainSelectionRow* row,
    uint8_t* player,
    uint32_t date,
    uint32_t season,
    uint32_t league_id,
    uint32_t team_id)
{
    memset(row, 0, sizeof(*row));
    row->date = date;
    row->season = season;
    row->league_id = league_id;
    row->team_id = team_id;
    kbo_captain_copy_team_name(team_id, row->team_name, sizeof(row->team_name));

    row->player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    row->nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
    row->current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    row->active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    row->current_league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
    row->age = *(uint16_t*)(player + OOTP27_PLAYER_AGE_OFFSET);
    row->salary = kbo_captain_player_salary_for_season(player, season);
    kbo_player_team_seasons_count_for_player(team_id, player, &row->same_team_seasons);
    row->overall_value = kbo_read_player_i16(player, OOTP27_PLAYER_OVERALL_VALUE_OFFSET);
    row->talent_value = kbo_read_player_i16(player, OOTP27_PLAYER_TALENT_VALUE_OFFSET);
    row->ratings_value = kbo_read_player_i16(player, OOTP27_PLAYER_RATINGS_VALUE_OFFSET);
    row->career_value = kbo_read_player_i16(player, OOTP27_PLAYER_CAREER_VALUE_OFFSET);
    row->value_score = kbo_foreign_waiver_value_score(player);
    row->domestic = row->nation_id == OOTP27_KBO_KOREA_NATION_ID ? 1u : 0u;
    row->dfa = player[OOTP27_PLAYER_DFA_FLAG_OFFSET] != 0u ? 1u : 0u;
    row->restricted = (player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET] != 0u
        || player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET] != 0u) ? 1u : 0u;
    row->injured = player[OOTP27_PLAYER_INJURY_ACTIVE_OFFSET] != 0u ? 1u : 0u;
    row->score = kbo_captain_candidate_score(row);
    kbo_copy_player_display_name(player, row->player_name, sizeof(row->player_name));
    if (row->player_name[0] == '\0' || strcmp(row->player_name, "Unknown player") == 0) {
        snprintf(row->player_name, sizeof(row->player_name), "Player #%u", row->player_id);
    }
    snprintf(row->reason, sizeof(row->reason), "heuristic:veteran_value_salary_domestic_team_tenure");
}

int kbo_captain_select_for_preseason(
    uint32_t date,
    uint32_t season,
    uint32_t league_id,
    KboCaptainSelectionRow* rows,
    int max_rows,
    int* out_selected_count)
{
    if (out_selected_count != NULL) {
        *out_selected_count = 0;
    }
    if (!kbo_runtime_pause_for_save_if_needed("captain_selection_ai")) {
        return 0;
    }
    if (rows == NULL || max_rows <= 0 || league_id == 0u || season < KBO_SEASON_YEAR_MIN || season > KBO_SIM_YEAR_MAX) {
        return 0;
    }

    uint32_t team_ids[KBO_CAPTAIN_MAX_TEAMS] = {0};
    uint8_t* team_ptrs[KBO_CAPTAIN_MAX_TEAMS] = {0};
    int scanned_teams = 0;
    int unreadable_teams = 0;
    int team_count = collect_kbo_league_team_ids(
        league_id,
        team_ids,
        KBO_CAPTAIN_MAX_TEAMS,
        &scanned_teams,
        &unreadable_teams);
    if (team_count <= 0) {
        kbo_log_runtimef(
            "KBO captain selection skipped reason=no_teams league_id=%u scanned=%d unreadable=%d",
            league_id,
            scanned_teams,
            unreadable_teams);
        return 0;
    }

    int raw_team_count = team_count;
    int exhibition_count = 0;
    team_count = kbo_captain_filter_regular_team_ids(team_ids, team_count, max_rows, &exhibition_count);
    if (team_count <= 0) {
        kbo_log_runtimef(
            "KBO captain selection skipped reason=no_regular_teams league_id=%u scanned=%d unreadable=%d raw_teams=%d exhibition_skipped=%d",
            league_id,
            scanned_teams,
            unreadable_teams,
            raw_team_count,
            exhibition_count);
        return 0;
    }

    kbo_ensure_captain_seeds_loaded();
    for (int i = 0; i < team_count; i++) {
        memset(&rows[i], 0, sizeof(rows[i]));
        rows[i].date = date;
        rows[i].season = season;
        rows[i].league_id = league_id;
        rows[i].team_id = team_ids[i];
        rows[i].score = -2147483647;
        team_ptrs[i] = find_kbo_team_by_numeric_id_any_league(team_ids[i], 0);
        kbo_captain_copy_team_name(team_ids[i], rows[i].team_name, sizeof(rows[i].team_name));
        snprintf(rows[i].reason, sizeof(rows[i].reason), "no_eligible_candidate");
    }

    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        kbo_log_runtimef("KBO captain selection skipped reason=no_player_vector league_id=%u", league_id);
        return 0;
    }

    const char* snapshot_failure_reason = NULL;
    uintptr_t* player_snapshot = kbo_captain_copy_player_vector_snapshot(
        player_vector,
        player_count,
        &snapshot_failure_reason);
    if (player_snapshot == NULL) {
        kbo_log_runtimef(
            "KBO captain selection skipped reason=player_vector_snapshot_failed detail=%s vector=%p count=%d",
            snapshot_failure_reason != NULL ? snapshot_failure_reason : "",
            (void*)player_vector,
            player_count);
        return 0;
    }

    int scanned_players = 0;
    int selected_count = 0;
    for (int32_t i = 0; i < player_count; i++) {
        if ((i & 127) == 0 && kbo_runtime_save_in_progress()) {
            HeapFree(GetProcessHeap(), 0, player_snapshot);
            kbo_log_runtimef(
                "KBO captain selection aborted reason=save_in_progress date=%u season=%u league_id=%u scanned=%d",
                date,
                season,
                league_id,
                scanned_players);
            return 0;
        }
        uintptr_t player_ptr = player_snapshot[i];
        if (!kbo_player_pointer_plausible(player_ptr)
                || !memory_range_readable((void*)player_ptr, OOTP27_PLAYER_SCAN_BYTES)) {
            continue;
        }

        uint8_t* player = (uint8_t*)player_ptr;
        uint8_t retired = player[OOTP27_PLAYER_RETIRED_FLAG_OFFSET];
        uint16_t age = *(uint16_t*)(player + OOTP27_PLAYER_AGE_OFFSET);
        const KboCaptainSelectionPolicy* policy = kbo_captain_selection_policy();
        if (retired != 0u || (int32_t)age < policy->eligible_age_min || (int32_t)age > policy->eligible_age_max) {
            continue;
        }

        uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
        uint32_t team_id = active_team_id != 0u ? active_team_id : current_team_id;
        int team_index = kbo_captain_team_index(team_id, team_ids, team_count);
        if (team_index < 0 && current_team_id != team_id) {
            team_id = current_team_id;
            team_index = kbo_captain_team_index(team_id, team_ids, team_count);
        }
        if (team_index < 0) {
            continue;
        }

        scanned_players++;
        KboCaptainSelectionRow candidate;
        kbo_captain_fill_player_row(&candidate, player, date, season, league_id, team_id);
        KboCaptainSeed seed;
        if (kbo_find_captain_seed_for_player(
                season,
                league_id,
                team_id,
                team_ptrs[team_index],
                player,
                &seed)) {
            candidate.seeded = 1u;
            candidate.seed_priority = seed.priority;
            snprintf(candidate.seed_source, sizeof(candidate.seed_source), "%s", seed.source);
            snprintf(
                candidate.reason,
                sizeof(candidate.reason),
                "seed:%s",
                seed.player_key[0] != '\0' ? seed.player_key : "player_id");
            if (kbo_captain_player_name_is_generic_id(candidate.player_name)
                    && seed.player_name[0] != '\0') {
                snprintf(candidate.player_name, sizeof(candidate.player_name), "%s", seed.player_name);
            }
        }
        if (kbo_captain_candidate_should_replace(&rows[team_index], &candidate)) {
            rows[team_index] = candidate;
        }
    }

    for (int i = 0; i < team_count; i++) {
        if (rows[i].player_id != 0u) {
            selected_count++;
        }
    }

    HeapFree(GetProcessHeap(), 0, player_snapshot);
    if (out_selected_count != NULL) {
        *out_selected_count = selected_count;
    }
    kbo_log_runtimef(
        "KBO captain selection candidates date=%u season=%u league_id=%u teams=%d players=%d selected=%d raw_teams=%d exhibition_skipped=%d",
        date,
        season,
        league_id,
        team_count,
        scanned_players,
        selected_count,
        raw_team_count,
        exhibition_count);
    return team_count;
}
