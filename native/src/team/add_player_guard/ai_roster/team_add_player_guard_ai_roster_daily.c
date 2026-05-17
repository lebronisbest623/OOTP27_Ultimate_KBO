#include "team_add_player_guard_ai_roster_daily_internal.h"

static volatile LONG g_kbo_ai_roster_daily_callup_dirty = 0;

int kbo_run_foreign_ai_roster_daily_callup(const char* source);

static int kbo_ai_roster_daily_apply_rescue_enabled(void)
{
    return kbo_custom_foreign_policy_enabled()
        && !read_kbo_localappdata_flag_file("disable_ai_roster_foreign_apply_rescue.txt")
        && !read_kbo_localappdata_flag_file("disable_ai_roster_foreign_apply_rescue_team_add.txt");
}

void kbo_mark_foreign_ai_roster_daily_callup_dirty(const char* reason)
{
    LONG previous = InterlockedExchange(&g_kbo_ai_roster_daily_callup_dirty, 1);
    static volatile LONG dirty_log_count = 0;
    LONG slot = InterlockedIncrement(&dirty_log_count);
    if (previous == 0 && slot <= 200) {
        kbo_log_runtimef(
            "ootp ai roster foreign dirty daily callup marked reason=%s",
            reason != NULL ? reason : "");
    }
}

int kbo_consume_foreign_ai_roster_daily_callup_dirty(const char* source)
{
    if (InterlockedExchange(&g_kbo_ai_roster_daily_callup_dirty, 0) == 0) {
        return 0;
    }
    return kbo_run_foreign_ai_roster_daily_callup(
        source != NULL ? source : "foreign_ai_roster_dirty_callup");
}

static int kbo_ai_roster_daily_player_already_tried(uint32_t player_id, const uint32_t* tried_player_ids, int tried_count)
{
    if (player_id == 0u || tried_player_ids == NULL || tried_count <= 0) {
        return 0;
    }
    for (int i = 0; i < tried_count; i++) {
        if (tried_player_ids[i] == player_id) {
            return 1;
        }
    }
    return 0;
}

