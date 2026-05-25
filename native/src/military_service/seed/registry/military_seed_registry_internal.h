#ifndef NATIVE_SRC_MILITARY_SERVICE_MILITARY_SEED_REGISTRY_C_INTERNAL_H
#define NATIVE_SRC_MILITARY_SERVICE_MILITARY_SEED_REGISTRY_C_INTERNAL_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/files/atomic/core_atomic_file.h"
#include "../../../core/files/save_paths/platform/core_path_io.h"
#include "../../../core/logging/core_log.h"
#include "../../../core/sync/lock.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../team/lookup/team_lookup.h"
#include "../../players/state/military_player_state.h"
#include "military_seed_registry.h"
#include "../../calendar/military_service_date.h"
#include "../paths/military_seed_paths.h"

extern KboMilitaryServiceSeed g_kbo_military_service_seeds[KBO_MILITARY_SERVICE_SEED_MAX];
extern int g_kbo_military_service_seed_count;
extern KboLock g_kbo_military_service_seed_lock;
extern LONG g_kbo_military_service_seed_loaded;
extern ULONGLONG g_kbo_military_service_seed_last_resolve_tick;
extern char g_kbo_military_service_seed_loaded_key[MAX_PATH * 3];

void kbo_lock_military_service_seeds(void);
void kbo_unlock_military_service_seeds(void);
uint8_t* kbo_military_find_team_from_seed_code(const char* team_code);
int kbo_add_military_service_seed_locked(const KboMilitaryServiceSeed* seed);
int kbo_load_military_service_seed_file_locked(const char* path);
int kbo_load_military_service_resolved_cache_locked(void);
int kbo_persist_military_service_resolved_cache_locked(void);
uint32_t kbo_military_resolve_player_id_from_players_dat_record_start(
    const uint8_t* raw,
    size_t read,
    size_t key_pos);
uint32_t kbo_resolve_military_service_seed_key_from_players_dat(const char* key);
int kbo_resolve_military_service_seeds_locked(void);
void kbo_ensure_military_service_seeds_loaded(void);
int kbo_snapshot_military_service_seeds(KboMilitaryServiceSeed* out, int max_count);
int kbo_seed_registry_service_team_id_matches(
    uint32_t team_id,
    uint32_t service_team_id,
    uint32_t sang_id,
    uint32_t kpb_id);
int kbo_military_original_team_from_seed(
    uint32_t player_id,
    uint32_t service_team_id,
    uint32_t sang_id,
    uint32_t kpb_id,
    uint32_t* out_original_team_id,
    uint32_t* out_original_league_id);

#endif
