#include "../fa_rules_internal.h"

void kbo_fa_rules_copy_text(const char* value, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';
    if (value == NULL) {
        return;
    }
    size_t len = strlen(value);
    if (len >= out_size) {
        len = out_size - 1u;
    }
    if (len > 0u) {
        memcpy(out, value, len);
    }
    out[len] = '\0';
}

void kbo_fa_rules_add_case(KboFaRules* rules, const char* case_label)
{
    if (rules == NULL || case_label == NULL || case_label[0] == '\0'
            || rules->compensable_case_count >= KBO_FA_RULES_MAX_CASES) {
        return;
    }
    for (int i = 0; i < rules->compensable_case_count; i++) {
        if (strcmp(rules->compensable_cases[i], case_label) == 0) {
            return;
        }
    }
    kbo_fa_rules_copy_text(
        case_label,
        rules->compensable_cases[rules->compensable_case_count],
        sizeof(rules->compensable_cases[0]));
    rules->compensable_case_count++;
}

void kbo_fa_rules_init_defaults(KboFaRules* rules)
{
    if (rules == NULL) {
        return;
    }
    memset(rules, 0, sizeof(*rules));
    rules->version = 1u;
    rules->exclude_foreign_players = 1u;
    rules->protected_list_due_days = 3u;
    rules->age_grade_override_enabled = 1u;
    rules->age_grade_min_age = 35u;
    kbo_fa_rules_copy_text("C", rules->age_grade, sizeof(rules->age_grade));
    kbo_fa_rules_copy_text("built-in KBO defaults", rules->source_path, sizeof(rules->source_path));

    KboFaRuleGradeConfig* grade_a = kbo_fa_rules_ensure_grade(rules, "A");
    if (grade_a != NULL) {
        grade_a->protect_count = 20u;
        grade_a->cash_with_player_percent = 200u;
        grade_a->cash_only_percent = 300u;
        grade_a->requires_player_compensation = 1u;
    }
    KboFaRuleGradeConfig* grade_b = kbo_fa_rules_ensure_grade(rules, "B");
    if (grade_b != NULL) {
        grade_b->protect_count = 25u;
        grade_b->cash_with_player_percent = 100u;
        grade_b->cash_only_percent = 200u;
        grade_b->requires_player_compensation = 1u;
    }
    KboFaRuleGradeConfig* grade_c = kbo_fa_rules_ensure_grade(rules, "C");
    if (grade_c != NULL) {
        grade_c->cash_only_percent = 150u;
    }

    kbo_fa_rules_add_case(rules, "KBO_FA_APPROVED");
    kbo_fa_rules_add_case(rules, "KBO_FA_ELIGIBLE_NOT_APPROVED");
    kbo_fa_rules_add_case(rules, "KBO_FA_DEFERRED");
    kbo_fa_rules_add_case(rules, "KBO_FA_BY_HISTORY_UNGRADED");
    kbo_fa_rules_add_case(rules, "KBO_FA_CARRYOVER_UNSIGNED");
}

int kbo_fa_rules_find_json_value_span(
    const char* start,
    const char* end,
    const char* key,
    const char** out_start,
    const char** out_end)
{
    if (start == NULL || end == NULL || key == NULL || out_start == NULL || out_end == NULL) {
        return 0;
    }

    const char* p = start;
    while (p < end) {
        if (*p != '"') {
            p++;
            continue;
        }

        const char* key_start = p + 1;
        const char* key_stop = kbo_json_find_string_end(p, end);
        if (key_stop == NULL) {
            return 0;
        }

        int matched = kbo_json_string_equals_key(key_start, key_stop, key);
        p = key_stop + 1;
        if (!matched) {
            continue;
        }

        const char* colon = kbo_json_skip_ws(p, end);
        if (colon >= end || *colon != ':') {
            continue;
        }
        const char* value_start = kbo_json_skip_ws(colon + 1, end);
        const char* value_end = kbo_fa_rules_json_value_end(value_start, end);
        if (value_start >= end || value_end < value_start) {
            return 0;
        }
        *out_start = value_start;
        *out_end = value_end;
        return 1;
    }
    return 0;
}

