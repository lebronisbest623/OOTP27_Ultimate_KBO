#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>

#include "cbt_exceptions.h"
#include "../../core/logging/core_log.h"
#include "../../fa_salary_snapshot/grading/salary_snapshot_grade_rows.h"

#define KBO_CBT_EXCEPTION_AUTO_TEAM_MAX 64

int kbo_cbt_exception_auto_designate_missing(uint32_t season, const char* source)
{
    if (season < 1982u || season > 2200u) {
        return 0;
    }

    KboFaSalarySnapshotGrade* grades = (KboFaSalarySnapshotGrade*)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        (SIZE_T)KBO_FA_SALARY_SNAPSHOT_GRADE_MAX * sizeof(KboFaSalarySnapshotGrade));
    if (grades == NULL) {
        kbo_log_runtimef("KBO CBT exception auto skipped season=%u source=%s reason=grade_alloc_failed", season, source != NULL ? source : "");
        return 0;
    }

    int grade_count = kbo_fa_salary_snapshot_load_grade_rows(
        season,
        grades,
        KBO_FA_SALARY_SNAPSHOT_GRADE_MAX,
        NULL,
        0);
    if (grade_count <= 0) {
        HeapFree(GetProcessHeap(), 0, grades);
        kbo_log_runtimef("KBO CBT exception auto skipped season=%u source=%s reason=no_salary_snapshot", season, source != NULL ? source : "");
        return 0;
    }

    KboCbtExceptionDesignation existing[KBO_CBT_EXCEPTION_MAX];
    int existing_count = kbo_cbt_exception_load_designations(existing, KBO_CBT_EXCEPTION_MAX);

    uint32_t team_ids[KBO_CBT_EXCEPTION_AUTO_TEAM_MAX] = {0};
    int32_t best_salary[KBO_CBT_EXCEPTION_AUTO_TEAM_MAX] = {0};
    int best_grade_index[KBO_CBT_EXCEPTION_AUTO_TEAM_MAX];
    int skip_log_count = 0;
    int team_count = 0;
    for (int i = 0; i < KBO_CBT_EXCEPTION_AUTO_TEAM_MAX; i++) {
        best_grade_index[i] = -1;
    }

    for (int i = 0; i < grade_count; i++) {
        KboFaSalarySnapshotGrade* grade = &grades[i];
        if (grade->ranking_team_id == 0u
                || grade->foreign_flag != 0u
                || grade->salary <= 0
                || grade->player_key[0] == '\0') {
            continue;
        }
        if (kbo_cbt_exception_find_designation(existing, existing_count, season, grade->ranking_team_id, grade->player_key) >= 0) {
            continue;
        }

        int team_slot = -1;
        for (int t = 0; t < team_count; t++) {
            if (team_ids[t] == grade->ranking_team_id) {
                team_slot = t;
                break;
            }
        }
        if (team_slot < 0) {
            int has_existing_for_team = 0;
            for (int e = 0; e < existing_count; e++) {
                if (existing[e].season == season && existing[e].team_id == grade->ranking_team_id) {
                    has_existing_for_team = 1;
                    break;
                }
            }
            if (has_existing_for_team || team_count >= KBO_CBT_EXCEPTION_AUTO_TEAM_MAX) {
                continue;
            }
            team_slot = team_count++;
            team_ids[team_slot] = grade->ranking_team_id;
        }

        int season_count = 0;
        if (!kbo_cbt_exception_player_eligible(grade->ranking_team_id, grade->player_key, &season_count)) {
            if (skip_log_count < 64) {
                skip_log_count++;
                kbo_log_runtimef(
                    "KBO CBT exception auto candidate skipped season=%u team=%u player_key=%s player_name=%s salary=%d reason=ineligible team_seasons=%d source=%s",
                    season,
                    grade->ranking_team_id,
                    grade->player_key,
                    grade->player_name,
                    grade->salary,
                    season_count,
                    source != NULL ? source : "");
            }
            continue;
        }
        if (best_grade_index[team_slot] < 0 || grade->salary > best_salary[team_slot]) {
            best_grade_index[team_slot] = i;
            best_salary[team_slot] = grade->salary;
        }
    }

    int created = 0;
    for (int t = 0; t < team_count; t++) {
        int index = best_grade_index[t];
        if (index < 0) {
            continue;
        }
        KboFaSalarySnapshotGrade* grade = &grades[index];
        if (kbo_cbt_exception_save_designation(season, grade->ranking_team_id, grade->player_key, grade->player_name)) {
            created++;
            kbo_log_runtimef(
                "KBO CBT exception auto designated season=%u team=%u player_key=%s player_name=%s salary=%d credit=%d source=%s",
                season,
                grade->ranking_team_id,
                grade->player_key,
                grade->player_name,
                grade->salary,
                grade->salary / 2,
                source != NULL ? source : "");
        }
    }

    HeapFree(GetProcessHeap(), 0, grades);
    kbo_log_runtimef(
        "KBO CBT exception auto complete season=%u created=%d source=%s",
        season,
        created,
        source != NULL ? source : "");
    return created;
}
