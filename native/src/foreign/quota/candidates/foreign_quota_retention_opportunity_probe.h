#ifndef KBOFIX_SRC_FOREIGN_QUOTA_RETENTION_OPPORTUNITY_PROBE_H_
#define KBOFIX_SRC_FOREIGN_QUOTA_RETENTION_OPPORTUNITY_PROBE_H_

#include <stdint.h>

typedef struct KboForeignRetentionOpportunitySummary {
    uint32_t team_id;
    uint32_t today;
    uint32_t best_player_id;
    int32_t best_score;
    uint8_t best_asian;
    uint32_t active_rights;
    uint32_t protectable_rights;
    uint32_t protectable_asian;
    uint32_t protectable_non_asian;
} KboForeignRetentionOpportunitySummary;

int kbo_retention_opportunity_get_summary(
    uint32_t team_id,
    uint32_t today,
    KboForeignRetentionOpportunitySummary* out_summary);
int32_t kbo_retention_opportunity_score_margin_for_best(int32_t best_score);

int kbo_retention_opportunity_probe_should_block(
    uint32_t team_id,
    uint8_t* candidate,
    uint32_t today,
    uint32_t asian_count,
    uint32_t non_asian_count,
    uint32_t pending_asian_count,
    uint32_t pending_non_asian_count,
    uint32_t asian_after,
    uint32_t non_asian_after,
    uint32_t effective_before,
    uint32_t effective_after,
    uint32_t effective_limit,
    int candidate_pending,
    int already_in_org,
    int allowed);

#endif
