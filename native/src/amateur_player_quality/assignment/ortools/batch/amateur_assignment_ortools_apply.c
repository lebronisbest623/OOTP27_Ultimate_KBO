#include "..\amateur_assignment_ortools.h"
#include "../../../../core/logging/rule_audit.h"
#include "../../../../team/assignment/assignment/team_assignment.h"
#include "../../../../foreign/common/player_eval/foreign_waiver_player_eval.h"

int kbo_amateur_apply_post_original_batch_assignments(
    uintptr_t* league_players,
    uint32_t* league_player_ids,
    int32_t player_count,
    uint32_t league_id,
    KboAmateurAssignmentCandidate* candidates,
    int candidate_count,
    const char* reason)
{
    int moved = 0;
    int kept = 0;
    int target_not_found = 0;
    int stale_or_invalid = 0;
    int failed = 0;

    for (int32_t i = 0; i < player_count; i++) {
        uint32_t expected_player_id = league_player_ids != NULL ? league_player_ids[i] : 0u;
        uint8_t* player = expected_player_id != 0u ? kbo_find_player_by_id(expected_player_id, NULL, NULL) : NULL;
        if (player == NULL) {
            player = (uint8_t*)league_players[i];
        }
        if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
            stale_or_invalid++;
            continue;
        }
        uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        if (expected_player_id != 0u && player_id != expected_player_id) {
            stale_or_invalid++;
            continue;
        }
        uint32_t source_team_id = kbo_amateur_player_assignment_team_id(player);
        if (player_id == 0u
                || source_team_id == 0u
                || kbo_amateur_player_assignment_league_id(player) != league_id) {
            stale_or_invalid++;
            continue;
        }

        uint32_t target_team_id = kbo_amateur_batch_lookup(league_id, player_id);
        if (target_team_id == 0u || target_team_id == source_team_id) {
            kept++;
            continue;
        }

        uintptr_t target_team_ptr = kbo_amateur_candidate_team_ptr_by_id(
            candidates,
            candidate_count,
            target_team_id);
        if (target_team_ptr == 0) {
            target_not_found++;
            continue;
        }

        int called_pre_change = 0;
        int called_register = 0;
        int called_attach = 0;
        kbo_assign_player_to_team_like_ootp(
            player,
            (uint8_t*)target_team_ptr,
            league_id,
            &called_pre_change,
            &called_register,
            &called_attach);

        uint32_t after_team_id = kbo_amateur_player_assignment_team_id(player);
        if (after_team_id == target_team_id) {
            moved++;
            kbo_amateur_assignment_note_player_count_delta(league_id, source_team_id, player, -1);
            kbo_amateur_assignment_note_player_count_delta(league_id, target_team_id, player, 1);
            kbo_amateur_assignment_mark_processed(player_id, target_team_id);
        } else {
            failed++;
        }
    }
    if (moved > 0) {
        kbo_amateur_assignment_refresh_player_counts(league_id, candidates, candidate_count);
    }

    KboLogFields audit_fields;
    kbo_log_fields_init(&audit_fields);
    kbo_log_field_u32(&audit_fields, "league_id", league_id);
    kbo_log_field_i32(&audit_fields, "players", player_count);
    kbo_log_field_i32(&audit_fields, "moved", moved);
    kbo_log_field_i32(&audit_fields, "kept", kept);
    kbo_log_field_i32(&audit_fields, "target_not_found", target_not_found);
    kbo_log_field_i32(&audit_fields, "stale_or_invalid", stale_or_invalid);
    kbo_log_field_i32(&audit_fields, "failed", failed);
    kbo_rule_audit_emit_fields(
        "amateur.assignment.post_original_apply",
        failed == 0 ? "apply_batch" : "partial_batch",
        reason,
        "amateur_assignment",
        &audit_fields);
    kbo_log_runtimef(
        "amateur OR-Tools post-original batch applied league=%u moved=%d kept=%d players=%d target_not_found=%d stale_or_invalid=%d failed=%d reason=%s",
        league_id,
        moved,
        kept,
        player_count,
        target_not_found,
        stale_or_invalid,
        failed,
        reason != NULL ? reason : "");
    return moved;
}

