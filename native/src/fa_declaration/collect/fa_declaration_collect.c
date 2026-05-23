#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../fa_declaration_internal.h"
#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../fa_market_classification/api/fa_market_classification.h"
#include "../../fa_market_classification/policy/fa_market_policy.h"
#include "../../fa_salary_snapshot/grading/salary_snapshot_grade_rows.h"
#include "../../foreign/common/player_eval/foreign_waiver_player_eval.h"
#include "../../runtime_memory/runtime_memory.h"
#include "../../team/lookup/team_lookup.h"

int kbo_fa_declaration_add_market_candidate(
    const KboFaMarketClassification* row,
    uint32_t event_yyyymmdd,
    uint32_t season,
    uint32_t league_id,
    const KboFaSalarySnapshotGrade* grades,
    int grade_count,
    KboFaDeclarationCandidate* candidates,
    int* candidate_count)
{
    if (row == NULL
            || candidates == NULL
            || candidate_count == NULL
            || *candidate_count >= KBO_FA_DECLARATION_MAX
            || !kbo_fa_declaration_case_candidate(row->case_label)
            || row->foreign_player != 0u
            || row->retired_flag != 0u
            || row->player_id == 0u) {
        return 0;
    }
    if (strcmp(row->case_label, "KBO_FA_CARRYOVER_UNSIGNED") == 0
            || (strcmp(row->case_label, "KBO_FA_BY_HISTORY_UNGRADED") == 0
                && row->fa_filing_season != 0u
                && season != 0u
                && row->fa_filing_season < season)) {
        return 0;
    }
    if (kbo_fa_declaration_find_candidate(candidates, *candidate_count, row->player_id) >= 0) {
        return 0;
    }

    KboFaDeclarationCandidate* candidate = &candidates[(*candidate_count)++];
    memset(candidate, 0, sizeof(*candidate));
    candidate->player_id = row->player_id;
    candidate->declaration_date = event_yyyymmdd;
    candidate->season = season;
    candidate->team_id = row->original_team_id != 0u ? row->original_team_id : row->active_team_id;
    candidate->league_id = kbo_fa_declaration_team_league(candidate->team_id, league_id);
    candidate->nation_id = row->nation_id;
    candidate->age = row->age;
    candidate->contract_level = row->contract_level;
    candidate->dfa = row->dfa;
    candidate->retired_flag = row->retired_flag;
    candidate->salary = row->fa_grade_salary;
    candidate->fa_demand = row->fa_demand;
    candidate->from_market = 1u;
    snprintf(candidate->player_name, sizeof(candidate->player_name), "%s", row->player_name);
    snprintf(candidate->case_label, sizeof(candidate->case_label), "%s", row->case_label);
    snprintf(candidate->grade, sizeof(candidate->grade), "%s", row->grade[0] != '\0' ? row->grade : "UNKNOWN");
    snprintf(candidate->reason, sizeof(candidate->reason), "%.*s", (int)sizeof(candidate->reason) - 1, row->reason);

    uint32_t ignored_team = 0u;
    uint32_t ignored_league = 0u;
    uint8_t* player = kbo_find_player_by_id(row->player_id, &ignored_team, &ignored_league);
    kbo_fa_declaration_fill_from_player(candidate, player, season);
    kbo_fa_declaration_apply_salary_grade(candidate, grades, grade_count);
    kbo_fa_declaration_decide(candidate);
    return 1;
}

static int kbo_fa_declaration_team_is_kbo(uint32_t team_id, uint32_t league_id)
{
    if (team_id == 0u) {
        return 0;
    }
    uint8_t* team = find_kbo_team_by_numeric_id_any_league(team_id, 0);
    if (team == NULL || !memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return 0;
    }
    uint32_t team_league_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
    return team_league_id != 0u && (league_id == 0u || team_league_id == league_id);
}

static uint32_t kbo_fa_declaration_player_service_time_days(uint8_t* player)
{
    if (player == NULL || !memory_range_readable(player + OOTP27_PLAYER_SERVICE_TIME_DAYS_OFFSET, sizeof(uint16_t))) {
        return 0u;
    }
    return (uint32_t)*(uint16_t*)(player + OOTP27_PLAYER_SERVICE_TIME_DAYS_OFFSET);
}

