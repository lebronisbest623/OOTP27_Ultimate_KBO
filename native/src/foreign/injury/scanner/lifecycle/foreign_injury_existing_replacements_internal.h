#ifndef KBOFIX_SRC_FOREIGN_INJURY_SCANNER_LIFECYCLE_FOREIGN_INJURY_EXISTING_REPLACEMENTS_INTERNAL_H_
#define KBOFIX_SRC_FOREIGN_INJURY_SCANNER_LIFECYCLE_FOREIGN_INJURY_EXISTING_REPLACEMENTS_INTERNAL_H_

#include "logs/foreign_injury_existing_replacements_logs.h"
#include "team_cache/foreign_injury_existing_replacements_team_cache.h"

/* Snapshot-aware replacement for kbo_foreign_injury_replacement_player_reserved_locked.
   Searches the provided snapshot array instead of the global array. */
static inline int kbo_foreign_injury_replacement_player_reserved_snapshot(
    uint32_t replacement_player_id,
    const KboForeignInjuryReplacement* owner_rec,
    const KboForeignInjuryReplacement* records,
    int record_count)
{
    if (replacement_player_id == 0u) {
        return 0;
    }
    for (int i = 0; i < record_count; i++) {
        const KboForeignInjuryReplacement* rec = &records[i];
        if (rec == owner_rec || rec->replacement_player_id != replacement_player_id) {
            continue;
        }
        if (kbo_foreign_injury_status_uses_slot(rec->status)
                || rec->status == KBO_FOREIGN_INJURY_STATUS_PENDING) {
            return 1;
        }
    }
    return 0;
}

int kbo_foreign_injury_repair_closed_existing_replacement(
    KboForeignInjuryReplacement* rec,
    uint32_t today,
    const char* source,
    KboForeignInjuryReplacement* active_news,
    int active_capacity,
    int* active_count,
    KboForeignInjuryTeamLookupCacheEntry* team_cache,
    int* team_cache_count,
    int team_cache_capacity,
    const KboForeignInjuryReplacement* snapshot,
    int snapshot_count);

#endif
