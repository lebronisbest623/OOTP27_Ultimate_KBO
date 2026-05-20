#include "../award_schedule_probe_module.h"
#include "../../../runtime_memory/runtime_memory.h"

static int kbo_award_schedule_title_is_relevant(const char* title)
{
    if (title == NULL || title[0] == '\0') {
        return 0;
    }
    return strstr(title, "Award") != NULL
        || strstr(title, "award") != NULL
        || strstr(title, "MVP") != NULL
        || strstr(title, "Rookie") != NULL
        || strstr(title, "Golden") != NULL
        || strstr(title, "Glove") != NULL
        || strstr(title, "Voting") != NULL
        || strstr(title, "vote") != NULL
        || strstr(title, "\xec\x83\x81") != NULL
        || strstr(title, "\xec\x8b\x9c\xec\x83\x81") != NULL
        || strstr(title, "\xec\x88\x98\xec\x83\x81") != NULL
        || strstr(title, "\xec\x8b\xa0\xec\x9d\xb8") != NULL
        || strstr(title, "\xea\xb3\xa8\xeb\x93\xa0") != NULL
        || strstr(title, "\xea\xb8\x80\xeb\x9f\xac\xeb\xb8\x8c") != NULL
        || strstr(title, "\xed\x88\xac\xed\x91\x9c") != NULL
        || strstr(title, "\xeb\xb0\x9c\xed\x91\x9c") != NULL
        || strstr(title, "\xec\xb5\x9c\xec\x9a\xb0\xec\x88\x98") != NULL;
}

void kbo_award_schedule_log_event_inventory(uint32_t league_id, uint32_t current_date)
{
    uintptr_t event_manager = get_kbo_league_event_manager();
    if (event_manager == 0
            || !memory_range_readable((void*)event_manager, OOTP27_EVENT_MANAGER_EVENT_COUNT_OFFSET + sizeof(int32_t))) {
        kbo_log_runtimef(
            "KBO award schedule event inventory skipped date=%08u league_id=%u reason=event_manager_unreadable",
            current_date,
            league_id);
        return;
    }

    uintptr_t event_vector = *(uintptr_t*)(event_manager + OOTP27_EVENT_MANAGER_EVENT_VECTOR_OFFSET);
    int32_t event_count = *(int32_t*)(event_manager + OOTP27_EVENT_MANAGER_EVENT_COUNT_OFFSET);
    if (event_vector == 0 || event_count <= 0 || event_count > KBO_RUNTIME_MAX_EVENT_VECTOR_COUNT
            || !memory_range_readable((void*)event_vector, (SIZE_T)event_count * sizeof(uintptr_t))) {
        kbo_log_runtimef(
            "KBO award schedule event inventory skipped date=%08u league_id=%u manager=%p count=%d reason=event_vector_unreadable",
            current_date,
            league_id,
            (void*)event_manager,
            event_count);
        return;
    }

    int relevant = 0;
    int voting_type = 0;
    for (int32_t i = 0; i < event_count; i++) {
        uintptr_t event_ptr = *(uintptr_t*)(event_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (event_ptr == 0 || !memory_range_readable((void*)event_ptr, 0x48)) {
            continue;
        }
        uint8_t* event = (uint8_t*)event_ptr;
        if (event[OOTP27_LEAGUE_EVENT_DELETED_OFFSET] != 0
                || *(uint32_t*)(event + OOTP27_LEAGUE_EVENT_LEAGUE_ID_OFFSET) != league_id) {
            continue;
        }

        uint32_t event_type = *(uint16_t*)(event + OOTP27_LEAGUE_EVENT_TYPE_OFFSET);
        char title[160] = {0};
        copy_ootp_string_object_raw_text(event, OOTP27_LEAGUE_EVENT_NAME_STRING_OFFSET, title, sizeof(title));
        int is_voting_type = event_type == KBO_AWARD_VOTING_EVENT_TYPE;
        if (!is_voting_type && !kbo_award_schedule_title_is_relevant(title)) {
            continue;
        }

        uint32_t event_year = *(uint16_t*)(event + OOTP27_LEAGUE_EVENT_YEAR_OFFSET);
        uint32_t event_month = event[OOTP27_LEAGUE_EVENT_MONTH_OFFSET];
        uint32_t event_day = event[OOTP27_LEAGUE_EVENT_DAY_OFFSET];
        uint32_t event_over = *(uint16_t*)(event + OOTP27_LEAGUE_EVENT_EVENT_OVER_OFFSET);
        relevant++;
        if (is_voting_type) {
            voting_type++;
        }
        kbo_log_runtimef(
            "KBO award schedule event inventory item date=%08u league_id=%u event=%p type=%u event_date=%04u-%02u-%02u over=%u title=%s",
            current_date,
            league_id,
            (void*)event_ptr,
            event_type,
            event_year,
            event_month,
            event_day,
            event_over,
            title);
    }
    kbo_log_runtimef(
        "KBO award schedule event inventory summary date=%08u league_id=%u manager=%p count=%d relevant=%d voting_type=%d",
        current_date,
        league_id,
        (void*)event_manager,
        event_count,
        relevant,
        voting_type);
}
