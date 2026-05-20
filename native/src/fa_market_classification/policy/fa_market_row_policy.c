#include "../internal/fa_market_policy_internal.h"
#include "../../core/dates/constants/kbo_date_constants.h"

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

int kbo_fa_market_row_is_undrafted_domestic(const KboFaMarketClassification* row)
{
    if (row == NULL
            || row->nation_id != OOTP27_KBO_KOREA_NATION_ID
            || row->foreign_player
            || row->current_team_id != 0u
            || row->retired_flag != 0u
            || row->draft_eligible != 0u
            || row->generation_context != 0u
            || row->generation_grade == 0u) {
        return 0;
    }

    const KboFaMarketPolicy* policy = kbo_fa_market_policy();
    if (row->draft_league_id == (uint32_t)policy->undrafted_college_league_id
            && row->draft_subtype == (uint8_t)policy->undrafted_college_draft_subtype
            && row->age <= (uint16_t)policy->undrafted_college_age_max) {
        return 1;
    }
    if (row->draft_league_id == (uint32_t)policy->undrafted_high_school_league_id
            && row->age <= (uint16_t)policy->undrafted_high_school_age_max) {
        return 1;
    }
    return 0;
}

static int kbo_fa_market_row_can_be_independent_source(const KboFaMarketClassification* row)
{
    if (row == NULL
            || row->nation_id != OOTP27_KBO_KOREA_NATION_ID
            || row->foreign_player
            || row->current_team_id != 0u
            || row->retired_flag != 0u
            || row->draft_eligible != 0u) {
        return 0;
    }
    return 1;
}

static int kbo_fa_market_classified_team_independent_kind(uint32_t team_id)
{
    return kbo_team_classification_independent_kind_for_team(team_id);
}

int kbo_fa_market_row_independent_source_kind(const KboFaMarketClassification* row)
{
    if (!kbo_fa_market_row_can_be_independent_source(row)) {
        return KBO_TEAM_CLASSIFICATION_INDEPENDENT_KIND_NONE;
    }

    int original_kind = kbo_fa_market_classified_team_independent_kind(row->original_team_id);
    if (original_kind != KBO_TEAM_CLASSIFICATION_INDEPENDENT_KIND_NONE) {
        return original_kind;
    }
    int active_kind = kbo_fa_market_classified_team_independent_kind(row->active_team_id);
    if (active_kind != KBO_TEAM_CLASSIFICATION_INDEPENDENT_KIND_NONE) {
        return active_kind;
    }

    uint32_t original_team_league_id = kbo_fa_market_get_team_league_id(row->original_team_id);
    uint32_t active_team_league_id = kbo_fa_market_get_team_league_id(row->active_team_id);
    const KboFaMarketPolicy* policy = kbo_fa_market_policy();
    uint32_t independent_league_id = (uint32_t)policy->independent_league_id;
    if (independent_league_id != 0u
            && (original_team_league_id == independent_league_id
                || active_team_league_id == independent_league_id
                || row->original_league_id == independent_league_id
                || row->current_league_id == independent_league_id
                || row->draft_league_id == independent_league_id)) {
        return KBO_TEAM_CLASSIFICATION_INDEPENDENT_KIND_LEAGUE;
    }
    return KBO_TEAM_CLASSIFICATION_INDEPENDENT_KIND_NONE;
}

int kbo_fa_market_row_is_independent_league_fa(const KboFaMarketClassification* row)
{
    return kbo_fa_market_row_independent_source_kind(row)
        == KBO_TEAM_CLASSIFICATION_INDEPENDENT_KIND_LEAGUE;
}

uint32_t kbo_fa_market_history_date_u32(const KboFaMarketHistoryCase* history)
{
    if (history == NULL || history->history_date[0] == '\0') {
        return 0u;
    }
    uint32_t date = 0u;
    if (kbo_parse_yyyymmdd(history->history_date, &date)) {
        return date;
    }
    return kbo_fa_filing_parse_u32(history->history_date);
}

static uint32_t kbo_fa_market_filing_season_from_date(uint32_t yyyymmdd)
{
    uint32_t season = yyyymmdd / 10000u;
    uint32_t month = (yyyymmdd / 100u) % 100u;
    uint32_t day = yyyymmdd % 100u;
    if (season < KBO_SEASON_YEAR_MIN || season > KBO_RECORD_YEAR_MAX || month == 0u || month > 12u || day == 0u || day > 31u) {
        return 0u;
    }
    return season;
}

