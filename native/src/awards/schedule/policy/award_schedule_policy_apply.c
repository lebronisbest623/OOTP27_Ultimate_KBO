#include "award_schedule_policy_apply_internal.h"
#include "../../../runtime_memory/runtime_memory.h"

static OotpCreateLeagueEventFn g_kbo_award_schedule_create_league_event_original = NULL;
static volatile LONG g_kbo_award_schedule_create_event_missing_original_logs = 0;

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
    if (event_vector == 0 || event_count <= 0 || event_count > KBO_RUNTIME_MAX_EVENT_VECTOR_COUNT
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
    (void)kbo_current_date_tick_latest_published_date(&current_date);
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
