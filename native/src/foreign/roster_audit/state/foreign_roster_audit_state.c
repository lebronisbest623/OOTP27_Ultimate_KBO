#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/dates/core_current_date.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../core/logging/core_log.h"
#include "../../../core/files/save_paths/core_save_paths.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../team/lookup/team_lookup.h"
#include "../../common/player_eval/foreign_waiver_player_eval.h"
#include "../paths/foreign_roster_audit_paths.h"

#include "../internal/foreign_roster_audit_internal.h"

#define KBO_FOREIGN_ROSTER_AUDIT_MAX 8192

typedef struct KboForeignRosterAuditState {
    uint32_t player_id;
    uint32_t nation_id;
    uint32_t current_team_id;
    uint32_t active_team_id;
    uint32_t original_team_id;
    uint32_t current_league_id;
    uint32_t loan_team_id;
    uint8_t restricted;
    uint8_t secondary_restricted;
    uint8_t dfa;
    uint8_t loan_active;
    uint8_t injury_active;
    int score;
    uint32_t seen_generation;
} KboForeignRosterAuditState;

KboForeignRosterAuditState g_kbo_foreign_roster_audit[KBO_FOREIGN_ROSTER_AUDIT_MAX] = {{0}};
int g_kbo_foreign_roster_audit_count = 0;
uint32_t g_kbo_foreign_roster_audit_generation = 0u;
char g_kbo_foreign_roster_audit_save_path[MAX_PATH] = {0};

const char* kbo_foreign_roster_audit_change_type(
    const KboForeignRosterAuditState* old_state,
    const KboForeignRosterAuditState* new_state)
{
    if (old_state == NULL) {
        return "NEW_FOREIGN";
    }
    if (old_state->current_team_id != 0u && new_state->current_team_id == 0u
            && old_state->active_team_id != 0u && new_state->active_team_id == 0u) {
        return "RELEASE_OBSERVED";
    }
    if (old_state->current_team_id != 0u && new_state->current_team_id == 0u) {
        return "CURRENT_TEAM_CLEARED";
    }
    if (old_state->active_team_id != 0u && new_state->active_team_id == 0u) {
        return "ACTIVE_TEAM_CLEARED";
    }
    if (old_state->current_team_id == 0u && new_state->current_team_id != 0u) {
        return "CURRENT_TEAM_ASSIGNED";
    }
    if (old_state->restricted != new_state->restricted
            || old_state->secondary_restricted != new_state->secondary_restricted
            || old_state->dfa != new_state->dfa
            || old_state->loan_active != new_state->loan_active
            || old_state->injury_active != new_state->injury_active) {
        return "STATUS_CHANGED";
    }
    if (old_state->current_league_id != new_state->current_league_id
            || old_state->loan_team_id != new_state->loan_team_id) {
        return "ASSIGNMENT_CHANGED";
    }
    return "STATE_CHANGED";
}

KboForeignRosterAuditState* kbo_find_foreign_roster_audit_state(uint32_t player_id)
{
    if (player_id == 0u) {
        return NULL;
    }
    for (int i = 0; i < g_kbo_foreign_roster_audit_count; i++) {
        if (g_kbo_foreign_roster_audit[i].player_id == player_id) {
            return &g_kbo_foreign_roster_audit[i];
        }
    }
    return NULL;
}

