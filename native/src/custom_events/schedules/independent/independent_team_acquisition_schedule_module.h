#ifndef KBOFIX_SRC_CUSTOM_EVENTS_SCHEDULES_INDEPENDENT_INDEPENDENT_TEAM_ACQUISITION_SCHEDULE_MODULE_H_
#define KBOFIX_SRC_CUSTOM_EVENTS_SCHEDULES_INDEPENDENT_INDEPENDENT_TEAM_ACQUISITION_SCHEDULE_MODULE_H_

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdint.h>

#define KBO_INDEPENDENT_ACQUISITION_LEAGUE_ID_OFFSET_COUNT 4u
#define KBO_INDEPENDENT_ACQUISITION_LEAGUE_SCAN_MAX_REGION ((SIZE_T)0x00400000u)

typedef struct KboIndependentAcquisitionMemoryStartCache {
    uint32_t league_id;
    uint32_t season;
    uint32_t start_date;
    uintptr_t league_ptr;
    char save_path[MAX_PATH];
} KboIndependentAcquisitionMemoryStartCache;

extern KboIndependentAcquisitionMemoryStartCache g_kbo_independent_acquisition_memory_start_cache;

uint32_t kbo_independent_team_acquisition_add_months(
    uint32_t yyyymmdd,
    uint32_t months);

int kbo_independent_team_acquisition_read_start_date_from_ptr(
    uintptr_t league_ptr,
    uint32_t expected_year,
    uint32_t* out_start_date);

int kbo_independent_team_acquisition_candidate_score(
    uintptr_t candidate,
    uint32_t league_id,
    uint32_t expected_year,
    uint32_t* out_start_date);

int kbo_independent_team_acquisition_resolve_start_date(
    uint32_t anchor_league_id,
    uint32_t event_league_id,
    uint32_t today,
    uint32_t* out_start_date,
    const char** out_start_source);

#endif
