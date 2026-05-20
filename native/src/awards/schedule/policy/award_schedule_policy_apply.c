#include "../award_schedule_probe_module.h"

static OotpCreateLeagueEventFn g_kbo_award_schedule_create_league_event_original = NULL;
static volatile LONG g_kbo_award_schedule_create_event_missing_original_logs = 0;

static uint32_t kbo_award_schedule_rule_date(const KboAwardScheduleRule* rule, uint32_t year)
{
    if (rule == NULL || year < 1982u || year > 2400u) {
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

static int kbo_award_schedule_event_might_match_policy(uint32_t event_type, const char* title)
{
    if (event_type == OOTP27_EVENT_TYPE_CUSTOM_EVENT) {
        return 0;
    }
    return kbo_award_schedule_event_type_is_native_award_candidate(event_type)
        || kbo_award_schedule_title_is_award_candidate(title);
}

static int kbo_award_schedule_event_is_legacy_custom_placeholder(
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

static int kbo_award_schedule_write_u8(uint8_t* slot, uint8_t value)
{
    if (slot == NULL || !memory_range_readable(slot, sizeof(*slot))) {
        return 0;
    }
    DWORD old_protect = 0;
    if (!VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE, &old_protect)) {
        return 0;
    }
    *slot = value;
    DWORD ignored = 0;
    VirtualProtect(slot, sizeof(*slot), old_protect, &ignored);
    return 1;
}

static int kbo_award_schedule_write_u16(uint8_t* slot, uint16_t value)
{
    if (slot == NULL || !memory_range_readable(slot, sizeof(value))) {
        return 0;
    }
    DWORD old_protect = 0;
    if (!VirtualProtect(slot, sizeof(value), PAGE_READWRITE, &old_protect)) {
        return 0;
    }
    *(uint16_t*)slot = value;
    DWORD ignored = 0;
    VirtualProtect(slot, sizeof(value), old_protect, &ignored);
    return 1;
}

static int kbo_award_schedule_write_date(uint8_t* event, uint32_t month, uint32_t day)
{
    if (event == NULL || month < 1u || month > 12u || day < 1u || day > 31u) {
        return 0;
    }
    int wrote_month = kbo_award_schedule_write_u8(
        event + OOTP27_LEAGUE_EVENT_MONTH_OFFSET,
        (uint8_t)month);
    int wrote_day = kbo_award_schedule_write_u8(
        event + OOTP27_LEAGUE_EVENT_DAY_OFFSET,
        (uint8_t)day);
    return wrote_month && wrote_day;
}

static int kbo_award_schedule_write_year(uint8_t* event, uint32_t year)
{
    if (event == NULL || year < 1982u || year > 2400u) {
        return 0;
    }
    return kbo_award_schedule_write_u16(
        event + OOTP27_LEAGUE_EVENT_YEAR_OFFSET,
        (uint16_t)year);
}

static int kbo_award_schedule_set_deleted(uint8_t* event, uint8_t deleted)
{
    if (event == NULL) {
        return 0;
    }
    if (event[OOTP27_LEAGUE_EVENT_DELETED_OFFSET] == deleted) {
        return 0;
    }
    return kbo_award_schedule_write_u8(event + OOTP27_LEAGUE_EVENT_DELETED_OFFSET, deleted);
}

static int kbo_award_schedule_set_event_over(uint8_t* event, uint16_t value)
{
    if (event == NULL
            || !memory_range_readable(event, OOTP27_LEAGUE_EVENT_EVENT_OVER_OFFSET + sizeof(uint16_t))) {
        return 0;
    }
    if (*(uint16_t*)(event + OOTP27_LEAGUE_EVENT_EVENT_OVER_OFFSET) == value) {
        return 0;
    }
    return kbo_award_schedule_write_u16(
        event + OOTP27_LEAGUE_EVENT_EVENT_OVER_OFFSET,
        value);
}

static uint32_t kbo_award_schedule_event_date(uint8_t* event)
{
    if (event == NULL
            || !memory_range_readable(event, OOTP27_LEAGUE_EVENT_MONTH_OFFSET + sizeof(uint8_t))) {
        return 0u;
    }
    uint32_t year = *(uint16_t*)(event + OOTP27_LEAGUE_EVENT_YEAR_OFFSET);
    uint32_t month = event[OOTP27_LEAGUE_EVENT_MONTH_OFFSET];
    uint32_t day = event[OOTP27_LEAGUE_EVENT_DAY_OFFSET];
    if (year < 1982u || year > 2400u || month < 1u || month > 12u || day < 1u || day > 31u) {
        return 0u;
    }
    return year * 10000u + month * 100u + day;
}

static const KboAwardScheduleRule* kbo_award_schedule_find_rule(
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

static int kbo_award_schedule_close_native_event(
    uint8_t* event,
    const KboAwardScheduleRule* rule,
    const char* title,
    uint32_t event_type,
    uint32_t league_id,
    uint32_t old_date,
    uint32_t target_date,
    uint32_t current_date,
    const char* path,
    const char* source,
    const char* reason)
{
    if (event == NULL || rule == NULL) {
        return 0;
    }
    uint16_t old_over = *(uint16_t*)(event + OOTP27_LEAGUE_EVENT_EVENT_OVER_OFFSET);
    int changed = kbo_award_schedule_set_event_over(event, 1u);
    if (changed) {
        kbo_log_runtimef(
            "KBO award schedule native event closed rule=%s title=%s type=%u league_id=%u event=%p old_date=%u target=%u current=%u old_over=%u reason=%s source=%s policy=%s",
            rule->id,
            title != NULL ? title : "",
            event_type,
            league_id,
            event,
            old_date,
            target_date,
            current_date,
            (uint32_t)old_over,
            reason != NULL ? reason : "",
            source != NULL ? source : "",
            path != NULL ? path : "");
    }
    return changed;
}

static int kbo_award_schedule_apply_rule_to_native_event(
    uint8_t* event,
    const KboAwardScheduleRule* rule,
    const char* title,
    uint32_t event_type,
    uint32_t league_id,
    uint32_t current_date,
    const char* path,
    const char* source)
{
    if (event == NULL || rule == NULL) {
        return 0;
    }

    uint32_t old_date = kbo_award_schedule_event_date(event);
    if (old_date == 0u) {
        return 0;
    }

    uint32_t event_year = old_date / 10000u;
    uint32_t old_month = (old_date / 100u) % 100u;
    uint32_t old_day = old_date % 100u;
    uint32_t target = kbo_award_schedule_rule_date(rule, event_year);
    if (target == 0u) {
        return 0;
    }

    if (current_date != 0u && current_date > target) {
        return kbo_award_schedule_close_native_event(
            event,
            rule,
            title,
            event_type,
            league_id,
            old_date,
            target,
            current_date,
            path,
            source,
            "target_date_already_passed");
    }
    if (current_date != 0u && old_date < current_date && old_date != target) {
        return kbo_award_schedule_close_native_event(
            event,
            rule,
            title,
            event_type,
            league_id,
            old_date,
            target,
            current_date,
            path,
            source,
            "original_date_already_passed");
    }

    uint32_t target_month = (target / 100u) % 100u;
    uint32_t target_day = target % 100u;
    if (old_month == target_month && old_day == target_day) {
        return 0;
    }

    if (!kbo_award_schedule_write_date(event, target_month, target_day)) {
        return 0;
    }
    kbo_log_runtimef(
        "KBO award schedule native event moved rule=%s title=%s type=%u league_id=%u event=%p date=%04u-%02u-%02u->%04u-%02u-%02u current=%u source=%s policy=%s",
        rule->id,
        title != NULL ? title : "",
        event_type,
        league_id,
        event,
        event_year,
        old_month,
        old_day,
        event_year,
        target_month,
        target_day,
        current_date,
        source != NULL ? source : "",
        path != NULL ? path : "");
    return 1;
}

static uint32_t kbo_award_schedule_retired_placeholder_year(uint32_t current_date)
{
    uint32_t current_year = current_date / 10000u;
    return current_year > 1982u && current_year <= 2400u ? current_year - 1u : 1982u;
}

static int kbo_award_schedule_retire_legacy_custom_placeholder(uint8_t* event, uint32_t current_date)
{
    if (event == NULL) {
        return 0;
    }
    uint32_t target_year = kbo_award_schedule_retired_placeholder_year(current_date);
    uint32_t old_year = *(uint16_t*)(event + OOTP27_LEAGUE_EVENT_YEAR_OFFSET);
    uint32_t old_month = event[OOTP27_LEAGUE_EVENT_MONTH_OFFSET];
    uint32_t old_day = event[OOTP27_LEAGUE_EVENT_DAY_OFFSET];

    int changed = 0;
    if (old_year != target_year) {
        changed |= kbo_award_schedule_write_year(event, target_year);
    }
    if (old_month != 1u || old_day != 1u) {
        changed |= kbo_award_schedule_write_date(event, 1u, 1u);
    }
    changed |= kbo_award_schedule_set_event_over(event, 1u);
    /* event_over spans the deleted byte in this layout, so deleted must be restored last. */
    changed |= kbo_award_schedule_set_deleted(event, 1u);
    return changed;
}

static int kbo_award_schedule_apply_policy(
    uint32_t league_id,
    uint32_t current_date,
    const KboAwardSchedulePolicy* policy,
    const char* path)
{
    uintptr_t event_manager = get_kbo_league_event_manager();
    if (event_manager == 0
            || !memory_range_readable((void*)event_manager, OOTP27_EVENT_MANAGER_EVENT_COUNT_OFFSET + sizeof(int32_t))) {
        return 0;
    }

    uintptr_t event_vector = *(uintptr_t*)(event_manager + OOTP27_EVENT_MANAGER_EVENT_VECTOR_OFFSET);
    int32_t event_count = *(int32_t*)(event_manager + OOTP27_EVENT_MANAGER_EVENT_COUNT_OFFSET);
    if (event_vector == 0 || event_count <= 0 || event_count > 20000
            || !memory_range_readable((void*)event_vector, (SIZE_T)event_count * sizeof(uintptr_t))) {
        return 0;
    }

    int changed = 0;
    for (int32_t i = 0; i < event_count; i++) {
        uintptr_t event_ptr = *(uintptr_t*)(event_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (event_ptr == 0 || !memory_range_readable((void*)event_ptr, 0x48)) {
            continue;
        }
        uint8_t* event = (uint8_t*)event_ptr;
        if (*(uint32_t*)(event + OOTP27_LEAGUE_EVENT_LEAGUE_ID_OFFSET) != league_id) {
            continue;
        }
        if (event[OOTP27_LEAGUE_EVENT_DELETED_OFFSET] != 0) {
            continue;
        }

        char title[160] = {0};
        if (!copy_ootp_string_object_raw_text(event, OOTP27_LEAGUE_EVENT_NAME_STRING_OFFSET, title, sizeof(title))) {
            continue;
        }

        uint32_t event_type = *(uint16_t*)(event + OOTP27_LEAGUE_EVENT_TYPE_OFFSET);
        if (event_type == OOTP27_EVENT_TYPE_CUSTOM_EVENT) {
            if (kbo_award_schedule_event_is_legacy_custom_placeholder(event_type, title, policy)) {
                uint32_t old_year = *(uint16_t*)(event + OOTP27_LEAGUE_EVENT_YEAR_OFFSET);
                uint32_t old_month = event[OOTP27_LEAGUE_EVENT_MONTH_OFFSET];
                uint32_t old_day = event[OOTP27_LEAGUE_EVENT_DAY_OFFSET];
                int retired = kbo_award_schedule_retire_legacy_custom_placeholder(event, current_date);
                if (retired) {
                    changed++;
                    kbo_log_runtimef(
                        "KBO award schedule legacy custom event retired title=%s league_id=%u event=%p date=%04u-%02u-%02u->%04u-01-01 policy=%s",
                        title,
                        league_id,
                        (void*)event_ptr,
                        old_year,
                        old_month,
                        old_day,
                        kbo_award_schedule_retired_placeholder_year(current_date),
                        path != NULL ? path : "");
                }
            }
            continue;
        }

        if (*(uint16_t*)(event + OOTP27_LEAGUE_EVENT_EVENT_OVER_OFFSET) != 0) {
            continue;
        }

        if (!kbo_award_schedule_event_might_match_policy(event_type, title)) {
            continue;
        }

        const KboAwardScheduleRule* rule = kbo_award_schedule_find_rule(policy, event_type, title);
        if (rule != NULL
                && kbo_award_schedule_apply_rule_to_native_event(
                    event,
                    rule,
                    title,
                    event_type,
                    league_id,
                    current_date,
                    path,
                    "date_tick_apply")) {
            changed++;
        }
    }
    return changed;
}

void kbo_award_schedule_set_create_league_event_original(OotpCreateLeagueEventFn original_func)
{
    g_kbo_award_schedule_create_league_event_original = original_func;
}

int kbo_award_schedule_adjust_created_event(
    void* event_ptr,
    uint32_t event_type_hint,
    uint32_t league_id_hint,
    const char* title_hint,
    uint32_t current_date,
    const char* source)
{
    if (event_ptr == NULL || !memory_range_readable(event_ptr, 0x48)) {
        return 0;
    }

    uint8_t* event = (uint8_t*)event_ptr;
    if (event[OOTP27_LEAGUE_EVENT_DELETED_OFFSET] != 0) {
        return 0;
    }

    uint32_t event_type = *(uint16_t*)(event + OOTP27_LEAGUE_EVENT_TYPE_OFFSET);
    if (event_type == 0u) {
        event_type = event_type_hint;
    }
    if (event_type == OOTP27_EVENT_TYPE_CUSTOM_EVENT) {
        return 0;
    }

    uint32_t league_id = *(uint32_t*)(event + OOTP27_LEAGUE_EVENT_LEAGUE_ID_OFFSET);
    if (league_id == 0u) {
        league_id = league_id_hint;
    }
    uint32_t kbo_league_id = kbo_resolve_kbo_league_id();
    if (kbo_league_id != 0u && league_id != kbo_league_id) {
        return 0;
    }

    char title[160] = {0};
    if (!copy_ootp_string_object_raw_text(event, OOTP27_LEAGUE_EVENT_NAME_STRING_OFFSET, title, sizeof(title))
            && title_hint != NULL) {
        snprintf(title, sizeof(title), "%s", title_hint);
    }
    if (title[0] == '\0' && title_hint != NULL) {
        snprintf(title, sizeof(title), "%s", title_hint);
    }
    if (!kbo_award_schedule_event_might_match_policy(event_type, title)) {
        return 0;
    }

    char* json = NULL;
    DWORD json_size = 0u;
    char path[MAX_PATH] = {0};
    if (!kbo_award_schedule_load_text(&json, &json_size, path, sizeof(path))) {
        static volatile LONG s_missing_log_count = 0;
        if (InterlockedIncrement(&s_missing_log_count) <= 10) {
            kbo_log_runtimef(
                "KBO award schedule create-event policy unavailable source=%s file=%s",
                source != NULL ? source : "",
                KBO_AWARD_SCHEDULE_POLICY_FILE);
        }
        return 0;
    }

    int changed = 0;
    KboAwardSchedulePolicy policy;
    if (kbo_award_schedule_parse_policy(json, &policy)) {
        const KboAwardScheduleRule* rule = kbo_award_schedule_find_rule(&policy, event_type, title);
        if (rule != NULL) {
            changed = kbo_award_schedule_apply_rule_to_native_event(
                event,
                rule,
                title,
                event_type,
                league_id,
                current_date,
                path,
                source);
        }
    } else {
        kbo_log_runtimef(
            "KBO award schedule create-event policy parse failed source=%s path=%s size=%lu",
            source != NULL ? source : "",
            path,
            (unsigned long)json_size);
    }
    free(json);
    return changed;
}

__declspec(noinline) void* ootp_kbo_award_schedule_create_league_event_wrapper(
    void* event_manager,
    void* date,
    uint32_t event_type,
    uint32_t league_id,
    const char* title,
    uint16_t aux_id)
{
    OotpCreateLeagueEventFn original = g_kbo_award_schedule_create_league_event_original;
    if (original == NULL) {
        if (InterlockedIncrement(&g_kbo_award_schedule_create_event_missing_original_logs) <= 10) {
            kbo_log_runtime_line("KBO award schedule create-event hook skipped reason=missing_original");
        }
        return NULL;
    }

    void* event = original(event_manager, date, event_type, league_id, title, aux_id);
    if (event == NULL) {
        return NULL;
    }

    uint32_t current_date = 0u;
    if (!kbo_get_current_yyyymmdd(&current_date)) {
        (void)kbo_current_date_tick_latest_published_date(&current_date);
    }
    (void)kbo_award_schedule_adjust_created_event(
        event,
        event_type,
        league_id,
        title,
        current_date,
        "create_event_hook");
    return event;
}

void kbo_award_schedule_apply_once(uint32_t league_id, uint32_t current_date, int ensure_events)
{
    (void)ensure_events;
    char* json = NULL;
    DWORD json_size = 0u;
    char path[MAX_PATH] = {0};
    if (!kbo_award_schedule_load_text(&json, &json_size, path, sizeof(path))) {
        static volatile LONG s_missing_log_count = 0;
        if (InterlockedIncrement(&s_missing_log_count) <= 10) {
            kbo_log_runtimef("KBO award schedule policy unavailable file=%s", KBO_AWARD_SCHEDULE_POLICY_FILE);
        }
        return;
    }

    KboAwardSchedulePolicy policy;
    if (kbo_award_schedule_parse_policy(json, &policy)) {
        int changed = kbo_award_schedule_apply_policy(league_id, current_date, &policy, path);
        if (changed > 0) {
            kbo_log_runtimef("KBO award schedule policy applied changed=%d rules=%u path=%s", changed, policy.rule_count, path);
        }
    } else {
        kbo_log_runtimef("KBO award schedule policy parse failed path=%s size=%lu", path, (unsigned long)json_size);
    }
    free(json);
}