void kbo_fa_market_apply_history_filing_metadata(
    KboFaMarketClassification* row,
    const KboFaMarketHistoryCase* history)
{
    if (row == NULL || history == NULL || !history->found || !history->became_free_agent) {
        return;
    }

    uint32_t filing_date = kbo_fa_market_history_date_u32(history);
    uint32_t filing_season = kbo_fa_market_filing_season_from_date(filing_date);
    if (row->fa_filing_date == 0u && filing_date != 0u) {
        row->fa_filing_date = filing_date;
    }
    if (row->fa_filing_season == 0u && filing_season != 0u) {
        row->fa_filing_season = filing_season;
    }
}

int kbo_fa_market_history_is_carryover_unsigned(
    const KboFaMarketClassification* row,
    const KboFaMarketHistoryCase* history,
    uint32_t today_yyyymmdd)
{
    if (row == NULL || history == NULL || !history->found || !history->became_free_agent) {
        return 0;
    }

    uint32_t filing_season = row->fa_filing_season;
    if (filing_season == 0u) {
        filing_season = kbo_fa_market_filing_season_from_date(kbo_fa_market_history_date_u32(history));
    }
    uint32_t current_year = today_yyyymmdd / 10000u;
    if (filing_season < KBO_SEASON_YEAR_MIN || filing_season > KBO_RECORD_YEAR_MAX || current_year < KBO_SEASON_YEAR_MIN || current_year > KBO_RECORD_YEAR_MAX) {
        return 0;
    }
    return filing_season < current_year;
}

void kbo_fa_market_set_history_reason(
    KboFaMarketClassification* row,
    const KboFaMarketHistoryCase* history,
    const char* prefix)
{
    if (row == NULL) {
        return;
    }
    if (history != NULL && history->history_date[0] != '\0' && history->history_text[0] != '\0') {
        snprintf(
            row->reason,
            sizeof(row->reason),
            "%s history=%s %.96s",
            prefix != NULL ? prefix : "player history match",
            history->history_date,
            history->history_text);
        return;
    }
    snprintf(row->reason, sizeof(row->reason), "%s", prefix != NULL ? prefix : "player history match");
}

int kbo_fa_market_apply_history_case(
    KboFaMarketClassification* row,
    const KboFaMarketHistoryCase* history)
{
    if (row == NULL || history == NULL || !history->found) {
        return 0;
    }

    if (history->undrafted_free_agent) {
        snprintf(row->case_label, sizeof(row->case_label), "DOMESTIC_UNDRAFTED_FREE_AGENT");
        kbo_fa_market_set_history_reason(row, history, "player history says undrafted free agent");
        return 1;
    }

    if (history->released) {
        snprintf(row->case_label, sizeof(row->case_label), "DOMESTIC_RELEASED_NON_FA");
        kbo_fa_market_set_history_reason(row, history, "player history says released, not official FA");
        return 1;
    }

    if (history->became_free_agent) {
        kbo_fa_market_apply_history_filing_metadata(row, history);

        int independent_kind = kbo_fa_market_row_independent_source_kind(row);
        if (independent_kind == KBO_TEAM_CLASSIFICATION_INDEPENDENT_KIND_FUTURES) {
            snprintf(row->case_label, sizeof(row->case_label), "DOMESTIC_INDEPENDENT_FUTURES_FA");
            kbo_fa_market_set_history_reason(row, history, "player history says free agent from independent futures-team context");
            return 1;
        }
        if (independent_kind == KBO_TEAM_CLASSIFICATION_INDEPENDENT_KIND_LEAGUE) {
            snprintf(row->case_label, sizeof(row->case_label), "DOMESTIC_INDEPENDENT_LEAGUE_FA");
            kbo_fa_market_set_history_reason(row, history, "player history says free agent from true independent-league context");
            return 1;
        }

        snprintf(row->case_label, sizeof(row->case_label), "KBO_FA_BY_HISTORY_UNGRADED");
        kbo_fa_market_set_history_reason(row, history, "player history says became a free agent; grade pending salary snapshot or seed");
        return 1;
    }

    return 0;
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

void kbo_fa_market_mark_history_case(KboFaMarketHistoryCase* history)
{
    if (history == NULL || history->history_text[0] == '\0') {
        return;
    }
    history->became_free_agent = strcmp(history->history_text, "[G]Became a free agent.") == 0;
    history->undrafted_free_agent =
        strstr(history->history_text, "Was not drafted and became a free agent") != NULL;
    history->released = strncmp(history->history_text, "[G]Released by ", 15) == 0;
}

