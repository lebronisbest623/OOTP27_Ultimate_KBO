#ifndef KBOFIX_SRC_MILITARY_SERVICE_SELECTION_RESULTS_STORE_H_
#define KBOFIX_SRC_MILITARY_SERVICE_SELECTION_RESULTS_STORE_H_

#include <stdint.h>
#include <windows.h>

#include "../news/military_selection_news.h"

typedef struct KboMilitarySelectionResultEntry {
    uint32_t year;
    uint32_t announcement_date;
    uint32_t player_id;
    uint32_t original_team_id;
    uint32_t original_league_id;
    uint32_t service_team_id;
    uint32_t return_date;
    uint16_t age;
    uint8_t position_group;
    uint8_t position_role;
    int32_t score;
} KboMilitarySelectionResultEntry;

int kbo_load_military_selection_result_history(
    KboMilitarySelectionResultEntry* out,
    int max_count,
    const char* source);

int kbo_append_military_selection_result_history(
    uint32_t year,
    uint32_t announcement_date,
    uint32_t service_team_id,
    KboMilitarySelectionNewsEntry* entries,
    int entry_count,
    const char* source);

#endif
