#ifndef KBOFIX_SRC_FOREIGN_INJURY_SCANNER_LOOP_FOREIGN_INJURY_SCANNER_PLAYER_LOOP_INTERNAL_H_
#define KBOFIX_SRC_FOREIGN_INJURY_SCANNER_LOOP_FOREIGN_INJURY_SCANNER_PLAYER_LOOP_INTERNAL_H_

#include "foreign_injury_scanner_player_loop.h"

typedef struct KboForeignInjuryScannerPlayerLoopContext {
    uint32_t configured_league_id;
    uint32_t today;
    uint32_t live_date;
    int live_injury_fields_available;
    int process_existing_replacements;
    int captured_live_date;
    const char* source;
} KboForeignInjuryScannerPlayerLoopContext;

int kbo_foreign_injury_closed_record_should_repair_locked(
    uint32_t injured_player_id,
    const KboForeignInjuryLiveMemory* live_injury,
    uint32_t today,
    int inactive_roster_present,
    int roster_hold_flags_present);

KboForeignInjuryScannerPlayerLoopResult kbo_foreign_injury_scan_player_for_replacement(
    uintptr_t player_ptr,
    const KboForeignInjuryScannerPlayerLoopContext* context);

#endif
