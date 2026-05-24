#ifndef KBOFIX_SRC_CUSTOM_EVENTS_FOREIGN_PRIORITY_EVENT_SCHEDULE_INTERNAL_H_
#define KBOFIX_SRC_CUSTOM_EVENTS_FOREIGN_PRIORITY_EVENT_SCHEDULE_INTERNAL_H_

#include "foreign_priority_event_schedule.h"
#include "../audit/foreign_priority_event_audit.h"
#include "../../secondary_draft/secondary_draft.h"

typedef struct KboForeignPriorityScheduleDates {
    uint32_t anchor_date;
    uint32_t open_date;
    uint32_t close_date;
    uint32_t fa_declaration_date;
    uint32_t intl_established_fa_date;
    uint32_t military_selection_date;
    uint32_t secondary_draft_protection_open_date;
    uint32_t secondary_draft_protection_deadline_date;
    uint32_t secondary_draft_date;
    int secondary_draft_year;
} KboForeignPriorityScheduleDates;

typedef struct KboForeignPriorityScheduleResult {
    int created_open;
    int created_close;
    int created_fa_declaration;
    int created_intl_established_fa;
    int pruned_old_intl_established_fa;
    int created_military;
    int created_secondary_draft;
    int secondary_draft_window_news;
    int ready;
    int changed;
    int already_scheduled;
} KboForeignPriorityScheduleResult;

uint32_t kbo_recent_foreign_waiver_marker_anchor(uint32_t today_yyyymmdd, const char* source);
uint32_t kbo_custom_event_add_months_yyyymmdd(uint32_t yyyymmdd, uint32_t months);
int kbo_foreign_priority_ready_cache_hit(uint32_t anchor_date, uint32_t league_id);
void kbo_foreign_priority_ready_cache_store(uint32_t anchor_date, uint32_t league_id);
int kbo_foreign_priority_schedule_events(
    uint32_t league_id,
    uint32_t today,
    uint32_t offseason_starts_yyyymmdd,
    const KboForeignPriorityScheduleDates* dates,
    const KboSecondaryDraftWindow* secondary_draft_window,
    int registered_secondary_draft_window,
    const char* source,
    KboForeignPriorityEventAudit* audit,
    KboForeignPriorityScheduleResult* out);

#endif
