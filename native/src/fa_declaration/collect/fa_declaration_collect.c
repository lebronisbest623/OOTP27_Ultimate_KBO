#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../fa_declaration_internal.h"
#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../fa_market_classification/api/fa_market_classification.h"
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
    snprintf(candidate->reason, sizeof(candidate->reason), "%s", row->reason);

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

static int kbo_fa_declaration_active_player_candidate(
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
    uint16_t age = *(uint16_t*)(player + OOTP27_PLAYER_AGE_OFFSET);
    if (player_id == 0u
            || current_team_id == 0u
            || nation_id != OOTP27_KBO_KOREA_NATION_ID
            || player[OOTP27_PLAYER_RETIRED_FLAG_OFFSET] != 0u
            || player[OOTP27_PLAYER_DFA_FLAG_OFFSET] != 0u
            || player[OOTP27_PLAYER_DRAFT_ELIGIBLE_OFFSET] != 0u
            || age < 29u
            || !kbo_fa_declaration_team_is_kbo(current_team_id, league_id)) {
        return 0;
    }

    int32_t next_salary = 0;
    int32_t salary = kbo_fa_declaration_contract_salary_for_season(player, season, &next_salary);
    int32_t score = kbo_foreign_waiver_value_score(player);
    if (salary <= 0 || next_salary > 0) {
        return 0;
    }
    if (score < 45000 && salary < 120000000) {
        return 0;
    }

    if (out_salary != NULL) {
        *out_salary = salary;
    }
    return 1;
}

int kbo_fa_declaration_collect_active_fallback(
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
    if (candidates == NULL || candidate_count == NULL) {
        return -1;
    }

    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        return -1;
    }

    int added = 0;
    for (int32_t i = 0; i < player_count && *candidate_count < KBO_FA_DECLARATION_MAX; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }
        uint8_t* player = (uint8_t*)player_ptr;
        if (out_scanned != NULL) {
            (*out_scanned)++;
        }
        uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        if (kbo_fa_declaration_find_candidate(candidates, *candidate_count, player_id) >= 0) {
            continue;
        }

        int32_t salary = 0;
        if (!kbo_fa_declaration_active_player_candidate(player, season, league_id, &salary)) {
            continue;
        }

        KboFaDeclarationCandidate* candidate = &candidates[(*candidate_count)++];
        memset(candidate, 0, sizeof(*candidate));
        candidate->declaration_date = event_yyyymmdd;
        candidate->season = season;
        candidate->team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        candidate->league_id = kbo_fa_declaration_team_league(candidate->team_id, league_id);
        candidate->salary = salary;
        snprintf(candidate->case_label, sizeof(candidate->case_label), "KBO_FA_ELIGIBLE_PROXY");
        snprintf(candidate->grade, sizeof(candidate->grade), "UNKNOWN");
        snprintf(
            candidate->reason,
            sizeof(candidate->reason),
            "active domestic expiring-contract proxy; actual FA transition not linked yet");
        kbo_fa_declaration_fill_from_player(candidate, player, season);
        kbo_fa_declaration_apply_salary_grade(candidate, grades, grade_count);
        kbo_fa_declaration_decide(candidate);
        added++;
    }
    return added;
}
