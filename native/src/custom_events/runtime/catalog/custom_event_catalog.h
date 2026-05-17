#ifndef KBOFIX_SRC_CUSTOM_EVENTS_RUNTIME_CATALOG_CUSTOM_EVENT_CATALOG_H_
#define KBOFIX_SRC_CUSTOM_EVENTS_RUNTIME_CATALOG_CUSTOM_EVENT_CATALOG_H_

#include <stddef.h>
#include <stdint.h>

#include "../names/custom_event_names.h"

typedef struct KboCustomEventSchedulePolicy {
    int32_t foreign_priority_fa_declaration_offset_days;
    int32_t foreign_priority_intl_established_fa_offset_days;
    int32_t foreign_priority_military_selection_offset_months;
    int32_t independent_team_acquisition_open_offset_months;
} KboCustomEventSchedulePolicy;

int kbo_custom_event_catalog_title_key(
    KboCustomEventKind kind,
    char* out,
    size_t out_size);
const KboCustomEventSchedulePolicy* kbo_custom_event_schedule_policy(void);

#endif
