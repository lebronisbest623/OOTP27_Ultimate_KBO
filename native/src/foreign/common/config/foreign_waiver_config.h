#ifndef KBOFIX_SRC_FOREIGN_FOREIGN_WAIVER_CONFIG_H_
#define KBOFIX_SRC_FOREIGN_FOREIGN_WAIVER_CONFIG_H_

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

#define KBO_FOREIGN_POLICY_VALUE_THRESHOLD_FILE "foreign_policy_value_threshold.txt"
#define KBO_FOREIGN_POLICY_ASIAN_VALUE_THRESHOLD_FILE "asian_quota_value_threshold.txt"
#define KBO_FOREIGN_POLICY_AI_TARGET_TEAM_FILE "foreign_policy_ai_target_team_id.txt"
#define KBO_FOREIGN_POLICY_FORCED_PLAYER_IDS_FILE "foreign_policy_forced_player_ids.txt"

uint32_t kbo_read_u32_leading_number_from_foreign_policy_file(const char* file_name);
uint32_t kbo_get_foreign_waiver_auto_target_team_id(void);
int kbo_is_forced_foreign_candidate_id(uint32_t player_id);

#endif
