#ifndef KBO_FOREIGN_QUOTA_COUNTS_INTERNAL_H
#define KBO_FOREIGN_QUOTA_COUNTS_INTERNAL_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include "../foreign_quota_counts.h"
#include "../../../../bootstrap/abi/ootp_offsets.h"
#include "../../../../bootstrap/profiling/profiler.h"
#include "../../../../core/logging/core_log.h"
#include "../../../../core/sync/lock.h"
#include "../../../../runtime_memory/runtime_memory.h"
#include "../../../../team/lookup/team_lookup.h"
#include "../../../../team/assignment/org_query/team_org_assignment_query.h"
#include "../../../common/player_eval/foreign_waiver_player_eval.h"
#include "../../../injury/api/foreign_injury.h"
#include "../../../replacement_seed/api/foreign_replacement_seed.h"

enum {
    KBO_FOREIGN_ORG_COUNT_CACHE_SIZE = 512,
    KBO_FOREIGN_ORG_SNAPSHOT_MAX_TEAMS = 256,
    KBO_FOREIGN_ORG_PARENT_CACHE_SIZE = 512,
    KBO_FOREIGN_ORG_TEAM_GENERATION_CACHE_SIZE = 1024
};

typedef struct KboForeignOrgCountCacheEntry {
    uint32_t team_id;
    uint32_t foreign_count;
    uint32_t asian_count;
    uint32_t non_asian_count;
    uint32_t generation;
    DWORD tick;
} KboForeignOrgCountCacheEntry;

typedef struct KboForeignOrgSnapshotEntry {
    uint32_t team_id;
    uint32_t foreign_count;
    uint32_t asian_count;
    uint32_t non_asian_count;
} KboForeignOrgSnapshotEntry;

extern KboForeignOrgCountCacheEntry g_kbo_foreign_org_count_cache[KBO_FOREIGN_ORG_COUNT_CACHE_SIZE];
extern KboForeignOrgSnapshotEntry g_kbo_foreign_org_snapshot[KBO_FOREIGN_ORG_SNAPSHOT_MAX_TEAMS];
extern int g_kbo_foreign_org_snapshot_count;
extern DWORD g_kbo_foreign_org_snapshot_tick;
extern KboLock g_kbo_foreign_org_snapshot_lock;
extern volatile LONG g_kbo_foreign_org_snapshot_rebuild_in_progress;
extern volatile LONG g_kbo_foreign_org_snapshot_mutation_generation;
extern volatile LONG g_kbo_foreign_org_snapshot_published_generation;
extern volatile LONG g_kbo_foreign_org_count_cache_generation;
extern uint32_t g_kbo_foreign_org_team_generation_team_ids[KBO_FOREIGN_ORG_TEAM_GENERATION_CACHE_SIZE];
extern LONG g_kbo_foreign_org_team_generations[KBO_FOREIGN_ORG_TEAM_GENERATION_CACHE_SIZE];
extern uint32_t g_kbo_foreign_org_parent_cache_team_ids[KBO_FOREIGN_ORG_PARENT_CACHE_SIZE];
extern uint32_t g_kbo_foreign_org_parent_cache_org_ids[KBO_FOREIGN_ORG_PARENT_CACHE_SIZE];

uint32_t kbo_foreign_org_team_id_for_team_id(uint32_t team_id);
void kbo_foreign_org_count_bump_team_generation(uint32_t team_id);
void kbo_foreign_org_count_cache_invalidate_team(uint32_t team_id);
void kbo_foreign_org_count_cache_store(uint32_t team_id, uint32_t foreign_count, uint32_t asian_count, uint32_t non_asian_count, DWORD now);
int kbo_foreign_org_count_cache_hit(uint32_t team_id, uint32_t* out_foreign_count, uint32_t* out_asian_quota_count, uint32_t* out_non_asian_foreign_count);
LONG kbo_foreign_org_snapshot_note_mutation(void);
int kbo_foreign_org_snapshot_rebuild_into(KboForeignOrgSnapshotEntry* entries, int* out_entry_count);
int kbo_foreign_org_snapshot_get(uint32_t team_id, DWORD now, uint32_t* out_foreign_count, uint32_t* out_asian_quota_count, uint32_t* out_non_asian_foreign_count, int* out_rebuilt);

#endif
