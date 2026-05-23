#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>

#include "foreign_ai_fast_fill_candidate_priority.h"
#include "foreign_ai_fast_fill_controller.h"
#include "../common/player_eval/foreign_waiver_player_eval.h"
#include "../common/policy/foreign_player_policy.h"
#include "../common/policy/foreign_waiver_policy.h"
#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../core/logging/core_log.h"
#include "../../core/sync/lock.h"
#include "../../runtime_memory/runtime_memory.h"
#include "../../team/assignment/org_query/team_org_assignment_query.h"
#include "../../team/lookup/team_lookup.h"

enum { KBO_FAST_FILL_CANDIDATE_PRIORITY_CACHE_SIZE = 32 };

typedef struct KboFastFillCandidatePriorityCacheEntry {
    uint32_t team_id;
    uint32_t today;
    uint32_t vacancy_started_on;
    uint32_t last_seen_on;
    uint8_t effective_vacant;
    uint8_t asian_quota_vacant;
    uintptr_t player_ptr;
    uint32_t player_id;
    int32_t score;
    uint8_t valid;
} KboFastFillCandidatePriorityCacheEntry;

static KboLock g_kbo_fast_fill_candidate_priority_cache_lock = KBO_LOCK_INIT;
static KboFastFillCandidatePriorityCacheEntry
    g_kbo_fast_fill_candidate_priority_cache[KBO_FAST_FILL_CANDIDATE_PRIORITY_CACHE_SIZE];

static int kbo_fast_fill_offer_candidate_market_candidate(
    uint8_t* player,
    uint32_t team_id,
    uint32_t today,
    const KboForeignFastFillContext* context,
    int32_t* out_score)
{
    if (out_score != NULL) { *out_score = 0; }
    if (player == NULL
            || team_id == 0u
            || today == 0u
            || context == NULL
            || !context->vacant
            || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    if (player_id == 0u
            || player[OOTP27_PLAYER_RETIRED_FLAG_OFFSET] != 0u
            || !kbo_player_is_foreign_for_kbo_rights(player)
            || kbo_player_current_assignment_matches_team_or_affiliate(player, team_id)
            || *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET) != 0u
            || *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET) != 0u
            || *(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET) != 0u
            || *(uint32_t*)(player + OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET) != 0u
            || !memory_range_readable(player + OOTP27_PLAYER_FA_DEMAND_SALARY_OFFSET, sizeof(int32_t))
            || !kbo_foreign_policy_demand_salary_plausible(
                *(int32_t*)(player + OOTP27_PLAYER_FA_DEMAND_SALARY_OFFSET))) {
        return 0;
    }

    int candidate_asian = kbo_player_is_asian_quota_slot_candidate(player) ? 1 : 0;
    if (!kbo_foreign_ai_fast_fill_candidate_solves_context(context, candidate_asian)) {
        return 0;
    }

    uint32_t pending_asian = 0u;
    uint32_t pending_non_asian = 0u;
    int candidate_pending = 0;
    kbo_custom_foreign_count_pending_offers(
        team_id,
        today,
        player_id,
        &pending_asian,
        &pending_non_asian,
        &candidate_pending);
    if (candidate_pending) {
        return 0;
    }

    uint32_t effective_before = 0u;
    uint32_t effective_after = 0u;
    uint32_t effective_limit = KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT;
    uint8_t slot_type = 0u;
    uint32_t injured_player_id = 0u;
    if (!kbo_custom_foreign_policy_team_allows_candidate(
            team_id,
            player,
            &effective_before,
            &effective_after,
            &effective_limit,
            &slot_type,
            &injured_player_id)) {
        return 0;
    }
    (void)effective_before;
    (void)effective_limit;
    (void)slot_type;
    (void)injured_player_id;
    if (effective_after > KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT) {
        return 0;
    }

    int32_t score = kbo_foreign_waiver_value_score(player);
    if (score < kbo_get_foreign_waiver_value_threshold_for_player(player)) {
        return 0;
    }
    if (out_score != NULL) { *out_score = score; }
    return 1;
}

