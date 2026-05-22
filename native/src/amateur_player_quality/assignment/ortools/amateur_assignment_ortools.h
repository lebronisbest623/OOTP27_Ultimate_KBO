#ifndef KBOFIX_AMATEUR_ASSIGNMENT_ORTOOLS_H_
#define KBOFIX_AMATEUR_ASSIGNMENT_ORTOOLS_H_

#include "../../internal/amateur_assignment_internal.h"

typedef struct KboAmateurBatchAssignment {
    uint32_t player_id;
    uint32_t league_id;
    uint32_t target_team_id;
} KboAmateurBatchAssignment;

#define KBO_AMATEUR_LEAGUE_BATCH_PLAYER_MAX 4096
#define KBO_AMATEUR_LEAGUE_BATCH_TEAM_MAX 256

typedef struct KboAmateurDeferredTeamAdd {
    uintptr_t team_ptr;
    uintptr_t player_ptr;
    uintptr_t arg3;
    uintptr_t arg4;
    uintptr_t arg5;
    uintptr_t arg6;
    uintptr_t arg7;
    uintptr_t arg8;
    uint32_t caller_rva;
    uint32_t league_id;
    uint32_t source_team_id;
    uint32_t player_id;
} KboAmateurDeferredTeamAdd;

extern KboAmateurBatchAssignment g_kbo_amateur_batch_assignments[KBO_AMATEUR_LEAGUE_BATCH_PLAYER_MAX];
extern LONG g_kbo_amateur_batch_assignment_count;
extern KboLock g_kbo_amateur_batch_assignment_lock;
extern uintptr_t g_kbo_amateur_league_batch_players[KBO_AMATEUR_LEAGUE_BATCH_PLAYER_MAX];
extern uintptr_t g_kbo_amateur_league_batch_source_teams[KBO_AMATEUR_LEAGUE_BATCH_PLAYER_MAX];
extern uint32_t g_kbo_amateur_league_batch_player_ids[KBO_AMATEUR_LEAGUE_BATCH_PLAYER_MAX];
extern uint32_t g_kbo_amateur_league_batch_source_team_ids[KBO_AMATEUR_LEAGUE_BATCH_PLAYER_MAX];
extern uint32_t g_kbo_amateur_league_batch_team_ids[KBO_AMATEUR_LEAGUE_BATCH_TEAM_MAX];
extern uint32_t g_kbo_amateur_league_batch_league_id;
extern int32_t g_kbo_amateur_league_batch_player_count;
extern int32_t g_kbo_amateur_league_batch_team_count;
extern int32_t g_kbo_amateur_league_batch_candidate_count;
extern DWORD g_kbo_amateur_league_batch_last_tick;
extern volatile LONG g_kbo_amateur_league_batch_flush_thread_started;
extern KboAmateurDeferredTeamAdd g_kbo_amateur_deferred_team_adds[KBO_AMATEUR_LEAGUE_BATCH_PLAYER_MAX];
extern int32_t g_kbo_amateur_deferred_team_add_count;

int kbo_amateur_reroute_disabled_cached(void);
int kbo_amateur_deferred_add_disabled_cached(void);
int kbo_amateur_verbose_log_enabled_cached(void);
void kbo_amateur_batch_lock(void);
void kbo_amateur_batch_unlock(void);
uint32_t kbo_amateur_batch_lookup(uint32_t league_id, uint32_t player_id);
void kbo_amateur_league_batch_clear(uint32_t league_id);
int kbo_amateur_league_batch_has_team(uint32_t team_id);
int32_t kbo_amateur_league_batch_find_player_index(uint32_t player_id);
int kbo_amateur_league_batch_has_player(uint32_t player_id);
int kbo_amateur_local_player_list_has_id(uintptr_t* players, int32_t count, uint32_t player_id);
uintptr_t kbo_amateur_candidate_team_ptr_by_id(
    KboAmateurAssignmentCandidate* candidates,
    int count,
    uint32_t team_id);
uint32_t kbo_amateur_batch_resolve_source_team_id(uint8_t* player, uintptr_t source_team_ptr);
int kbo_amateur_batch_source_index(uint32_t* team_ids, int count, uint32_t team_id);
int kbo_amateur_ortools_write_request(
    const char* path,
    uint8_t* player,
    uint32_t league_id,
    uint32_t current_team_id,
    uint8_t current_reputation,
    int32_t quality_score,
    int player_tier,
    int32_t target_reputation,
    KboAmateurAssignmentCandidate* candidates,
    int count);
int kbo_amateur_ortools_write_batch_request(
    const char* path,
    uintptr_t* players,
    uintptr_t* source_teams,
    uint32_t* player_ids,
    uint32_t* batch_source_team_ids,
    int32_t player_count,
    uint32_t league_id,
    KboAmateurAssignmentCandidate* candidates,
    int count,
    int incoming_batch);
uint32_t kbo_amateur_ortools_read_result(const char* result_path);
int kbo_amateur_ortools_read_batch_result(const char* result_path, uint32_t league_id);
int kbo_amateur_apply_post_original_batch_assignments(
    uintptr_t* league_players,
    uint32_t* league_player_ids,
    int32_t player_count,
    uint32_t league_id,
    KboAmateurAssignmentCandidate* candidates,
    int candidate_count,
    const char* reason);
void kbo_amateur_apply_deferred_original_fallback(
    KboAmateurDeferredTeamAdd* deferred_team_adds,
    int32_t deferred_count,
    uint32_t league_id,
    const char* reason);
void kbo_amateur_apply_deferred_ortools_batch(
    KboAmateurDeferredTeamAdd* deferred_team_adds,
    int32_t deferred_count,
    uint32_t league_id,
    KboAmateurAssignmentCandidate* candidates,
    int candidate_count,
    const char* reason);
int kbo_amateur_flush_league_batch_ortools(const char* reason, int force);
int kbo_amateur_start_league_switch_flush_locked(const char* reason, uint32_t next_league_id);
void kbo_amateur_start_league_batch_flush_thread(void);

#endif
