#ifndef KBOFIX_SRC_CAPTAIN_THREAD_MAINTENANCE_CAPTAIN_SELECTION_MAINTENANCE_HELPERS_H_
#define KBOFIX_SRC_CAPTAIN_THREAD_MAINTENANCE_CAPTAIN_SELECTION_MAINTENANCE_HELPERS_H_

#include <stdint.h>

void kbo_captain_audit_maintenance(
    const char* decision,
    const char* reason,
    const char* source,
    uint32_t date,
    uint32_t season,
    uint32_t league_id,
    uint32_t league_season,
    uint8_t phase,
    int csv_exists,
    int calendar_recovery,
    int calendar_preseason,
    int seed_startup,
    int preseason_first_day);

int kbo_captain_emit_initial_selection_news_from_csv_or_defer(
    uint32_t date,
    uint32_t season,
    uint32_t league_id,
    uint32_t league_season,
    uint8_t phase,
    int csv_exists,
    int calendar_recovery,
    int calendar_preseason,
    int seed_startup,
    int preseason_first_day,
    const char* source);

int kbo_captain_write_missing_csv_or_defer(
    uint32_t date,
    uint32_t season,
    uint32_t league_id,
    uint8_t phase,
    const char* source);

#endif
