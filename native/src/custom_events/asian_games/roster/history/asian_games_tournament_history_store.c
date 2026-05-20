#include "../asian_games_roster_store.h"
#include <stdio.h>
#include <string.h>
#include "../../../../core/logging/core_log.h"
#include "../../../../core/csv/core_csv.h"
#include "../../../../core/files/atomic/core_atomic_file.h"

int kbo_load_asian_games_tournament_history(
    KboAsianGamesTournamentHistoryEntry* out,
    int max_count,
    const char* source)
{
    if (out == NULL || max_count <= 0) {
        return 0;
    }
    memset(out, 0, sizeof(*out) * (size_t)max_count);

    char path[MAX_PATH] = {0};
    if (!kbo_get_asian_games_tournament_history_csv_path(path, sizeof(path))) {
        return 0;
    }

    KboCsvReader* reader = kbo_csv_reader_open(path);
    if (reader == NULL) {
        return 0;
    }

    int count = 0;
    while (count < max_count && kbo_csv_reader_next_row(reader)) {
        char fields[3][64];
        int field_count = kbo_csv_reader_read_trimmed_fields(reader, (char*)fields, sizeof(fields[0]), 3);
        if (field_count < 3
                || fields[0][0] == '\0'
                || _stricmp(fields[0], "year") == 0) {
            continue;
        }

        uint32_t year = kbo_csv_parse_u32_text(fields[0], 10);
        uint32_t final_date = kbo_csv_parse_u32_text(fields[1], 10);
        uint32_t result = kbo_csv_parse_u32_text(fields[2], 10);
        if (year == 0u || final_date == 0u
                || (result != KBO_ASIAN_GAMES_RESULT_GOLD
                    && result != KBO_ASIAN_GAMES_RESULT_NO_GOLD)) {
            continue;
        }

        out[count].year = year;
        out[count].final_date = final_date;
        out[count].result = (uint8_t)result;
        count++;
    }

    kbo_csv_reader_close(reader);
    kbo_log_runtimef("KBO Asian Games tournament history load source=%s count=%d path=%s", source != NULL ? source : "", count, path);
    return count;
}

int kbo_append_asian_games_tournament_history(
    uint32_t year,
    uint32_t final_date,
    uint8_t result,
    const char* source)
{
    if (year == 0u || final_date == 0u
            || (result != KBO_ASIAN_GAMES_RESULT_GOLD
                && result != KBO_ASIAN_GAMES_RESULT_NO_GOLD)) {
        return 0;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_get_asian_games_tournament_history_csv_path(path, sizeof(path))) {
        return 0;
    }

    KboAsianGamesTournamentHistoryEntry entries[128];
    int count = kbo_load_asian_games_tournament_history(
        entries,
        (int)(sizeof(entries) / sizeof(entries[0])),
        source);
    int found = 0;
    for (int i = 0; i < count; i++) {
        if (entries[i].year == year) {
            entries[i].final_date = final_date;
            entries[i].result = result;
            found = 1;
            break;
        }
    }
    if (!found && count < (int)(sizeof(entries) / sizeof(entries[0]))) {
        entries[count].year = year;
        entries[count].final_date = final_date;
        entries[count].result = result;
        count++;
    }

    char tmp_path[MAX_PATH] = {0};
    HANDLE file = kbo_atomic_open_tmp(path, tmp_path, sizeof(tmp_path));
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }

    DWORD written = 0;
    const char* header = "year,final_date,result\r\n";
    WriteFile(file, header, (DWORD)strlen(header), &written, NULL);

    int ok = 1;
    for (int i = 0; i < count; i++) {
        char line[64] = {0};
        int len = snprintf(
            line,
            sizeof(line),
            "%u,%u,%u\r\n",
            entries[i].year,
            entries[i].final_date,
            (uint32_t)entries[i].result);
        if (len <= 0 || !WriteFile(file, line, (DWORD)len, &written, NULL) || written != (DWORD)len) {
            ok = 0;
            break;
        }
    }

    if (!ok) {
        kbo_atomic_abort(file, tmp_path);
        return 0;
    }
    if (!kbo_atomic_commit(file, tmp_path, path)) {
        return 0;
    }

    kbo_log_runtimef(
        "KBO Asian Games tournament history save source=%s year=%u final=%u result=%u path=%s",
        source != NULL ? source : "",
        year,
        final_date,
        (uint32_t)result,
        path);
    return 1;
}
