#ifndef KBOFIX_FOREIGN_INJURY_EXISTING_REPLACEMENTS_PLAYER_STATE_H_
#define KBOFIX_FOREIGN_INJURY_EXISTING_REPLACEMENTS_PLAYER_STATE_H_

#include "../../foreign_injury_scanner_internal.h"

int kbo_foreign_injury_replacement_unavailable_by_long_injury(
    const KboForeignInjuryReplacement* rec,
    uint32_t today);
int kbo_foreign_injury_runtime_injury_present(uint8_t* player);
int kbo_foreign_injury_roster_hold_flags_present(uint8_t* player);

#endif
