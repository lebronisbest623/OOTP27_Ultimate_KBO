#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/csv/core_csv.h"
#include "../../../core/files/atomic/core_atomic_file.h"
#include "../../../core/files/save_paths/core_save_paths.h"
#include "../../../core/logging/core_log.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../team/lookup/team_lookup.h"
#include "../../players/state/military_player_state.h"
#include "military_selection_results_store.h"

static int kbo_get_military_selection_result_history_path(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return 0;
    }
    out[0] = '\0';
    return kbo_get_save_scoped_data_file("military_selection_results.csv", out, out_size);
}

static int kbo_military_selection_result_history_find(
    KboMilitarySelectionResultEntry* entries,
    int count,
    uint32_t year,
    uint32_t player_id)
{
    if (entries == NULL || count <= 0 || year == 0u || player_id == 0u) {
        return -1;
    }
    for (int i = 0; i < count; i++) {
        if (entries[i].year == year && entries[i].player_id == player_id) {
            return i;
        }
    }
    return -1;
}

static int kbo_write_military_selection_result_history(
    KboMilitarySelectionResultEntry* entries,
    int count,
    const char* source)
{
    char path[MAX_PATH] = {0};
    if (!kbo_get_military_selection_result_history_path(path, sizeof(path))) {
        return 0;
    }

    char tmp_path[MAX_PATH] = {0};
    HANDLE file = kbo_atomic_open_tmp(path, tmp_path, sizeof(tmp_path));
    if (file == INVALID_HANDLE_VALUE) {
        kbo_log_runtimef(
            "KBO military selection results save skipped source=%s reason=create_failed gle=%lu path=%s",
            source != NULL ? source : "",
            GetLastError(),
            path);
        return 0;
    }

    DWORD written = 0;
    const char* header =
        "year,announcement_date,player_id,original_team_id,original_league_id,service_team_id,return_date,age,position_group,position_role,score\r\n";
    WriteFile(file, header, (DWORD)strlen(header), &written, NULL);

    int ok = 1;
    for (int i = 0; i < count; i++) {
        KboMilitarySelectionResultEntry* entry = &entries[i];
        if (entry->year == 0u || entry->player_id == 0u) {
            continue;
        }
        char line[192] = {0};
        int len = snprintf(
            line,
            sizeof(line),
            "%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%d\r\n",
            entry->year,
            entry->announcement_date,
            entry->player_id,
            entry->original_team_id,
            entry->original_league_id,
            entry->service_team_id,
            entry->return_date,
            (uint32_t)entry->age,
            (uint32_t)entry->position_group,
            (uint32_t)entry->position_role,
            entry->score);
        if (len <= 0 || len >= (int)sizeof(line)
                || !WriteFile(file, line, (DWORD)len, &written, NULL)
                || written != (DWORD)len) {
            ok = 0;
            break;
        }
    }

    if (!ok) {
        kbo_atomic_abort(file, tmp_path);
        return 0;
    }
    if (!kbo_atomic_commit(file, tmp_path, path)) {
        kbo_log_runtimef(
            "KBO military selection results save failed source=%s reason=atomic_commit path=%s",
            source != NULL ? source : "",
            path);
        return 0;
    }

    kbo_log_runtimef(
        "KBO military selection results save source=%s count=%d path=%s",
        source != NULL ? source : "",
        count,
        path);
    return 1;
}

int kbo_load_military_selection_result_history(
    KboMilitarySelectionResultEntry* out,
    int max_count,
    const char* source)
{
    if (out == NULL || max_count <= 0) {
        return 0;
    }
    memset(out, 0, sizeof(*out) * (size_t)max_count);

    char path[MAX_PATH] = {0};
    if (!kbo_get_military_selection_result_history_path(path, sizeof(path))) {
        return 0;
    }

    KboCsvReader* reader = kbo_csv_reader_open(path);
    if (reader == NULL) {
        return 0;
    }

    int count = 0;
    while (count < max_count && kbo_csv_reader_next_row(reader)) {
        char fields[11][64];
        int field_count = kbo_csv_reader_read_trimmed_fields(
            reader,
            (char*)fields,
            sizeof(fields[0]),
            11);
        if (field_count < 7
                || fields[0][0] == '\0'
                || _stricmp(fields[0], "year") == 0) {
            continue;
        }

        uint32_t year = kbo_csv_parse_u32_text(fields[0], 10);
        uint32_t player_id = kbo_csv_parse_u32_text(fields[2], 10);
        if (year == 0u || player_id == 0u) {
            continue;
        }

        KboMilitarySelectionResultEntry* entry = &out[count++];
        entry->year = year;
        entry->announcement_date = kbo_csv_parse_u32_text(fields[1], 10);
        entry->player_id = player_id;
        entry->original_team_id = kbo_csv_parse_u32_text(fields[3], 10);
        entry->original_league_id = kbo_csv_parse_u32_text(fields[4], 10);
        entry->service_team_id = kbo_csv_parse_u32_text(fields[5], 10);
        entry->return_date = kbo_csv_parse_u32_text(fields[6], 10);
        if (field_count > 7) {
            entry->age = (uint16_t)kbo_csv_parse_u32_text(fields[7], 10);
        }
        if (field_count > 8) {
            entry->position_group = (uint8_t)kbo_csv_parse_u32_text(fields[8], 10);
        }
        if (field_count > 9) {
            entry->position_role = (uint8_t)kbo_csv_parse_u32_text(fields[9], 10);
        }
        if (field_count > 10) {
            entry->score = (int32_t)strtol(fields[10], NULL, 10);
        }
    }

    kbo_csv_reader_close(reader);
    kbo_log_runtimef(
        "KBO military selection results load source=%s count=%d path=%s",
        source != NULL ? source : "",
        count,
        path);
    return count;
}

