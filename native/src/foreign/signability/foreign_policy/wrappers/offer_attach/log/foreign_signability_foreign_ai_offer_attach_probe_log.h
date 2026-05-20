#ifndef KBOFIX_SRC_FOREIGN_SIGNABILITY_FOREIGN_POLICY_WRAPPERS_OFFER_ATTACH_FOREIGN_SIGNABILITY_FOREIGN_AI_OFFER_ATTACH_PROBE_LOG_H_
#define KBOFIX_SRC_FOREIGN_SIGNABILITY_FOREIGN_POLICY_WRAPPERS_OFFER_ATTACH_FOREIGN_SIGNABILITY_FOREIGN_AI_OFFER_ATTACH_PROBE_LOG_H_

#include <stdint.h>

int32_t kbo_offer_probe_player_value_score(uint8_t* player);
void kbo_log_foreign_ai_offer_attach(
    uintptr_t player_ptr,
    uintptr_t offer_slot_ptr,
    uintptr_t caller_return_ptr);
void kbo_log_foreign_ai_offer_build(
    uintptr_t player_ptr,
    int32_t team_id,
    uintptr_t flag_ptr,
    uintptr_t offer_ptr);
void kbo_log_foreign_ai_offer_final_gate(
    uintptr_t team_ptr,
    uintptr_t player_ptr,
    int32_t salary,
    uintptr_t offer_ptr,
    uint8_t result);

#endif
