#include "../internal/captain_selection_internal.h"
#include "../../core/csv/core_csv.h"
#include "../../core/files/atomic/core_atomic_file.h"
#include "parse/captain_selection_csv_parse.h"
#include "../../core/dates/constants/kbo_date_constants.h"

int kbo_captain_selection_csv_path(uint32_t season, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0 || season < KBO_SEASON_YEAR_MIN || season > KBO_SIM_YEAR_MAX) {
        return 0;
    }

    char file_name[96] = {0};
    snprintf(file_name, sizeof(file_name), "captains_%04u.csv", season);
    return kbo_get_save_scoped_data_file(file_name, out, out_size);
}

int kbo_captain_selection_csv_exists(uint32_t season)
{
    char path[MAX_PATH] = {0};
    if (!kbo_captain_selection_csv_path(season, path, sizeof(path))) {
        return 0;
    }

    WIN32_FILE_ATTRIBUTE_DATA attrs;
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &attrs)) {
        return 0;
    }
    return attrs.nFileSizeHigh != 0u || attrs.nFileSizeLow != 0u;
}

int kbo_captain_load_selection_csv(
    uint32_t season,
    KboCaptainSelectionRow* rows,
    int max_rows)
{
    if (rows == NULL || max_rows <= 0 || season < KBO_SEASON_YEAR_MIN || season > KBO_SIM_YEAR_MAX) {
        return 0;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_captain_selection_csv_path(season, path, sizeof(path))) {
        return 0;
    }

    int count = 0;
    KboCsvReader* reader = kbo_csv_reader_open(path);
    if (reader == NULL) {
        return 0;
    }

    while (count < max_rows && kbo_csv_reader_next_row(reader)) {
        char fields[KBO_CAPTAIN_SELECTION_CSV_MAX_FIELDS][KBO_CAPTAIN_SELECTION_CSV_FIELD_SIZE];
        int field_count = kbo_csv_reader_read_trimmed_fields(
            reader,
            (char*)fields,
            KBO_CAPTAIN_SELECTION_CSV_FIELD_SIZE,
            KBO_CAPTAIN_SELECTION_CSV_MAX_FIELDS);
        KboCaptainSelectionRow row;
        if (kbo_captain_parse_selection_csv_fields(fields, field_count, season, &row)) {
            rows[count++] = row;
        }
    }

    kbo_csv_reader_close(reader);
    return count;
}

static void kbo_captain_write_csv_text(HANDLE file, const char* text)
{
    DWORD written = 0;
    WriteFile(file, "\"", 1, &written, NULL);
    if (text != NULL) {
        for (const char* p = text; *p != '\0'; p++) {
            if (*p == '"') {
                WriteFile(file, "\"\"", 2, &written, NULL);
            } else {
                WriteFile(file, p, 1, &written, NULL);
            }
        }
    }
    WriteFile(file, "\"", 1, &written, NULL);
}

static int kbo_captain_write_header(HANDLE file)
{
    const char* header =
        "date,season,source,league_id,team_id,team_name,captain_player_id,captain_name,"
        "score,reason,seeded,seed_priority,seed_source,nation_id,domestic,current_team_id,active_team_id,current_league_id,"
        "age,salary,value_score,same_team_seasons,overall_value,talent_value,ratings_value,career_value,"
        "dfa,restricted,injured\r\n";
    DWORD written = 0;
    return WriteFile(file, header, (DWORD)strlen(header), &written, NULL)
        && written == strlen(header);
}

int kbo_captain_write_selection_csv(
    const KboCaptainSelectionRow* rows,
    int row_count,
    const char* source,
    char* out_path,
    size_t out_path_size)
{
    if (out_path != NULL && out_path_size > 0) {
        out_path[0] = '\0';
    }
    if (rows == NULL || row_count <= 0) {
        return 0;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_captain_selection_csv_path(rows[0].season, path, sizeof(path))) {
        kbo_log_runtimef(
            "KBO captain selection csv skipped source=%s reason=path_unavailable season=%u",
            source != NULL ? source : "",
            rows[0].season);
        return 0;
    }

    char tmp_path[MAX_PATH] = {0};
    HANDLE file = kbo_atomic_open_tmp(path, tmp_path, sizeof(tmp_path));
    if (file == INVALID_HANDLE_VALUE) {
        kbo_log_runtimef("KBO captain selection csv open failed path=%s gle=%lu", path, GetLastError());
        return 0;
    }

    int ok = kbo_captain_write_header(file);
    DWORD written = 0;
    for (int i = 0; ok && i < row_count; i++) {
        const KboCaptainSelectionRow* row = &rows[i];
        char text[512] = {0};
        int len = snprintf(
            text,
            sizeof(text),
            "%u,%u,",
            row->date,
            row->season);
        if (len <= 0 || !WriteFile(file, text, (DWORD)len, &written, NULL)) {
            ok = 0;
            break;
        }
        kbo_captain_write_csv_text(file, source != NULL ? source : "");
        len = snprintf(
            text,
            sizeof(text),
            ",%u,%u,",
            row->league_id,
            row->team_id);
        if (len <= 0 || !WriteFile(file, text, (DWORD)len, &written, NULL)) {
            ok = 0;
            break;
        }
        kbo_captain_write_csv_text(file, row->team_name);
        len = snprintf(text, sizeof(text), ",%u,", row->player_id);
        if (len <= 0 || !WriteFile(file, text, (DWORD)len, &written, NULL)) {
            ok = 0;
            break;
        }
        kbo_captain_write_csv_text(file, row->player_name);
        len = snprintf(
            text,
            sizeof(text),
            ",%d,",
            row->score);
        if (len <= 0 || !WriteFile(file, text, (DWORD)len, &written, NULL)) {
            ok = 0;
            break;
        }
        kbo_captain_write_csv_text(file, row->reason);
        len = snprintf(
            text,
            sizeof(text),
            ",%u,%d,",
            (uint32_t)row->seeded,
            row->seed_priority);
        if (len <= 0 || !WriteFile(file, text, (DWORD)len, &written, NULL)) {
            ok = 0;
            break;
        }
        kbo_captain_write_csv_text(file, row->seed_source);
        len = snprintf(
            text,
            sizeof(text),
            ",%u,%u,%u,%u,%u,%u,%d,%d,%d,%d,%d,%d,%d,%u,%u,%u\r\n",
            row->nation_id,
            (uint32_t)row->domestic,
            row->current_team_id,
            row->active_team_id,
            row->current_league_id,
            (uint32_t)row->age,
            row->salary,
            row->value_score,
            row->same_team_seasons,
            (int)row->overall_value,
            (int)row->talent_value,
            (int)row->ratings_value,
            (int)row->career_value,
            (uint32_t)row->dfa,
            (uint32_t)row->restricted,
            (uint32_t)row->injured);
        if (len <= 0 || !WriteFile(file, text, (DWORD)len, &written, NULL)) {
            ok = 0;
        }
    }

    if (!ok) {
        kbo_atomic_abort(file, tmp_path);
        kbo_log_runtimef("KBO captain selection csv write failed path=%s", path);
        return 0;
    }
    if (!kbo_atomic_commit(file, tmp_path, path)) {
        kbo_log_runtimef("KBO captain selection csv write failed path=%s", path);
        return 0;
    }

    if (out_path != NULL && out_path_size > 0) {
        snprintf(out_path, out_path_size, "%s", path);
    }
    return 1;
}
