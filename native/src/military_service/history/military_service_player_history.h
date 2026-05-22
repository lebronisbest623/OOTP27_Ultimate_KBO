#ifndef KBOFIX_SRC_MILITARY_SERVICE_HISTORY_MILITARY_SERVICE_PLAYER_HISTORY_H_
#define KBOFIX_SRC_MILITARY_SERVICE_HISTORY_MILITARY_SERVICE_PLAYER_HISTORY_H_

#include <stdint.h>

#include "../selection/news/military_selection_news.h"

int kbo_record_military_selection_player_history_batch(
    uint32_t event_yyyymmdd,
    uint32_t service_team_id,
    KboMilitarySelectionNewsEntry* entries,
    int entry_count,
    const char* source);

int kbo_record_military_seed_assignment_player_history(
    uint32_t player_id,
    uint32_t service_team_id,
    uint32_t original_team_id,
    uint32_t event_yyyymmdd,
    const char* source);

#endif
