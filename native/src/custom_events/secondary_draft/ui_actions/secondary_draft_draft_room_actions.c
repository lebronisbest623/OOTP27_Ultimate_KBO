#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "secondary_draft_ui_actions_internal.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../../core/core_league_context_parts/api/league_context_lookup.h"

static int kbo_secondary_draft_expected_pick_count(int team_count)
{
    if (team_count <= 0) {
        return 0;
    }
    int extra_teams = team_count < KBO_SECONDARY_DRAFT_EXTRA_TEAMS
        ? team_count
        : KBO_SECONDARY_DRAFT_EXTRA_TEAMS;
    return (team_count * KBO_SECONDARY_DRAFT_BASE_ROUNDS)
        + (extra_teams * (KBO_SECONDARY_DRAFT_ROUNDS - KBO_SECONDARY_DRAFT_BASE_ROUNDS));
}

int kbo_secondary_draft_collect_draft_pool_rows(
    uint32_t season,
    uint32_t drafting_team_id,
    KboSecondaryDraftCandidateRow* rows,
    int max_rows)
{
    if (season == 0u || drafting_team_id == 0u || rows == NULL || max_rows <= 0
            || !kbo_secondary_draft_ensure_schema("secondary_draft_ui_draft_pool_schema")) {
        return 0;
    }
    if (!kbo_secondary_draft_draft_window_open(season)) {
        return 0;
    }
    memset(rows, 0, (size_t)max_rows * sizeof(rows[0]));
    uint32_t league_id = kbo_resolve_kbo_league_id();
    if (league_id == 0u) {
        return 0;
    }
    KboSecondaryDraftTeam teams[KBO_SECONDARY_DRAFT_TEAM_MAX];
    memset(teams, 0, sizeof(teams));
    int team_count = kbo_secondary_draft_collect_main_teams(league_id, teams, KBO_SECONDARY_DRAFT_TEAM_MAX);
    if (team_count <= 0) {
        return 0;
    }
    KboSecondaryDraftCandidate* candidates = (KboSecondaryDraftCandidate*)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        (SIZE_T)KBO_SECONDARY_DRAFT_CANDIDATE_MAX * sizeof(KboSecondaryDraftCandidate));
    if (candidates == NULL) {
        return 0;
    }
    int candidate_count = kbo_secondary_draft_collect_candidates(
        teams,
        team_count,
        candidates,
        KBO_SECONDARY_DRAFT_CANDIDATE_MAX);
    (void)kbo_secondary_draft_auto_submit_missing_protection_lists(
        season,
        teams,
        team_count,
        candidates,
        candidate_count,
        "hub_secondary_draft_draft_pool_auto_protect");
    (void)kbo_secondary_draft_mark_protected_players(candidates, candidate_count, team_count, season);
    qsort(candidates, (size_t)candidate_count, sizeof(candidates[0]), kbo_secondary_draft_candidate_cmp_desc);
    uint32_t drafted_ids[KBO_SECONDARY_DRAFT_UI_MAX_ROWS] = {0};
    int drafted_id_count = kbo_secondary_draft_sql_load_result_player_ids(
        season,
        drafted_ids,
        KBO_SECONDARY_DRAFT_UI_MAX_ROWS);

    int drafting_index = kbo_secondary_draft_team_index_by_id(teams, team_count, drafting_team_id);
    int count = 0;
    for (int i = 0; i < candidate_count && count < max_rows; i++) {
        KboSecondaryDraftCandidate* c = &candidates[i];
        if (c->protected_player
                || c->selected
                || c->owner_index < 0
                || c->owner_index == drafting_index
                || kbo_secondary_draft_id_list_contains(drafted_ids, drafted_id_count, c->player_id)) {
            continue;
        }
        kbo_secondary_draft_fill_ui_candidate_row_with_status(
            c,
            teams[c->owner_index].name,
            season,
            0,
            0,
            0,
            &rows[count]);
        rows[count].eligible = 1;
        snprintf(rows[count].status_label, sizeof(rows[count].status_label), "Pickable");
        count++;
    }
    HeapFree(GetProcessHeap(), 0, candidates);
    return count;
}

