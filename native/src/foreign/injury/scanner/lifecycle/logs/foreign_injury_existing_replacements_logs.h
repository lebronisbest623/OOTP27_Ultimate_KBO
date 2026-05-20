#ifndef KBOFIX_FOREIGN_INJURY_EXISTING_REPLACEMENTS_LOGS_H_
#define KBOFIX_FOREIGN_INJURY_EXISTING_REPLACEMENTS_LOGS_H_

#include "../../foreign_injury_scanner_internal.h"

typedef enum KboForeignInjuryExistingWaitLogKind {
    KBO_FOREIGN_INJURY_EXISTING_WAIT_ACTIVE_RESTORE,
    KBO_FOREIGN_INJURY_EXISTING_WAIT_SUPPRESS_EARLY_CLOSE,
    KBO_FOREIGN_INJURY_EXISTING_WAIT_STALE_WITHOUT_BASIS,
    KBO_FOREIGN_INJURY_EXISTING_WAIT_INACTIVE_RETURN,
    KBO_FOREIGN_INJURY_EXISTING_WAIT_TOP_TEAM_RETURN
} KboForeignInjuryExistingWaitLogKind;

typedef struct KboForeignInjuryExistingWaitLogContext {
    const KboForeignInjuryReplacement* rec;
    uint8_t* injured;
    const KboForeignInjuryLiveMemory* live_injury;
    int active_roster_present;
    int inactive_roster_present;
    int roster_hold_flags_present;
    uint32_t today;
    int expected_end_reached;
    const char* source;
} KboForeignInjuryExistingWaitLogContext;

void kbo_foreign_injury_log_existing_replacement_wait(
    KboForeignInjuryExistingWaitLogKind kind,
    const KboForeignInjuryExistingWaitLogContext* context);
void kbo_foreign_injury_emit_active_replacement_news_batch(
    const KboForeignInjuryReplacement* active_news,
    int active_count,
    uint32_t today,
    const char* source);

#endif