int kbo_append_military_selection_result_history(
    uint32_t year,
    uint32_t announcement_date,
    uint32_t service_team_id,
    KboMilitarySelectionNewsEntry* entries,
    int entry_count,
    const char* source)
{
    if (year == 0u || entries == NULL || entry_count <= 0) {
        return 0;
    }

    const int history_capacity = OOTP27_KBO_MAX_SPECIAL_HISTORY_KEYS;
    KboMilitarySelectionResultEntry* history = (KboMilitarySelectionResultEntry*)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        (SIZE_T)history_capacity * sizeof(KboMilitarySelectionResultEntry));
    if (history == NULL) {
        return 0;
    }
    int count = kbo_load_military_selection_result_history(
        history,
        history_capacity,
        source);
    if (count < 0) {
        count = 0;
    }
    if (count > history_capacity) {
        count = history_capacity;
    }

    int upserted = 0;
    for (int i = 0; i < entry_count; i++) {
        KboMilitarySelectionNewsEntry* selected = &entries[i];
        if (selected->player_id == 0u) {
            continue;
        }

        int index = kbo_military_selection_result_history_find(
            history,
            count,
            year,
            selected->player_id);
        if (index < 0) {
            if (count >= history_capacity) {
                continue;
            }
            index = count++;
        }

        KboMilitarySelectionResultEntry* entry = &history[index];
        memset(entry, 0, sizeof(*entry));
        entry->year = year;
        entry->announcement_date = announcement_date;
        entry->player_id = selected->player_id;
        entry->original_team_id = selected->original_team_id;
        entry->service_team_id = service_team_id;
        entry->score = selected->score;

        uintptr_t player_ptr = selected->player_ptr;
        if (!kbo_player_pointer_plausible(player_ptr)) {
            player_ptr = (uintptr_t)kbo_military_find_player_by_id(selected->player_id);
        }
        if (kbo_player_pointer_plausible(player_ptr)) {
            uint8_t* player = (uint8_t*)player_ptr;
            entry->original_league_id = *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_LEAGUE_ID_OFFSET);
            if (entry->original_league_id == 0u) {
                entry->original_league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
            }
            entry->return_date = kbo_military_effective_return_yyyymmdd(player);
            entry->age = memory_range_readable(player + OOTP27_PLAYER_AGE_OFFSET, sizeof(uint16_t))
                ? *(uint16_t*)(player + OOTP27_PLAYER_AGE_OFFSET)
                : 0u;
            entry->position_group = memory_range_readable(player + OOTP27_PLAYER_POSITION_GROUP_OFFSET, sizeof(uint8_t))
                ? player[OOTP27_PLAYER_POSITION_GROUP_OFFSET]
                : 0u;
            entry->position_role = memory_range_readable(player + OOTP27_PLAYER_POSITION_ROLE_OFFSET, sizeof(uint8_t))
                ? player[OOTP27_PLAYER_POSITION_ROLE_OFFSET]
                : 0u;
        }
        if (entry->original_league_id == 0u) {
            entry->original_league_id = selected->original_league_id;
        }
        upserted++;
    }

    if (upserted <= 0) {
        HeapFree(GetProcessHeap(), 0, history);
        return 0;
    }

    if (!kbo_write_military_selection_result_history(history, count, source)) {
        HeapFree(GetProcessHeap(), 0, history);
        return 0;
    }

    kbo_log_runtimef(
        "KBO military selection results appended source=%s year=%u announcement=%u service_team=%u upserted=%d total=%d",
        source != NULL ? source : "",
        year,
        announcement_date,
        service_team_id,
        upserted,
        count);
    HeapFree(GetProcessHeap(), 0, history);
    return upserted;
}
