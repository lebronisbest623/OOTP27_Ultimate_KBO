#include "../../internal/fa_market_policy_internal.h"

#include <stdio.h>
#include <string.h>

static void kbo_fa_market_reason_append(char* reason, size_t reason_size, const char* text)
{
    if (reason == NULL || reason_size == 0u || text == NULL) {
        return;
    }

    size_t pos = 0u;
    while (pos < reason_size && reason[pos] != '\0') {
        pos++;
    }
    if (pos >= reason_size) {
        reason[reason_size - 1u] = '\0';
        return;
    }

    for (size_t i = 0u; text[i] != '\0' && pos + 1u < reason_size; i++) {
        reason[pos++] = text[i];
    }
    reason[pos] = '\0';
}

static void kbo_fa_market_reason_append_u32(char* reason, size_t reason_size, uint32_t value)
{
    char text[16] = {0};
    snprintf(text, sizeof(text), "%u", value);
    kbo_fa_market_reason_append(reason, reason_size, text);
}

static void kbo_fa_market_reason_append_i32(char* reason, size_t reason_size, int32_t value)
{
    char text[16] = {0};
    snprintf(text, sizeof(text), "%d", value);
    kbo_fa_market_reason_append(reason, reason_size, text);
}

int kbo_fa_market_grade_is_unknown(const char* grade)
{
    return grade == NULL
        || grade[0] == '\0'
        || _stricmp(grade, "UNKNOWN") == 0
        || strcmp(grade, "-") == 0;
}

uint32_t kbo_fa_market_display_grade_sort_rank(const char* grade)
{
    if (grade != NULL && _stricmp(grade, "A") == 0) { return 1u; }
    if (grade != NULL && _stricmp(grade, "B") == 0) { return 2u; }
    if (grade != NULL && _stricmp(grade, "C") == 0) { return 3u; }
    return 4u;
}

static int kbo_fa_market_case_uses_original_team(const char* case_label)
{
    return case_label != NULL
        && (strcmp(case_label, "KBO_FA_APPROVED") == 0
            || strcmp(case_label, "KBO_FA_ELIGIBLE_NOT_APPROVED") == 0
            || strcmp(case_label, "KBO_FA_DEFERRED") == 0
            || strcmp(case_label, "KBO_FA_BY_HISTORY_UNGRADED") == 0
            || strcmp(case_label, "KBO_FA_CARRYOVER_UNSIGNED") == 0);
}

uint32_t kbo_fa_market_display_team_id(const KboFaMarketClassification* row)
{
    if (row == NULL) {
        return 0u;
    }
    if (row->rights_team_id != 0u) {
        return row->rights_team_id;
    }
    if (row->original_team_id != 0u && kbo_fa_market_case_uses_original_team(row->case_label)) {
        return row->original_team_id;
    }
    return row->current_team_id;
}

void kbo_fa_market_format_salary(int32_t salary, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';
    if (salary <= 0) {
        snprintf(out, out_size, "-");
        return;
    }

    char raw[32] = {0};
    char formatted[48] = {0};
    snprintf(raw, sizeof(raw), "%d", salary);
    size_t raw_len = strlen(raw);
    size_t pos = 0;
    for (size_t i = 0; i < raw_len && pos + 1 < sizeof(formatted); i++) {
        if (i > 0 && ((raw_len - i) % 3u) == 0u && pos + 1 < sizeof(formatted)) {
            formatted[pos++] = ',';
        }
        formatted[pos++] = raw[i];
    }
    formatted[pos] = '\0';
    snprintf(out, out_size, "%s", formatted);
}

int kbo_fa_market_apply_age_grade_override(KboFaMarketClassification* row, const KboFaRules* rules)
{
    if (row == NULL
            || rules == NULL
            || !rules->age_grade_override_enabled
            || rules->age_grade_min_age == 0u
            || rules->age_grade[0] == '\0'
            || (rules->exclude_foreign_players && row->foreign_player)
            || row->age < rules->age_grade_min_age
            || !kbo_fa_rules_case_is_compensable(rules, row->case_label)) {
        return 0;
    }

    if (_stricmp(row->grade, rules->age_grade) != 0) {
        snprintf(row->grade, sizeof(row->grade), "%s", rules->age_grade);
        row->fa_grade_auto = 1u;
        kbo_fa_market_reason_append(row->reason, sizeof(row->reason), "; age >= ");
        kbo_fa_market_reason_append_u32(row->reason, sizeof(row->reason), rules->age_grade_min_age);
        kbo_fa_market_reason_append(row->reason, sizeof(row->reason), " FA grade override=");
        kbo_fa_market_reason_append(row->reason, sizeof(row->reason), rules->age_grade);
    }
    snprintf(row->fa_grade_flag, sizeof(row->fa_grade_flag), "AGE_%u_%s", rules->age_grade_min_age, rules->age_grade);
    return 1;
}

