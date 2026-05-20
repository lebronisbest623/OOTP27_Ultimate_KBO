#ifndef KBOFIX_SRC_COMPETITIVE_BALANCE_TAX_EVENTS_CBT_EVENT_HELPERS_H_
#define KBOFIX_SRC_COMPETITIVE_BALANCE_TAX_EVENTS_CBT_EVENT_HELPERS_H_

#include <stdint.h>

#include "../../custom_events/runtime/names/custom_event_names.h"

int kbo_cbt_should_log_no_date(void);
int kbo_cbt_ensure_salary_snapshot_rows(
    uint32_t season,
    uint32_t event_yyyymmdd,
    const char* source);
int kbo_process_due_cbt_custom_event(
    uint32_t today,
    uint32_t league_id,
    uint32_t event_date,
    KboCustomEventKind kind,
    const char* title,
    const char* source);

#endif
