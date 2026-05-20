#ifndef KBOFIX_SRC_TEAM_TEAM_ROSTER_ARRAYS_H_
#define KBOFIX_SRC_TEAM_TEAM_ROSTER_ARRAYS_H_

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

int kbo_remove_player_id_from_team_fixed_array(uint8_t* team, uint32_t array_offset, uint32_t player_id);
int kbo_team_fixed_array_contains_player(uint8_t* team, uint32_t array_offset, uint32_t player_id);
int kbo_add_player_id_to_team_fixed_array(uint8_t* team, uint32_t array_offset, uint32_t player_id);
int kbo_team_roster_arrays_contain_player(uint8_t* team, uint32_t player_id);
int kbo_remove_player_id_from_team_active_roster_arrays(uint8_t* team, uint32_t player_id);
int kbo_remove_player_id_from_known_team_roster_arrays(uint8_t* team, uint32_t player_id);
int kbo_add_player_id_to_team_assignment_arrays(uint8_t* team, uint32_t player_id);

#endif
