#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../secondary_draft_internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../../core/league_roles/kbo_league_roles.h"
#include "../../../core/logging/core_log.h"

static uint32_t kbo_secondary_draft_resolve_event_season(
    uint32_t event_yyyymmdd,
    KboSecondaryDraftWindow* out_window)
{
    if (out_window != NULL) {
        memset(out_window, 0, sizeof(*out_window));
    }
    if (event_yyyymmdd == 0u) {
        return 0u;
    }

    KboSecondaryDraftWindow window;
    memset(&window, 0, sizeof(window));
    if (kbo_secondary_draft_load_window_for_draft_date(event_yyyymmdd, &window)
            && kbo_secondary_draft_is_odd_season(window.season)) {
        if (out_window != NULL) {
            *out_window = window;
        }
        return window.season;
    }

    uint32_t season = event_yyyymmdd / 10000u;
    return kbo_secondary_draft_is_odd_season(season) ? season : 0u;
}

int kbo_secondary_draft_completion_valid(uint32_t league_id, uint32_t event_yyyymmdd)
{
    (void)league_id;
    uint32_t season = kbo_secondary_draft_resolve_event_season(event_yyyymmdd, NULL);
    if (season == 0u) {
        return 1;
    }
    return kbo_secondary_draft_sql_run_exists(season)
        || kbo_secondary_draft_sql_result_count(season) > 0;
}

int kbo_handle_secondary_draft_event(uint32_t event_yyyymmdd, const char* source)
{
    KboSecondaryDraftWindow window;
    uint32_t season = kbo_secondary_draft_resolve_event_season(event_yyyymmdd, &window);
    if (season == 0u) {
        kbo_log_runtimef(
            "KBO secondary draft skipped source=%s date=%u reason=even_or_invalid_year",
            source != NULL ? source : "",
            event_yyyymmdd);
        return 1;
    }

    uint32_t league_id = window.league_id != 0u
        ? window.league_id
        : kbo_league_role_main_league_id();
    if (league_id == 0u) {
        league_id = kbo_resolve_kbo_league_id();
    }
    if (league_id == 0u) {
        kbo_log_runtimef(
            "KBO secondary draft skipped source=%s date=%u reason=league_unavailable",
            source != NULL ? source : "",
            event_yyyymmdd);
        return 0;
    }
    if (kbo_secondary_draft_sql_run_exists(season)) {
        kbo_log_runtimef(
            "KBO secondary draft skipped source=%s season=%u reason=already_completed",
            source != NULL ? source : "",
            season);
        return 1;
    }
    int existing_results = kbo_secondary_draft_sql_result_count(season);
    if (existing_results > 0) {
        kbo_secondary_draft_sql_mark_run(season, event_yyyymmdd, league_id, existing_results, 0, 0, 0, source);
        kbo_log_runtimef(
            "KBO secondary draft completed from existing partial ledger source=%s season=%u picks=%d",
            source != NULL ? source : "",
            season,
            existing_results);
        return 1;
    }

    KboSecondaryDraftTeam teams[KBO_SECONDARY_DRAFT_TEAM_MAX];
    int team_count = kbo_secondary_draft_collect_main_teams(league_id, teams, KBO_SECONDARY_DRAFT_TEAM_MAX);
    if (team_count <= 1) {
        kbo_log_runtimef(
            "KBO secondary draft skipped source=%s season=%u reason=teams_unavailable league=%u teams=%d",
            source != NULL ? source : "",
            season,
            league_id,
            team_count);
        return 0;
    }
    qsort(teams, (size_t)team_count, sizeof(teams[0]), kbo_secondary_draft_team_order_cmp);

    KboSecondaryDraftCandidate* candidates = (KboSecondaryDraftCandidate*)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        (SIZE_T)KBO_SECONDARY_DRAFT_CANDIDATE_MAX * sizeof(KboSecondaryDraftCandidate));
    if (candidates == NULL) {
        kbo_log_runtime_line("KBO secondary draft skipped reason=candidate_alloc_failed");
        return 0;
    }

    int candidate_count = kbo_secondary_draft_collect_candidates(
        season,
        teams,
        team_count,
        candidates,
        KBO_SECONDARY_DRAFT_CANDIDATE_MAX);
    int auto_submitted_lists = kbo_secondary_draft_auto_submit_missing_protection_lists(
        season,
        teams,
        team_count,
        candidates,
        candidate_count,
        "secondary_draft_event_auto_protect");
    int protected_count = kbo_secondary_draft_mark_protected_players(candidates, candidate_count, team_count, season);

    KboSecondaryDraftPickRow picks[KBO_SECONDARY_DRAFT_RESULT_MAX];
    memset(picks, 0, sizeof(picks));
    int pick_count = 0;
    int64_t cash_total = 0;

    for (uint32_t round = 1u; round <= KBO_SECONDARY_DRAFT_ROUNDS; round++) {
        int round_team_limit = kbo_secondary_draft_round_team_limit(team_count, round);
        for (int team_index = 0; team_index < round_team_limit; team_index++) {
            int picked = 0;
            for (int attempt = 0; attempt < candidate_count && !picked; attempt++) {
                int candidate_index = kbo_secondary_draft_best_candidate_for_team(
                    candidates,
                    candidate_count,
                    teams,
                    team_index);
                if (candidate_index < 0) {
                    break;
                }
                KboSecondaryDraftCandidate* c = &candidates[candidate_index];
                c->selected = 1u;
                KboSecondaryDraftPickRow pick;
                if (!kbo_secondary_draft_apply_pick(
                        c,
                        &teams[c->owner_index],
                        &teams[team_index],
                        league_id,
                        event_yyyymmdd,
                        round,
                        (uint32_t)pick_count + 1u,
                        &pick)) {
                    continue;
                }
                teams[c->owner_index].loss_count++;
                if (pick_count < KBO_SECONDARY_DRAFT_RESULT_MAX) {
                    picks[pick_count] = pick;
                    pick_count++;
                    cash_total += (int64_t)pick.cash_amount;
                    kbo_secondary_draft_sql_append_pick(season, event_yyyymmdd, league_id, &pick, source);
                }
                picked = 1;
            }
        }
    }

    int run_saved = kbo_secondary_draft_sql_mark_run(
        season,
        event_yyyymmdd,
        league_id,
        pick_count,
        candidate_count,
        protected_count,
        cash_total,
        source);
    int news_created = kbo_secondary_draft_emit_results_news(
        event_yyyymmdd,
        league_id,
        picks,
        pick_count,
        candidate_count,
        protected_count,
        cash_total,
        source != NULL ? source : "secondary_draft");

    kbo_log_runtimef(
        "KBO secondary draft completed source=%s season=%u league=%u teams=%d candidates=%d protected=%d auto_submitted_lists=%d picks=%d cash_total=%lld run_saved=%d news=%d",
        source != NULL ? source : "",
        season,
        league_id,
        team_count,
        candidate_count,
        protected_count,
        auto_submitted_lists,
        pick_count,
        (long long)cash_total,
        run_saved,
        news_created);

    HeapFree(GetProcessHeap(), 0, candidates);
    return run_saved ? 1 : 0;
}
