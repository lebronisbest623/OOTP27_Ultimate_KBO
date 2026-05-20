#ifndef KBOFIX_SRC_CUSTOM_EVENTS_ASIAN_GAMES_LIFECYCLE_MAINTENANCE_POLICY_H_
#define KBOFIX_SRC_CUSTOM_EVENTS_ASIAN_GAMES_LIFECYCLE_MAINTENANCE_POLICY_H_

#include <stdint.h>
#include "../../asian_games/state/asian_games_state.h"

int kbo_asian_games_restricted_hold_active(uint32_t today_yyyymmdd, uint32_t departure_yyyymmdd, uint32_t return_yyyymmdd);
int32_t kbo_asian_games_restricted_days_left(uint32_t today_yyyymmdd, uint32_t return_yyyymmdd);
int kbo_asian_games_roster_entry_needs_restricted_hold(const KboAsianGamesRosterEntry* entry, uint32_t today_yyyymmdd);

#endif
