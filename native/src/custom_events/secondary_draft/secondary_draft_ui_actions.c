#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "secondary_draft_ui_actions_internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../../team/lookup/team_lookup.h"

int kbo_secondary_draft_collect_team_list_rows(
    uint32_t season,
    uint32_t team_id,
    KboSecondaryDraftCandidateRow* rows,
    int max_rows,
    int* out_submitted,
    int* out_saved_count)
{
    if (out_submitted != NULL) { *out_submitted = 0; }
    if (out_saved_count != NULL) { *out_saved_count = 0; }
    if (season == 0u || team_id == 0u || rows == NULL || max_rows <= 0
            || !kbo_secondary_draft_ensure_schema("secondary_draft_ui_team_list_schema")) {
        return 0;
    }
    memset(rows, 0, (size_t)max_rows * sizeof(rows[0]));
    int saved_count = kbo_secondary_draft_sql_protected_count(season, team_id);
    int submitted_count = 0;
    int submitted = kbo_secondary_draft_sql_team_submitted(season, team_id, &submitted_count);
    (void)submitted_count;
    if (out_submitted != NULL) { *out_submitted = submitted; }
    if (out_saved_count != NULL) { *out_saved_count = saved_count; }
    uint32_t protected_ids[KBO_SECONDARY_DRAFT_PROTECTED_LIST_LIMIT] = {0};
    int protected_id_count = kbo_secondary_draft_sql_load_protected_player_ids(
        season,
        team_id,
        protected_ids,
        KBO_SECONDARY_DRAFT_PROTECTED_LIST_LIMIT);
    uint32_t drafted_ids[KBO_SECONDARY_DRAFT_UI_MAX_ROWS] = {0};
    int drafted_id_count = kbo_secondary_draft_sql_load_result_player_ids(
        season,
        drafted_ids,
        KBO_SECONDARY_DRAFT_UI_MAX_ROWS);

    KboSecondaryDraftCandidate* candidates = (KboSecondaryDraftCandidate*)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        (SIZE_T)KBO_SECONDARY_DRAFT_CANDIDATE_MAX * sizeof(KboSecondaryDraftCandidate));
    if (candidates == NULL) {
        return 0;
    }
    KboSecondaryDraftTeam team;
    int candidate_count = kbo_secondary_draft_collect_team_candidates(
        team_id,
        &team,
        candidates,
        KBO_SECONDARY_DRAFT_CANDIDATE_MAX);
    qsort(candidates, (size_t)candidate_count, sizeof(candidates[0]), kbo_secondary_draft_candidate_cmp_desc);

    int count = 0;
    for (int i = 0; i < candidate_count && count < max_rows; i++) {
        int saved = kbo_secondary_draft_id_list_contains(
            protected_ids,
            protected_id_count,
            candidates[i].player_id);
        int drafted = kbo_secondary_draft_id_list_contains(
            drafted_ids,
            drafted_id_count,
            candidates[i].player_id);
        kbo_secondary_draft_fill_ui_candidate_row_with_status(
            &candidates[i],
            team.name,
            season,
            submitted,
            saved,
            drafted,
            &rows[count]);
        count++;
    }
    HeapFree(GetProcessHeap(), 0, candidates);
    return count;
}

