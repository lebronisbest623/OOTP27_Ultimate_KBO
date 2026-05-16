#ifndef KBOFIX_SRC_CUSTOM_EVENTS_CUSTOM_EVENT_RUNNER_H_
#define KBOFIX_SRC_CUSTOM_EVENTS_CUSTOM_EVENT_RUNNER_H_

#include <stdint.h>
#include <windows.h>

#include "../names/custom_event_names.h"

#define KBO_CUSTOM_EVENT_RUN_ALREADY_COMPLETED 2
#define KBO_CUSTOM_EVENT_RUN_IN_PROGRESS -2

int kbo_run_custom_event_by_kind(
    uintptr_t event_ptr,
    uint32_t league_id,
    uint32_t event_yyyymmdd,
    KboCustomEventKind kind,
    const char* title,
    const char* source);

#endif
