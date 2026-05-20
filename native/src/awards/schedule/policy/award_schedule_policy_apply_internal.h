#ifndef KBOFIX_SRC_AWARDS_SCHEDULE_POLICY_AWARD_SCHEDULE_POLICY_APPLY_INTERNAL_H_
#define KBOFIX_SRC_AWARDS_SCHEDULE_POLICY_AWARD_SCHEDULE_POLICY_APPLY_INTERNAL_H_

#include "../award_schedule_probe_module.h"

uint32_t kbo_award_schedule_rule_date(const KboAwardScheduleRule* rule, uint32_t year);
int kbo_award_schedule_event_might_match_policy(uint32_t event_type, const char* title);
int kbo_award_schedule_event_is_legacy_custom_placeholder(
    uint32_t event_type,
    const char* title,
    const KboAwardSchedulePolicy* policy);
const KboAwardScheduleRule* kbo_award_schedule_find_rule(
    const KboAwardSchedulePolicy* policy,
    uint32_t event_type,
    const char* title);
int kbo_award_schedule_apply_rule_to_native_event(
    uint8_t* event,
    const KboAwardScheduleRule* rule,
    const char* title,
    uint32_t event_type,
    uint32_t league_id,
    uint32_t current_date,
    const char* path,
    const char* source);
uint32_t kbo_award_schedule_retired_placeholder_year(uint32_t current_date);
int kbo_award_schedule_retire_legacy_custom_placeholder(uint8_t* event, uint32_t current_date);

#endif