void kbo_fa_market_apply_salary_snapshot_grade(
    KboFaMarketClassification* row,
    const KboFaSalarySnapshotGrade* salary_grades,
    int salary_grade_count,
    const KboFaRules* rules)
{
    KboFaRules local_rules;
    if (rules == NULL) {
        kbo_fa_rules_load(&local_rules);
        rules = &local_rules;
    }

    if (row == NULL || !kbo_fa_rules_case_is_compensable(rules, row->case_label)) {
        return;
    }
    if ((rules->exclude_foreign_players && row->foreign_player)
            || strcmp(row->case_label, "FOREIGN_FREE") == 0
            || strcmp(row->case_label, "FOREIGN_RESERVED_RIGHT") == 0) {
        return;
    }

    int age_grade_override = kbo_fa_market_apply_age_grade_override(row, rules);

    const KboFaSalarySnapshotGrade* grade =
        kbo_find_fa_salary_snapshot_grade(salary_grades, salary_grade_count, row->player_id);
    if (grade == NULL) {
        if (!age_grade_override) {
            snprintf(row->fa_grade_flag, sizeof(row->fa_grade_flag), "SNAPSHOT_MISSING");
        }
        return;
    }

    row->fa_grade_salary = grade->salary;
    row->fa_grade_overall_rank = grade->overall_rank;
    row->fa_grade_team_rank = grade->team_rank;
    row->fa_grade_snapshot_team_id = grade->ranking_team_id;
    row->fa_grade_snapshot_date = grade->snapshot_date;
    row->fa_grade_opening_day = grade->opening_day;
    if (!age_grade_override
            && kbo_fa_market_grade_is_unknown(row->grade)
            && !kbo_fa_market_grade_is_unknown(grade->grade)) {
        snprintf(row->grade, sizeof(row->grade), "%s", grade->grade);
        row->fa_grade_auto = 1u;
        kbo_fa_market_reason_append(row->reason, sizeof(row->reason), "; opening-day salary grade=");
        kbo_fa_market_reason_append(row->reason, sizeof(row->reason), row->grade);
        kbo_fa_market_reason_append(row->reason, sizeof(row->reason), " salary=");
        kbo_fa_market_reason_append_i32(row->reason, sizeof(row->reason), row->fa_grade_salary);
        kbo_fa_market_reason_append(row->reason, sizeof(row->reason), " overall_rank=");
        kbo_fa_market_reason_append_u32(row->reason, sizeof(row->reason), row->fa_grade_overall_rank);
        kbo_fa_market_reason_append(row->reason, sizeof(row->reason), " team_rank=");
        kbo_fa_market_reason_append_u32(row->reason, sizeof(row->reason), row->fa_grade_team_rank);
    }

    if (row->original_team_id != 0u
            && grade->ranking_team_id != 0u
            && row->original_team_id != grade->ranking_team_id) {
        row->fa_grade_team_changed_review = 1u;
        snprintf(
            row->fa_grade_flag,
            sizeof(row->fa_grade_flag),
            "TEAM_CHANGED_REVIEW");
    } else if (age_grade_override) {
        snprintf(row->fa_grade_flag, sizeof(row->fa_grade_flag), "AGE_%u_%s", rules->age_grade_min_age, rules->age_grade);
    } else if (row->fa_grade_auto) {
        snprintf(row->fa_grade_flag, sizeof(row->fa_grade_flag), "AUTO");
    } else {
        snprintf(row->fa_grade_flag, sizeof(row->fa_grade_flag), "SNAPSHOT");
    }

}

void kbo_fa_market_apply_carryover_salary_snapshot_grade(
    KboFaMarketClassification* row,
    const KboFaSalarySnapshotGrade* salary_grades,
    int salary_grade_count,
    const KboFaRules* rules,
    uint32_t current_year)
{
    if (row == NULL
            || salary_grades == NULL
            || salary_grade_count <= 0
            || row->fa_filing_season == 0u
            || row->fa_filing_season >= current_year) {
        return;
    }

    KboFaRules local_rules;
    if (rules == NULL) {
        kbo_fa_rules_load(&local_rules);
        rules = &local_rules;
    }
    if (!kbo_fa_rules_case_is_compensable(rules, row->case_label)) {
        return;
    }

    const KboFaSalarySnapshotGrade* grade =
        kbo_find_fa_salary_snapshot_grade(salary_grades, salary_grade_count, row->player_id);
    if (grade == NULL || grade->salary <= 0 || kbo_fa_market_grade_is_unknown(grade->grade)) {
        return;
    }

    row->fa_grade_salary = grade->salary;
    row->fa_grade_overall_rank = grade->overall_rank;
    row->fa_grade_team_rank = grade->team_rank;
    row->fa_grade_snapshot_team_id = grade->ranking_team_id;
    row->fa_grade_snapshot_date = grade->snapshot_date;
    row->fa_grade_opening_day = grade->opening_day;
    snprintf(row->grade, sizeof(row->grade), "%s", grade->grade);
    row->fa_grade_auto = 1u;
    snprintf(row->fa_grade_flag, sizeof(row->fa_grade_flag), "CARRYOVER_SNAPSHOT");
    if (row->original_team_id != 0u
            && grade->ranking_team_id != 0u
            && row->original_team_id != grade->ranking_team_id) {
        row->fa_grade_team_changed_review = 1u;
    } else {
        row->fa_grade_team_changed_review = 0u;
    }
    kbo_fa_market_reason_append(
        row->reason,
        sizeof(row->reason),
        "; carryover FA grade restored from filing-season salary snapshot");
    kbo_fa_market_reason_append(row->reason, sizeof(row->reason), " salary=");
    kbo_fa_market_reason_append_i32(row->reason, sizeof(row->reason), row->fa_grade_salary);
}
