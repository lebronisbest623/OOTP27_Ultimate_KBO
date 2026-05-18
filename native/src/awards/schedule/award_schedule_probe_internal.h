#ifndef KBO_AWARD_SCHEDULE_PROBE_INTERNAL_H
#define KBO_AWARD_SCHEDULE_PROBE_INTERNAL_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "award_schedule_probe.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../bootstrap/abi/ootp_typedefs.h"
#include "../../core/core_flags/api/flags_api.h"
#include "../../core/events/core_league_events.h"
#include "../../core/core_league_context_parts/event_manager/event_manager.h"
#include "../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../core/dates/core_current_date.h"
#include "../../core/dates/tick/current_date_tick_capture.h"
#include "../../core/files/save_paths/core_save_paths.h"
#include "../../core/logging/core_log.h"
#include "../../core/text/ootp_text_encoding.h"
#include "../../runtime_memory/runtime_memory.h"
#include "../../team/names/team_string.h"

#define KBO_AWARD_PITCHER_NAME_OFFSET 0x150u
#define KBO_AWARD_MVP_NAME_OFFSET     0x168u
#define KBO_AWARD_ROOKIE_NAME_OFFSET  0x180u
#define KBO_AWARD_DEFENSE_NAME_OFFSET 0x198u
#define KBO_AWARD_FLAGS_BASE_OFFSET   0x385u
#define KBO_AWARD_FLAGS_COUNT         12u
#define KBO_AWARD_RULES_BASE_OFFSET   0x380u
#define KBO_AWARD_RULES_BYTES         0x80u
#define KBO_AWARD_HOF_RULES_OFFSET    0xc20u
#define KBO_AWARD_HOF_RULES_BYTES     0x30u
#define KBO_AWARD_SCHEDULE_POLICY_FILE "award_schedule_policy.json"
#define KBO_AWARD_SCHEDULE_MAX_RULES 16u
#define KBO_AWARD_SCHEDULE_MAX_TITLES 8u
#define KBO_AWARD_SCHEDULE_MAX_OVERRIDES 16u
#define KBO_AWARD_SCHEDULE_MAX_EVENT_TYPES 8u
#define KBO_AWARD_VOTING_EVENT_TYPE 0x10u

typedef struct KboAwardScheduleOverride {
    uint32_t year;
    uint32_t month;
    uint32_t day;
} KboAwardScheduleOverride;

typedef struct KboAwardScheduleRule {
    char id[48];
    char label[96];
    char label_en[96];
    char label_ko[96];
    char titles[KBO_AWARD_SCHEDULE_MAX_TITLES][96];
    uint32_t title_count;
    uint16_t event_types[KBO_AWARD_SCHEDULE_MAX_EVENT_TYPES];
    uint32_t event_type_count;
    uint32_t default_month;
    uint32_t default_day;
    KboAwardScheduleOverride overrides[KBO_AWARD_SCHEDULE_MAX_OVERRIDES];
    uint32_t override_count;
} KboAwardScheduleRule;

typedef struct KboAwardSchedulePolicy {
    KboAwardScheduleRule rules[KBO_AWARD_SCHEDULE_MAX_RULES];
    uint32_t rule_count;
} KboAwardSchedulePolicy;

extern volatile LONG g_kbo_award_schedule_probe_started;
int kbo_award_schedule_load_text(char** out_text, DWORD* out_size, char* out_path, size_t out_path_size);
int kbo_award_schedule_parse_policy(const char* json, KboAwardSchedulePolicy* out);
void kbo_award_schedule_apply_once(uint32_t league_id, uint32_t current_date, int ensure_events);
int kbo_award_schedule_adjust_created_event(
    void* event_ptr,
    uint32_t event_type_hint,
    uint32_t league_id_hint,
    const char* title_hint,
    uint32_t current_date,
    const char* source);
void kbo_award_schedule_set_create_league_event_original(OotpCreateLeagueEventFn original_func);
void kbo_award_schedule_log_event_inventory(uint32_t league_id, uint32_t current_date);

#endif