static int kbo_ai_roster_daily_candidate_status_ok(uint8_t* player, const KboAiRosterDailyCandidateSummary* summary)
{
    if (player == NULL || summary == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    if (*(uint8_t*)(player + OOTP27_PLAYER_RETIRED_FLAG_OFFSET) != 0u
            || *(uint8_t*)(player + OOTP27_PLAYER_DFA_FLAG_OFFSET) != 0u
            || *(uint8_t*)(player + OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET) != 0u
            || *(uint8_t*)(player + OOTP27_PLAYER_INJURY_ACTIVE_OFFSET) != 0u
            || summary->f25 < KBO_AI_ROSTER_FOREIGN_F25_MIN
            || summary->f65 != 0u
            || (summary->status26 != 0u && summary->status26 != 1u)) {
        return 0;
    }
    return 1;
}

static uintptr_t* kbo_ai_roster_daily_copy_player_vector_snapshot(
    uintptr_t player_vector,
    int32_t player_count,
    const char** out_failure_reason)
{
    if (out_failure_reason != NULL) {
        *out_failure_reason = "unknown";
    }
    if (player_vector == 0u || player_count <= 0 || player_count > 200000) {
        if (out_failure_reason != NULL) { *out_failure_reason = "invalid_vector"; }
        return NULL;
    }
    if ((SIZE_T)player_count > ((SIZE_T)-1 / sizeof(uintptr_t))) {
        if (out_failure_reason != NULL) { *out_failure_reason = "count_overflow"; }
        return NULL;
    }

    SIZE_T player_vector_bytes = (SIZE_T)player_count * sizeof(uintptr_t);
    if (!memory_range_readable((void*)player_vector, player_vector_bytes)) {
        if (out_failure_reason != NULL) { *out_failure_reason = "unreadable_vector"; }
        return NULL;
    }

    uintptr_t* snapshot = (uintptr_t*)HeapAlloc(GetProcessHeap(), 0, player_vector_bytes);
    if (snapshot == NULL) {
        if (out_failure_reason != NULL) { *out_failure_reason = "alloc_failed"; }
        return NULL;
    }

    SIZE_T bytes_read = 0;
    if (!ReadProcessMemory(
            GetCurrentProcess(),
            (LPCVOID)player_vector,
            snapshot,
            player_vector_bytes,
            &bytes_read)
            || bytes_read != player_vector_bytes) {
        HeapFree(GetProcessHeap(), 0, snapshot);
        if (out_failure_reason != NULL) { *out_failure_reason = "copy_failed"; }
        return NULL;
    }

    if (out_failure_reason != NULL) {
        *out_failure_reason = NULL;
    }
    return snapshot;
}

static uintptr_t kbo_ai_roster_daily_choose_candidate(
    const uintptr_t* player_snapshot,
    int32_t player_count,
    const uint32_t* tried_player_ids,
    int tried_count,
    uint32_t* out_active_team_id,
    int64_t* out_score,
    KboAiRosterDailyCandidateSummary* out_summary,
    KboAiRosterDailyCallupScan* out_scan)
{
    if (out_active_team_id != NULL) { *out_active_team_id = 0u; }
    if (out_score != NULL) { *out_score = 0; }
    if (out_summary != NULL) {
        memset(out_summary, 0, sizeof(*out_summary));
        out_summary->index = -1;
    }
    if (out_scan != NULL) { memset(out_scan, 0, sizeof(*out_scan)); }
    if (player_snapshot == NULL
            || player_count <= 0
            || player_count > 200000) {
        return 0u;
    }

    uint32_t kbo_league_id = kbo_get_foreign_waiver_league_id();
    if (kbo_league_id == 0u) {
        return 0u;
    }

    uintptr_t best_ptr = 0u;
    uint32_t best_active_team_id = 0u;
    int64_t best_score = INT64_MIN;
    KboAiRosterDailyCandidateSummary best_summary = {0};
    best_summary.index = -1;

    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t candidate_ptr = player_snapshot[i];
        if (!kbo_player_pointer_plausible(candidate_ptr)
                || !memory_range_readable((void*)candidate_ptr, OOTP27_PLAYER_SCAN_BYTES)) {
            continue;
        }

        if (out_scan != NULL) { out_scan->scanned++; }
        uint8_t* candidate = (uint8_t*)candidate_ptr;
        if (!kbo_player_is_foreign_for_kbo_rights(candidate)) {
            continue;
        }
        if (out_scan != NULL) { out_scan->foreign_seen++; }

        KboAiRosterDailyCandidateSummary summary;
        kbo_ai_roster_daily_fill_summary(&summary, i, candidate);
        if (summary.player_id == 0u
                || kbo_ai_roster_daily_player_already_tried(summary.player_id, tried_player_ids, tried_count)) {
            continue;
        }
        if (!kbo_ai_roster_daily_candidate_status_ok(candidate, &summary)) {
            if (out_scan != NULL) { out_scan->skipped_status++; }
            continue;
        }
        if (summary.league_id != kbo_league_id + 1u) {
            if (out_scan != NULL) { out_scan->skipped_not_minor++; }
            continue;
        }

        uint32_t active_team_id = 0u;
        if (!kbo_ai_roster_daily_minor_callup_allows((int32_t)summary.league_id, candidate, &active_team_id)) {
            if (active_team_id == 0u) {
                if (out_scan != NULL) { out_scan->skipped_no_team++; }
            } else if (out_scan != NULL) {
                out_scan->blocked_limit++;
            }
            continue;
        }
        if (summary.current_team_id == active_team_id
                && summary.active_team_id == active_team_id
                && summary.league_id == kbo_league_id) {
            continue;
        }

        if (out_scan != NULL) { out_scan->eligible_seen++; }
        int64_t score = kbo_ai_roster_daily_score(&summary, candidate);
        if (score > best_score) {
            best_score = score;
            best_ptr = candidate_ptr;
            best_active_team_id = active_team_id;
            best_summary = summary;
        }
    }

    if (best_ptr == 0u) {
        return 0u;
    }
    if (out_active_team_id != NULL) { *out_active_team_id = best_active_team_id; }
    if (out_score != NULL) { *out_score = best_score; }
    if (out_summary != NULL) { *out_summary = best_summary; }
    return best_ptr;
}

