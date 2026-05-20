#include "../award_schedule_policy_apply_internal.h"

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

int kbo_award_schedule_apply_rule_to_native_event(
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

uint32_t kbo_award_schedule_retired_placeholder_year(uint32_t current_date)
{
    uint32_t current_year = current_date / 10000u;
    return current_year > 1982u && current_year <= 2400u ? current_year - 1u : 1982u;
}

int kbo_award_schedule_retire_legacy_custom_placeholder(uint8_t* event, uint32_t current_date)
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