int kbo_secondary_draft_set_protected_player(
    uint32_t season,
    uint32_t team_id,
    uint32_t player_id,
    int protect,
    const char* source)
{
    if (season == 0u || team_id == 0u || player_id == 0u
            || !kbo_secondary_draft_ensure_schema("secondary_draft_set_protected_schema")) {
        return 0;
    }
    if (!kbo_secondary_draft_protection_window_open(season)) {
        return 0;
    }
    if (kbo_secondary_draft_sql_team_submitted(season, team_id, NULL)) {
        return 0;
    }

    KboSecondaryDraftCandidate* candidates = (KboSecondaryDraftCandidate*)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        (SIZE_T)KBO_SECONDARY_DRAFT_CANDIDATE_MAX * sizeof(KboSecondaryDraftCandidate));
    if (candidates == NULL) {
        return 0;
    }
    KboSecondaryDraftTeam team;
    int candidate_count = kbo_secondary_draft_collect_team_candidates(
        team_id,
        &team,
        candidates,
        KBO_SECONDARY_DRAFT_CANDIDATE_MAX);
    int ok = 0;
    for (int i = 0; i < candidate_count; i++) {
        KboSecondaryDraftCandidate* c = &candidates[i];
        if (c->player_id != player_id) {
            continue;
        }
        if (c->auto_protected || kbo_secondary_draft_sql_result_player_exists(season, player_id)) {
            break;
        }
        if (protect) {
            int already = kbo_secondary_draft_sql_player_protected(season, team_id, player_id);
            int saved_count = kbo_secondary_draft_sql_protected_count(season, team_id);
            if (!already && saved_count >= KBO_SECONDARY_DRAFT_PROTECTED_LIST_LIMIT) {
                break;
            }
            ok = kbo_secondary_draft_sql_write_protected_player(
                season,
                team_id,
                player_id,
                c->player_name,
                team.name,
                source);
        } else {
            ok = kbo_secondary_draft_sql_delete_protected_player(season, team_id, player_id);
        }
        break;
    }
    HeapFree(GetProcessHeap(), 0, candidates);
    return ok;
}

int kbo_secondary_draft_autofill_protected_list(uint32_t season, uint32_t team_id, const char* source)
{
    if (season == 0u || team_id == 0u
            || !kbo_secondary_draft_protection_window_open(season)
            || kbo_secondary_draft_sql_team_submitted(season, team_id, NULL)) {
        return 0;
    }
    KboSecondaryDraftCandidate* candidates = (KboSecondaryDraftCandidate*)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        (SIZE_T)KBO_SECONDARY_DRAFT_CANDIDATE_MAX * sizeof(KboSecondaryDraftCandidate));
    if (candidates == NULL) {
        return 0;
    }
    KboSecondaryDraftTeam team;
    int candidate_count = kbo_secondary_draft_collect_team_candidates(
        team_id,
        &team,
        candidates,
        KBO_SECONDARY_DRAFT_CANDIDATE_MAX);
    qsort(candidates, (size_t)candidate_count, sizeof(candidates[0]), kbo_secondary_draft_candidate_cmp_desc);
    if (!kbo_secondary_draft_sql_clear_protected_team(season, team_id)) {
        HeapFree(GetProcessHeap(), 0, candidates);
        return 0;
    }
    int written = 0;
    for (int i = 0; i < candidate_count && written < KBO_SECONDARY_DRAFT_PROTECTED_LIST_LIMIT; i++) {
        if (candidates[i].auto_protected
                || kbo_secondary_draft_sql_result_player_exists(season, candidates[i].player_id)) {
            continue;
        }
        if (kbo_secondary_draft_sql_write_protected_player(
                season,
                team_id,
                candidates[i].player_id,
                candidates[i].player_name,
                team.name,
                source)) {
            written++;
        }
    }
    HeapFree(GetProcessHeap(), 0, candidates);
    return written;
}

int kbo_secondary_draft_submit_protected_list(uint32_t season, uint32_t team_id, const char* source)
{
    if (season == 0u || team_id == 0u) {
        return 0;
    }
    if (!kbo_secondary_draft_protection_window_open(season)) {
        return 0;
    }
    int saved_count = kbo_secondary_draft_sql_protected_count(season, team_id);
    if (saved_count > KBO_SECONDARY_DRAFT_PROTECTED_LIST_LIMIT) {
        return 0;
    }
    uint8_t* team = find_kbo_team_by_numeric_id_any_league(team_id, 1);
    char team_name[96] = {0};
    kbo_secondary_draft_copy_team_name(team, team_id, team_name, sizeof(team_name));
    return kbo_secondary_draft_sql_submit_team(season, team_id, team_name, saved_count, source);
}
