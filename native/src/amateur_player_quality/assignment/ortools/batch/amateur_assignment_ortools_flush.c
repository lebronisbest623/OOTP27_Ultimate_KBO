#include "..\amateur_assignment_ortools_internal.h"
#include "../../policy/amateur_assignment_policy_values.h"
#include "../../../../core/optimizer/kbo_optimizer.h"
#include "../../../../core/logging/rule_audit.h"
#include "../../../../team/assignment/assignment/team_assignment.h"

static void kbo_amateur_audit_ortools_batch(
    const char* decision,
    const char* reason_code,
    const char* source,
    uint32_t league_id,
    int players,
    int teams,
    int candidates,
    int assignments,
    int deferred,
    int force,
    uint64_t idle_ms)
{
    KboLogFields audit_fields;
    kbo_log_fields_init(&audit_fields);
    kbo_log_field_u32(&audit_fields, "league_id", league_id);
    if (players >= 0) { kbo_log_field_i32(&audit_fields, "players", players); }
    if (teams >= 0) { kbo_log_field_i32(&audit_fields, "teams", teams); }
    if (candidates >= 0) { kbo_log_field_i32(&audit_fields, "candidates", candidates); }
    if (assignments >= 0) { kbo_log_field_i32(&audit_fields, "assignments", assignments); }
    if (deferred >= 0) { kbo_log_field_i32(&audit_fields, "deferred", deferred); }
    if (force >= 0) { kbo_log_field_i32(&audit_fields, "force", force); }
    if (idle_ms > 0u) { kbo_log_field_u64(&audit_fields, "idle_ms", idle_ms); }
    kbo_rule_audit_emit_fields(
        "amateur.assignment.ortools_batch",
        decision,
        reason_code,
        source,
        &audit_fields);
}

