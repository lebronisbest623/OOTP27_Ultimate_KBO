#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../core/csv/core_csv.h"
#include "../../core/files/atomic/core_atomic_file.h"
#include "../../core/logging/core_log.h"
#include "../state/fa_compensation_paths_parse.h"
#include "fa_compensation_records.h"
#include "../../core/dates/constants/kbo_date_constants.h"

static void kbo_fa_compensation_write_csv_text(HANDLE file, const char* text)
{
    DWORD written = 0;
    WriteFile(file, "\"", 1, &written, NULL);
    if (text != NULL) {
        const char* p = text;
        while (*p != '\0') {
            if (*p == '"') {
                WriteFile(file, "\"\"", 2, &written, NULL);
            } else {
                WriteFile(file, p, 1, &written, NULL);
            }
            p++;
        }
    }
    WriteFile(file, "\"", 1, &written, NULL);
}

int kbo_load_fa_compensation_records(
    KboFaCompensationRecord* records,
    int max_records,
    char* out_path,
    size_t out_path_size)
{
    if (out_path != NULL && out_path_size > 0) {
        out_path[0] = '\0';
    }
    if (records == NULL || max_records <= 0) {
        return 0;
    }
    memset(records, 0, (SIZE_T)max_records * sizeof(records[0]));

    char path[MAX_PATH] = {0};
    if (!kbo_get_fa_compensation_path(path, sizeof(path))) {
        return 0;
    }
    if (out_path != NULL && out_path_size > 0) {
        snprintf(out_path, out_path_size, "%s", path);
    }

    KboCsvReader* reader = kbo_csv_reader_open(path);
    if (reader == NULL) {
        return 0;
    }

    int count = 0;
    while (count < max_records && kbo_csv_reader_next_row(reader)) {
        char fields[16][128];
        int field_count = kbo_csv_reader_read_trimmed_fields(reader, (char*)fields, sizeof(fields[0]), 16);
        if (field_count < 16 || fields[0][0] < '0' || fields[0][0] > '9') {
            continue;
        }

        KboFaCompensationRecord* rec = &records[count];
        rec->player_id = kbo_fa_compensation_parse_u32(fields[0]);
        rec->signed_on_yyyymmdd = kbo_fa_compensation_parse_u32(fields[1]);
        rec->season = kbo_fa_compensation_parse_u32(fields[2]);
        rec->league_id = kbo_fa_compensation_parse_u32(fields[3]);
        rec->original_team_id = kbo_fa_compensation_parse_u32(fields[4]);
        rec->signing_team_id = kbo_fa_compensation_parse_u32(fields[5]);
        kbo_fa_compensation_copy_token(fields[6], rec->grade, sizeof(rec->grade));
        rec->previous_salary = kbo_fa_compensation_parse_i32(fields[7]);
        rec->cash_with_player = kbo_fa_compensation_parse_u32(fields[8]);
        rec->cash_only = kbo_fa_compensation_parse_u32(fields[9]);
        rec->protect_count = kbo_fa_compensation_parse_u32(fields[10]);
        rec->requires_player_compensation = kbo_fa_compensation_parse_u32(fields[11]) != 0u ? 1u : 0u;
        rec->status = (uint8_t)kbo_fa_compensation_parse_u32(fields[12]);
        kbo_fa_compensation_copy_token(fields[13], rec->case_label, sizeof(rec->case_label));
        kbo_fa_compensation_copy_token(fields[14], rec->player_name, sizeof(rec->player_name));
        kbo_fa_compensation_copy_token(fields[15], rec->source, sizeof(rec->source));
        if (rec->player_id != 0u && rec->season >= KBO_SEASON_YEAR_MIN && rec->season <= KBO_RECORD_YEAR_MAX) {
            count++;
        } else {
            memset(rec, 0, sizeof(*rec));
        }
    }

    kbo_csv_reader_close(reader);
    return count;
}

