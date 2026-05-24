#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "secondary_draft_ui_actions_internal.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/dates/tick/current_date_tick_capture.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../team/lookup/team_lookup.h"

int kbo_secondary_draft_candidate_cmp_desc(const void* left, const void* right)
{
    const KboSecondaryDraftCandidate* a = (const KboSecondaryDraftCandidate*)left;
    const KboSecondaryDraftCandidate* b = (const KboSecondaryDraftCandidate*)right;
    if (a->value_score != b->value_score) {
        return a->value_score > b->value_score ? -1 : 1;
    }
    if (a->age != b->age) {
        return a->age < b->age ? -1 : 1;
    }
    if (a->player_id != b->player_id) {
        return a->player_id < b->player_id ? -1 : 1;
    }
    return 0;
}

uint32_t kbo_secondary_draft_current_date_or_season_anchor(uint32_t season)
{
    uint32_t year = 0u;
    uint32_t month = 0u;
    uint32_t day = 0u;
    if (kbo_current_date_tick_latest_components(&year, &month, &day)
            && year != 0u
            && month != 0u
            && day != 0u) {
        return year * 10000u + month * 100u + day;
    }
    return season != 0u ? season * 10000u + 101u : 0u;
}

void kbo_secondary_draft_fill_ui_candidate_row(
    const KboSecondaryDraftCandidate* candidate,
    const char* team_name,
    uint32_t season,
    int submitted,
    KboSecondaryDraftCandidateRow* out)
{
    int saved_protected = candidate != NULL
        ? kbo_secondary_draft_sql_player_protected(season, candidate->owner_team_id, candidate->player_id)
        : 0;
    int already_drafted = candidate != NULL
        ? kbo_secondary_draft_sql_result_player_exists(season, candidate->player_id)
        : 0;
    kbo_secondary_draft_fill_ui_candidate_row_with_status(
        candidate,
        team_name,
        season,
        submitted,
        saved_protected,
        already_drafted,
        out);
}

void kbo_secondary_draft_fill_ui_candidate_row_with_status(
    const KboSecondaryDraftCandidate* candidate,
    const char* team_name,
    uint32_t season,
    int submitted,
    int saved_protected,
    int already_drafted,
    KboSecondaryDraftCandidateRow* out)
{
    if (candidate == NULL || out == NULL) {
        return;
    }
    memset(out, 0, sizeof(*out));
    out->season = season;
    out->player_id = candidate->player_id;
    out->team_id = candidate->owner_team_id;
    out->age = candidate->age;
    out->service_days = candidate->service_days;
    out->total_seasons = candidate->total_seasons;
    out->total_seasons_known = candidate->total_seasons_known;
    out->value_score = candidate->value_score;
    out->auto_protected = candidate->auto_protected ? 1 : 0;
    out->saved_protected = saved_protected ? 1 : 0;
    out->submitted = submitted ? 1 : 0;
    out->already_drafted = already_drafted ? 1 : 0;
    out->eligible = !out->auto_protected && !out->submitted && !out->already_drafted;
    snprintf(out->player_name, sizeof(out->player_name), "%s", candidate->player_name);
    snprintf(out->team_name, sizeof(out->team_name), "%s", team_name != NULL ? team_name : "");
    if (out->already_drafted) {
        snprintf(out->status_label, sizeof(out->status_label), "Drafted");
    } else if (out->auto_protected) {
        snprintf(out->status_label, sizeof(out->status_label), "Auto");
    } else if (out->submitted && out->saved_protected) {
        snprintf(out->status_label, sizeof(out->status_label), "Submitted");
    } else if (out->submitted) {
        snprintf(out->status_label, sizeof(out->status_label), "Locked");
    } else if (out->saved_protected) {
        snprintf(out->status_label, sizeof(out->status_label), "Protected");
    } else {
        snprintf(out->status_label, sizeof(out->status_label), "Available");
    }
}

int kbo_secondary_draft_collect_team_candidates(
    uint32_t team_id,
    KboSecondaryDraftTeam* out_team,
    KboSecondaryDraftCandidate* candidates,
    int max_candidates)
{
    if (team_id == 0u || out_team == NULL || candidates == NULL || max_candidates <= 0) {
        return 0;
    }
    memset(out_team, 0, sizeof(*out_team));
    out_team->team = find_kbo_team_by_numeric_id_any_league(team_id, 1);
    if (out_team->team == NULL || !memory_range_readable(out_team->team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return 0;
    }
    out_team->team_id = team_id;
    uint32_t parent_team_id = *(uint32_t*)(out_team->team + OOTP27_KBO_TEAM_PARENT_TEAM_ID_OFFSET);
    out_team->org_team_id = parent_team_id != 0u ? parent_team_id : team_id;
    out_team->league_id = *(uint32_t*)(out_team->team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
    kbo_secondary_draft_copy_team_name(out_team->team, team_id, out_team->name, sizeof(out_team->name));
    return kbo_secondary_draft_collect_candidates(out_team, 1, candidates, max_candidates);
}
