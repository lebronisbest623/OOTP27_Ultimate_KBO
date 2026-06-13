#ifndef KBOFIX_SRC_CUSTOM_EVENTS_FOREIGN_PRIORITY_EVENT_SCHEDULE_INTERNAL_H_
#define KBOFIX_SRC_CUSTOM_EVENTS_FOREIGN_PRIORITY_EVENT_SCHEDULE_INTERNAL_H_

#include "foreign_priority_event_schedule.h"

#include "../audit/foreign_priority_event_audit.h"
#include "../../runtime/catalog/custom_event_catalog.h"
#include "../../secondary_draft/secondary_draft.h"

uint32_t kbo_recent_foreign_waiver_marker_anchor(uint32_t today_yyyymmdd, const char* source);
uint32_t kbo_custom_event_add_months_yyyymmdd(uint32_t yyyymmdd, uint32_t months);
int kbo_foreign_priority_ready_cache_hit(uint32_t anchor_date, uint32_t league_id);
void kbo_foreign_priority_ready_cache_store(uint32_t anchor_date, uint32_t league_id);
int kbo_foreign_priority_prepare_secondary_draft_schedule(
    const char* source,
    uint32_t offseason_starts_yyyymmdd,
    uint32_t anchor_date,
    uint32_t close_date,
    uint32_t fa_declaration_date,
    uint32_t intl_established_fa_date,
    const KboCustomEventSchedulePolicy* event_policy,
    KboForeignPriorityEventAudit* audit,
    int* required,
    uint32_t* season,
    uint32_t* protection_open_date,
    uint32_t* deadline_date,
    uint32_t* draft_date);
void kbo_foreign_priority_schedule_secondary_draft_event(
    uint32_t league_id,
    const char* source,
    int required,
    uint32_t season,
    uint32_t protection_open_date,
    uint32_t deadline_date,
    uint32_t draft_date,
    const char* title,
    KboSecondaryDraftWindow* window,
    int* exists,
    int* window_exists,
    int* created,
    int* pruned,
    int* registered_window,
    int* emitted_window_news);

#endif
