#ifndef KBOFIX_SRC_CUSTOM_EVENTS_ASIAN_GAMES_PLAYER_ELIGIBILITY_H_
#define KBOFIX_SRC_CUSTOM_EVENTS_ASIAN_GAMES_PLAYER_ELIGIBILITY_H_

#include <stdint.h>

int kbo_asian_games_player_status_allows_selection(uint8_t* player);
uint8_t kbo_asian_games_player_military_unserved(uint8_t* player);

#endif
