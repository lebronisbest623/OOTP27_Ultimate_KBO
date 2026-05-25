/* All-Star team-split CSV path, storage, and loading. */

#include <stdio.h>
#include <string.h>
#include <windows.h>

#include "allstar_csv_parse.h"
#include "../allstar_league_context/allstar_league_context.h"
#include "../../core/csv/core_csv.h"
#include "../../core/dates/constants/kbo_date_constants.h"
#include "../../core/files/save_paths/core_save_paths.h"
#include "../../core/files/save_paths/platform/core_path_io.h"

static int get_kbo_allstar_teams_csv_path(char* path, size_t path_size)
{
    if (path == NULL || path_size == 0) {
        return 0;
    }
    path[0] = '\0';

    if (kbo_get_save_scoped_data_file("config\\allstar_teams.csv", path, path_size)
            && kbo_get_file_attributes_utf8(path) != INVALID_FILE_ATTRIBUTES) {
        return 1;
    }

    path[0] = '\0';
    HMODULE exe = GetModuleHandleA(NULL);
    char host_stats_dir[KBO_UTF8_PATH_BYTES] = {0};
    if (exe != NULL) {
        WCHAR host_w[KBO_WIDE_PATH_CHARS] = {0};
        DWORD got = GetModuleFileNameW(exe, host_w, KBO_WIDE_PATH_CHARS);
        char host[KBO_UTF8_PATH_BYTES] = {0};
        if (got > 0 && got < KBO_WIDE_PATH_CHARS
                && kbo_wide_to_utf8_path(host_w, host, sizeof(host))) {
            char* slash = strrchr(host, '\\');
            if (slash != NULL) {
                *slash = '\0';
                snprintf(host_stats_dir, sizeof(host_stats_dir), "%s\\data\\stats\\KBO", host);
                snprintf(path, path_size, "%s\\allstar_teams.csv", host_stats_dir);
                if (kbo_get_file_attributes_utf8(path) != INVALID_FILE_ATTRIBUTES) {
                    return 1;
                }
            }
        }
    }

    if (kbo_get_global_data_file("allstar_teams.csv", path, path_size)) {
        if (kbo_get_file_attributes_utf8(path) != INVALID_FILE_ATTRIBUTES) {
            return 1;
        }
    }

    if (host_stats_dir[0] != '\0') {
        snprintf(path, path_size, "%s\\Teams.csv", host_stats_dir);
        if (kbo_get_file_attributes_utf8(path) != INVALID_FILE_ATTRIBUTES) {
            return 1;
        }
    }

    if (kbo_get_global_data_file("Teams.csv", path, path_size)) {
        if (kbo_get_file_attributes_utf8(path) != INVALID_FILE_ATTRIBUTES) {
            return 1;
        }
    }

    return path[0] != '\0';
}

static void add_allstar_team_row(uint16_t year, const char* team_id, const char* team_name, const char* current_city, uint8_t side)
{
    if (year < (int)KBO_HISTORY_YEAR_MIN || year > (int)KBO_SIM_YEAR_MAX || side == 0 || side > 2
            || team_id == NULL || team_id[0] == '\0'
            || g_allstar_team_row_count >= OOTP27_KBO_MAX_ALLSTAR_TEAM_ROWS) {
        return;
    }

    for (int i = 0; i < g_allstar_team_row_count; i++) {
        KboAllstarTeamRow* existing = &g_allstar_team_rows[i];
        if (existing->year == year
                && ascii_equals_ignore_case(existing->team_id, team_id)
                && existing->side == side) {
            return;
        }
    }

    KboAllstarTeamRow* row = &g_allstar_team_rows[g_allstar_team_row_count++];
    row->year = year;
    row->side = side;
    snprintf(row->team_id, sizeof(row->team_id), "%s", team_id);
    snprintf(row->team_name, sizeof(row->team_name), "%s", team_name != NULL ? team_name : "");
    snprintf(row->current_city, sizeof(row->current_city), "%s", current_city != NULL ? current_city : "");
}

void load_allstar_team_rules_once(void)
{
    if (InterlockedCompareExchange(&g_allstar_team_rules_loaded, 1, 0) != 0) {
        return;
    }

    char path[KBO_UTF8_PATH_BYTES] = {0};
    if (!get_kbo_allstar_teams_csv_path(path, sizeof(path))) {
        kbo_log_runtime_line("all-star team split load failed: could not resolve allstar_teams.csv path");
        return;
    }

    KboCsvReader* reader = kbo_csv_reader_open(path);
    if (reader == NULL) {
        kbo_log_runtimef("all-star team split load skipped: csv reader open failed path=%s", path);
        return;
    }

    if (!kbo_csv_reader_next_row(reader)) {
        kbo_csv_reader_close(reader);
        kbo_log_runtimef("all-star team split load failed: empty file path=%s", path);
        return;
    }
    char fields[KBO_ALLSTAR_CSV_MAX_COLUMNS][KBO_ALLSTAR_CSV_FIELD_SIZE];
    int field_count = kbo_csv_reader_read_trimmed_fields(
        reader,
        (char*)fields,
        KBO_ALLSTAR_CSV_FIELD_SIZE,
        KBO_ALLSTAR_CSV_MAX_COLUMNS);

    int year_col = -1;
    int team_id_col = -1;
    int name_col = -1;
    int allstar_col = -1;
    kbo_csv_find_allstar_team_columns_from_fields(
        fields,
        field_count,
        &year_col,
        &team_id_col,
        &name_col,
        &allstar_col);
    if (year_col < 0 || team_id_col < 0 || allstar_col < 0) {
        kbo_csv_reader_close(reader);
        kbo_log_runtimef(
            "all-star team split load skipped: all-star CSV missing columns year_col=%d team_id_col=%d name_col=%d allstar_col=%d path=%s",
            year_col,
            team_id_col,
            name_col,
            allstar_col,
            path);
        return;
    }

    int rows = 0;
    int loaded = 0;
    int invalid_side = 0;
    while (kbo_csv_reader_next_row(reader)) {
        field_count = kbo_csv_reader_read_trimmed_fields(
            reader,
            (char*)fields,
            KBO_ALLSTAR_CSV_FIELD_SIZE,
            KBO_ALLSTAR_CSV_MAX_COLUMNS);
        uint16_t year = 0;
        char team_id[16] = {0};
        char team_name[96] = {0};
        char current_city[64] = {0};
        uint8_t side = 0;
        kbo_csv_extract_allstar_team_fields_from_fields(
            fields,
            field_count,
            year_col,
            team_id_col,
            name_col,
            allstar_col,
            &year,
            team_id,
            sizeof(team_id),
            team_name,
            sizeof(team_name),
            current_city,
            sizeof(current_city),
            &side);
        rows++;
        if (year == 0 || team_id[0] == '\0') {
            continue;
        }
        if (side == 0) {
            invalid_side++;
            continue;
        }

        int before = g_allstar_team_row_count;
        add_allstar_team_row(year, team_id, team_name, current_city, side);
        if (g_allstar_team_row_count != before) {
            loaded++;
        }
    }

    kbo_csv_reader_close(reader);
    kbo_log_runtimef(
        "all-star team split load: rows=%d loaded=%d invalid_side=%d total=%d year_col=%d team_id_col=%d name_col=%d allstar_col=%d path=%s",
        rows,
        loaded,
        invalid_side,
        g_allstar_team_row_count,
        year_col,
        team_id_col,
        name_col,
        allstar_col,
        path);
}
