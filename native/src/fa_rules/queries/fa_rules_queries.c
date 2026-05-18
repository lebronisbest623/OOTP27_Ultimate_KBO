#include "../fa_rules_internal.h"

int kbo_fa_rules_load(KboFaRules* rules)
{
    if (rules == NULL) {
        return 0;
    }
    kbo_fa_rules_init_defaults(rules);

    char path[MAX_PATH] = {0};
    if (!kbo_fa_rules_resolve_existing_path(path, sizeof(path))) {
        return 0;
    }

    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }
    DWORD size = GetFileSize(file, NULL);
    if (size == INVALID_FILE_SIZE || size == 0u || size > KBO_FLAGS_JSON_MAX_BYTES) {
        CloseHandle(file);
        return 0;
    }

    char* buffer = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)size + 1u);
    if (buffer == NULL) {
        CloseHandle(file);
        return 0;
    }

    DWORD read = 0;
    int loaded = 0;
    if (ReadFile(file, buffer, size, &read, NULL)) {
        buffer[read] = '\0';
        kbo_fa_rules_parse_json(rules, buffer, read);
        kbo_fa_rules_copy_text(path, rules->source_path, sizeof(rules->source_path));
        rules->loaded_from_file = 1u;
        loaded = 1;
    }
    CloseHandle(file);
    HeapFree(GetProcessHeap(), 0, buffer);
    return loaded;
}

int kbo_fa_rules_case_is_compensable(const KboFaRules* rules, const char* case_label)
{
    if (rules == NULL || case_label == NULL || case_label[0] == '\0') {
        return 0;
    }
    for (int i = 0; i < rules->compensable_case_count; i++) {
        if (strcmp(rules->compensable_cases[i], case_label) == 0) {
            return 1;
        }
        if (strcmp(case_label, "KBO_FA_CARRYOVER_UNSIGNED") == 0
                && strcmp(rules->compensable_cases[i], "KBO_FA_BY_HISTORY_UNGRADED") == 0) {
            return 1;
        }
    }
    return 0;
}

int kbo_fa_rules_grade_is_compensable(const KboFaRules* rules, const char* grade)
{
    const KboFaRuleGradeConfig* rule = kbo_fa_rules_find_grade(rules, grade);
    return rule != NULL && rule->cash_only_percent > 0u;
}

void kbo_fa_rules_calculate_compensation(
    const KboFaRules* rules,
    const char* grade,
    int32_t previous_salary,
    uint32_t* out_cash_with_player,
    uint32_t* out_cash_only,
    uint32_t* out_protect_count,
    uint8_t* out_requires_player)
{
    if (out_cash_with_player != NULL) { *out_cash_with_player = 0u; }
    if (out_cash_only != NULL) { *out_cash_only = 0u; }
    if (out_protect_count != NULL) { *out_protect_count = 0u; }
    if (out_requires_player != NULL) { *out_requires_player = 0u; }
    if (previous_salary <= 0) {
        return;
    }

    const KboFaRuleGradeConfig* rule = kbo_fa_rules_find_grade(rules, grade);
    if (rule == NULL) {
        return;
    }

    uint64_t salary = (uint64_t)(uint32_t)previous_salary;
    uint64_t amount = 0u;
    if (rule->cash_with_player_percent > 0u) {
        amount = (salary * (uint64_t)rule->cash_with_player_percent) / 100u;
        if (out_cash_with_player != NULL) {
            *out_cash_with_player = amount > UINT32_MAX ? UINT32_MAX : (uint32_t)amount;
        }
    }
    if (rule->cash_only_percent > 0u) {
        amount = (salary * (uint64_t)rule->cash_only_percent) / 100u;
        if (out_cash_only != NULL) {
            *out_cash_only = amount > UINT32_MAX ? UINT32_MAX : (uint32_t)amount;
        }
    }
    if (out_protect_count != NULL) {
        *out_protect_count = rule->protect_count;
    }
    if (out_requires_player != NULL) {
        *out_requires_player = rule->requires_player_compensation;
    }
}

