#ifndef KBOFIX_SRC_FOREIGN_INJURY_SCANNER_LIFECYCLE_FOREIGN_INJURY_EXISTING_REPLACEMENTS_INTERNAL_H_
#define KBOFIX_SRC_FOREIGN_INJURY_SCANNER_LIFECYCLE_FOREIGN_INJURY_EXISTING_REPLACEMENTS_INTERNAL_H_

#include "logs/foreign_injury_existing_replacements_logs.h"
#include "team_cache/foreign_injury_existing_replacements_team_cache.h"

int kbo_foreign_injury_repair_closed_existing_replacement(
    KboForeignInjuryReplacement* rec,
    uint32_t today,
    const char* source,
    KboForeignInjuryReplacement* active_news,
    int active_capacity,
    int* active_count,
    KboForeignInjuryTeamLookupCacheEntry* team_cache,
    int* team_cache_count,
    int team_cache_capacity);

#endif