static int kbo_amateur_batch_players_have_current_assignments(uint32_t league_id, int32_t player_count)
{
    int checked = 0;
    for (int32_t i = 0; i < player_count; i++) {
        uint8_t* player = (uint8_t*)g_kbo_amateur_league_batch_players[i];
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

static int kbo_amateur_apply_post_original_batch_assignments(
    uintptr_t* league_players,
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
        uint8_t* player = (uint8_t*)league_players[i];
        if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
            stale_or_invalid++;
            continue;
        }
        uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
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

int kbo_amateur_flush_league_batch_ortools(const char* reason, int force)
{
    uint32_t league_id = 0u;
    int32_t accumulated_players = 0;
    int32_t optimizer_player_count = 0;
    int32_t accumulated_teams = 0;
    int32_t deferred_count = 0;
    uintptr_t league_players[KBO_AMATEUR_LEAGUE_BATCH_PLAYER_MAX];
    uintptr_t league_source_teams[KBO_AMATEUR_LEAGUE_BATCH_PLAYER_MAX];
    KboAmateurDeferredTeamAdd deferred_team_adds[KBO_AMATEUR_LEAGUE_BATCH_PLAYER_MAX];

    kbo_amateur_batch_lock();
    league_id = g_kbo_amateur_league_batch_league_id;
    accumulated_players = g_kbo_amateur_league_batch_player_count;
    accumulated_teams = g_kbo_amateur_league_batch_team_count;
    deferred_count = g_kbo_amateur_deferred_team_add_count;
    const KboAmateurPlayerQualityPolicy* policy = kbo_amateur_player_quality_policy();
    DWORD idle_ms = (accumulated_teams >= policy->ortools_batch_near_complete_teams
            || accumulated_players >= policy->ortools_batch_near_complete_players)
        ? (DWORD)policy->ortools_batch_near_complete_idle_ms
        : (DWORD)policy->ortools_batch_idle_ms;
    int waiting_for_original_adds = deferred_count <= 0
        && accumulated_players > 1
        && !force
        && !kbo_amateur_batch_players_have_current_assignments(league_id, accumulated_players);
    if ((league_id != KBO_HIGH_SCHOOL_LEAGUE_ID && league_id != KBO_COLLEGE_LEAGUE_ID)
            || accumulated_players <= 1
            || accumulated_teams <= 0
            || waiting_for_original_adds
            || (!force && GetTickCount() - g_kbo_amateur_league_batch_last_tick < idle_ms)) {
        if (force || waiting_for_original_adds) {
            kbo_amateur_audit_ortools_batch(
                "skip", waiting_for_original_adds ? "pending_original_team_add" : "batch_not_ready", reason, league_id,
                accumulated_players, accumulated_teams, -1, -1, -1,
                force ? 1 : 0, (uint64_t)idle_ms);
        }
        kbo_amateur_batch_unlock();
        return 0;
    }
    memcpy(league_players, g_kbo_amateur_league_batch_players, (size_t)accumulated_players * sizeof(uintptr_t));
    memcpy(league_source_teams, g_kbo_amateur_league_batch_source_teams, (size_t)accumulated_players * sizeof(uintptr_t));
    memcpy(deferred_team_adds, g_kbo_amateur_deferred_team_adds, (size_t)deferred_count * sizeof(deferred_team_adds[0]));
    kbo_amateur_league_batch_clear(league_id);
    kbo_amateur_batch_unlock();

    optimizer_player_count = accumulated_players;
    if (deferred_count > 0) {
        memset(league_players, 0, sizeof(league_players));
        memset(league_source_teams, 0, sizeof(league_source_teams));
        optimizer_player_count = 0;
        for (int32_t i = 0; i < deferred_count && optimizer_player_count < KBO_AMATEUR_LEAGUE_BATCH_PLAYER_MAX; i++) {
            KboAmateurDeferredTeamAdd* item = &deferred_team_adds[i];
            if (item->league_id != league_id || item->player_id == 0u || item->player_ptr == 0 || item->team_ptr == 0) {
                continue;
            }
            if (kbo_amateur_local_player_list_has_id(league_players, optimizer_player_count, item->player_id)) {
                continue;
            }
            league_players[optimizer_player_count] = item->player_ptr;
            league_source_teams[optimizer_player_count] = item->team_ptr;
            optimizer_player_count++;
        }
    }
    if (optimizer_player_count <= 1) {
        kbo_amateur_audit_ortools_batch(
            "fallback", "no_optimizer_players", reason, league_id,
            optimizer_player_count, accumulated_teams, -1, -1, deferred_count, -1, 0u);
        kbo_amateur_apply_deferred_original_fallback(deferred_team_adds, deferred_count, league_id, "no_optimizer_players");
        return 0;
    }

    KboAmateurAssignmentCandidate* candidates = NULL;
    int count = kbo_amateur_assignment_get_cached_candidates(league_id, &candidates);
    if (count <= 1 || candidates == NULL) {
        kbo_amateur_audit_ortools_batch(
            "fallback", "no_candidates", reason, league_id,
            optimizer_player_count, accumulated_teams, count, -1, deferred_count, -1, 0u);
        kbo_amateur_apply_deferred_original_fallback(deferred_team_adds, deferred_count, league_id, "no_candidates");
        return 0;
    }

    char request_path[MAX_PATH * 3] = {0};
    char result_path[MAX_PATH * 3] = {0};
    if (!kbo_get_save_scoped_data_file("amateur_assignment_ortools_batch_request.csv", request_path, sizeof(request_path))
            || !kbo_get_save_scoped_data_file("amateur_assignment_ortools_batch_result.csv", result_path, sizeof(result_path))) {
        kbo_amateur_audit_ortools_batch(
            "fallback", "path_unavailable", reason, league_id,
            optimizer_player_count, accumulated_teams, count, -1, deferred_count, -1, 0u);
        kbo_amateur_apply_deferred_original_fallback(deferred_team_adds, deferred_count, league_id, "path_unavailable");
        return 0;
    }

    if (!kbo_amateur_ortools_write_batch_request(
            request_path,
            league_players,
            league_source_teams,
            optimizer_player_count,
            league_id,
            candidates,
            count,
            deferred_count > 0)) {
        kbo_amateur_audit_ortools_batch(
            "fallback", "write_failed", reason, league_id,
            optimizer_player_count, accumulated_teams, count, -1, deferred_count, -1, 0u);
        kbo_amateur_apply_deferred_original_fallback(deferred_team_adds, deferred_count, league_id, "write_failed");
        return 0;
    }
    if (!kbo_optimizer_run_mode("amateur_assignment", request_path, result_path, 30000u)) {
        kbo_amateur_audit_ortools_batch(
            "fallback", "ortools_failed", reason, league_id,
            optimizer_player_count, accumulated_teams, count, -1, deferred_count, -1, 0u);
        kbo_amateur_apply_deferred_original_fallback(deferred_team_adds, deferred_count, league_id, "ortools_failed");
        return 0;
    }
    int assigned = kbo_amateur_ortools_read_batch_result(result_path, league_id);
    kbo_amateur_audit_ortools_batch(
        "optimize", "result_loaded", reason, league_id,
        optimizer_player_count, accumulated_teams, count, assigned, deferred_count, -1, 0u);
    kbo_log_runtimef(
        "amateur OR-Tools league batch prepared league=%u teams=%d/%d players=%d assignments=%d deferred=%d reason=%s",
        league_id,
        accumulated_teams,
        count,
        optimizer_player_count,
        assigned,
        deferred_count,
        reason != NULL ? reason : "");
    if (deferred_count <= 0) {
        (void)kbo_amateur_apply_post_original_batch_assignments(
            league_players,
            optimizer_player_count,
            league_id,
            candidates,
            count,
            reason);
        return assigned;
    }
    int applied = 0;
    int fallback_applied = 0;
    int target_not_found = 0;
    int target_add_failed = 0;
    int source_retry_applied = 0;
    int still_failed = 0;
    for (int32_t i = 0; i < deferred_count; i++) {
        KboAmateurDeferredTeamAdd* item = &deferred_team_adds[i];
        uintptr_t source_team_ptr = kbo_amateur_candidate_team_ptr_by_id(candidates, count, item->source_team_id);
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
            for (int c = 0; c < count; c++) {
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
    return assigned;
}
