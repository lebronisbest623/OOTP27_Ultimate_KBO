#ifndef KBOFIX_SRC_FOREIGN_QUOTA_TEAM_POLICY_H_
#define KBOFIX_SRC_FOREIGN_QUOTA_TEAM_POLICY_H_

#include <stdint.h>

#define KBO_FOREIGN_QUOTA_FUTURES_INDEPENDENT_FOREIGN_LIMIT 4u

typedef struct KboForeignQuotaTeamPolicySnapshot {
    uint32_t team_id;
    uint32_t base_effective_limit;
    uint8_t military_or_police;
    uint8_t blocks_foreign_ownership;
    uint8_t futures_independent;
    uint8_t allows_injury_extra_slots;
} KboForeignQuotaTeamPolicySnapshot;

int kbo_foreign_quota_team_policy_snapshot(
    uint32_t team_id,
    KboForeignQuotaTeamPolicySnapshot* out_snapshot);
int kbo_foreign_quota_team_is_military_or_police(uint32_t team_id);
int kbo_foreign_quota_team_blocks_foreign_ownership(uint32_t team_id);
int kbo_foreign_quota_team_is_futures_independent(uint32_t team_id);
int kbo_foreign_quota_team_allows_injury_extra_slots(uint32_t team_id);
uint32_t kbo_foreign_quota_base_effective_limit_for_team(uint32_t team_id);
uint32_t kbo_foreign_quota_effective_count_for_team(
    uint32_t team_id,
    uint32_t asian_count,
    uint32_t non_asian_foreign_count);

#endif
