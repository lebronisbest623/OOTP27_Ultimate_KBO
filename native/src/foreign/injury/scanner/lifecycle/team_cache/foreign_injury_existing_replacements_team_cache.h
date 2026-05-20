#ifndef KBOFIX_FOREIGN_INJURY_EXISTING_REPLACEMENTS_TEAM_CACHE_H_
#define KBOFIX_FOREIGN_INJURY_EXISTING_REPLACEMENTS_TEAM_CACHE_H_

#include "../../foreign_injury_scanner_internal.h"

typedef struct KboForeignInjuryTeamLookupCacheEntry {
    uint32_t team_id;
    uint8_t* team;
} KboForeignInjuryTeamLookupCacheEntry;

uint8_t* kbo_foreign_injury_cached_team_lookup(
    uint32_t team_id,
    KboForeignInjuryTeamLookupCacheEntry* cache,
    int* cache_count,
    int cache_capacity);

#endif