void kbo_amateur_apply_deferred_ortools_batch(
    KboAmateurDeferredTeamAdd* deferred_team_adds,
    int32_t deferred_count,
    uint32_t league_id,
    KboAmateurAssignmentCandidate* candidates,
    int candidate_count,
    const char* reason)
{
    int applied = 0;
    int fallback_applied = 0;
    int target_not_found = 0;
    int target_add_failed = 0;
    int source_retry_applied = 0;
    int still_failed = 0;
    for (int32_t i = 0; i < deferred_count; i++) {
        KboAmateurDeferredTeamAdd* item = &deferred_team_adds[i];
        uintptr_t source_team_ptr = kbo_amateur_candidate_team_ptr_by_id(
            candidates,
            candidate_count,
            item->source_team_id);
        if (source_team_ptr == 0) {
            if (kbo_resolve_amateur_assignment_league_id_for_team_and_player(
                    (uint8_t*)item->team_ptr,
                    (uint8_t*)item->player_ptr) != item->league_id) {
                still_failed++;
                continue;
            }
            source_team_ptr = item->team_ptr;
        }
        uintptr_t target_team_ptr = source_team_ptr;
        uint32_t target_team_id = kbo_amateur_batch_lookup(item->league_id, item->player_id);
        int target_found = 1;
        if (target_team_id != 0u && target_team_id != item->source_team_id) {
            target_found = 0;
            for (int c = 0; c < candidate_count; c++) {
                if (candidates[c].team_id == target_team_id) {
                    target_team_ptr = (uintptr_t)candidates[c].team;
                    target_found = 1;
                    break;
                }
            }
            if (!target_found) {
                target_not_found++;
                fallback_applied++;
                target_team_ptr = source_team_ptr;
            }
        } else {
            fallback_applied++;
        }
        uint8_t result = kbo_team_add_player_guard_call_original(
            target_team_ptr,
            item->player_ptr,
            item->arg3,
            item->arg4,
            item->arg5,
            item->arg6,
            item->arg7,
            item->arg8);
        if (result == 0u && target_team_ptr != source_team_ptr) {
            target_add_failed++;
            result = kbo_team_add_player_guard_call_original(
                source_team_ptr,
                item->player_ptr,
                item->arg3,
                item->arg4,
                item->arg5,
                item->arg6,
                item->arg7,
                item->arg8);
            if (result != 0u) {
                source_retry_applied++;
                target_team_ptr = source_team_ptr;
            }
        }
        if (result == 0u
                && source_team_ptr != item->team_ptr
                && kbo_resolve_amateur_assignment_league_id_for_team_and_player(
                    (uint8_t*)item->team_ptr,
                    (uint8_t*)item->player_ptr) == item->league_id) {
            result = kbo_team_add_player_guard_call_original(
                item->team_ptr,
                item->player_ptr,
                item->arg3,
                item->arg4,
                item->arg5,
                item->arg6,
                item->arg7,
                item->arg8);
            if (result != 0u) {
                source_retry_applied++;
                target_team_ptr = item->team_ptr;
            }
        }
        if (result != 0u) {
            applied++;
            kbo_amateur_team_add_player_note_original_success(
                target_team_ptr,
                item->player_ptr,
                target_team_ptr == source_team_ptr ? "deferred_original_success" : "deferred_ortools_success",
                result);
        } else {
            still_failed++;
            static volatile LONG failed_log_count = 0;
            LONG slot = InterlockedIncrement(&failed_log_count);
            if (slot <= 20 || kbo_amateur_verbose_log_enabled_cached()) {
                kbo_log_runtimef(
                    "amateur deferred team-add failed player=%u source_team=%u target_team=%u target_found=%d",
                    item->player_id,
                    item->source_team_id,
                    target_team_id,
                    target_found);
            }
        }
    }
    if (deferred_count > 0) {
        do {
            KboLogFields audit_fields;
            kbo_log_fields_init(&audit_fields);
            kbo_log_field_u32(&audit_fields, "league_id", league_id);
            kbo_log_field_i32(&audit_fields, "applied", applied);
            kbo_log_field_i32(&audit_fields, "deferred", deferred_count);
            kbo_log_field_i32(&audit_fields, "fallback_original", fallback_applied);
            kbo_log_field_i32(&audit_fields, "target_not_found", target_not_found);
            kbo_log_field_i32(&audit_fields, "target_add_failed", target_add_failed);
            kbo_log_field_i32(&audit_fields, "source_retry", source_retry_applied);
            kbo_log_field_i32(&audit_fields, "still_failed", still_failed);
            kbo_rule_audit_emit_fields(
                "amateur.assignment.deferred_apply",
                "apply_batch",
                reason,
                "amateur_assignment",
                &audit_fields);
        } while (0);
        kbo_log_runtimef(
            "amateur deferred team-add batch applied league=%u applied=%d/%d fallback_original=%d target_not_found=%d target_add_failed=%d source_retry_applied=%d still_failed=%d",
            league_id,
            applied,
            deferred_count,
            fallback_applied,
            target_not_found,
            target_add_failed,
            source_retry_applied,
            still_failed);
    }
}
