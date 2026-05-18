#ifndef KBOFIX_SRC_COMPETITIVE_BALANCE_TAX_DRAFT_CBT_DRAFT_ORDER_PENALTY_H_
#define KBOFIX_SRC_COMPETITIVE_BALANCE_TAX_DRAFT_CBT_DRAFT_ORDER_PENALTY_H_

#include <stdint.h>

int kbo_cbt_apply_draft_order_penalties(uintptr_t draft_state, const char* source);
int kbo_cbt_apply_primary_league_draft_order_penalties(uint32_t league_id, const char* source);
void kbo_cbt_note_draft_order_state(uintptr_t draft_state);
int kbo_cbt_apply_pending_draft_order_penalties(const char* source);

#endif
