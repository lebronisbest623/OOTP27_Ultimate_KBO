#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>

#include "../../../../../bootstrap/abi/ootp_offsets.h"
#include "../../../../../core/core_flags/api/flags_api.h"
#include "../../../../../core/logging/core_log.h"
#include "../../../../../runtime_memory/runtime_memory.h"
#include "../../../../../team/assignment/org_query/team_org_assignment_query.h"
#include "../../../../../team/assignment/roster_arrays/team_roster_arrays.h"
#include "../../../../../team/lookup/team_lookup.h"
#include "../../../../common/dates/foreign_waiver_date.h"
#include "../../../../common/player_eval/foreign_waiver_player_eval.h"
#include "../../../../controller/foreign_ai_controller.h"
#include "../../../../rights/query/foreign_waiver_rights_query.h"
#include "../candidate_array/foreign_signability_ai_fa_candidate_array.h"
#include "foreign_signability_foreign_ai_fa_retained_candidates.h"

int32_t kbo_ai_fa_status_force_retained_market_candidates(
    uintptr_t frame_ptr,
    uint32_t requester_team_id,
    uintptr_t candidate_array,
    int32_t insert_index)
{
    if (candidate_array == 0
            || insert_index < 0
            || requester_team_id == 0u
            || !kbo_ai_fa_status_retention_priority_enabled()) {
        return insert_index;
    }
    uint32_t today = 0u;
    if (!kbo_get_foreign_waiver_current_yyyymmdd(&today)) {
        return insert_index;
    }

    if (kbo_ai_fa_status_retention_recently_attempted(
            frame_ptr,
            candidate_array,
            requester_team_id,
            today)) {
        return insert_index;
    }

    kbo_prune_expired_foreign_waiver_rights(today);
    kbo_ensure_foreign_waiver_rights_loaded_for_lookup();

    uint32_t player_ids[KBO_AI_FA_STATUS_FORCED_RETENTION_MAX] = {0};
    int player_count = 0;
    kbo_lock_enter(&g_kbo_foreign_waiver_rights_lock);
    for (int i = 0; i < g_kbo_foreign_waiver_rights_count && player_count < KBO_AI_FA_STATUS_FORCED_RETENTION_MAX; i++) {
        const KboForeignWaiverRetention* rec = &g_kbo_foreign_waiver_rights[i];
        if (rec->team_id == requester_team_id && kbo_is_foreign_waiver_right_active(rec, today)) {
            player_ids[player_count++] = rec->player_id;
        }
    }
    kbo_lock_leave(&g_kbo_foreign_waiver_rights_lock);

    if (player_count == 0) {
        return insert_index;
    }

    KboAiFaStatusRetainedCandidate candidates[KBO_AI_FA_STATUS_FORCED_RETENTION_MAX] = {0};
    int candidate_count = 0;
    int considered = 0;
    int protectable = 0;
    int already_present = 0;
    int rejected = 0;
    for (int i = 0; i < player_count; i++) {
        uint32_t retained_player_id = player_ids[i];
        uint8_t* retained_player = kbo_find_player_by_id(retained_player_id, NULL, NULL);
        KboAiFaStatusRetainedCandidate candidate = {0};
        considered++;
        if (!kbo_ai_fa_status_evaluate_retained_market_candidate(
                retained_player,
                retained_player_id,
                requester_team_id,
                today,
                &candidate)) {
            rejected++;
            kbo_ai_fa_status_log_retained_candidate_eval(
                requester_team_id,
                today,
                &candidate,
                0);
            continue;
        }
        kbo_ai_fa_status_log_retained_candidate_eval(
            requester_team_id,
            today,
            &candidate,
            1);
        protectable++;
        if (candidate_count < KBO_AI_FA_STATUS_FORCED_RETENTION_MAX) {
            candidates[candidate_count++] = candidate;
        }
    }

    kbo_ai_fa_status_sort_retained_candidates(candidates, candidate_count);

    static volatile LONG retention_team_log_count = 0;
    LONG team_slot = InterlockedIncrement(&retention_team_log_count);
    if (team_slot <= 400) {
        kbo_log_runtimef(
            "foreign retention priority: team_probe team=%u today=%u rights=%d considered=%d protectable=%d rejected=%d index=%d candidate_array=%p top_player=%u top_score=%d",
            requester_team_id,
            today,
            player_count,
            considered,
            protectable,
            rejected,
            insert_index,
            (void*)candidate_array,
            candidate_count > 0 ? candidates[0].player_id : 0u,
            candidate_count > 0 ? candidates[0].score : 0);
    }

    for (int i = 0; i < candidate_count; i++) {
        KboAiFaStatusRetainedCandidate* candidate = &candidates[i];
        if (kbo_ai_fa_status_candidate_array_contains(candidate_array, insert_index, candidate->player_ptr)) {
            already_present++;
            continue;
        }

        int32_t before_index = insert_index;
        insert_index = kbo_ai_fa_status_insert_candidate_ptr(
            frame_ptr,
            candidate_array,
            insert_index,
            candidate->player_ptr);
        if (insert_index != before_index) {
            static volatile LONG retention_force_log_count = 0;
            LONG slot = InterlockedIncrement(&retention_force_log_count);
            if (slot <= 200) {
                kbo_log_runtimef(
                    "foreign retention priority: force_retained_candidate team=%u player=%u score=%d threshold=%d demand=%d pos=%u/%u asian=%u in_org=%d market=%d holder_org=%d current=%u active=%u original=%u default=%u loan=%u draft=%u level=%u index=%d next=%d today=%u",
                    requester_team_id,
                    candidate->player_id,
                    candidate->score,
                    candidate->threshold,
                    candidate->fa_demand,
                    (uint32_t)candidate->position_group,
                    (uint32_t)candidate->position_role,
                    (uint32_t)candidate->asian,
                    candidate->already_in_org,
                    candidate->market_free_agent,
                    candidate->holder_org_candidate,
                    candidate->current_team_id,
                    candidate->active_team_id,
                    candidate->original_team_id,
                    candidate->default_team_id,
                    candidate->loan_team_id,
                    candidate->draft_league_id,
                    (uint32_t)candidate->contract_level,
                    before_index,
                    insert_index,
                    today);
            }
        } else {
            static volatile LONG retention_failed_log_count = 0;
            LONG slot = InterlockedIncrement(&retention_failed_log_count);
            if (slot <= 200) {
                kbo_log_runtimef(
                    "foreign retention priority: insert_failed team=%u player=%u score=%d index=%d slot_accessible=%d candidate_array=%p today=%u",
                    requester_team_id,
                    candidate->player_id,
                    candidate->score,
                    before_index,
                    kbo_ai_fa_status_candidate_slot_accessible(candidate_array, before_index),
                    (void*)candidate_array,
                    today);
            }
        }
    }

    if (protectable == 0 || protectable == already_present) {
        static volatile LONG retention_no_force_log_count = 0;
        LONG slot = InterlockedIncrement(&retention_no_force_log_count);
        if (slot <= 80) {
            kbo_log_runtimef(
                "foreign retention priority: no_force team=%u today=%u rights=%d considered=%d protectable=%d rejected=%d already_present=%d index=%d",
                requester_team_id,
                today,
                player_count,
                considered,
                protectable,
                rejected,
                already_present,
                insert_index);
        }
    }

    return insert_index;
}
