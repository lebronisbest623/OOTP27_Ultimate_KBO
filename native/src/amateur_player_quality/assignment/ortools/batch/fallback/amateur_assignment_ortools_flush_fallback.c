#include "../flush/amateur_assignment_ortools_flush_support.h"
#include "../../amateur_assignment_ortools.h"
#include "../../../policy/amateur_assignment_policy_values.h"
#include "../../../../../core/logging/rule_audit.h"
#include "../../../../../team/assignment/assignment/team_assignment.h"
#include "../../../../../foreign/common/player_eval/foreign_waiver_player_eval.h"

int kbo_amateur_batch_players_have_current_assignments(uint32_t league_id, int32_t player_count)
{
    int checked = 0;
    for (int32_t i = 0; i < player_count; i++) {
        uint32_t expected_player_id = g_kbo_amateur_league_batch_player_ids[i];
        uint8_t* player = (uint8_t*)g_kbo_amateur_league_batch_players[i];
        if (player != NULL && memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
            uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
            if (expected_player_id != 0u && player_id != expected_player_id) {
                player = NULL;
            }
        } else {
            player = NULL;
        }
        if (player == NULL && expected_player_id != 0u) {
            player = kbo_find_player_by_id(expected_player_id, NULL, NULL);
        }
        if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
            return 0;
        }
        uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        int16_t age = *(int16_t*)(player + OOTP27_PLAYER_AGE_OFFSET);
        if (player_id == 0u || !kbo_amateur_player_age_eligible(league_id, age)) {
            return 0;
        }
        if (kbo_amateur_player_assignment_league_id(player) != league_id
                || kbo_amateur_player_assignment_team_id(player) == 0u) {
            return 0;
        }
        checked++;
    }
    return checked > 1;
}

void kbo_amateur_apply_deferred_original_fallback(
    KboAmateurDeferredTeamAdd* deferred_team_adds,
    int32_t deferred_count,
    uint32_t league_id,
    const char* reason)
{
    int applied = 0;
    int skipped_cross_league = 0;
    for (int32_t i = 0; i < deferred_count; i++) {
        KboAmateurDeferredTeamAdd* item = &deferred_team_adds[i];
        if (kbo_resolve_amateur_assignment_league_id_for_team_and_player(
                (uint8_t*)item->team_ptr,
                (uint8_t*)item->player_ptr) != league_id) {
            skipped_cross_league++;
            continue;
        }
        uint8_t result = kbo_team_add_player_guard_call_original(
            item->team_ptr,
            item->player_ptr,
            item->arg3,
            item->arg4,
            item->arg5,
            item->arg6,
            item->arg7,
            item->arg8);
        if (result != 0u) {
            applied++;
            kbo_amateur_team_add_player_note_original_success(
                item->team_ptr,
                item->player_ptr,
                "deferred_original_fallback",
                result);
        }
    }
    if (deferred_count > 0) {
                do {
            KboLogFields audit_fields;
            kbo_log_fields_init(&audit_fields);
            kbo_log_field_u32(&audit_fields, "league_id", league_id);
            kbo_log_field_i32(&audit_fields, "applied", applied);
            kbo_log_field_i32(&audit_fields, "deferred", deferred_count);
            kbo_log_field_i32(&audit_fields, "skipped_cross_league", skipped_cross_league);
            kbo_rule_audit_emit_fields(
                "amateur.assignment.deferred_fallback",
                "apply_original",
                reason,
                "amateur_assignment",
                &audit_fields);
        } while (0);
        kbo_log_runtimef(
            "amateur deferred team-add fallback applied league=%u applied=%d/%d skipped_cross_league=%d reason=%s",
            league_id,
            applied,
            deferred_count,
            skipped_cross_league,
            reason != NULL ? reason : "");
    }
}


