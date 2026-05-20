#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/logging/core_log.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../team/lookup/team_lookup.h"
#include "../../players/state/military_player_state.h"
#include "military_selection_results_store.h"
#include "sql/military_selection_results_sql_store.h"

static int kbo_get_military_selection_result_history_path(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return 0;
    }
    out[0] = '\0';
    return kbo_military_selection_results_sql_path(out, out_size);
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

    int count = 0;
    if (!kbo_military_selection_results_sql_load(out, max_count, &count)) {
        return 0;
    }

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

    KboMilitarySelectionResultEntry* rows = (KboMilitarySelectionResultEntry*)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        (SIZE_T)entry_count * sizeof(KboMilitarySelectionResultEntry));
    if (rows == NULL) {
        return 0;
    }

    int upserted = 0;
    for (int i = 0; i < entry_count; i++) {
        KboMilitarySelectionNewsEntry* selected = &entries[i];
        if (selected->player_id == 0u) {
            continue;
        }

        KboMilitarySelectionResultEntry* entry = &rows[upserted];
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
        HeapFree(GetProcessHeap(), 0, rows);
        return 0;
    }

    if (!kbo_military_selection_results_sql_upsert_many(rows, upserted)) {
        HeapFree(GetProcessHeap(), 0, rows);
        return 0;
    }

    kbo_log_runtimef(
        "KBO military selection results appended source=%s year=%u announcement=%u service_team=%u upserted=%d total=%d",
        source != NULL ? source : "",
        year,
        announcement_date,
        service_team_id,
        upserted,
        upserted);
    HeapFree(GetProcessHeap(), 0, rows);
    return upserted;
}
