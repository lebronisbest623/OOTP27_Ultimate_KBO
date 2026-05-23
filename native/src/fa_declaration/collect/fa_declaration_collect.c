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
    snprintf(candidate->reason, sizeof(candidate->reason), "%.*s", (int)sizeof(candidate->reason) - 1, row->reason);

    uint32_t ignored_team = 0u;
    uint32_t ignored_league = 0u;
    uint8_t* player = kbo_find_player_by_id(row->player_id, &ignored_team, &ignored_league);
    kbo_fa_declaration_fill_from_player(candidate, player, season);
    kbo_fa_declaration_apply_salary_grade(candidate, grades, grade_count);
    kbo_fa_declaration_decide(candidate);
    return 1;
}
