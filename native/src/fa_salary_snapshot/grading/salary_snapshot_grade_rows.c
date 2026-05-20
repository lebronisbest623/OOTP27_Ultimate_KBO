#include "salary_snapshot_grade_rows.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../paths/salary_snapshot_paths_dates.h"
#include "../sql/fa_salary_snapshot_sql_store.h"
#include "../../fa_market_classification/policy/fa_market_policy.h"
#include "../../core/dates/constants/kbo_date_constants.h"

static const char* kbo_fa_salary_snapshot_grade_for_ranks(uint32_t overall_rank, uint32_t team_rank, int32_t salary)
{
    if (salary <= 0 || (overall_rank == 0u && team_rank == 0u)) {
        return "UNKNOWN";
    }
    const KboFaMarketPolicy* policy = kbo_fa_market_policy();
    if ((overall_rank != 0u && overall_rank <= (uint32_t)policy->salary_grade_a_overall_rank_max)
            && (team_rank != 0u && team_rank <= (uint32_t)policy->salary_grade_a_team_rank_max)) {
        return "A";
    }
    if ((overall_rank != 0u && overall_rank <= (uint32_t)policy->salary_grade_b_overall_rank_max)
            && (team_rank != 0u && team_rank <= (uint32_t)policy->salary_grade_b_team_rank_max)) {
        return "B";
    }
    return "C";
}

static int __cdecl kbo_fa_salary_snapshot_grade_compare_overall(const void* a, const void* b)
{
    const KboFaSalarySnapshotGrade* left = (const KboFaSalarySnapshotGrade*)a;
    const KboFaSalarySnapshotGrade* right = (const KboFaSalarySnapshotGrade*)b;

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

static int __cdecl kbo_fa_salary_snapshot_grade_compare_team(const void* a, const void* b)
{
    const KboFaSalarySnapshotGrade* left = (const KboFaSalarySnapshotGrade*)a;
    const KboFaSalarySnapshotGrade* right = (const KboFaSalarySnapshotGrade*)b;

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

static void kbo_fa_salary_snapshot_assign_grade_overall_ranks(KboFaSalarySnapshotGrade* rows, int count)
{
    if (rows == NULL || count <= 0) {
        return;
    }

    qsort(rows, (size_t)count, sizeof(KboFaSalarySnapshotGrade), kbo_fa_salary_snapshot_grade_compare_overall);

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

static void kbo_fa_salary_snapshot_assign_grade_team_ranks(KboFaSalarySnapshotGrade* rows, int count)
{
    if (rows == NULL || count <= 0) {
        return;
    }

    qsort(rows, (size_t)count, sizeof(KboFaSalarySnapshotGrade), kbo_fa_salary_snapshot_grade_compare_team);

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

int kbo_fa_salary_snapshot_load_grade_rows(
    uint32_t season,
    KboFaSalarySnapshotGrade* rows,
    int max_rows,
    char* out_path,
    size_t out_path_size)
{
    if (out_path != NULL && out_path_size > 0) {
        out_path[0] = '\0';
    }
    if (rows == NULL || max_rows <= 0 || season < KBO_SEASON_YEAR_MIN || season > KBO_SIM_YEAR_MAX) {
        return 0;
    }
    memset(rows, 0, (SIZE_T)max_rows * sizeof(rows[0]));

    char path[MAX_PATH] = {0};
    if (!kbo_fa_salary_snapshot_path(season, path, sizeof(path))) {
        return 0;
    }
    if (out_path != NULL && out_path_size > 0) {
        snprintf(out_path, out_path_size, "%s", path);
    }

    int count = 0;
    if (!kbo_fa_salary_snapshot_sql_load_grade_rows(season, rows, max_rows, &count)) {
        return 0;
    }

    kbo_fa_salary_snapshot_assign_grade_overall_ranks(rows, count);
    kbo_fa_salary_snapshot_assign_grade_team_ranks(rows, count);
    for (int i = 0; i < count; i++) {
        snprintf(
            rows[i].grade,
            sizeof(rows[i].grade),
            "%s",
            kbo_fa_salary_snapshot_grade_for_ranks(
                rows[i].overall_rank,
                rows[i].team_rank,
                rows[i].salary));
    }
    return count;
}

int kbo_fa_salary_snapshot_load_fa_market_grade_rows(
    uint32_t current_year,
    KboFaSalarySnapshotGrade* rows,
    int max_rows,
    char* out_path,
    size_t out_path_size,
    uint32_t* out_grade_season)
{
    if (out_grade_season != NULL) {
        *out_grade_season = 0u;
    }

    int count = kbo_fa_salary_snapshot_load_grade_rows(
        current_year,
        rows,
        max_rows,
        out_path,
        out_path_size);
    if (count > 0) {
        if (out_grade_season != NULL) {
            *out_grade_season = current_year;
        }
        return count;
    }

    if (current_year <= KBO_SEASON_YEAR_MIN) {
        return count;
    }

    uint32_t previous_year = current_year - 1u;
    count = kbo_fa_salary_snapshot_load_grade_rows(
        previous_year,
        rows,
        max_rows,
        out_path,
        out_path_size);
    if (count > 0 && out_grade_season != NULL) {
        *out_grade_season = previous_year;
    }
    return count;
}

const KboFaSalarySnapshotGrade* kbo_find_fa_salary_snapshot_grade(
    const KboFaSalarySnapshotGrade* rows,
    int row_count,
    uint32_t player_id)
{
    if (rows == NULL || row_count <= 0 || player_id == 0u) {
        return NULL;
    }
    for (int i = 0; i < row_count; i++) {
        if (rows[i].player_id == player_id) {
            return &rows[i];
        }
    }
    return NULL;
}
