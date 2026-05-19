#ifndef KBOFIX_SRC_TEAM_TEAM_HUMAN_CONTROL_H_
#define KBOFIX_SRC_TEAM_TEAM_HUMAN_CONTROL_H_

#include <stdint.h>

int kbo_collect_human_controlled_team_ids(uint32_t* out_team_ids, int max_team_ids, const char* source);
int kbo_team_is_human_controlled(uint32_t team_id, const char* source);

#endif