int kbo_persist_fa_compensation_records(const KboFaCompensationRecord* records, int record_count)
{
    char path[MAX_PATH] = {0};
    if (!kbo_get_fa_compensation_path(path, sizeof(path))) {
        kbo_log_runtime_line("KBO FA compensation persist skipped reason=path_unavailable");
        return 0;
    }

    char dir[MAX_PATH] = {0};
    snprintf(dir, sizeof(dir), "%s", path);
    char* slash = strrchr(dir, '\\');
    if (slash != NULL) {
        *slash = '\0';
        CreateDirectoryA(dir, NULL);
    }

    char tmp_path[MAX_PATH] = {0};
    HANDLE file = kbo_atomic_open_tmp(path, tmp_path, sizeof(tmp_path));
    if (file == INVALID_HANDLE_VALUE) {
        kbo_log_runtimef("KBO FA compensation persist failed reason=create_tmp gle=%lu path=%s", GetLastError(), path);
        return 0;
    }

    DWORD written = 0;
    const char* header =
        "player_id,signed_on,season,league_id,original_team_id,signing_team_id,grade,"
        "previous_salary,cash_with_player,cash_only,protect_count,requires_player_compensation,"
        "status,kbo_case,player_name,source\r\n";
    WriteFile(file, header, (DWORD)strlen(header), &written, NULL);

    for (int i = 0; i < record_count; i++) {
        const KboFaCompensationRecord* rec = &records[i];
        if (rec->player_id == 0u || rec->season == 0u) {
            continue;
        }
        char prefix[192] = {0};
        int len = snprintf(
            prefix,
            sizeof(prefix),
            "%u,%u,%u,%u,%u,%u,",
            rec->player_id,
            rec->signed_on_yyyymmdd,
            rec->season,
            rec->league_id,
            rec->original_team_id,
            rec->signing_team_id);
        if (len > 0 && len < (int)sizeof(prefix)) {
            WriteFile(file, prefix, (DWORD)len, &written, NULL);
        }
        kbo_fa_compensation_write_csv_text(file, rec->grade);

        char middle[192] = {0};
        len = snprintf(
            middle,
            sizeof(middle),
            ",%d,%u,%u,%u,%u,%u,",
            rec->previous_salary,
            rec->cash_with_player,
            rec->cash_only,
            rec->protect_count,
            (uint32_t)rec->requires_player_compensation,
            (uint32_t)rec->status);
        if (len > 0 && len < (int)sizeof(middle)) {
            WriteFile(file, middle, (DWORD)len, &written, NULL);
        }
        kbo_fa_compensation_write_csv_text(file, rec->case_label);
        WriteFile(file, ",", 1, &written, NULL);
        kbo_fa_compensation_write_csv_text(file, rec->player_name);
        WriteFile(file, ",", 1, &written, NULL);
        kbo_fa_compensation_write_csv_text(file, rec->source);
        WriteFile(file, "\r\n", 2, &written, NULL);
    }

    int ok = kbo_atomic_commit(file, tmp_path, path);
    if (!ok) {
        kbo_log_runtimef("KBO FA compensation atomic commit failed path=%s", path);
        return 0;
    }
    kbo_log_runtimef("KBO FA compensation persisted records=%d path=%s", record_count, path);
    return 1;
}

int kbo_append_fa_compensation_record(const KboFaCompensationRecord* rec)
{
    if (rec == NULL || rec->player_id == 0u || rec->season == 0u) {
        return 0;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_get_fa_compensation_path(path, sizeof(path))) {
        kbo_log_runtime_line("KBO FA compensation append skipped reason=path_unavailable");
        return 0;
    }

    char dir[MAX_PATH] = {0};
    snprintf(dir, sizeof(dir), "%s", path);
    char* slash = strrchr(dir, '\\');
    if (slash != NULL) {
        *slash = '\0';
        CreateDirectoryA(dir, NULL);
    }

    DWORD attrs = GetFileAttributesA(path);
    int needs_header = (attrs == INVALID_FILE_ATTRIBUTES);
    HANDLE file = CreateFileA(
        path,
        FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == INVALID_HANDLE_VALUE) {
        kbo_log_runtimef("KBO FA compensation append failed reason=open gle=%lu path=%s", GetLastError(), path);
        return 0;
    }

    DWORD written = 0;
    if (needs_header) {
        const char* header =
            "player_id,signed_on,season,league_id,original_team_id,signing_team_id,grade,"
            "previous_salary,cash_with_player,cash_only,protect_count,requires_player_compensation,"
            "status,kbo_case,player_name,source\r\n";
        WriteFile(file, header, (DWORD)strlen(header), &written, NULL);
    }

    char prefix[192] = {0};
    int len = snprintf(
        prefix,
        sizeof(prefix),
        "%u,%u,%u,%u,%u,%u,",
        rec->player_id,
        rec->signed_on_yyyymmdd,
        rec->season,
        rec->league_id,
        rec->original_team_id,
        rec->signing_team_id);
    if (len > 0 && len < (int)sizeof(prefix)) {
        WriteFile(file, prefix, (DWORD)len, &written, NULL);
    }
    kbo_fa_compensation_write_csv_text(file, rec->grade);

    char middle[192] = {0};
    len = snprintf(
        middle,
        sizeof(middle),
        ",%d,%u,%u,%u,%u,%u,",
        rec->previous_salary,
        rec->cash_with_player,
        rec->cash_only,
        rec->protect_count,
        (uint32_t)rec->requires_player_compensation,
        (uint32_t)rec->status);
    if (len > 0 && len < (int)sizeof(middle)) {
        WriteFile(file, middle, (DWORD)len, &written, NULL);
    }
    kbo_fa_compensation_write_csv_text(file, rec->case_label);
    WriteFile(file, ",", 1, &written, NULL);
    kbo_fa_compensation_write_csv_text(file, rec->player_name);
    WriteFile(file, ",", 1, &written, NULL);
    kbo_fa_compensation_write_csv_text(file, rec->source);
    WriteFile(file, "\r\n", 2, &written, NULL);
    CloseHandle(file);
    return 1;
}

int kbo_fa_compensation_find_existing(
    const KboFaCompensationRecord* records,
    int record_count,
    uint32_t player_id,
    uint32_t season,
    uint32_t original_team_id,
    uint32_t signing_team_id)
{
    if (records == NULL || player_id == 0u || season == 0u) {
        return -1;
    }
    for (int i = 0; i < record_count; i++) {
        const KboFaCompensationRecord* rec = &records[i];
        if (rec->player_id == player_id
                && rec->season == season
                && rec->original_team_id == original_team_id
                && rec->signing_team_id == signing_team_id) {
            return i;
        }
    }
    return -1;
}
