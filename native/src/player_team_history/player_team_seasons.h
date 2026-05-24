#ifndef KBOFIX_SRC_PLAYER_TEAM_HISTORY_PLAYER_TEAM_SEASONS_H_
#define KBOFIX_SRC_PLAYER_TEAM_HISTORY_PLAYER_TEAM_SEASONS_H_

#include <stdint.h>

int kbo_player_team_seasons_count_by_key(uint32_t team_id, const char* player_key, int* out_season_count);
int kbo_player_team_seasons_count_for_player(uint32_t team_id, uint8_t* player, int* out_season_count);
int kbo_player_team_total_seasons_for_player(uint8_t* player, int* out_season_count);

#endif
