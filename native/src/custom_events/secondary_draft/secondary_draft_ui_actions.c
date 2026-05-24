#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "secondary_draft_ui_actions_internal.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../foreign/common/player_eval/foreign_waiver_player_eval.h"
#include "../../runtime_memory/runtime_memory.h"
#include "../../team/lookup/team_lookup.h"
#include "../../team/names/team_name_cache.h"

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

static int kbo_secondary_draft_resolve_single_team(
    uint32_t team_id,
    KboSecondaryDraftTeam* out_team)
{
    if (team_id == 0u || out_team == NULL) {
        return 0;
    }
    memset(out_team, 0, sizeof(*out_team));
    out_team->team = find_kbo_team_by_numeric_id_any_league(team_id, 1);
    if (out_team->team == NULL
            || !memory_range_readable(out_team->team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return 0;
    }
    out_team->team_id = team_id;
    uint32_t parent_team_id = *(uint32_t*)(out_team->team + OOTP27_KBO_TEAM_PARENT_TEAM_ID_OFFSET);
    out_team->org_team_id = parent_team_id != 0u ? parent_team_id : team_id;
    out_team->league_id = *(uint32_t*)(out_team->team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
    kbo_secondary_draft_copy_team_name(out_team->team, team_id, out_team->name, sizeof(out_team->name));
    return 1;
}

static int kbo_secondary_draft_validate_single_protect_candidate(
    uint32_t team_id,
    uint32_t player_id,
    KboSecondaryDraftTeam* out_team,
    char* out_player_name,
    size_t player_name_size)
{
    if (!kbo_secondary_draft_resolve_single_team(team_id, out_team)) {
        return 0;
    }
    uint8_t* player = kbo_find_player_by_id(player_id, NULL, NULL);
    if (player == NULL || !kbo_secondary_draft_player_status_ok(player)) {
        return 0;
    }
    if (kbo_secondary_draft_owner_index_for_player(player, out_team, 1) < 0) {
        return 0;
    }
    if (kbo_secondary_draft_player_auto_protected_by_tenure(player, NULL, NULL)) {
        return 0;
    }
    if (out_player_name != NULL && player_name_size > 0u) {
        out_player_name[0] = '\0';
        kbo_copy_player_display_name(player, out_player_name, player_name_size);
        if (out_player_name[0] == '\0') {
            snprintf(out_player_name, player_name_size, "Player #%u", player_id);
        }
    }
    return 1;
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

    if (!protect) {
        if (kbo_secondary_draft_sql_result_player_exists(season, player_id)) {
            return 0;
        }
        return kbo_secondary_draft_sql_delete_protected_player(season, team_id, player_id);
    }

    if (kbo_secondary_draft_sql_result_player_exists(season, player_id)) {
        return 0;
    }

    int already = kbo_secondary_draft_sql_player_protected(season, team_id, player_id);
    int saved_count = kbo_secondary_draft_sql_protected_count(season, team_id);
    if (!already && saved_count >= KBO_SECONDARY_DRAFT_PROTECTED_LIST_LIMIT) {
        return 0;
    }

    KboSecondaryDraftTeam team;
    char player_name[96] = {0};
    if (!kbo_secondary_draft_validate_single_protect_candidate(
        team_id,
        player_id,
        &team,
        player_name,
        sizeof(player_name))) {
        return 0;
    }
    return kbo_secondary_draft_sql_write_protected_player(
        season,
        team_id,
        player_id,
        player_name,
        team.name,
        source);
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
