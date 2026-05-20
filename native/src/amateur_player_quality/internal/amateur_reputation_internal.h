#ifndef KBOFIX_SRC_AMATEUR_PLAYER_QUALITY_INTERNAL_AMATEUR_REPUTATION_INTERNAL_H_
#define KBOFIX_SRC_AMATEUR_PLAYER_QUALITY_INTERNAL_AMATEUR_REPUTATION_INTERNAL_H_

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../api/amateur_player_quality.h"
#include "../assignment/policy/amateur_assignment_policy_values.h"
#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../core/dates/core_current_date.h"
#include "../../core/logging/core_log.h"
#include "../../core/files/save_paths/core_save_paths.h"
#include "../../core/core_flags/api/flags_api.h"
#include "../../core/sync/spin_lock.h"
#include "../../runtime_memory/runtime_memory.h"
#include "../../team/add_player_guard/team_add_player_guard.h"
#include "../../team/lookup/team_lookup.h"
#include "../../team/names/team_string.h"
#include "../../competitive_balance_tax/draft/penalty/cbt_draft_penalty.h"

extern KboAmateurReputationSeed g_kbo_amateur_reputation_seeds[KBO_AMATEUR_REPUTATION_SEED_MAX];
extern int g_kbo_amateur_reputation_seed_count;
extern KboSpinLock g_kbo_amateur_reputation_seed_lock;
extern LONG g_kbo_amateur_reputation_seed_loaded;
extern char g_kbo_amateur_reputation_seed_loaded_path[MAX_PATH * 3];
extern KboAmateurResolvedTeamReputation g_kbo_amateur_resolved_team_reputations[KBO_AMATEUR_ASSIGNMENT_TEAM_MAX];
extern int g_kbo_amateur_resolved_team_reputation_count;
extern uint32_t g_kbo_amateur_reputation_last_update_high_school_year;
extern uint32_t g_kbo_amateur_reputation_last_update_college_year;

void kbo_lock_amateur_reputation_seeds(void);
void kbo_unlock_amateur_reputation_seeds(void);
int kbo_get_reputation_seed_path(const char* file_name, char* out, size_t out_size);
int kbo_amateur_reputation_add_seed(
    uint32_t league_id,
    uint32_t team_id,
    const char* team_abbr,
    const char* team_name,
    const char* nick_name,
    uint32_t reputation);
int kbo_load_reputation_seed_path_into_cache(const char* path);
int kbo_get_amateur_reputation_history_path(char* out, size_t out_size);
int kbo_amateur_reputation_history_has_year(uint32_t league_id, uint32_t year);
int kbo_apply_amateur_reputation_history_to_cache(void);
void kbo_ensure_amateur_reputation_seeds_loaded(void);
int kbo_find_amateur_team_reputation_for_league(uint32_t league_id, uint32_t team_id, uint8_t* out_reputation);
int kbo_find_amateur_team_reputation_by_memory_team(uint32_t league_id, uint8_t* team, uint8_t* out_reputation);
uint32_t kbo_resolve_amateur_assignment_league_id_for_team_ptr(uint8_t* team);
uint32_t kbo_resolve_amateur_assignment_league_id_for_team_and_player(uint8_t* team, uint8_t* player);
int kbo_compare_amateur_reputation_update_rows(const void* a, const void* b);
uint8_t kbo_amateur_reputation_clamp_for_league(uint32_t league_id, int32_t value);
void kbo_apply_amateur_reputation_balanced_deltas(
    uint32_t league_id,
    KboAmateurReputationUpdateRow* rows,
    int row_count,
    int32_t* out_raw_delta_sum,
    int32_t* out_balance_adjustment,
    int32_t* out_final_delta_sum);
int kbo_append_amateur_reputation_history(
    uint32_t league_id,
    const KboAmateurReputationUpdateRow* rows,
    int row_count,
    const char* source,
    uint32_t year);
int kbo_update_amateur_reputation_for_league(uint32_t league_id, const char* source, uint32_t year);
void kbo_update_amateur_reputation_from_team_records(const char* source);

#endif
