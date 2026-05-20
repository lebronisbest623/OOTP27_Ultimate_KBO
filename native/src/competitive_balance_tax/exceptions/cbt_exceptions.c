#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cbt_exceptions.h"
#include "../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../core/csv/core_csv.h"
#include "../../core/files/atomic/core_atomic_file.h"
#include "../../core/files/save_paths/core_save_paths.h"
#include "../../core/logging/core_log.h"
#include "../../core/season/season_calendar.h"
#include "../../fa_salary_snapshot/csv/salary_snapshot_csv_parse.h"
#include "../../fa_salary_snapshot/grading/salary_snapshot_grade_rows.h"
#include "../../foreign/common/dates/foreign_waiver_date.h"
#include "../rules/cbt_rules.h"
#include "../../core/dates/constants/kbo_date_constants.h"

static int kbo_cbt_exception_designation_path(char* out, size_t out_size)
{
    return kbo_get_save_scoped_data_file("cbt_exception_players.csv", out, out_size);
}

static int kbo_cbt_opening_day_cache_path(char* out, size_t out_size)
{
    return kbo_get_save_scoped_data_file("cbt_opening_days.csv", out, out_size);
}

static int kbo_cbt_opening_day_valid(uint32_t season, uint32_t opening_day)
{
    uint32_t year = opening_day / 10000u;
    uint32_t month = (opening_day / 100u) % 100u;
    uint32_t day = opening_day % 100u;
    return season >= KBO_SEASON_YEAR_MIN
        && season <= KBO_SIM_YEAR_MAX
        && year == season
        && month >= 1u
        && month <= 12u
        && day >= 1u
        && day <= 31u;
}

static int kbo_cbt_opening_day_cache_load(uint32_t season, uint32_t* out_opening_day)
{
    if (out_opening_day != NULL) {
        *out_opening_day = 0u;
    }
    if (out_opening_day == NULL || season < KBO_SEASON_YEAR_MIN || season > KBO_SIM_YEAR_MAX) {
        return 0;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_cbt_opening_day_cache_path(path, sizeof(path))) {
        return 0;
    }

    FILE* file = fopen(path, "r");
    if (file == NULL) {
        return 0;
    }

    char line[128] = {0};
    uint32_t found = 0u;
    while (fgets(line, sizeof(line), file) != NULL) {
        unsigned int row_season = 0u;
        unsigned int row_opening_day = 0u;
        if (sscanf(line, "%u,%u", &row_season, &row_opening_day) == 2
                && (uint32_t)row_season == season
                && kbo_cbt_opening_day_valid(season, (uint32_t)row_opening_day)) {
            found = (uint32_t)row_opening_day;
        }
    }
    fclose(file);

    if (found == 0u) {
        return 0;
    }
    *out_opening_day = found;
    return 1;
}

static int kbo_cbt_opening_day_snapshot_load(uint32_t season, uint32_t* out_opening_day)
{
    if (out_opening_day != NULL) {
        *out_opening_day = 0u;
    }
    if (out_opening_day == NULL || season < KBO_SEASON_YEAR_MIN || season > KBO_SIM_YEAR_MAX) {
        return 0;
    }

    KboFaSalarySnapshotGrade row;
    memset(&row, 0, sizeof(row));
    if (kbo_fa_salary_snapshot_load_grade_rows(season, &row, 1, NULL, 0) <= 0
            || !kbo_cbt_opening_day_valid(season, row.opening_day)) {
        return 0;
    }

    *out_opening_day = row.opening_day;
    return 1;
}

int kbo_cbt_exception_resolve_opening_day(uint32_t season, uint32_t* out_opening_day)
{
    if (out_opening_day != NULL) {
        *out_opening_day = 0u;
    }
    if (season < KBO_SEASON_YEAR_MIN || season > KBO_SIM_YEAR_MAX) {
        return 0;
    }

    uint32_t league_id = kbo_resolve_kbo_league_id();
    uint32_t opening_day = 0u;
    if (kbo_season_calendar_resolve_opening_day(
            league_id,
            season,
            0u,
            &opening_day)
            && opening_day / 10000u == season) {
        if (out_opening_day != NULL) {
            *out_opening_day = opening_day;
        }
        return 1;
    }

    if (kbo_cbt_opening_day_cache_load(season, &opening_day)) {
        if (league_id != 0u) {
            (void)kbo_season_calendar_store_opening_day(
                league_id,
                season,
                opening_day,
                0u,
                "legacy_cbt_opening_days");
        }
        if (out_opening_day != NULL) {
            *out_opening_day = opening_day;
        }
        return 1;
    }

    if (kbo_cbt_opening_day_snapshot_load(season, &opening_day)) {
        if (league_id != 0u) {
            (void)kbo_season_calendar_store_opening_day(
                league_id,
                season,
                opening_day,
                0u,
                "fa_salary_snapshot");
        }
        if (out_opening_day != NULL) {
            *out_opening_day = opening_day;
        }
        return 1;
    }
    return 0;
}

int kbo_cbt_exception_designation_window_open(uint32_t season, uint32_t current_date)
{
    uint32_t opening_day = 0u;
    if (current_date == 0u || !kbo_cbt_exception_resolve_opening_day(season, &opening_day)) {
        return 0;
    }
    KboCbtRules rules;
    kbo_cbt_rules_load(&rules);
    return current_date >= opening_day
        && current_date <= kbo_add_days_yyyymmdd(opening_day, rules.exception_deadline_days_after_opening);
}

