#include "..\amateur_assignment_ortools.h"
#include "flush/amateur_assignment_ortools_flush_support.h"
#include "../../../../core/core_flags/api/flags_api.h"
#include "../../policy/amateur_assignment_policy_values.h"
#include "../../../../core/optimizer/kbo_optimizer.h"
#include "../../../../core/logging/rule_audit.h"
#include "../../../../team/assignment/assignment/team_assignment.h"
#include "../../../../foreign/common/player_eval/foreign_waiver_player_eval.h"

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


typedef struct KboAmateurLeagueBatchFlushJob {
    char reason[64];
    uint32_t league_id;
    int32_t accumulated_players;
    int32_t accumulated_teams;
    int32_t deferred_count;
    uintptr_t league_players[KBO_AMATEUR_LEAGUE_BATCH_PLAYER_MAX];
    uintptr_t league_source_teams[KBO_AMATEUR_LEAGUE_BATCH_PLAYER_MAX];
    uint32_t league_player_ids[KBO_AMATEUR_LEAGUE_BATCH_PLAYER_MAX];
    uint32_t league_source_team_ids[KBO_AMATEUR_LEAGUE_BATCH_PLAYER_MAX];
    KboAmateurDeferredTeamAdd deferred_team_adds[KBO_AMATEUR_LEAGUE_BATCH_PLAYER_MAX];
} KboAmateurLeagueBatchFlushJob;

static KboLock g_kbo_amateur_batch_flush_work_lock = KBO_LOCK_INIT;

static void kbo_amateur_flush_job_set_reason(KboAmateurLeagueBatchFlushJob* job, const char* reason)
{
    if (job == NULL) {
        return;
    }
    snprintf(
        job->reason,
        sizeof(job->reason),
        "%s",
        (reason != NULL && reason[0] != '\0') ? reason : "unknown");
}

static void kbo_amateur_fill_flush_job_from_current_batch_locked(
    KboAmateurLeagueBatchFlushJob* job,
    const char* reason,
    uint32_t league_id,
    int32_t accumulated_players,
    int32_t accumulated_teams,
    int32_t deferred_count)
{
    memset(job, 0, sizeof(*job));
    kbo_amateur_flush_job_set_reason(job, reason);
    job->league_id = league_id;
    job->accumulated_players = accumulated_players;
    job->accumulated_teams = accumulated_teams;
    job->deferred_count = deferred_count;
    memcpy(job->league_players, g_kbo_amateur_league_batch_players, (size_t)accumulated_players * sizeof(uintptr_t));
    memcpy(job->league_source_teams, g_kbo_amateur_league_batch_source_teams, (size_t)accumulated_players * sizeof(uintptr_t));
    memcpy(job->league_player_ids, g_kbo_amateur_league_batch_player_ids, (size_t)accumulated_players * sizeof(uint32_t));
    memcpy(job->league_source_team_ids, g_kbo_amateur_league_batch_source_team_ids, (size_t)accumulated_players * sizeof(uint32_t));
    memcpy(job->deferred_team_adds, g_kbo_amateur_deferred_team_adds, (size_t)deferred_count * sizeof(job->deferred_team_adds[0]));
}

