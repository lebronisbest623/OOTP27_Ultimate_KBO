#ifndef KBOFIX_SRC_CUSTOM_EVENTS_RUNTIME_SQL_CUSTOM_EVENT_SQL_STORE_H_
#define KBOFIX_SRC_CUSTOM_EVENTS_RUNTIME_SQL_CUSTOM_EVENT_SQL_STORE_H_

#include <stdint.h>

#include "../names/custom_event_names.h"

int kbo_custom_event_sql_marker_exists(uint32_t event_yyyymmdd, const char* name);
int kbo_custom_event_sql_marker_exists_for_kind(uint32_t event_yyyymmdd, KboCustomEventKind kind);
void kbo_custom_event_sql_marker_record(uint32_t event_yyyymmdd, const char* name, const char* source);
void kbo_custom_event_sql_prune_rewound_markers(const char* source);

uint32_t kbo_custom_event_sql_calendar_cursor_read(const char* source);
int kbo_custom_event_sql_calendar_cursor_write(uint32_t today_yyyymmdd, const char* source);

int kbo_custom_event_sql_ledger_completed(uint32_t league_id, uint32_t event_yyyymmdd, KboCustomEventKind kind);
void kbo_custom_event_sql_ledger_record(
    uint32_t league_id,
    uint32_t event_yyyymmdd,
    KboCustomEventKind kind,
    const char* status,
    int result,
    const char* title,
    const char* detail,
    const char* source);

#endif
