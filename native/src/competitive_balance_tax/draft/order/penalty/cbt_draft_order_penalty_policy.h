#ifndef KBOFIX_SRC_COMPETITIVE_BALANCE_TAX_DRAFT_ORDER_PENALTY_CBT_DRAFT_ORDER_PENALTY_POLICY_H_
#define KBOFIX_SRC_COMPETITIVE_BALANCE_TAX_DRAFT_ORDER_PENALTY_CBT_DRAFT_ORDER_PENALTY_POLICY_H_

#include <stdint.h>

typedef struct KboCbtDraftPenaltyInfo {
    uint32_t season;
    uint32_t team_id;
    uint32_t stages;
} KboCbtDraftPenaltyInfo;

int kbo_cbt_draft_order_penalty_for_team(uint32_t team_id, KboCbtDraftPenaltyInfo* out);
int kbo_cbt_draft_order_team_is_main_kbo(uint32_t team_id);

#endif
