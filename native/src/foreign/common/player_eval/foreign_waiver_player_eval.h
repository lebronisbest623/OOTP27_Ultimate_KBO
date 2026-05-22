#ifndef KBOFIX_SRC_FOREIGN_FOREIGN_WAIVER_PLAYER_EVAL_H_
#define KBOFIX_SRC_FOREIGN_FOREIGN_WAIVER_PLAYER_EVAL_H_

#include <stdint.h>

int16_t kbo_read_player_i16(uint8_t* player, uint32_t offset);
int32_t kbo_foreign_waiver_value_score(uint8_t* player);
int32_t kbo_get_foreign_waiver_value_threshold(void);
int32_t kbo_get_foreign_waiver_asian_value_threshold(void);
int32_t kbo_get_foreign_waiver_value_threshold_for_player(uint8_t* player);
uint8_t* kbo_find_player_by_id(uint32_t player_id, uint32_t* out_current_team_id, uint32_t* out_current_league_id);
int kbo_player_is_retired(uint8_t* player);
int kbo_player_is_active_for_roster_scan(uint8_t* player);
int kbo_player_is_foreign_for_kbo_rights(uint8_t* player);
int kbo_load_asian_quota_nation_ids_once(void);
int kbo_nation_is_asian_quota_candidate(uint32_t nation_id);
/* Stable slot identity for prospective offers; independent of transient FA demand. */
int kbo_player_is_asian_quota_slot_candidate(uint8_t* player);
/* Current-contract AQ compliance for players already being counted on rosters. */
int kbo_player_is_asian_quota_candidate(uint8_t* player);

#endif
