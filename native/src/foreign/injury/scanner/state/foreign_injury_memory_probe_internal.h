#ifndef KBOFIX_FOREIGN_INJURY_MEMORY_PROBE_INTERNAL_H_
#define KBOFIX_FOREIGN_INJURY_MEMORY_PROBE_INTERNAL_H_

#include "../foreign_injury_scanner_internal.h"

#define KBO_FOREIGN_INJURY_MEMORY_PROBE_MAX 2048
#define KBO_FOREIGN_INJURY_MEMORY_PROBE_BASE 0x820u
#define KBO_FOREIGN_INJURY_MEMORY_PROBE_BYTES 0x200u
#define KBO_FOREIGN_INJURY_MEMORY_PROBE_FOCUS_BASE 0x840u
#define KBO_FOREIGN_INJURY_MEMORY_PROBE_FOCUS_BYTES 0x80u

typedef struct KboForeignInjuryMemoryProbeSnapshot {
    uint32_t player_id;
    uint32_t team_id;
    uint32_t league_id;
    uint32_t current_team_id;
    uint32_t active_team_id;
    uint32_t loan_team_id;
    uint32_t original_team_id;
    uint32_t default_team_id;
    uint32_t date;
    int16_t injury_aux_0876;
    uint8_t slot_type;
    uint8_t injury_active;
    uint8_t inactive_roster_present;
    uint8_t has_assignment;
    uint8_t restricted;
    uint8_t secondary_restricted;
    uint8_t dfa;
    uint8_t loan_active;
    uint8_t bytes[KBO_FOREIGN_INJURY_MEMORY_PROBE_BYTES];
} KboForeignInjuryMemoryProbeSnapshot;

const char* kbo_foreign_injury_memory_probe_reason(
    const KboForeignInjuryMemoryProbeSnapshot* before,
    const KboForeignInjuryMemoryProbeSnapshot* after,
    int initial);
int kbo_foreign_injury_memory_probe_reason_is_anchor(const char* reason);
void kbo_foreign_injury_memory_probe_emit_transition(
    const KboForeignInjuryMemoryProbeSnapshot* before,
    const KboForeignInjuryMemoryProbeSnapshot* after,
    int initial,
    const char* source);

#endif