static int kbo_amateur_process_league_batch_snapshot(KboAmateurLeagueBatchFlushJob* job)
{
    if (job == NULL) {
        return 0;
    }

    int result = 0;
    kbo_lock_enter(&g_kbo_amateur_batch_flush_work_lock);

    uint32_t league_id = job->league_id;
    int32_t accumulated_players = job->accumulated_players;
    int32_t accumulated_teams = job->accumulated_teams;
    int32_t deferred_count = job->deferred_count;
    int32_t optimizer_player_count = accumulated_players;

    if (deferred_count > 0) {
        memset(job->league_players, 0, sizeof(job->league_players));
        memset(job->league_source_teams, 0, sizeof(job->league_source_teams));
        memset(job->league_player_ids, 0, sizeof(job->league_player_ids));
        memset(job->league_source_team_ids, 0, sizeof(job->league_source_team_ids));
        optimizer_player_count = 0;
        for (int32_t i = 0; i < deferred_count && optimizer_player_count < KBO_AMATEUR_LEAGUE_BATCH_PLAYER_MAX; i++) {
            KboAmateurDeferredTeamAdd* item = &job->deferred_team_adds[i];
            if (item->league_id != league_id || item->player_id == 0u || item->player_ptr == 0 || item->team_ptr == 0) {
                continue;
            }
            if (kbo_amateur_local_player_list_has_id(job->league_players, optimizer_player_count, item->player_id)) {
                continue;
            }
            job->league_players[optimizer_player_count] = item->player_ptr;
            job->league_source_teams[optimizer_player_count] = item->team_ptr;
            job->league_player_ids[optimizer_player_count] = item->player_id;
            job->league_source_team_ids[optimizer_player_count] = item->source_team_id;
            optimizer_player_count++;
        }
    }
    if (optimizer_player_count <= 1) {
        kbo_amateur_audit_ortools_batch(
            "fallback", "no_optimizer_players", job->reason, league_id,
            optimizer_player_count, accumulated_teams, -1, -1, deferred_count, -1, 0u);
        kbo_amateur_apply_deferred_original_fallback(job->deferred_team_adds, deferred_count, league_id, "no_optimizer_players");
        goto done;
    }

    KboAmateurAssignmentCandidate* candidates = NULL;
    int count = kbo_amateur_assignment_get_cached_candidates(league_id, &candidates);
    if (count <= 1 || candidates == NULL) {
        kbo_amateur_audit_ortools_batch(
            "fallback", "no_candidates", job->reason, league_id,
            optimizer_player_count, accumulated_teams, count, -1, deferred_count, -1, 0u);
        kbo_amateur_apply_deferred_original_fallback(job->deferred_team_adds, deferred_count, league_id, "no_candidates");
        goto done;
    }

    char request_path[MAX_PATH * 3] = {0};
    char result_path[MAX_PATH * 3] = {0};
    char request_name[160] = {0};
    char result_name[160] = {0};
    DWORD tick = GetTickCount();
    DWORD tid = GetCurrentThreadId();
    snprintf(
        request_name,
        sizeof(request_name),
        "work\\amateur_assignment_ortools_batch_request_%u_%lu_%lu.csv",
        league_id,
        (unsigned long)tid,
        (unsigned long)tick);
    snprintf(
        result_name,
        sizeof(result_name),
        "work\\amateur_assignment_ortools_batch_result_%u_%lu_%lu.csv",
        league_id,
        (unsigned long)tid,
        (unsigned long)tick);
    if (!kbo_get_save_scoped_data_file(request_name, request_path, sizeof(request_path))
            || !kbo_get_save_scoped_data_file(result_name, result_path, sizeof(result_path))) {
        kbo_amateur_audit_ortools_batch(
            "fallback", "path_unavailable", job->reason, league_id,
            optimizer_player_count, accumulated_teams, count, -1, deferred_count, -1, 0u);
        kbo_amateur_apply_deferred_original_fallback(job->deferred_team_adds, deferred_count, league_id, "path_unavailable");
        goto done;
    }

    /* League batch flushes are generated amateur cohorts, even after OOTP has
       already attached them to their original teams. Keep optimizer capacity
       semantics on the incoming-player path. */
    if (!kbo_amateur_ortools_write_batch_request(
            request_path,
            job->league_players,
            job->league_source_teams,
            job->league_player_ids,
            job->league_source_team_ids,
            optimizer_player_count,
            league_id,
            candidates,
            count,
            1)) {
        kbo_amateur_audit_ortools_batch(
            "fallback", "write_failed", job->reason, league_id,
            optimizer_player_count, accumulated_teams, count, -1, deferred_count, -1, 0u);
        kbo_amateur_apply_deferred_original_fallback(job->deferred_team_adds, deferred_count, league_id, "write_failed");
        goto done;
    }
    if (!kbo_optimizer_run_mode("amateur_assignment", request_path, result_path, 30000u)) {
        kbo_amateur_audit_ortools_batch(
            "fallback", "ortools_failed", job->reason, league_id,
            optimizer_player_count, accumulated_teams, count, -1, deferred_count, -1, 0u);
        kbo_amateur_apply_deferred_original_fallback(job->deferred_team_adds, deferred_count, league_id, "ortools_failed");
        goto done;
    }
    int assigned = kbo_amateur_ortools_read_batch_result(result_path, league_id);
    kbo_amateur_audit_ortools_batch(
        "optimize", "result_loaded", job->reason, league_id,
        optimizer_player_count, accumulated_teams, count, assigned, deferred_count, -1, 0u);
    kbo_log_runtimef(
        "amateur OR-Tools league batch prepared league=%u teams=%d/%d players=%d assignments=%d deferred=%d reason=%s",
        league_id,
        accumulated_teams,
        count,
        optimizer_player_count,
        assigned,
        deferred_count,
        job->reason);
    if (deferred_count <= 0) {
        (void)kbo_amateur_apply_post_original_batch_assignments(
            job->league_players,
            job->league_player_ids,
            optimizer_player_count,
            league_id,
            candidates,
            count,
            job->reason);
        result = assigned;
        goto done;
    }
    kbo_amateur_apply_deferred_ortools_batch(
        job->deferred_team_adds,
        deferred_count,
        league_id,
        candidates,
        count,
        job->reason);
    result = assigned;

done:
    kbo_lock_leave(&g_kbo_amateur_batch_flush_work_lock);
    return result;
}

