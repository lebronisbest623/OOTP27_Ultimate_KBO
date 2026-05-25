#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>

#include "../domestic_fa_orphan_rescue.h"

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/league_roles/kbo_league_roles.h"
#include "../../../core/logging/core_log.h"
#include "../../../foreign/common/dates/foreign_waiver_date.h"
#include "../../../foreign/common/player_eval/foreign_waiver_player_eval.h"
#include "../../../runtime_memory/runtime_memory.h"

static int kbo_domestic_fa_orphan_rescue_original_offer_candidate_replaceable(
    uintptr_t original_candidate_ptr,
    uint32_t* out_original_player_id,
    int32_t* out_original_score)
{
    if (out_original_player_id != NULL) {
        *out_original_player_id = 0u;
    }
    if (out_original_score != NULL) {
        *out_original_score = 0;
    }
    if (original_candidate_ptr == 0
            || !memory_range_readable((void*)original_candidate_ptr, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    uint8_t* original = (uint8_t*)original_candidate_ptr;
    uint32_t original_id = *(uint32_t*)(original + OOTP27_PLAYER_ID_OFFSET);
    if (original_id == 0u) {
        return 0;
    }
    if (out_original_player_id != NULL) {
        *out_original_player_id = original_id;
    }

    if (kbo_player_is_foreign_for_kbo_rights(original)) {
        return 0;
    }

    if (out_original_score != NULL) {
        *out_original_score = kbo_foreign_waiver_value_score(original);
    }
    return 1;
}

uintptr_t kbo_domestic_fa_orphan_rescue_offer_candidate_replacement(
    uintptr_t original_candidate_ptr,
    uint32_t requester_team_id,
    uint32_t requester_league_id,
    uint32_t today)
{
    if (original_candidate_ptr == 0
            || requester_team_id == 0u
            || requester_league_id == 0u
            || !kbo_domestic_fa_orphan_rescue_enabled()) {
        return original_candidate_ptr;
    }
    if (requester_league_id != kbo_league_role_main_league_id()) {
        return original_candidate_ptr;
    }
    if (today == 0u && !kbo_get_foreign_waiver_current_yyyymmdd(&today)) {
        return original_candidate_ptr;
    }

    uint32_t original_player_id = 0u;
    int32_t original_score = 0;
    if (!kbo_domestic_fa_orphan_rescue_original_offer_candidate_replaceable(
            original_candidate_ptr,
            &original_player_id,
            &original_score)) {
        return original_candidate_ptr;
    }

    KboDomesticFaOrphanRescueCachedCandidate candidates[KBO_DOMESTIC_FA_ORPHAN_RESCUE_CACHE_MAX];
    int candidate_count = kbo_domestic_fa_orphan_rescue_collect_cached(
        today,
        candidates,
        KBO_DOMESTIC_FA_ORPHAN_RESCUE_CACHE_MAX);
    if (candidate_count <= 0) {
        return original_candidate_ptr;
    }

    int dry_run = kbo_domestic_fa_orphan_rescue_dry_run();
    int start_index = kbo_domestic_fa_orphan_rescue_team_start_index(
        requester_team_id,
        candidate_count);
    if (original_player_id != 0u) {
        start_index = (start_index + (int)(original_player_id % (uint32_t)candidate_count)) % candidate_count;
    }

    const KboFaMarketPolicy* policy = kbo_fa_market_policy();
    for (int pass = 0; pass < 2; pass++) {
        int original_team_pass = pass == 0;
        for (int attempt = 0; attempt < candidate_count; attempt++) {
            int candidate_index = (start_index + attempt) % candidate_count;
            const KboDomesticFaOrphanRescueCachedCandidate* candidate = &candidates[candidate_index];
            int original_team_fit = kbo_domestic_fa_orphan_rescue_candidate_original_team_fit(
                candidate,
                requester_team_id);
            if (original_team_pass != original_team_fit) {
                continue;
            }
            if (!kbo_domestic_fa_orphan_rescue_candidate_team_allowed(
                    candidate,
                    requester_team_id,
                    policy)) {
                continue;
            }
            if (candidate->value_score < original_score) {
                continue;
            }

            uint8_t* player = kbo_find_player_by_id(candidate->player_id, NULL, NULL);
            if (!kbo_domestic_fa_orphan_rescue_player_can_enter_market(player, candidate->player_id)) {
                continue;
            }
            uintptr_t replacement_ptr = (uintptr_t)player;
            if (replacement_ptr == original_candidate_ptr) {
                continue;
            }

            kbo_domestic_fa_orphan_rescue_record_candidate_evidence(
                candidate,
                requester_team_id,
                -2,
                dry_run ? -2 : -1,
                today,
                dry_run);

            static LONG replace_log_count = 0;
            LONG slot = InterlockedIncrement(&replace_log_count);
            if (slot <= 500) {
                kbo_log_runtimef(
                    "domestic FA orphan rescue candidate %s player=%u requester_team=%u team_fit=%s original=%u original_score=%d rescue_score=%d today=%u market_days=%u grade=%s case=%s",
                    dry_run ? "offer_replace_dry_run" : "offer_replace",
                    candidate->player_id,
                    requester_team_id,
                    original_team_fit ? "original" : "cross",
                    original_player_id,
                    original_score,
                    candidate->value_score,
                    today,
                    candidate->market_days,
                    candidate->grade,
                    candidate->case_label);
            }

            return dry_run ? original_candidate_ptr : replacement_ptr;
        }
    }

    return original_candidate_ptr;
}
