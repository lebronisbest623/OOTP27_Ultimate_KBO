#include "foreign_injury_existing_replacements_team_cache.h"

uint8_t* kbo_foreign_injury_cached_team_lookup(
    uint32_t team_id,
    KboForeignInjuryTeamLookupCacheEntry* cache,
    int* cache_count,
    int cache_capacity)
{
    if (team_id == 0u) {
        return NULL;
    }
    for (int i = 0; i < *cache_count; i++) {
        if (cache[i].team_id == team_id) {
            return cache[i].team;
        }
    }
    uint8_t* team = find_kbo_team_by_numeric_id_any_league(team_id, 1);
    if (*cache_count < cache_capacity) {
        cache[*cache_count].team_id = team_id;
        cache[*cache_count].team = team;
        (*cache_count)++;
    }
    return team;
}
