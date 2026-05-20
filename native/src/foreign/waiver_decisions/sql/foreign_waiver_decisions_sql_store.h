#ifndef KBOFIX_SRC_FOREIGN_WAIVER_DECISIONS_SQL_FOREIGN_WAIVER_DECISIONS_SQL_STORE_H_
#define KBOFIX_SRC_FOREIGN_WAIVER_DECISIONS_SQL_FOREIGN_WAIVER_DECISIONS_SQL_STORE_H_

#include <stddef.h>
#include <stdint.h>

typedef struct KboForeignWaiverDecisionBreakdown {
    int retained;
    int skipped;
    int ai_retained;
    int ai_skipped;
    int user_retained;
    int user_skipped;
    int available;
} KboForeignWaiverDecisionBreakdown;

int kbo_foreign_waiver_decisions_sql_append(
    const char* source,
    const char* action,
    uint32_t decision_date,
    uint32_t window_start,
    uint32_t window_end,
    uint32_t team_id,
    uint32_t player_id,
    int score,
    int forced,
    int executed);
int kbo_foreign_waiver_decisions_sql_exists(
    uint32_t window_end,
    uint32_t team_id,
    uint32_t player_id);
int kbo_foreign_waiver_decisions_sql_latest_action(
    uint32_t window_end,
    uint32_t team_id,
    uint32_t player_id,
    char* out_action,
    size_t out_action_size);
int kbo_foreign_waiver_decisions_sql_breakdown(
    uint32_t window_end,
    KboForeignWaiverDecisionBreakdown* out_breakdown);

#endif
