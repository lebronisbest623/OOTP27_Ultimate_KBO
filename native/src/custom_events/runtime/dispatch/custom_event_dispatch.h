#ifndef KBOFIX_SRC_CUSTOM_EVENTS_CUSTOM_EVENT_DISPATCH_H_
#define KBOFIX_SRC_CUSTOM_EVENTS_CUSTOM_EVENT_DISPATCH_H_

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

#include "../names/custom_event_names.h"

int kbo_dispatch_custom_event_by_kind(uintptr_t event_ptr, KboCustomEventKind kind, uint32_t event_yyyymmdd, uint32_t event_year, uint32_t event_month, uint32_t event_day, const char* source);
int kbo_dispatch_custom_event(uintptr_t event_ptr, const char* name, uint32_t event_yyyymmdd, uint32_t event_year, uint32_t event_month, uint32_t event_day, const char* source);
int kbo_custom_event_completed_state_is_valid(uint32_t league_id, uint32_t event_yyyymmdd, KboCustomEventKind kind);

#endif