int kbo_run_foreign_ai_roster_daily_callup(const char* source)
{
    if (!kbo_ai_roster_daily_apply_rescue_enabled()
            || read_kbo_localappdata_flag_file("disable_ai_roster_foreign_daily_callup.txt")) {
        return 0;
    }

    uintptr_t player_vector = 0u;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        static volatile LONG no_vector_log_count = 0;
        if (InterlockedIncrement(&no_vector_log_count) <= 5) {
            kbo_log_runtimef("ootp ai roster foreign daily callup skipped source=%s reason=no_player_vector", source != NULL ? source : "");
        }
        return 0;
    }

    const char* snapshot_failure_reason = NULL;
    uintptr_t* player_snapshot = kbo_ai_roster_daily_copy_player_vector_snapshot(
        player_vector,
        player_count,
        &snapshot_failure_reason);
    if (player_snapshot == NULL) {
        static volatile LONG snapshot_log_count = 0;
        if (InterlockedIncrement(&snapshot_log_count) <= 20) {
            kbo_log_runtimef(
                "ootp ai roster foreign daily callup skipped source=%s reason=player_vector_snapshot_failed detail=%s vector=%p count=%d",
                source != NULL ? source : "",
                snapshot_failure_reason != NULL ? snapshot_failure_reason : "",
                (void*)player_vector,
                player_count);
        }
        return 0;
    }

    uint32_t tried_player_ids[KBO_AI_ROSTER_DAILY_TRIED_MAX] = {0};
    int tried_count = 0;
    int attempts = 0;
    int applied = 0;
    int failed = 0;
    KboAiRosterDailyCallupScan total_scan = {0};

    while (attempts < kbo_foreign_player_policy()->ai_roster_daily_callup_max_attempts
            && tried_count < KBO_AI_ROSTER_DAILY_TRIED_MAX) {
        uint32_t active_team_id = 0u;
        int64_t score = 0;
        KboAiRosterDailyCandidateSummary before;
        KboAiRosterDailyCallupScan scan;
        uintptr_t player_ptr = kbo_ai_roster_daily_choose_candidate(
            player_snapshot,
            player_count,
            tried_player_ids,
            tried_count,
            &active_team_id,
            &score,
            &before,
            &scan);

        total_scan.scanned += scan.scanned;
        total_scan.foreign_seen += scan.foreign_seen;
        total_scan.eligible_seen += scan.eligible_seen;
        total_scan.blocked_limit += scan.blocked_limit;
        total_scan.skipped_status += scan.skipped_status;
        total_scan.skipped_not_minor += scan.skipped_not_minor;
        total_scan.skipped_no_team += scan.skipped_no_team;

        if (player_ptr == 0u || before.player_id == 0u || active_team_id == 0u) {
            break;
        }

        tried_player_ids[tried_count++] = before.player_id;
        attempts++;

        if (!memory_range_readable((void*)player_ptr, OOTP27_PLAYER_SCAN_BYTES)) {
            failed++;
            continue;
        }

        uint8_t* active_team = find_kbo_team_by_numeric_id_any_league(active_team_id, 1);
        if (active_team == NULL || !memory_range_readable(active_team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            failed++;
            continue;
        }

        uint32_t team_id = *(uint32_t*)(active_team + OOTP27_KBO_TEAM_ID_OFFSET);
        uint32_t team_league_id = *(uint32_t*)(active_team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
        if (team_id == 0u || team_id != active_team_id || team_league_id == 0u) {
            failed++;
            continue;
        }

        uint8_t result = kbo_team_add_player_guard_call_original((uintptr_t)active_team, player_ptr, 0u, 0u, 0u, 0u, 0u, 0u);
        KboAiRosterDailyCandidateSummary after;
        kbo_ai_roster_daily_fill_summary(&after, before.index, (uint8_t*)player_ptr);
        int moved_to_major = result != 0u
            && after.current_team_id == active_team_id
            && after.active_team_id == active_team_id
            && after.league_id == team_league_id;
        if (moved_to_major) {
            applied++;
        } else {
            failed++;
        }

        static volatile LONG detail_log_count = 0;
        LONG detail_slot = InterlockedIncrement(&detail_log_count);
        if (detail_slot <= 240) {
            kbo_log_runtimef(
                "ootp ai roster foreign daily callup #%ld source=%s attempt=%d result=%u moved=%d team=%u team_league=%u player=%u nation=%u score=%lld current=%u->%u active=%u->%u league=%u->%u status24=%u->%u status25=%u->%u status26=%u->%u f25=%u f62=%u->%u f65=%u->%u overall=%d ratings=%d",
                detail_slot,
                source != NULL ? source : "",
                attempts,
                (uint32_t)result,
                moved_to_major,
                active_team_id,
                team_league_id,
                before.player_id,
                before.nation_id,
                (long long)score,
                before.current_team_id,
                after.current_team_id,
                before.active_team_id,
                after.active_team_id,
                before.league_id,
                after.league_id,
                before.status24,
                after.status24,
                before.status25,
                after.status25,
                before.status26,
                after.status26,
                before.f25,
                before.f62,
                after.f62,
                before.f65,
                after.f65,
                after.overall,
                after.ratings);
        }
    }

    static volatile LONG summary_log_count = 0;
    LONG summary_slot = InterlockedIncrement(&summary_log_count);
    if (summary_slot <= 1000 && (applied > 0 || failed > 0 || total_scan.eligible_seen > 0)) {
        kbo_log_runtimef(
            "ootp ai roster foreign daily callup summary #%ld source=%s attempts=%d applied=%d failed=%d scanned=%d foreign=%d eligible=%d blocked_limit=%d skipped_status=%d skipped_not_minor=%d skipped_no_team=%d",
            summary_slot,
            source != NULL ? source : "",
            attempts,
            applied,
            failed,
            total_scan.scanned,
            total_scan.foreign_seen,
            total_scan.eligible_seen,
            total_scan.blocked_limit,
            total_scan.skipped_status,
            total_scan.skipped_not_minor,
            total_scan.skipped_no_team);
    }
    HeapFree(GetProcessHeap(), 0, player_snapshot);
    return applied;
}