int kbo_cbt_exception_load_designations(KboCbtExceptionDesignation* rows, int max)
{
    if (rows == NULL || max <= 0) {
        return 0;
    }
    memset(rows, 0, (SIZE_T)max * sizeof(rows[0]));

    char path[MAX_PATH] = {0};
    if (!kbo_cbt_exception_designation_path(path, sizeof(path))) {
        return 0;
    }
    KboCsvReader* reader = kbo_csv_reader_open(path);
    if (reader == NULL) {
        return 0;
    }

    int count = 0;
    while (count < max && kbo_csv_reader_next_row(reader)) {
        char fields[4][128];
        int field_count = kbo_csv_reader_read_trimmed_fields(reader, (char*)fields, sizeof(fields[0]), 4);
        if (field_count < 4 || fields[0][0] < '0' || fields[0][0] > '9') {
            continue;
        }

        KboCbtExceptionDesignation row;
        memset(&row, 0, sizeof(row));
        row.season = (uint32_t)strtoul(fields[0], NULL, 10);
        row.team_id = (uint32_t)strtoul(fields[1], NULL, 10);
        snprintf(row.player_key, sizeof(row.player_key), "%.*s", (int)sizeof(row.player_key) - 1, fields[2]);
        snprintf(row.player_name, sizeof(row.player_name), "%.*s", (int)sizeof(row.player_name) - 1, fields[3]);
        if (row.season != 0u && row.team_id != 0u && row.player_key[0] != '\0') {
            rows[count++] = row;
        }
    }
    kbo_csv_reader_close(reader);
    return count;
}

static int kbo_cbt_exception_write_designations(const KboCbtExceptionDesignation* rows, int count)
{
    char path[MAX_PATH] = {0};
    if (!kbo_cbt_exception_designation_path(path, sizeof(path))) {
        return 0;
    }
    char tmp_path[MAX_PATH] = {0};
    HANDLE file = kbo_atomic_open_tmp(path, tmp_path, sizeof(tmp_path));
    if (file == INVALID_HANDLE_VALUE) {
        kbo_log_runtimef("KBO CBT exception designation open failed path=%s gle=%lu", path, GetLastError());
        return 0;
    }
    DWORD written = 0;
    const char* header = "season,team_id,player_key,player_name\r\n";
    WriteFile(file, header, (DWORD)strlen(header), &written, NULL);
    for (int i = 0; i < count; i++) {
        if (rows[i].season == 0u || rows[i].team_id == 0u || rows[i].player_key[0] == '\0') {
            continue;
        }
        char line[256] = {0};
        int len = snprintf(line, sizeof(line), "%u,%u,", rows[i].season, rows[i].team_id);
        if (len > 0) { WriteFile(file, line, (DWORD)len, &written, NULL); }
        kbo_fa_salary_snapshot_write_csv_text(file, rows[i].player_key);
        WriteFile(file, ",", 1, &written, NULL);
        kbo_fa_salary_snapshot_write_csv_text(file, rows[i].player_name);
        WriteFile(file, "\r\n", 2, &written, NULL);
    }
    if (!kbo_atomic_commit(file, tmp_path, path)) {
        kbo_log_runtimef("KBO CBT exception designation atomic commit failed path=%s gle=%lu", path, GetLastError());
        return 0;
    }
    return 1;
}

int kbo_cbt_exception_save_designation(uint32_t season, uint32_t team_id, const char* player_key, const char* player_name)
{
    if (season < KBO_SEASON_YEAR_MIN || season > KBO_SIM_YEAR_MAX || team_id == 0u || player_key == NULL || player_key[0] == '\0') {
        return 0;
    }
    if (!kbo_cbt_exception_player_eligible(team_id, player_key, NULL)) {
        kbo_log_runtimef("KBO CBT exception rejected season=%u team=%u player_key=%s reason=ineligible", season, team_id, player_key);
        return 0;
    }
    KboCbtExceptionDesignation rows[KBO_CBT_EXCEPTION_MAX];
    int count = kbo_cbt_exception_load_designations(rows, KBO_CBT_EXCEPTION_MAX);
    int slot = -1;
    for (int i = 0; i < count; i++) {
        if (rows[i].season == season && rows[i].team_id == team_id) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        if (count >= KBO_CBT_EXCEPTION_MAX) {
            return 0;
        }
        slot = count++;
    }
    memset(&rows[slot], 0, sizeof(rows[slot]));
    rows[slot].season = season;
    rows[slot].team_id = team_id;
    snprintf(rows[slot].player_key, sizeof(rows[slot].player_key), "%s", player_key);
    snprintf(rows[slot].player_name, sizeof(rows[slot].player_name), "%s", player_name != NULL ? player_name : "");
    return kbo_cbt_exception_write_designations(rows, count);
}

int kbo_cbt_exception_clear_designation(uint32_t season, uint32_t team_id)
{
    KboCbtExceptionDesignation rows[KBO_CBT_EXCEPTION_MAX];
    int count = kbo_cbt_exception_load_designations(rows, KBO_CBT_EXCEPTION_MAX);
    int out = 0;
    for (int i = 0; i < count; i++) {
        if (rows[i].season == season && rows[i].team_id == team_id) {
            continue;
        }
        rows[out++] = rows[i];
    }
    return kbo_cbt_exception_write_designations(rows, out);
}

int kbo_cbt_exception_find_designation(
    const KboCbtExceptionDesignation* rows,
    int count,
    uint32_t season,
    uint32_t team_id,
    const char* player_key)
{
    if (rows == NULL || count <= 0 || season == 0u || team_id == 0u || player_key == NULL || player_key[0] == '\0') {
        return -1;
    }
    for (int i = 0; i < count; i++) {
        if (rows[i].season == season
                && rows[i].team_id == team_id
                && _stricmp(rows[i].player_key, player_key) == 0) {
            return i;
        }
    }
    return -1;
}

