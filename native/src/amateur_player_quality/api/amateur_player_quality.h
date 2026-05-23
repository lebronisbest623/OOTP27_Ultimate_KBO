#ifndef KBOFIX_SRC_AMATEUR_PLAYER_QUALITY_AMATEUR_PLAYER_QUALITY_H_
#define KBOFIX_SRC_AMATEUR_PLAYER_QUALITY_AMATEUR_PLAYER_QUALITY_H_

#include <stdint.h>
#include <stddef.h>
#include <windows.h>

#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../core/league_roles/kbo_league_roles.h"

#define KBO_COLLEGE_LEAGUE_ID kbo_league_role_college_league_id()
#define KBO_HIGH_SCHOOL_LEAGUE_ID kbo_league_role_high_school_league_id()
#define KBO_AMATEUR_REPUTATION_SEED_MAX 384
#define KBO_AMATEUR_ASSIGNMENT_PROCESSED_MAX 16384
#define KBO_AMATEUR_ASSIGNMENT_PROCESSED_HASH_MAX 32768
#define KBO_AMATEUR_ASSIGNMENT_PROCESSED_HASH_MASK (KBO_AMATEUR_ASSIGNMENT_PROCESSED_HASH_MAX - 1)
#define KBO_AMATEUR_ASSIGNMENT_REJECTED_TARGET_MAX 128
#define KBO_AMATEUR_POSITION_BUCKET_COUNT 10
#define KBO_AMATEUR_TEAM_REGULAR_WINS_OFFSET   OOTP27_KBO_TEAM_REGULAR_WINS_OFFSET
#define KBO_AMATEUR_TEAM_REGULAR_LOSSES_OFFSET OOTP27_KBO_TEAM_REGULAR_LOSSES_OFFSET
#define KBO_AMATEUR_TEAM_REGULAR_TIES_OFFSET   OOTP27_KBO_TEAM_REGULAR_TIES_OFFSET
#define KBO_AMATEUR_ASSIGNMENT_TEAM_MAX 512

typedef struct KboAmateurReputationSeed {
    uint32_t league_id;
    uint32_t team_id;
    char team_abbr[32];
    char team_name[96];
    char nick_name[64];
    uint8_t reputation;
} KboAmateurReputationSeed;

typedef struct KboAmateurReputationUpdateRow {
    uint32_t league_id;
    uint32_t team_id;
    uint8_t old_reputation;
    uint8_t new_reputation;
    uint16_t wins;
    uint16_t losses;
    uint16_t ties;
    int32_t score;
} KboAmateurReputationUpdateRow;

typedef struct KboAmateurAssignmentProcessed {
    uint32_t player_id;
    uint32_t team_id;
} KboAmateurAssignmentProcessed;

typedef struct KboAmateurAssignmentCandidate {
    uint8_t* team;
    uint32_t team_id;
    uint8_t reputation;
    int32_t player_count;
    int32_t hitter_count;
    int32_t pitcher_count;
    int32_t catcher_count;
    int32_t infielder_count;
    int32_t outfielder_count;
    int32_t first_base_count;
    int32_t second_base_count;
    int32_t third_base_count;
    int32_t shortstop_count;
    int32_t left_field_count;
    int32_t center_field_count;
    int32_t right_field_count;
    int32_t designated_hitter_count;
} KboAmateurAssignmentCandidate;

typedef struct KboAmateurResolvedTeamReputation {
    uint8_t* team;
    uint32_t league_id;
    uint8_t reputation;
} KboAmateurResolvedTeamReputation;

extern KboAmateurReputationSeed g_kbo_amateur_reputation_seeds[KBO_AMATEUR_REPUTATION_SEED_MAX];
extern int g_kbo_amateur_reputation_seed_count;

void kbo_lock_amateur_reputation_seeds(void);
void kbo_unlock_amateur_reputation_seeds(void);
void kbo_ensure_amateur_reputation_seeds_loaded(void);
int kbo_get_amateur_reputation_history_path(char* out, size_t out_size);

void kbo_update_amateur_reputation_from_team_records(const char* source);
void start_kbo_amateur_reputation_update_thread(void);
uint32_t kbo_resolve_amateur_assignment_league_id_for_team_ptr(uint8_t* team);
uint32_t kbo_resolve_amateur_assignment_league_id_for_team_and_player(uint8_t* team, uint8_t* player);
int kbo_amateur_player_age_eligible(uint32_t league_id, int16_t age);
void kbo_amateur_assignment_mark_rejected_target(uint32_t league_id, uint32_t team_id);
int kbo_amateur_generation_team_add_caller(uint32_t caller_rva);
uintptr_t kbo_amateur_team_add_player_reroute_before_original(uintptr_t team_ptr, uintptr_t player_ptr, const char* source);
void kbo_amateur_team_add_player_note_original_success(uintptr_t team_ptr, uintptr_t player_ptr, const char* source, int original_result);
int kbo_amateur_defer_team_add_if_generation(
    uint32_t caller_rva,
    uintptr_t team_ptr,
    uintptr_t player_ptr,
    uintptr_t arg3,
    uintptr_t arg4,
    uintptr_t arg5,
    uintptr_t arg6,
    uintptr_t arg7,
    uintptr_t arg8);
void ootp_kbo_amateur_assignment_batch_probe(uintptr_t player_list_ptr, int32_t player_count, uintptr_t source_team_ptr);

#endif