static DWORD WINAPI kbo_amateur_async_league_batch_flush_thread(LPVOID parameter)
{
    KboAmateurLeagueBatchFlushJob* job = (KboAmateurLeagueBatchFlushJob*)parameter;
    (void)kbo_amateur_process_league_batch_snapshot(job);
    if (job != NULL) {
        HeapFree(GetProcessHeap(), 0, job);
    }
    return 0;
}

int kbo_amateur_start_league_switch_flush_locked(const char* reason, uint32_t next_league_id)
{
    uint32_t league_id = g_kbo_amateur_league_batch_league_id;
    int32_t accumulated_players = g_kbo_amateur_league_batch_player_count;
    int32_t accumulated_teams = g_kbo_amateur_league_batch_team_count;
    int32_t deferred_count = g_kbo_amateur_deferred_team_add_count;
    if ((league_id != KBO_HIGH_SCHOOL_LEAGUE_ID && league_id != KBO_COLLEGE_LEAGUE_ID)
            || accumulated_players <= 1
            || accumulated_teams <= 0) {
        return 0;
    }

    KboAmateurLeagueBatchFlushJob* job = (KboAmateurLeagueBatchFlushJob*)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        sizeof(KboAmateurLeagueBatchFlushJob));
    if (job == NULL) {
        kbo_log_runtimef(
            "amateur OR-Tools league switch async flush allocation failed league=%u players=%d teams=%d",
            league_id,
            accumulated_players,
            accumulated_teams);
        return 0;
    }
    kbo_amateur_fill_flush_job_from_current_batch_locked(
        job,
        reason,
        league_id,
        accumulated_players,
        accumulated_teams,
        deferred_count);

    if (!kbo_start_runtime_thread(
            kbo_amateur_async_league_batch_flush_thread,
            job,
            "amateur assignment OR-Tools league switch flush")) {
        HeapFree(GetProcessHeap(), 0, job);
        kbo_log_runtimef(
            "amateur OR-Tools league switch async flush thread failed league=%u players=%d teams=%d gle=%lu",
            league_id,
            accumulated_players,
            accumulated_teams,
            GetLastError());
        return 0;
    }
    kbo_amateur_league_batch_clear(next_league_id);
    kbo_log_runtimef(
        "amateur OR-Tools league switch batch detached old_league=%u next_league=%u teams=%d players=%d deferred=%d reason=%s",
        league_id,
        next_league_id,
        accumulated_teams,
        accumulated_players,
        deferred_count,
        (reason != NULL && reason[0] != '\0') ? reason : "");
    return 1;
}

int kbo_amateur_flush_league_batch_ortools(const char* reason, int force)
{
    uint32_t league_id = 0u;
    int32_t accumulated_players = 0;
    int32_t accumulated_teams = 0;
    int32_t deferred_count = 0;
    int32_t candidate_count_hint = 0;
    DWORD idle_ms = 0u;

    kbo_amateur_batch_lock();
    league_id = g_kbo_amateur_league_batch_league_id;
    accumulated_players = g_kbo_amateur_league_batch_player_count;
    accumulated_teams = g_kbo_amateur_league_batch_team_count;
    deferred_count = g_kbo_amateur_deferred_team_add_count;
    candidate_count_hint = g_kbo_amateur_league_batch_candidate_count;
    const KboAmateurPlayerQualityPolicy* policy = kbo_amateur_player_quality_policy();
    int32_t near_complete_teams = policy->ortools_batch_near_complete_teams;
    if (candidate_count_hint > 1) {
        int32_t dynamic_near_complete = candidate_count_hint - 1;
        if (near_complete_teams <= 0 || near_complete_teams > dynamic_near_complete) {
            near_complete_teams = dynamic_near_complete;
        }
    }
    int near_complete = (near_complete_teams > 0 && accumulated_teams >= near_complete_teams)
        || accumulated_players >= policy->ortools_batch_near_complete_players;
    idle_ms = near_complete
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
                accumulated_players, accumulated_teams, candidate_count_hint, -1, -1,
                force ? 1 : 0, (uint64_t)idle_ms);
        }
        kbo_amateur_batch_unlock();
        return 0;
    }

    KboAmateurLeagueBatchFlushJob* job = (KboAmateurLeagueBatchFlushJob*)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        sizeof(KboAmateurLeagueBatchFlushJob));
    if (job == NULL) {
        kbo_amateur_batch_unlock();
        return 0;
    }
    kbo_amateur_fill_flush_job_from_current_batch_locked(
        job,
        reason,
        league_id,
        accumulated_players,
        accumulated_teams,
        deferred_count);
    kbo_amateur_league_batch_clear(league_id);
    kbo_amateur_batch_unlock();

    int result = kbo_amateur_process_league_batch_snapshot(job);
    HeapFree(GetProcessHeap(), 0, job);
    return result;
}