static void kbo_fast_fill_offer_candidate_log(
    const char* reason,
    uint32_t team_id,
    uint32_t today,
    uint32_t original_id,
    uint32_t selected_id,
    int32_t original_score,
    int32_t selected_score,
    const KboForeignFastFillContext* context)
{
    static volatile LONG log_count = 0;
    LONG slot = InterlockedIncrement(&log_count);
    if (slot > 800) {
        return;
    }

    kbo_log_runtimef(
        "foreign ai fast-fill candidate priority: reason=%s team=%u original=%u selected=%u original_score=%d selected_score=%d vacant=%u effective_vacant=%u asian_quota_vacant=%u effective_with_pending=%u limit=%u today=%u",
        reason != NULL ? reason : "unknown",
        team_id,
        original_id,
        selected_id,
        original_score,
        selected_score,
        context != NULL ? (uint32_t)context->vacant : 0u,
        context != NULL ? (uint32_t)context->effective_vacant : 0u,
        context != NULL ? (uint32_t)context->asian_quota_vacant : 0u,
        context != NULL ? context->effective_with_pending : 0u,
        context != NULL ? context->limit : 0u,
        today);
}

uintptr_t kbo_foreign_ai_fast_fill_select_offer_candidate(
    uint8_t* original,
    uint32_t team_id,
    uint32_t today)
{
    if (team_id == 0u || today == 0u || original == NULL) {
        return 0;
    }

    KboForeignFastFillContext context = {0};
    if (!kbo_foreign_ai_fast_fill_get_context(team_id, &context)) {
        return 0;
    }

    uint32_t original_id = *(uint32_t*)(original + OOTP27_PLAYER_ID_OFFSET);
    int32_t original_score = 0;
    if (kbo_fast_fill_offer_candidate_market_candidate(
            original,
            team_id,
            today,
            &context,
            &original_score)) {
        kbo_fast_fill_offer_candidate_log(
            "original_solves_vacancy",
            team_id,
            today,
            original_id,
            original_id,
            original_score,
            original_score,
            &context);
        return (uintptr_t)original;
    }

    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        return 0;
    }

    uint32_t cache_index = (team_id ^ today ^ context.vacancy_started_on)
        % KBO_FAST_FILL_CANDIDATE_PRIORITY_CACHE_SIZE;
    kbo_lock_enter(&g_kbo_fast_fill_candidate_priority_cache_lock);
    KboFastFillCandidatePriorityCacheEntry cached =
        g_kbo_fast_fill_candidate_priority_cache[cache_index];
    kbo_lock_leave(&g_kbo_fast_fill_candidate_priority_cache_lock);
    if (cached.valid
            && cached.team_id == team_id
            && cached.today == today
            && cached.vacancy_started_on == context.vacancy_started_on
            && cached.last_seen_on == context.last_seen_on
            && cached.effective_vacant == context.effective_vacant
            && cached.asian_quota_vacant == context.asian_quota_vacant
            && cached.player_ptr != 0
            && kbo_player_pointer_plausible(cached.player_ptr)) {
        int32_t cached_score = 0;
        if (kbo_fast_fill_offer_candidate_market_candidate(
                (uint8_t*)cached.player_ptr,
                team_id,
                today,
                &context,
                &cached_score)
                && *(uint32_t*)((uint8_t*)cached.player_ptr + OOTP27_PLAYER_ID_OFFSET) == cached.player_id) {
            kbo_fast_fill_offer_candidate_log(
                "replaced_for_vacancy_cached",
                team_id,
                today,
                original_id,
                cached.player_id,
                original_score,
                cached_score,
                &context);
            return cached.player_ptr;
        }
    }

    uintptr_t best_ptr = 0;
    int32_t best_score = 0;
    uint32_t best_player_id = 0u;
    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }
        uint8_t* player = (uint8_t*)player_ptr;
        int32_t score = 0;
        if (!kbo_fast_fill_offer_candidate_market_candidate(
                player,
                team_id,
                today,
                &context,
                &score)) {
            continue;
        }
        if (best_ptr == 0 || score > best_score) {
            best_ptr = player_ptr;
            best_score = score;
            best_player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        }
    }

    kbo_lock_enter(&g_kbo_fast_fill_candidate_priority_cache_lock);
    g_kbo_fast_fill_candidate_priority_cache[cache_index] =
        (KboFastFillCandidatePriorityCacheEntry){
            .team_id = team_id,
            .today = today,
            .vacancy_started_on = context.vacancy_started_on,
            .last_seen_on = context.last_seen_on,
            .effective_vacant = context.effective_vacant,
            .asian_quota_vacant = context.asian_quota_vacant,
            .player_ptr = best_ptr,
            .player_id = best_player_id,
            .score = best_score,
            .valid = 1u
        };
    kbo_lock_leave(&g_kbo_fast_fill_candidate_priority_cache_lock);

    kbo_fast_fill_offer_candidate_log(
        best_ptr != 0 ? "replaced_for_vacancy" : "no_market_candidate",
        team_id,
        today,
        original_id,
        best_player_id,
        original_score,
        best_score,
        &context);
    return best_ptr;
}
