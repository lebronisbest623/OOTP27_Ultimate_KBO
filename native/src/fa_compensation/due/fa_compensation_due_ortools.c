#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../core/core_flags/api/flags_api.h"
#include "../../core/files/save_paths/core_save_paths.h"
#include "../../core/logging/core_log.h"
#include "../../core/optimizer/kbo_optimizer.h"
#include "../decisions/fa_compensation_decisions.h"
#include "../protection/fa_compensation_protection_score.h"
#include "fa_compensation_due_ortools.h"

static int kbo_fa_compensation_write_ortools_request(
    const char* path,
    const KboFaCompensationRecord* rec,
    const KboFaProtectedCandidate* candidates,
    int candidate_count)
{
    if (path == NULL || rec == NULL || candidates == NULL || candidate_count <= 0) {
        return 0;
    }
    char tmp_path[MAX_PATH] = {0};
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);
    FILE* file = fopen(tmp_path, "wb");
    if (file == NULL) {
        return 0;
    }
    fputs("player_id,score,decision_score,age,role,protect_count\r\n", file);
    for (int i = 0; i < candidate_count; i++) {
        char reason[96] = {0};
        int32_t decision_score = kbo_fa_compensation_player_decision_score(
            rec,
            &candidates[i],
            reason,
            sizeof(reason));
        fprintf(
            file,
            "%u,%d,%d,%u,%u,%u\r\n",
            candidates[i].player_id,
            candidates[i].score,
            decision_score,
            (uint32_t)candidates[i].age,
            (uint32_t)candidates[i].role,
            rec->protect_count);
    }
    fclose(file);
    if (!MoveFileExA(tmp_path, path, MOVEFILE_REPLACE_EXISTING)) {
        DeleteFileA(tmp_path);
        return 0;
    }
    return 1;
}

static int kbo_fa_compensation_candidate_index_by_player_id(
    const KboFaProtectedCandidate* candidates,
    int candidate_count,
    const uint8_t* used,
    uint32_t player_id)
{
    if (candidates == NULL || used == NULL || player_id == 0u) {
        return -1;
    }
    for (int i = 0; i < candidate_count; i++) {
        if (!used[i] && candidates[i].player_id == player_id) {
            return i;
        }
    }
    return -1;
}

int kbo_fa_compensation_apply_ortools_order(
    KboFaCompensationRecord* rec,
    KboFaProtectedCandidate* candidates,
    int candidate_count,
    const char* source)
{
    if (read_kbo_localappdata_flag_file("disable_fa_compensation_ortools.txt")
            || rec == NULL || candidates == NULL || candidate_count <= 0 || rec->protect_count == 0u) {
        return 0;
    }

    char request_path[MAX_PATH * 3] = {0};
    char result_path[MAX_PATH * 3] = {0};
    if (!kbo_get_save_scoped_data_file("fa_compensation_ortools_request.csv", request_path, sizeof(request_path))
            || !kbo_get_save_scoped_data_file("fa_compensation_ortools_result.csv", result_path, sizeof(result_path))) {
        return 0;
    }
    if (!kbo_fa_compensation_write_ortools_request(request_path, rec, candidates, candidate_count)) {
        return 0;
    }
    if (!kbo_optimizer_run_mode("fa_compensation", request_path, result_path, 8000u)) {
        return 0;
    }

    FILE* file = fopen(result_path, "rb");
    if (file == NULL) {
        return 0;
    }
    char line[256] = {0};
    if (fgets(line, sizeof(line), file) == NULL) {
        fclose(file);
        return 0;
    }

    KboFaProtectedCandidate ordered[KBO_FA_COMPENSATION_PROTECTED_LIST_MAX];
    uint8_t used[KBO_FA_COMPENSATION_PROTECTED_LIST_MAX] = {0};
    memset(ordered, 0, sizeof(ordered));
    int ordered_count = 0;
    while (ordered_count < candidate_count && fgets(line, sizeof(line), file) != NULL) {
        uint32_t player_id = (uint32_t)strtoul(line, NULL, 10);
        int index = kbo_fa_compensation_candidate_index_by_player_id(
            candidates,
            candidate_count,
            used,
            player_id);
        if (index < 0) {
            continue;
        }
        ordered[ordered_count++] = candidates[index];
        used[index] = 1u;
    }
    fclose(file);
    if (ordered_count <= 0) {
        return 0;
    }

    for (int i = 0; i < candidate_count && ordered_count < candidate_count; i++) {
        if (!used[i]) {
            ordered[ordered_count++] = candidates[i];
        }
    }
    memcpy(candidates, ordered, (SIZE_T)candidate_count * sizeof(candidates[0]));
    kbo_log_runtimef(
        "KBO FA compensation OR-Tools protected order applied source=%s fa_player=%u signing_team=%u candidates=%d protect=%u",
        source != NULL ? source : "",
        rec->player_id,
        rec->signing_team_id,
        candidate_count,
        rec->protect_count);
    return 1;
}
