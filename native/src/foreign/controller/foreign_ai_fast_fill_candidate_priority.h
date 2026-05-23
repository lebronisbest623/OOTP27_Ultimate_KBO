#ifndef KBOFIX_SRC_FOREIGN_CONTROLLER_FOREIGN_AI_FAST_FILL_CANDIDATE_PRIORITY_H_
#define KBOFIX_SRC_FOREIGN_CONTROLLER_FOREIGN_AI_FAST_FILL_CANDIDATE_PRIORITY_H_

#include <stdint.h>

uintptr_t kbo_foreign_ai_fast_fill_select_offer_candidate(
    uint8_t* original,
    uint32_t team_id,
    uint32_t today);

#endif
