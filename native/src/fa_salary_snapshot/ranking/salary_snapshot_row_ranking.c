#include "salary_snapshot_row_ranking.h"

#include <stdlib.h>

#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../core/dates/constants/kbo_date_constants.h"

int32_t kbo_fa_salary_snapshot_player_salary_for_season(uint8_t* player, uint32_t season)
{
    int32_t years[OOTP27_PLAYER_CONTRACT_SALARY_YEARS] = {0};
    for (uint32_t i = 0; i < OOTP27_PLAYER_CONTRACT_SALARY_YEARS; i++) {
        years[i] = *(int32_t*)(player + OOTP27_PLAYER_CONTRACT_SALARY_Y1_OFFSET + (i * sizeof(int32_t)));
    }

    int32_t start_year = *(int32_t*)(player + OOTP27_PLAYER_CONTRACT_START_YEAR_OFFSET);
    if (start_year >= (int32_t)KBO_SEASON_YEAR_MIN
            && start_year <= (int32_t)KBO_SIM_YEAR_MAX
            && season >= (uint32_t)start_year) {
        uint32_t index = season - (uint32_t)start_year;
        if (index < OOTP27_PLAYER_CONTRACT_SALARY_YEARS && years[index] > 0) {
            return years[index];
        }
    }

    if (years[0] > 0) {
        return years[0];
    }

    for (uint32_t i = 1; i < OOTP27_PLAYER_CONTRACT_SALARY_YEARS; i++) {
        if (years[i] > 0) {
            return years[i];
        }
    }

    return 0;
}

void kbo_fa_salary_snapshot_copy_contract_years(uint8_t* player, KboFaSalarySnapshotRow* row)
{
    if (player == NULL || row == NULL) {
        return;
    }

    row->contract_status = *(int32_t*)(player + OOTP27_PLAYER_CONTRACT_STATUS_OFFSET);
    row->contract_start_year = *(int32_t*)(player + OOTP27_PLAYER_CONTRACT_START_YEAR_OFFSET);
    for (uint32_t i = 0; i < OOTP27_PLAYER_CONTRACT_SALARY_YEARS; i++) {
        row->contract_years[i] = *(int32_t*)(player + OOTP27_PLAYER_CONTRACT_SALARY_Y1_OFFSET + (i * sizeof(int32_t)));
    }
}

int __cdecl kbo_fa_salary_snapshot_compare_overall(const void* a, const void* b)
{
    const KboFaSalarySnapshotRow* left = (const KboFaSalarySnapshotRow*)a;
    const KboFaSalarySnapshotRow* right = (const KboFaSalarySnapshotRow*)b;

    int left_has_salary = left->salary > 0 && !left->foreign_flag;
    int right_has_salary = right->salary > 0 && !right->foreign_flag;
    if (left_has_salary != right_has_salary) {
        return right_has_salary - left_has_salary;
    }
    if (left->salary != right->salary) {
        return right->salary > left->salary ? 1 : -1;
    }
    if (left->ranking_team_id != right->ranking_team_id) {
        return left->ranking_team_id < right->ranking_team_id ? -1 : 1;
    }
    if (left->player_id != right->player_id) {
        return left->player_id < right->player_id ? -1 : 1;
    }
    return 0;
}

static int __cdecl kbo_fa_salary_snapshot_compare_team(const void* a, const void* b)
{
    const KboFaSalarySnapshotRow* left = (const KboFaSalarySnapshotRow*)a;
    const KboFaSalarySnapshotRow* right = (const KboFaSalarySnapshotRow*)b;

    if (left->ranking_team_id != right->ranking_team_id) {
        return left->ranking_team_id < right->ranking_team_id ? -1 : 1;
    }

    int left_has_salary = left->salary > 0 && !left->foreign_flag;
    int right_has_salary = right->salary > 0 && !right->foreign_flag;
    if (left_has_salary != right_has_salary) {
        return right_has_salary - left_has_salary;
    }
    if (left->salary != right->salary) {
        return right->salary > left->salary ? 1 : -1;
    }
    if (left->player_id != right->player_id) {
        return left->player_id < right->player_id ? -1 : 1;
    }
    return 0;
}

void kbo_fa_salary_snapshot_assign_overall_ranks(KboFaSalarySnapshotRow* rows, int count)
{
    if (rows == NULL || count <= 0) {
        return;
    }

    qsort(rows, (size_t)count, sizeof(KboFaSalarySnapshotRow), kbo_fa_salary_snapshot_compare_overall);

    uint32_t ordinal = 0u;
    uint32_t current_rank = 0u;
    int32_t last_salary = -1;
    for (int i = 0; i < count; i++) {
        rows[i].overall_rank = 0u;
        rows[i].overall_ordinal = 0u;
        if (rows[i].foreign_flag || rows[i].salary <= 0) {
            continue;
        }

        ordinal++;
        if (rows[i].salary != last_salary) {
            current_rank = ordinal;
            last_salary = rows[i].salary;
        }
        rows[i].overall_rank = current_rank;
        rows[i].overall_ordinal = ordinal;
    }
}

void kbo_fa_salary_snapshot_assign_team_ranks(KboFaSalarySnapshotRow* rows, int count)
{
    if (rows == NULL || count <= 0) {
        return;
    }

    qsort(rows, (size_t)count, sizeof(KboFaSalarySnapshotRow), kbo_fa_salary_snapshot_compare_team);

    uint32_t current_team = 0u;
    uint32_t ordinal = 0u;
    uint32_t current_rank = 0u;
    int32_t last_salary = -1;
    for (int i = 0; i < count; i++) {
        rows[i].team_rank = 0u;
        rows[i].team_ordinal = 0u;
        if (rows[i].ranking_team_id == 0u || rows[i].foreign_flag || rows[i].salary <= 0) {
            continue;
        }

        if (rows[i].ranking_team_id != current_team) {
            current_team = rows[i].ranking_team_id;
            ordinal = 0u;
            current_rank = 0u;
            last_salary = -1;
        }

        ordinal++;
        if (rows[i].salary != last_salary) {
            current_rank = ordinal;
            last_salary = rows[i].salary;
        }
        rows[i].team_rank = current_rank;
        rows[i].team_ordinal = ordinal;
    }
}
