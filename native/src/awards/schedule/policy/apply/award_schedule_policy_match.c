#include "../award_schedule_policy_apply_internal.h"
#include "../../../../core/dates/constants/kbo_date_constants.h"

uint32_t kbo_award_schedule_rule_date(const KboAwardScheduleRule* rule, uint32_t year)
{
    if (rule == NULL || year < KBO_SEASON_YEAR_MIN || year > KBO_POLICY_YEAR_MAX) {
        return 0u;
    }
    uint32_t month = rule->default_month;
    uint32_t day = rule->default_day;
    for (uint32_t i = 0u; i < rule->override_count; i++) {
        if (rule->overrides[i].year == year) {
            month = rule->overrides[i].month;
            day = rule->overrides[i].day;
            break;
        }
    }
    if (month < 1u || month > 12u || day < 1u || day > 31u) {
        return 0u;
    }
    return year * 10000u + month * 100u + day;
}

static int kbo_award_title_matches_rule(const KboAwardScheduleRule* rule, const char* title)
{
    if (rule == NULL || title == NULL || title[0] == '\0') {
        return 0;
    }
    if ((rule->label[0] != '\0' && _stricmp(rule->label, title) == 0)
            || (rule->label_en[0] != '\0' && _stricmp(rule->label_en, title) == 0)
            || (rule->label_ko[0] != '\0' && strcmp(rule->label_ko, title) == 0)) {
        return 1;
    }
    for (uint32_t i = 0u; i < rule->title_count; i++) {
        if (_stricmp(rule->titles[i], title) == 0) {
            return 1;
        }
    }
    return 0;
}

static int kbo_award_event_type_matches_rule(const KboAwardScheduleRule* rule, uint32_t event_type)
{
    if (rule == NULL) {
        return 0;
    }
    for (uint32_t i = 0u; i < rule->event_type_count; i++) {
        if ((uint32_t)rule->event_types[i] == event_type) {
            return 1;
        }
    }
    return 0;
}

static int kbo_award_event_matches_rule(const KboAwardScheduleRule* rule, uint32_t event_type, const char* title)
{
    return kbo_award_event_type_matches_rule(rule, event_type)
        || kbo_award_title_matches_rule(rule, title);
}

static int kbo_award_schedule_ascii_contains_ignore_case(const char* text, const char* needle)
{
    if (text == NULL || needle == NULL || needle[0] == '\0') {
        return 0;
    }
    size_t needle_len = strlen(needle);
    for (size_t i = 0u; text[i] != '\0'; i++) {
        size_t j = 0u;
        while (j < needle_len && text[i + j] != '\0') {
            char a = text[i + j];
            char b = needle[j];
            if (a >= 'A' && a <= 'Z') {
                a = (char)(a - 'A' + 'a');
            }
            if (b >= 'A' && b <= 'Z') {
                b = (char)(b - 'A' + 'a');
            }
            if (a != b) {
                break;
            }
            j++;
        }
        if (j == needle_len) {
            return 1;
        }
    }
    return 0;
}

static int kbo_award_schedule_event_type_is_native_award_candidate(uint32_t event_type)
{
    return event_type == 22u
        || event_type == 23u
        || event_type == 24u
        || event_type == 25u
        || event_type == 31u;
}

static int kbo_award_schedule_title_is_award_candidate(const char* title)
{
    if (title == NULL || title[0] == '\0') {
        return 0;
    }
    if (kbo_award_schedule_ascii_contains_ignore_case(title, "of the Month")
            || kbo_award_schedule_ascii_contains_ignore_case(title, "of the Week")) {
        return 0;
    }
    return kbo_award_schedule_ascii_contains_ignore_case(title, "Award")
        || kbo_award_schedule_ascii_contains_ignore_case(title, "MVP")
        || kbo_award_schedule_ascii_contains_ignore_case(title, "Rookie")
        || kbo_award_schedule_ascii_contains_ignore_case(title, "Golden Glove")
        || kbo_award_schedule_ascii_contains_ignore_case(title, "Gold Glove")
        || kbo_award_schedule_ascii_contains_ignore_case(title, "Choi")
        || kbo_award_schedule_ascii_contains_ignore_case(title, "Pitcher")
        || kbo_award_schedule_ascii_contains_ignore_case(title, "Hitter")
        || kbo_award_schedule_ascii_contains_ignore_case(title, "Title");
}

int kbo_award_schedule_event_might_match_policy(uint32_t event_type, const char* title)
{
    if (event_type == OOTP27_EVENT_TYPE_CUSTOM_EVENT) {
        return 0;
    }
    return kbo_award_schedule_event_type_is_native_award_candidate(event_type)
        || kbo_award_schedule_title_is_award_candidate(title);
}

int kbo_award_schedule_event_is_legacy_custom_placeholder(
    uint32_t event_type,
    const char* title,
    const KboAwardSchedulePolicy* policy)
{
    if (policy == NULL || event_type != OOTP27_EVENT_TYPE_CUSTOM_EVENT || title == NULL || title[0] == '\0') {
        return 0;
    }
    for (uint32_t r = 0u; r < policy->rule_count; r++) {
        const KboAwardScheduleRule* rule = &policy->rules[r];
        if ((rule->label[0] != '\0' && _stricmp(rule->label, title) == 0)
                || (rule->label_en[0] != '\0' && _stricmp(rule->label_en, title) == 0)
                || (rule->label_ko[0] != '\0' && strcmp(rule->label_ko, title) == 0)) {
            return 1;
        }
    }
    return 0;
}

const KboAwardScheduleRule* kbo_award_schedule_find_rule(
    const KboAwardSchedulePolicy* policy,
    uint32_t event_type,
    const char* title)
{
    if (policy == NULL) {
        return NULL;
    }
    for (uint32_t r = 0u; r < policy->rule_count; r++) {
        const KboAwardScheduleRule* rule = &policy->rules[r];
        if (kbo_award_event_matches_rule(rule, event_type, title)) {
            return rule;
        }
    }
    return NULL;
}
