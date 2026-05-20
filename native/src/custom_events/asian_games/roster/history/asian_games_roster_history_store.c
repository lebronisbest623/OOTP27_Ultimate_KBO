#include "../internal/asian_games_roster_store_internal.h"
#include <stdio.h>
#include <string.h>
#include "../../../../bootstrap/abi/ootp_offsets.h"
#include "../../../../core/logging/core_log.h"
#include "../../../../core/csv/core_csv.h"
#include "../../../../core/files/atomic/core_atomic_file.h"

int kbo_load_asian_games_roster_history(
    KboAsianGamesRosterHistoryEntry* out,
    int max_count,
    const char* source)
{
    if (out == NULL || max_count <= 0) {
        return 0;
    }
    memset(out, 0, sizeof(*out) * (size_t)max_count);

    char path[MAX_PATH] = {0};
    if (!kbo_get_asian_games_roster_history_csv_path(path, sizeof(path))) {
        return 0;
    }

    KboCsvReader* reader = kbo_csv_reader_open(path);
    if (reader == NULL) {
        return 0;
    }

    int count = 0;
    while (count < max_count && kbo_csv_reader_next_row(reader)) {
        char fields[20][64];
        int field_count = kbo_csv_reader_read_trimmed_fields(reader, (char*)fields, sizeof(fields[0]), 20);
        if (field_count < 18
                || fields[0][0] == '\0'
                || _stricmp(fields[0], "year") == 0) {
            continue;
        }

        uint32_t year = kbo_csv_parse_u32_text(fields[0], 10);
        uint32_t player_id = kbo_csv_parse_u32_text(fields[2], 10);
        if (year == 0u || player_id == 0u) {
            continue;
        }

        KboAsianGamesRosterHistoryEntry* history = &out[count++];
        KboAsianGamesRosterEntry* entry = &history->entry;
        int has_military_unserved = field_count >= 20;
        int old_restricted_field = has_military_unserved ? 11 : 10;
        int tournament_result_field = has_military_unserved ? 19 : 18;
        uint32_t tournament_result = field_count > tournament_result_field
            ? kbo_csv_parse_u32_text(fields[tournament_result_field], 10)
            : 0u;

        history->year = year;
        history->index = kbo_csv_parse_u32_text(fields[1], 10);
        if (history->index == 0u) {
            history->index = (uint32_t)count;
        }
        history->tournament_result = (uint8_t)tournament_result;
        entry->player_id = player_id;
        entry->original_team_id = kbo_csv_parse_u32_text(fields[3], 10);
        entry->original_league_id = kbo_csv_parse_u32_text(fields[4], 10);
        entry->departure_date = kbo_csv_parse_u32_text(fields[5], 10);
        entry->return_date = kbo_csv_parse_u32_text(fields[6], 10);
        entry->age = (uint16_t)kbo_csv_parse_u32_text(fields[7], 10);
        entry->role = (uint8_t)kbo_csv_parse_u32_text(fields[8], 10);
        entry->wildcard = (uint8_t)kbo_csv_parse_u32_text(fields[9], 10);
        entry->military_unserved = has_military_unserved
            ? (uint8_t)kbo_csv_parse_u32_text(fields[10], 10)
            : 1u;
        entry->old_restricted = (uint8_t)kbo_csv_parse_u32_text(fields[old_restricted_field], 10);
        entry->old_secondary_restricted = (uint8_t)kbo_csv_parse_u32_text(fields[old_restricted_field + 1], 10);
        entry->old_injury_active = (uint8_t)kbo_csv_parse_u32_text(fields[old_restricted_field + 2], 10);
        entry->old_injury_days_left = (int16_t)strtol(fields[old_restricted_field + 3], NULL, 10);
        entry->departed = (uint8_t)kbo_csv_parse_u32_text(fields[old_restricted_field + 4], 10);
        entry->returned = (uint8_t)kbo_csv_parse_u32_text(fields[old_restricted_field + 5], 10);
        entry->exempted = (uint8_t)kbo_csv_parse_u32_text(fields[old_restricted_field + 6], 10);
        entry->score = (int32_t)strtol(fields[old_restricted_field + 7], NULL, 10);
        entry->player_ptr = 0;
    }

    kbo_csv_reader_close(reader);
    kbo_log_runtimef(
        "KBO Asian Games roster history load source=%s count=%d path=%s",
        source != NULL ? source : "",
        count,
        path);
    return count;
}

