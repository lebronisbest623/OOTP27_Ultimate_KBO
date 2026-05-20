#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../core/files/atomic/core_atomic_file.h"
#include "../../../core/files/save_paths/core_save_paths.h"
#include "../../../core/logging/core_log.h"
#include "../../../core/optimizer/kbo_optimizer.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../team/lookup/team_lookup.h"
#include "../../players/state/military_player_state.h"
#include "../draft/military_draft_queue.h"
#include "military_selection_event.h"
#include "../../../core/core_flags/keys/runtime_flag_keys.generated.h"

static int kbo_military_write_ortools_request(
    const char* path,
    uint16_t entry_year,
    uint32_t sang_id,
    int slots,
    int* out_considered)
{
    char tmp_path[MAX_PATH] = {0};
    HANDLE file = kbo_atomic_open_tmp(path, tmp_path, sizeof(tmp_path));
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }
    DWORD written = 0;
    const char* header = "player_id,score,age,role,original_team_id,slots\r\n";
    WriteFile(file, header, (DWORD)strlen(header), &written, NULL);
    int considered = 0;
    LONG count = g_kbo_military_draft_candidate_count;
    if (count < 0) { count = 0; }
    if (count > OOTP27_KBO_MAX_SPECIAL_HISTORY_KEYS) { count = OOTP27_KBO_MAX_SPECIAL_HISTORY_KEYS; }
    for (LONG i = 0; i < count; i++) {
        KboMilitaryDraftCandidate* candidate = &g_kbo_military_draft_candidates[i];
        if (candidate->player_id == 0u || candidate->entry_year != entry_year || candidate->selected != 0u) {
            continue;
        }
        uintptr_t player_ptr = candidate->player_ptr;
        if (!kbo_player_pointer_plausible(player_ptr)) {
            player_ptr = (uintptr_t)kbo_military_find_player_by_id(candidate->player_id);
            candidate->player_ptr = player_ptr;
        }
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }
        uint8_t* player = (uint8_t*)player_ptr;
        if (player[OOTP27_PLAYER_MILITARY_ACTIVE_OFFSET] == 0u
                || kbo_military_effective_days_left(player) <= 0
                || *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET) == sang_id) {
            continue;
        }
        int score = kbo_military_draft_candidate_score(player);
        char line[128] = {0};
        int len = snprintf(
            line,
            sizeof(line),
            "%u,%d,%u,%u,%u,%d\r\n",
            candidate->player_id,
            score,
            (uint32_t)(*(uint16_t*)(player + OOTP27_PLAYER_AGE_OFFSET)),
            (uint32_t)player[OOTP27_PLAYER_POSITION_GROUP_OFFSET],
            candidate->original_team_id,
            slots);
        if (len > 0 && len < (int)sizeof(line)) {
            WriteFile(file, line, (DWORD)len, &written, NULL);
        }
        considered++;
    }
    if (!kbo_atomic_commit(file, tmp_path, path)) {
        return 0;
    }
    if (out_considered != NULL) {
        *out_considered = considered;
    }
    return considered > 0;
}

static int kbo_military_read_ortools_result(
    const char* path,
    uint32_t* selected_player_ids,
    int selected_capacity)
{
    if (path == NULL || selected_player_ids == NULL || selected_capacity <= 0) {
        return 0;
    }
    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        return 0;
    }
    char line[256] = {0};
    if (fgets(line, sizeof(line), file) == NULL) {
        fclose(file);
        return 0;
    }
    int count = 0;
    while (count < selected_capacity && fgets(line, sizeof(line), file) != NULL) {
        uint32_t player_id = (uint32_t)strtoul(line, NULL, 10);
        if (player_id != 0u) {
            selected_player_ids[count++] = player_id;
        }
    }
    fclose(file);
    return count;
}

static KboMilitaryDraftCandidate* kbo_military_find_unselected_candidate(
    uint32_t player_id,
    uint16_t entry_year)
{
    LONG count = g_kbo_military_draft_candidate_count;
    if (count < 0) { count = 0; }
    if (count > OOTP27_KBO_MAX_SPECIAL_HISTORY_KEYS) { count = OOTP27_KBO_MAX_SPECIAL_HISTORY_KEYS; }
    for (LONG i = 0; i < count; i++) {
        KboMilitaryDraftCandidate* candidate = &g_kbo_military_draft_candidates[i];
        if (candidate->player_id == player_id && candidate->entry_year == entry_year && candidate->selected == 0u) {
            return candidate;
        }
    }
    return NULL;
}

int kbo_route_queued_military_draft_candidates_ortools(
    uint16_t entry_year,
    uint8_t* sang,
    uint32_t sang_id,
    uint32_t sang_league_id,
    int slots,
    KboMilitarySelectionNewsEntry* news_entries,
    int max_news_entries,
    int* out_considered,
    const char* source)
{
    if (read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_DISABLE_MILITARY_ORTOOLS_FILE)
            || entry_year == 0u || sang == NULL || slots <= 0) {
        return 0;
    }
    char request_path[MAX_PATH * 3] = {0};
    char result_path[MAX_PATH * 3] = {0};
    if (!kbo_get_save_scoped_data_file("work\\military_selection_ortools_request.csv", request_path, sizeof(request_path))
            || !kbo_get_save_scoped_data_file("work\\military_selection_ortools_result.csv", result_path, sizeof(result_path))) {
        return 0;
    }
    int considered = 0;
    if (!kbo_military_write_ortools_request(request_path, entry_year, sang_id, slots, &considered)) {
        if (out_considered != NULL) {
            *out_considered = considered;
        }
        return 0;
    }
    if (out_considered != NULL) {
        *out_considered = considered;
    }
    if (!kbo_optimizer_run_mode("military_selection", request_path, result_path, 8000u)) {
        return 0;
    }
    uint32_t selected_player_ids[64] = {0};
    int selected_count = kbo_military_read_ortools_result(
        result_path,
        selected_player_ids,
        (int)(sizeof(selected_player_ids) / sizeof(selected_player_ids[0])));
    if (selected_count <= 0) {
        return 0;
    }

    int routed = 0;
    for (int i = 0; i < selected_count && routed < slots; i++) {
        KboMilitaryDraftCandidate* candidate = kbo_military_find_unselected_candidate(
            selected_player_ids[i],
            entry_year);
        if (candidate == NULL) {
            continue;
        }
        uintptr_t player_ptr = candidate->player_ptr;
        if (!kbo_player_pointer_plausible(player_ptr)) {
            player_ptr = (uintptr_t)kbo_military_find_player_by_id(candidate->player_id);
            candidate->player_ptr = player_ptr;
        }
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }
        uint8_t* player = (uint8_t*)player_ptr;
        if (player[OOTP27_PLAYER_MILITARY_ACTIVE_OFFSET] == 0u
                || kbo_military_effective_days_left(player) <= 0
                || *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET) == sang_id) {
            continue;
        }
        int score = kbo_military_draft_candidate_score(player);
        routed += kbo_route_military_candidate_to_sang(
            candidate,
            player,
            sang,
            sang_id,
            sang_league_id,
            score,
            news_entries,
            max_news_entries,
            routed,
            entry_year,
            source) ? 1 : 0;
    }
    if (routed > 0) {
        kbo_log_runtimef(
            "KBO military OR-Tools selection processed source=%s year=%u considered=%d selected=%d routed=%d slots=%d",
            source != NULL ? source : "",
            entry_year,
            considered,
            selected_count,
            routed,
            slots);
    }
    return routed;
}