int kbo_fa_rules_read_bool(const char* start, const char* end, const char* key, uint8_t* out_value)
{
    const char* value_start = NULL;
    const char* value_end = NULL;
    int value = 0;
    if (out_value == NULL
            || !kbo_fa_rules_find_json_value_span(start, end, key, &value_start, &value_end)
            || !kbo_json_bool_value_at(value_start, value_end, &value)) {
        return 0;
    }
    *out_value = value ? 1u : 0u;
    return 1;
}

int kbo_fa_rules_read_u32(const char* start, const char* end, const char* key, uint32_t* out_value)
{
    const char* value_start = NULL;
    const char* value_end = NULL;
    int value = 0;
    if (out_value == NULL
            || !kbo_fa_rules_find_json_value_span(start, end, key, &value_start, &value_end)
            || !kbo_json_int_value_at(value_start, value_end, &value)
            || value < 0) {
        return 0;
    }
    *out_value = (uint32_t)value;
    return 1;
}

int kbo_fa_rules_copy_json_string_value(
    const char* value_start,
    const char* value_end,
    char* out,
    size_t out_size)
{
    if (value_start == NULL || value_end == NULL || out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';
    value_start = kbo_json_skip_ws(value_start, value_end);
    if (value_start >= value_end || *value_start != '"') {
        return 0;
    }
    const char* stop = kbo_json_find_string_end(value_start, value_end);
    if (stop == NULL) {
        return 0;
    }

    size_t written = 0;
    const char* p = value_start + 1;
    while (p < stop && written + 1u < out_size) {
        if (*p == '\\' && p + 1 < stop) {
            p++;
        }
        out[written++] = *p++;
    }
    out[written] = '\0';
    return out[0] != '\0';
}

int kbo_fa_rules_read_string(
    const char* start,
    const char* end,
    const char* key,
    char* out,
    size_t out_size)
{
    const char* value_start = NULL;
    const char* value_end = NULL;
    return kbo_fa_rules_find_json_value_span(start, end, key, &value_start, &value_end)
        && kbo_fa_rules_copy_json_string_value(value_start, value_end, out, out_size);
}

void kbo_fa_rules_parse_cases(KboFaRules* rules, const char* start, const char* end)
{
    if (rules == NULL || start == NULL || end == NULL) {
        return;
    }
    start = kbo_json_skip_ws(start, end);
    if (start >= end || *start != '[') {
        return;
    }

    char parsed[KBO_FA_RULES_MAX_CASES][48];
    memset(parsed, 0, sizeof(parsed));
    int count = 0;
    const char* p = start + 1;
    while (p < end && count < KBO_FA_RULES_MAX_CASES) {
        p = kbo_json_skip_ws(p, end);
        if (p >= end || *p == ']') {
            break;
        }
        if (*p == '"') {
            const char* stop = kbo_json_find_string_end(p, end);
            if (stop == NULL) {
                break;
            }
            if (kbo_fa_rules_copy_json_string_value(p, stop + 1, parsed[count], sizeof(parsed[count]))) {
                count++;
            }
            p = stop + 1;
        } else {
            p = kbo_fa_rules_json_value_end(p, end);
        }
        p = kbo_json_skip_ws(p, end);
        if (p < end && *p == ',') {
            p++;
        }
    }

    if (count > 0) {
        memset(rules->compensable_cases, 0, sizeof(rules->compensable_cases));
        rules->compensable_case_count = 0;
        for (int i = 0; i < count; i++) {
            kbo_fa_rules_add_case(rules, parsed[i]);
        }
    }
}

void kbo_fa_rules_parse_grade_object(
    KboFaRules* rules,
    const char* grade,
    const char* start,
    const char* end)
{
    KboFaRuleGradeConfig* rule = kbo_fa_rules_ensure_grade(rules, grade);
    if (rule == NULL || start == NULL || end == NULL) {
        return;
    }

    uint32_t u32_value = 0u;
    uint8_t bool_value = 0u;
    if (kbo_fa_rules_read_u32(start, end, "protectedCount", &u32_value)
            || kbo_fa_rules_read_u32(start, end, "protectCount", &u32_value)) {
        rule->protect_count = u32_value;
    }
    if (kbo_fa_rules_read_u32(start, end, "cashWithPlayerPercent", &u32_value)
            || kbo_fa_rules_read_u32(start, end, "cashPlusPlayerPercent", &u32_value)) {
        rule->cash_with_player_percent = u32_value;
    }
    if (kbo_fa_rules_read_u32(start, end, "cashOnlyPercent", &u32_value)) {
        rule->cash_only_percent = u32_value;
    }
    if (kbo_fa_rules_read_bool(start, end, "requiresPlayerCompensation", &bool_value)) {
        rule->requires_player_compensation = bool_value;
    } else {
        rule->requires_player_compensation =
            (rule->protect_count > 0u && rule->cash_with_player_percent > 0u) ? 1u : 0u;
    }
    if (kbo_fa_rules_read_bool(start, end, "enabled", &bool_value)) {
        rule->enabled = bool_value;
    }
}

void kbo_fa_rules_parse_json(KboFaRules* rules, const char* json, DWORD json_size)
{
    if (rules == NULL || json == NULL || json_size == 0u) {
        return;
    }
    const char* root_start = json;
    const char* root_end = json + json_size;
    const char* fa_start = root_start;
    const char* fa_end = root_end;
    const char* value_start = NULL;
    const char* value_end = NULL;

    uint32_t u32_value = 0u;
    uint8_t bool_value = 0u;
    if (kbo_fa_rules_read_u32(root_start, root_end, "version", &u32_value)) {
        rules->version = u32_value;
    }
    if (kbo_fa_rules_find_json_value_span(root_start, root_end, "faCompensation", &value_start, &value_end)) {
        fa_start = value_start;
        fa_end = value_end;
    }

    if (kbo_fa_rules_read_bool(fa_start, fa_end, "excludeForeignPlayers", &bool_value)) {
        rules->exclude_foreign_players = bool_value;
    }
    if (kbo_fa_rules_read_u32(fa_start, fa_end, "protectedListDueDays", &u32_value)
            && u32_value <= 30u) {
        rules->protected_list_due_days = u32_value;
    }

    if (kbo_fa_rules_find_json_value_span(fa_start, fa_end, "ageGradeOverride", &value_start, &value_end)) {
        if (kbo_fa_rules_read_bool(value_start, value_end, "enabled", &bool_value)) {
            rules->age_grade_override_enabled = bool_value;
        }
        if (kbo_fa_rules_read_u32(value_start, value_end, "minAge", &u32_value)) {
            rules->age_grade_min_age = u32_value;
        }
        kbo_fa_rules_read_string(value_start, value_end, "grade", rules->age_grade, sizeof(rules->age_grade));
    }

    if (kbo_fa_rules_find_json_value_span(fa_start, fa_end, "compensableCases", &value_start, &value_end)) {
        kbo_fa_rules_parse_cases(rules, value_start, value_end);
    }

    if (kbo_fa_rules_find_json_value_span(fa_start, fa_end, "grades", &value_start, &value_end)) {
        const char* grade_start = NULL;
        const char* grade_end = NULL;
        if (kbo_fa_rules_find_json_value_span(value_start, value_end, "A", &grade_start, &grade_end)) {
            kbo_fa_rules_parse_grade_object(rules, "A", grade_start, grade_end);
        }
        if (kbo_fa_rules_find_json_value_span(value_start, value_end, "B", &grade_start, &grade_end)) {
            kbo_fa_rules_parse_grade_object(rules, "B", grade_start, grade_end);
        }
        if (kbo_fa_rules_find_json_value_span(value_start, value_end, "C", &grade_start, &grade_end)) {
            kbo_fa_rules_parse_grade_object(rules, "C", grade_start, grade_end);
        }
    }
}

