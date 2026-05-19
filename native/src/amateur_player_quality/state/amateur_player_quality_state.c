#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../api/amateur_player_quality.h"
#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../core/dates/core_current_date.h"
#include "../../core/logging/core_log.h"
#include "../../core/files/save_paths/core_save_paths.h"
#include "../../core/core_flags/api/flags_api.h"
#include "../../runtime_memory/runtime_memory.h"
#include "../../team/lookup/team_lookup.h"
#include "../../team/names/team_string.h"
#include "../internal/amateur_assignment_internal.h"

KboAmateurReputationSeed g_kbo_amateur_reputation_seeds[KBO_AMATEUR_REPUTATION_SEED_MAX];
int g_kbo_amateur_reputation_seed_count = 0;
KboSpinLock g_kbo_amateur_reputation_seed_lock = KBO_SPIN_LOCK_INIT;
LONG g_kbo_amateur_reputation_seed_loaded = 0;
char g_kbo_amateur_reputation_seed_loaded_path[MAX_PATH * 3] = {0};
KboAmateurAssignmentProcessed g_kbo_amateur_assignment_processed[KBO_AMATEUR_ASSIGNMENT_PROCESSED_MAX];
KboAmateurAssignmentProcessed g_kbo_amateur_assignment_processed_hash[KBO_AMATEUR_ASSIGNMENT_PROCESSED_HASH_MAX];
LONG g_kbo_amateur_assignment_processed_count = 0;
LONG g_kbo_amateur_assignment_processed_hash_count = 0;
KboAmateurAssignmentCandidate g_kbo_amateur_assignment_high_school_candidates[KBO_AMATEUR_ASSIGNMENT_TEAM_MAX];
KboAmateurAssignmentCandidate g_kbo_amateur_assignment_college_candidates[KBO_AMATEUR_ASSIGNMENT_TEAM_MAX];
KboAmateurAssignmentProcessed g_kbo_amateur_assignment_rejected_targets[KBO_AMATEUR_ASSIGNMENT_REJECTED_TARGET_MAX];
int g_kbo_amateur_assignment_high_school_count = -1;
int g_kbo_amateur_assignment_college_count = -1;
LONG g_kbo_amateur_assignment_rejected_target_count = 0;
KboSpinLock g_kbo_amateur_assignment_candidate_lock = KBO_SPIN_LOCK_INIT;
KboAmateurResolvedTeamReputation g_kbo_amateur_resolved_team_reputations[KBO_AMATEUR_ASSIGNMENT_TEAM_MAX];
int g_kbo_amateur_resolved_team_reputation_count = 0;
uint32_t g_kbo_amateur_reputation_last_update_high_school_year = 0u;
uint32_t g_kbo_amateur_reputation_last_update_college_year = 0u;

int* kbo_amateur_assignment_count_ptr_for_league(uint32_t league_id)
{
    if (league_id == KBO_HIGH_SCHOOL_LEAGUE_ID) {
        return &g_kbo_amateur_assignment_high_school_count;
    }
    if (league_id == KBO_COLLEGE_LEAGUE_ID) {
        return &g_kbo_amateur_assignment_college_count;
    }
    return NULL;
}

KboAmateurAssignmentCandidate* kbo_amateur_assignment_cache_for_league(uint32_t league_id)
{
    if (league_id == KBO_HIGH_SCHOOL_LEAGUE_ID) {
        return g_kbo_amateur_assignment_high_school_candidates;
    }
    if (league_id == KBO_COLLEGE_LEAGUE_ID) {
        return g_kbo_amateur_assignment_college_candidates;
    }
    return NULL;
}

int kbo_amateur_player_age_eligible(uint32_t league_id, int16_t age);

