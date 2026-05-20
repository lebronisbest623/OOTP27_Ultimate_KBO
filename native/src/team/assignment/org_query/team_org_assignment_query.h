#ifndef KBOFIX_SRC_TEAM_TEAM_ORG_ASSIGNMENT_QUERY_H_
#define KBOFIX_SRC_TEAM_TEAM_ORG_ASSIGNMENT_QUERY_H_

#include <stdint.h>

uint32_t kbo_org_team_id_for_team_id(uint32_t team_id);
int kbo_team_ids_share_org(uint32_t left_team_id, uint32_t right_team_id);
int kbo_player_current_assignment_matches_team_or_affiliate(uint8_t* player, uint32_t team_id);

#endif
