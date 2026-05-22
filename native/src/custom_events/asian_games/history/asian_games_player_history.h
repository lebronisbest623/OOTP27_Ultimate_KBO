#ifndef KBOFIX_SRC_CUSTOM_EVENTS_ASIAN_GAMES_HISTORY_ASIAN_GAMES_PLAYER_HISTORY_H_
#define KBOFIX_SRC_CUSTOM_EVENTS_ASIAN_GAMES_HISTORY_ASIAN_GAMES_PLAYER_HISTORY_H_

#include <stdint.h>

#include "../state/asian_games_state.h"

int kbo_record_asian_games_selection_history(
    const KboAsianGamesRosterEntry* entry,
    uint32_t event_yyyymmdd,
    const char* source);

int kbo_record_asian_games_replacement_history(
    const KboAsianGamesRosterEntry* old_entry,
    const KboAsianGamesRosterEntry* new_entry,
    uint32_t event_yyyymmdd,
    const char* source);

int kbo_record_asian_games_final_history(
    const KboAsianGamesRosterEntry* entry,
    uint32_t event_yyyymmdd,
    uint8_t final_result,
    const char* source);

#endif
