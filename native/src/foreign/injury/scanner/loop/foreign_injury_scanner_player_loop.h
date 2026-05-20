#ifndef KBOFIX_FOREIGN_INJURY_SCANNER_PLAYER_LOOP_H_
#define KBOFIX_FOREIGN_INJURY_SCANNER_PLAYER_LOOP_H_

#include "../foreign_injury_scanner_internal.h"

typedef struct KboForeignInjuryScannerPlayerLoopResult {
    int scanned;
    int opened;
} KboForeignInjuryScannerPlayerLoopResult;

KboForeignInjuryScannerPlayerLoopResult kbo_foreign_injury_scan_player_loop(
    uintptr_t player_vector,
    int32_t player_count,
    uint32_t configured_league_id,
    uint32_t today,
    uint32_t live_date,
    int live_injury_fields_available,
    int process_existing_replacements,
    int captured_live_date,
    const char* source);

#endif