static uint32_t kbo_fa_declaration_player_service_seasons(uint8_t* player)
{
    uint32_t service_days = kbo_fa_declaration_player_service_time_days(player);
    const KboFaMarketPolicy* policy = kbo_fa_market_policy();
    uint32_t days_per_season = policy != NULL && policy->service_time_days_per_season > 0
        ? (uint32_t)policy->service_time_days_per_season
        : 145u;
    return service_days / days_per_season;
}

static int kbo_fa_declaration_roster_player_candidate(
    uint8_t* player,
    uint32_t season,
    uint32_t league_id,
    int32_t* out_salary)
{
    if (out_salary != NULL) {
        *out_salary = 0;
    }
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
    uint32_t service_seasons = kbo_fa_declaration_player_service_seasons(player);
    const KboFaMarketPolicy* policy = kbo_fa_market_policy();
    uint32_t service_seasons_min = policy != NULL && policy->fa_declaration_service_seasons_min > 0
        ? (uint32_t)policy->fa_declaration_service_seasons_min
        : 8u;
    if (player_id == 0u
            || current_team_id == 0u
            || nation_id != OOTP27_KBO_KOREA_NATION_ID
            || player[OOTP27_PLAYER_RETIRED_FLAG_OFFSET] != 0u
            || player[OOTP27_PLAYER_DFA_FLAG_OFFSET] != 0u
            || player[OOTP27_PLAYER_DRAFT_ELIGIBLE_OFFSET] != 0u
            || service_seasons < service_seasons_min
            || !kbo_fa_declaration_team_is_kbo(current_team_id, league_id)) {
        return 0;
    }

    int32_t next_salary = 0;
    int32_t salary = kbo_fa_declaration_contract_salary_for_season(player, season, &next_salary);
    if (salary <= 0 || next_salary > 0) {
        return 0;
    }

    if (out_salary != NULL) {
        *out_salary = salary;
    }
    return 1;
}

int kbo_fa_declaration_collect_roster_candidates(
    uint32_t event_yyyymmdd,
    uint32_t season,
    uint32_t league_id,
    const KboFaSalarySnapshotGrade* grades,
    int grade_count,
    KboFaDeclarationCandidate* candidates,
    int* candidate_count,
    int* out_scanned)
{
    if (out_scanned != NULL) {
        *out_scanned = 0;
    }
    if (grades == NULL || grade_count <= 0 || candidates == NULL || candidate_count == NULL) {
        return 0;
    }

    int added = 0;
    for (int i = 0; i < grade_count && *candidate_count < KBO_FA_DECLARATION_MAX; i++) {
        const KboFaSalarySnapshotGrade* grade = &grades[i];
        if (grade->player_id == 0u || grade->foreign_flag != 0u || grade->salary <= 0) {
            continue;
        }
        if (out_scanned != NULL) {
            (*out_scanned)++;
        }
        if (kbo_fa_declaration_find_candidate(candidates, *candidate_count, grade->player_id) >= 0) {
            continue;
        }

        uint32_t ignored_team = 0u;
        uint32_t ignored_league = 0u;
        uint8_t* player = kbo_find_player_by_id(grade->player_id, &ignored_team, &ignored_league);
        int32_t salary = 0;
        if (!kbo_fa_declaration_roster_player_candidate(player, season, league_id, &salary)) {
            continue;
        }

        KboFaDeclarationCandidate* candidate = &candidates[(*candidate_count)++];
        memset(candidate, 0, sizeof(*candidate));
        candidate->player_id = grade->player_id;
        candidate->declaration_date = event_yyyymmdd;
        candidate->season = season;
        candidate->team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        candidate->league_id = kbo_fa_declaration_team_league(candidate->team_id, league_id);
        candidate->salary = salary > 0 ? salary : grade->salary;
        snprintf(candidate->case_label, sizeof(candidate->case_label), "KBO_FA_ELIGIBLE_CURRENT");
        snprintf(candidate->grade, sizeof(candidate->grade), "%s", grade->grade[0] != '\0' ? grade->grade : "UNKNOWN");
        snprintf(
            candidate->reason,
            sizeof(candidate->reason),
            "current KBO roster expiring-contract candidate service_days=%u service_seasons=%u from opening-day salary snapshot",
            kbo_fa_declaration_player_service_time_days(player),
            kbo_fa_declaration_player_service_seasons(player));
        kbo_fa_declaration_fill_from_player(candidate, player, season);
        kbo_fa_declaration_apply_salary_grade(candidate, grades, grade_count);
        kbo_fa_declaration_decide(candidate);
        added++;
    }
    return added;
}