int kbo_save_asian_games_roster_history_csv(const char* source)
{
    if (g_kbo_asian_games_roster_year == 0u) {
        return 0;
    }
    LONG roster_count = g_kbo_asian_games_roster_count;
    if (roster_count <= 0 || roster_count > KBO_ASIAN_GAMES_ROSTER_SIZE) {
        return 0;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_get_asian_games_roster_history_csv_path(path, sizeof(path))) {
        return 0;
    }

    const int history_capacity = OOTP27_KBO_MAX_SPECIAL_HISTORY_KEYS;
    KboAsianGamesRosterHistoryEntry* history = (KboAsianGamesRosterHistoryEntry*)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        (SIZE_T)history_capacity * sizeof(KboAsianGamesRosterHistoryEntry));
    if (history == NULL) {
        return 0;
    }

    int loaded_count = kbo_load_asian_games_roster_history(history, history_capacity, source);
    if (loaded_count < 0) {
        loaded_count = 0;
    }
    if (loaded_count > history_capacity) {
        loaded_count = history_capacity;
    }

    int count = 0;
    for (int i = 0; i < loaded_count; i++) {
        if (history[i].year != g_kbo_asian_games_roster_year) {
            if (count != i) {
                history[count] = history[i];
            }
            count++;
        }
    }

    for (LONG i = 0; i < roster_count && count < history_capacity; i++) {
        KboAsianGamesRosterHistoryEntry* row = &history[count++];
        memset(row, 0, sizeof(*row));
        row->year = g_kbo_asian_games_roster_year;
        row->index = (uint32_t)i + 1u;
        row->entry = g_kbo_asian_games_roster[i];
        row->entry.player_ptr = 0u;
        row->tournament_result = g_kbo_asian_games_result;
    }

    char tmp_path[MAX_PATH] = {0};
    HANDLE file = kbo_atomic_open_tmp(path, tmp_path, sizeof(tmp_path));
    if (file == INVALID_HANDLE_VALUE) {
        HeapFree(GetProcessHeap(), 0, history);
        return 0;
    }

    DWORD written = 0;
    const char* header = "year,index,player_id,original_team_id,original_league_id,departure_date,return_date,age,role,wildcard,military_unserved,old_restricted,old_secondary_restricted,old_injury_active,old_injury_days_left,departed,returned,exempted,score,tournament_result\r\n";
    WriteFile(file, header, (DWORD)strlen(header), &written, NULL);

    int ok = 1;
    for (int i = 0; i < count; i++) {
        KboAsianGamesRosterHistoryEntry* row = &history[i];
        KboAsianGamesRosterEntry* entry = &row->entry;
        if (row->year == 0u || entry->player_id == 0u) {
            continue;
        }
        char line[512] = {0};
        int len = snprintf(
            line,
            sizeof(line),
            "%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%d,%u,%u,%u,%d,%u\r\n",
            row->year,
            row->index,
            entry->player_id,
            entry->original_team_id,
            entry->original_league_id,
            entry->departure_date,
            entry->return_date,
            (uint32_t)entry->age,
            (uint32_t)entry->role,
            (uint32_t)entry->wildcard,
            (uint32_t)entry->military_unserved,
            (uint32_t)entry->old_restricted,
            (uint32_t)entry->old_secondary_restricted,
            (uint32_t)entry->old_injury_active,
            (int)entry->old_injury_days_left,
            (uint32_t)entry->departed,
            (uint32_t)entry->returned,
            (uint32_t)entry->exempted,
            entry->score,
            (uint32_t)row->tournament_result);
        if (len <= 0 || len >= (int)sizeof(line)
                || !WriteFile(file, line, (DWORD)len, &written, NULL)
                || written != (DWORD)len) {
            ok = 0;
            break;
        }
    }

    if (!ok) {
        kbo_atomic_abort(file, tmp_path);
        HeapFree(GetProcessHeap(), 0, history);
        return 0;
    }
    if (!kbo_atomic_commit(file, tmp_path, path)) {
        HeapFree(GetProcessHeap(), 0, history);
        return 0;
    }

    kbo_log_runtimef(
        "KBO Asian Games roster history save source=%s year=%u roster=%ld total=%d path=%s",
        source != NULL ? source : "",
        g_kbo_asian_games_roster_year,
        roster_count,
        count,
        path);
    HeapFree(GetProcessHeap(), 0, history);
    return 1;
}
