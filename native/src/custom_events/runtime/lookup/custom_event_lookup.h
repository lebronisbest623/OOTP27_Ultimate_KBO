#ifndef KBOFIX_SRC_CUSTOM_EVENTS_CUSTOM_EVENT_LOOKUP_H_
#define KBOFIX_SRC_CUSTOM_EVENTS_CUSTOM_EVENT_LOOKUP_H_

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

#include "../names/custom_event_names.h"

uint32_t kbo_get_latest_offseason_starts_event(uint32_t today_yyyymmdd);
uint32_t kbo_detect_offseason_anchor_by_league_year(uint32_t league_id, uint32_t today_yyyymmdd, const char* source);
int kbo_custom_event_exists_for_date(uint32_t league_id, uint32_t event_yyyymmdd, int expect_open);
int kbo_custom_event_name_matches_local(const char* name);
int kbo_custom_event_exists_by_title_for_date(uint32_t league_id, uint32_t event_yyyymmdd, const char* expected_title);
int kbo_custom_event_exists_by_kind_for_date(uint32_t league_id, uint32_t event_yyyymmdd, KboCustomEventKind kind);
int kbo_prune_duplicate_custom_events_by_kind_for_date(
    uint32_t league_id,
    uint32_t event_yyyymmdd,
    KboCustomEventKind kind,
    const char* source);
int kbo_delete_custom_events_by_kind_for_date(
    uint32_t league_id,
    uint32_t event_yyyymmdd,
    KboCustomEventKind kind,
    const char* source);

#endif