int kbo_secondary_draft_manual_pick_player(
    uint32_t season,
    uint32_t drafting_team_id,
    uint32_t player_id,
    const char* source)
{
    if (season == 0u || drafting_team_id == 0u || player_id == 0u
            || kbo_secondary_draft_sql_result_player_exists(season, player_id)) {
        return 0;
    }
    if (!kbo_secondary_draft_draft_window_open(season)) {
        return 0;
    }
    uint32_t league_id = kbo_resolve_kbo_league_id();
    if (league_id == 0u) {
        return 0;
    }
    KboSecondaryDraftTeam teams[KBO_SECONDARY_DRAFT_TEAM_MAX];
    memset(teams, 0, sizeof(teams));
    int team_count = kbo_secondary_draft_collect_main_teams(league_id, teams, KBO_SECONDARY_DRAFT_TEAM_MAX);
    if (team_count <= 0) {
        return 0;
    }
    qsort(teams, (size_t)team_count, sizeof(teams[0]), kbo_secondary_draft_team_order_cmp);
    int drafting_index = kbo_secondary_draft_team_index_by_id(teams, team_count, drafting_team_id);
    if (drafting_index < 0) {
        return 0;
    }
    KboSecondaryDraftCandidate* candidates = (KboSecondaryDraftCandidate*)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        (SIZE_T)KBO_SECONDARY_DRAFT_CANDIDATE_MAX * sizeof(KboSecondaryDraftCandidate));
    if (candidates == NULL) {
        return 0;
    }
    int candidate_count = kbo_secondary_draft_collect_candidates(
        teams,
        team_count,
        candidates,
        KBO_SECONDARY_DRAFT_CANDIDATE_MAX);
    (void)kbo_secondary_draft_auto_submit_missing_protection_lists(
        season,
        teams,
        team_count,
        candidates,
        candidate_count,
        "hub_secondary_draft_manual_pick_auto_protect");
    int protected_count = kbo_secondary_draft_mark_protected_players(candidates, candidate_count, team_count, season);
    int candidate_index = -1;
    for (int i = 0; i < candidate_count; i++) {
        if (candidates[i].player_id == player_id) {
            candidate_index = i;
            break;
        }
    }
    if (candidate_index < 0
            || candidates[candidate_index].protected_player
            || candidates[candidate_index].owner_index < 0
            || candidates[candidate_index].owner_index == drafting_index) {
        HeapFree(GetProcessHeap(), 0, candidates);
        return 0;
    }
    int result_count = kbo_secondary_draft_sql_result_count(season);
    uint32_t pick_no = (uint32_t)result_count + 1u;
    uint32_t round = team_count > 0 ? ((pick_no - 1u) / (uint32_t)team_count) + 1u : 1u;
    KboSecondaryDraftPick pick;
    uint32_t event_yyyymmdd = kbo_secondary_draft_current_date_or_season_anchor(season);
    int picked = kbo_secondary_draft_apply_pick(
        &candidates[candidate_index],
        &teams[candidates[candidate_index].owner_index],
        &teams[drafting_index],
        league_id,
        event_yyyymmdd,
        round,
        pick_no,
        &pick);
    if (!picked) {
        HeapFree(GetProcessHeap(), 0, candidates);
        return 0;
    }
    int saved = kbo_secondary_draft_sql_append_pick(season, event_yyyymmdd, league_id, &pick, source);
    if (saved) {
        KboSecondaryDraftRunSummary summary;
        memset(&summary, 0, sizeof(summary));
        int64_t cash_total = pick.cash_amount;
        if (kbo_secondary_draft_load_run_summary(season, &summary)) {
            cash_total += summary.cash_total;
        }
        kbo_secondary_draft_sql_mark_run(
            season,
            event_yyyymmdd,
            league_id,
            result_count + 1,
            candidate_count,
            protected_count,
            cash_total,
            source);
        if (result_count + 1 >= kbo_secondary_draft_expected_pick_count(team_count)) {
            kbo_secondary_draft_emit_stored_results_news(
                season,
                event_yyyymmdd,
                league_id,
                candidate_count,
                protected_count,
                cash_total,
                source != NULL ? source : "hub_secondary_draft_pick");
        }
    }
    HeapFree(GetProcessHeap(), 0, candidates);
    return saved;
}
