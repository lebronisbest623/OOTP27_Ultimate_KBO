#ifndef KBOFIX_SRC_FOREIGN_CONTROLLER_FOREIGN_AI_FAST_FILL_CONTROLLER_H_
#define KBOFIX_SRC_FOREIGN_CONTROLLER_FOREIGN_AI_FAST_FILL_CONTROLLER_H_

#include <stdint.h>

typedef struct KboForeignFastFillContext {
    uint32_t team_id;
    uint32_t vacancy_started_on;
    uint32_t last_seen_on;
    uint32_t asian_count;
    uint32_t non_asian_count;
    uint32_t pending_asian_count;
    uint32_t pending_non_asian_count;
    uint32_t effective_count;
    uint32_t effective_with_pending;
    uint32_t limit;
    uint8_t effective_vacant;
    uint8_t asian_quota_vacant;
    uint8_t vacant;
} KboForeignFastFillContext;

int kbo_foreign_ai_fast_fill_get_context(
    uint32_t team_id,
    KboForeignFastFillContext* out_context);
int kbo_foreign_ai_fast_fill_candidate_solves_context(
    const KboForeignFastFillContext* context,
    int candidate_asian_quota);
int kbo_foreign_ai_fast_fill_controller_tick(uint32_t today, const char* source);

#endif
