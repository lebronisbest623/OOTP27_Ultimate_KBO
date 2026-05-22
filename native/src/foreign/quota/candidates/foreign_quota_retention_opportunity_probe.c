#include "foreign_quota_retention_opportunity_probe.h"
#include "retention_score/foreign_quota_retention_score_gate.h"

#include "../internal/foreign_quota_internal.h"
#include "../../controller/foreign_ai_controller.h"
#include "../../common/policy/foreign_player_policy.h"
#include "../../rights/query/foreign_waiver_rights_query.h"
#include "../../../core/core_flags/keys/runtime_flag_keys.generated.h"

#define KBO_RETENTION_OPPORTUNITY_SCAN_MAX 128
#define KBO_RETENTION_OPPORTUNITY_CACHE_SIZE 64

typedef struct KboForeignRetentionOpportunity {
    uint32_t team_id;
    uint32_t today;
    uint32_t best_player_id;
    int32_t best_score;
    uint8_t best_asian;
    uint32_t active_rights;
    uint32_t protectable_rights;
    uint32_t protectable_asian;
    uint32_t protectable_non_asian;
} KboForeignRetentionOpportunity;

typedef struct KboForeignRetentionOpportunityCacheEntry {
    uint32_t team_id;
    uint32_t today;
    LONG rights_generation;
    uint8_t valid;
    KboForeignRetentionOpportunity opportunity;
} KboForeignRetentionOpportunityCacheEntry;

static KboForeignRetentionOpportunityCacheEntry
    g_kbo_retention_opportunity_cache[KBO_RETENTION_OPPORTUNITY_CACHE_SIZE];
static KboLock g_kbo_retention_opportunity_cache_lock = KBO_LOCK_INIT;

static void kbo_retention_opportunity_cache_lock(void)
{
    kbo_lock_enter(&g_kbo_retention_opportunity_cache_lock);
}

static void kbo_retention_opportunity_cache_unlock(void)
{
    kbo_lock_leave(&g_kbo_retention_opportunity_cache_lock);
}

static int kbo_retention_opportunity_player_protectable(
    uint8_t* player,
    uint32_t expected_player_id,
    uint32_t team_id,
    uint32_t today,
    int32_t* out_score,
    uint8_t* out_asian)
{
    if (out_score != NULL) { *out_score = 0; }
    if (out_asian != NULL) { *out_asian = 0u; }
    if (player == NULL
            || expected_player_id == 0u
            || team_id == 0u
            || today == 0u
            || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }
    if (*(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET) != expected_player_id
            || player[OOTP27_PLAYER_RETIRED_FLAG_OFFSET] != 0u
            || !kbo_player_is_foreign_for_kbo_rights(player)
            || !kbo_has_active_foreign_waiver_right(team_id, expected_player_id, today)
            || kbo_player_current_assignment_matches_team_or_affiliate(player, team_id)) {
        return 0;
    }
    if (!memory_range_readable(player + OOTP27_PLAYER_FA_DEMAND_SALARY_OFFSET, sizeof(int32_t))
            || !kbo_foreign_policy_demand_salary_plausible(
                *(int32_t*)(player + OOTP27_PLAYER_FA_DEMAND_SALARY_OFFSET))) {
        return 0;
    }

    int32_t score = kbo_foreign_waiver_value_score(player);
    if (score < kbo_get_foreign_waiver_value_threshold_for_player(player)) {
        return 0;
    }

    uint8_t asian = kbo_player_is_asian_quota_candidate(player) ? 1u : 0u;
    if (out_score != NULL) { *out_score = score; }
    if (out_asian != NULL) { *out_asian = asian; }
    return 1;
}

static int kbo_retention_opportunity_cache_get(
    uint32_t team_id,
    uint32_t today,
    KboForeignRetentionOpportunity* out_opportunity)
{
    if (out_opportunity != NULL) {
        *out_opportunity = (KboForeignRetentionOpportunity){0};
    }
    if (team_id == 0u || today == 0u || out_opportunity == NULL) {
        return 0;
    }

    LONG rights_generation = InterlockedCompareExchange(&g_kbo_foreign_waiver_rights_generation, 0, 0);
    uint32_t cache_index = (team_id ^ (today >> 4)) % KBO_RETENTION_OPPORTUNITY_CACHE_SIZE;
    kbo_retention_opportunity_cache_lock();
    KboForeignRetentionOpportunityCacheEntry cached =
        g_kbo_retention_opportunity_cache[cache_index];
    if (cached.valid
            && cached.team_id == team_id
            && cached.today == today
            && cached.rights_generation == rights_generation) {
        *out_opportunity = cached.opportunity;
        kbo_retention_opportunity_cache_unlock();
        return out_opportunity->protectable_rights > 0u;
    }
    kbo_retention_opportunity_cache_unlock();

    uint32_t player_ids[KBO_RETENTION_OPPORTUNITY_SCAN_MAX] = {0};
    int player_count = 0;
    KboForeignRetentionOpportunity opportunity = {
        .team_id = team_id,
        .today = today
    };

    kbo_ensure_foreign_waiver_rights_loaded_for_lookup();
    kbo_lock_enter(&g_kbo_foreign_waiver_rights_lock);
    for (int i = 0; i < g_kbo_foreign_waiver_rights_count
            && player_count < KBO_RETENTION_OPPORTUNITY_SCAN_MAX; i++) {
        const KboForeignWaiverRetention* rec = &g_kbo_foreign_waiver_rights[i];
        if (rec->team_id == team_id && kbo_is_foreign_waiver_right_active(rec, today)) {
            player_ids[player_count++] = rec->player_id;
            opportunity.active_rights++;
        }
    }
    kbo_lock_leave(&g_kbo_foreign_waiver_rights_lock);

    for (int i = 0; i < player_count; i++) {
        uint32_t player_id = player_ids[i];
        uint8_t* player = kbo_find_player_by_id(player_id, NULL, NULL);
        int32_t score = 0;
        uint8_t asian = 0u;
        if (!kbo_retention_opportunity_player_protectable(
                player,
                player_id,
                team_id,
                today,
                &score,
                &asian)) {
            continue;
        }

        opportunity.protectable_rights++;
        if (asian) {
            opportunity.protectable_asian++;
        } else {
            opportunity.protectable_non_asian++;
        }
        if (opportunity.best_player_id == 0u || score > opportunity.best_score) {
            opportunity.best_player_id = player_id;
            opportunity.best_score = score;
            opportunity.best_asian = asian;
        }
    }

    kbo_retention_opportunity_cache_lock();
    g_kbo_retention_opportunity_cache[cache_index] = (KboForeignRetentionOpportunityCacheEntry){
        .team_id = team_id,
        .today = today,
        .rights_generation = InterlockedCompareExchange(&g_kbo_foreign_waiver_rights_generation, 0, 0),
        .valid = 1u,
        .opportunity = opportunity
    };
    kbo_retention_opportunity_cache_unlock();

    *out_opportunity = opportunity;
    return opportunity.protectable_rights > 0u;
}

static int32_t kbo_retention_opportunity_score_margin(int32_t best_score)
{
    const KboForeignPlayerPolicy* policy = kbo_foreign_player_policy();
    int32_t margin = best_score / policy->retention_margin_divisor;
    if (margin < policy->retention_margin_min) {
        margin = policy->retention_margin_min;
    }
    if (margin > policy->retention_margin_max) {
        margin = policy->retention_margin_max;
    }
    return margin;
}

int32_t kbo_retention_opportunity_score_margin_for_best(int32_t best_score)
{
    return kbo_retention_opportunity_score_margin(best_score);
}

int kbo_retention_opportunity_get_summary(
    uint32_t team_id,
    uint32_t today,
    KboForeignRetentionOpportunitySummary* out_summary)
{
    if (out_summary != NULL) {
        *out_summary = (KboForeignRetentionOpportunitySummary){0};
    }
    if (out_summary == NULL) {
        return 0;
    }

    KboForeignRetentionOpportunity opportunity = {0};
    int result = kbo_retention_opportunity_cache_get(team_id, today, &opportunity);
    *out_summary = (KboForeignRetentionOpportunitySummary){
        .team_id = opportunity.team_id,
        .today = opportunity.today,
        .best_player_id = opportunity.best_player_id,
        .best_score = opportunity.best_score,
        .best_asian = opportunity.best_asian,
        .active_rights = opportunity.active_rights,
        .protectable_rights = opportunity.protectable_rights,
        .protectable_asian = opportunity.protectable_asian,
        .protectable_non_asian = opportunity.protectable_non_asian
    };
    return result;
}

static int kbo_retention_opportunity_probe_logs_enabled(void)
{
    static volatile LONG initialized = 0;
    static volatile LONG enabled = 0;
    if (InterlockedCompareExchange(&initialized, 1, 0) == 0) {
        int on = read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_ENABLE_KBO_RETENTION_OPPORTUNITY_PROBE_LOGS_FILE)
            || read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_ENABLE_KBO_CUSTOM_FOREIGN_OFFER_LOGS_FILE);
        InterlockedExchange(&enabled, on ? 1 : 0);
    }
    return InterlockedCompareExchange(&enabled, 0, 0) != 0;
}

int kbo_retention_opportunity_probe_should_block(
    uint32_t team_id,
    uint8_t* candidate,
    uint32_t today,
    uint32_t asian_count,
    uint32_t non_asian_count,
    uint32_t pending_asian_count,
    uint32_t pending_non_asian_count,
    uint32_t asian_after,
    uint32_t non_asian_after,
    uint32_t effective_before,
    uint32_t effective_after,
    uint32_t effective_limit,
    int candidate_pending,
    int already_in_org,
    int allowed)
{
    if (team_id == 0u
            || today == 0u
            || candidate == NULL
            || !memory_range_readable(candidate, OOTP27_PLAYER_SCAN_BYTES)
            || !kbo_player_is_foreign_for_kbo_rights(candidate)) {
        return 0;
    }
    if (!kbo_foreign_ai_controller_enabled()) {
        return 0;
    }

    uint32_t candidate_id = *(uint32_t*)(candidate + OOTP27_PLAYER_ID_OFFSET);
    if (candidate_id == 0u) {
        return 0;
    }

    int retained_by_team = kbo_has_active_foreign_waiver_right(team_id, candidate_id, today);
    KboForeignRetentionOpportunity opportunity = {0};
    kbo_retention_opportunity_cache_get(team_id, today, &opportunity);
    if (!retained_by_team && opportunity.protectable_rights == 0u) {
        return 0;
    }

    int32_t candidate_score = kbo_foreign_waiver_value_score(candidate);
    uint8_t candidate_asian = kbo_player_is_asian_quota_slot_candidate(candidate) ? 1u : 0u;
    uint8_t candidate_pos_group = *(uint8_t*)(candidate + OOTP27_PLAYER_POSITION_GROUP_OFFSET);
    uint32_t reserve_asian = opportunity.protectable_asian;
    uint32_t reserve_non_asian = opportunity.protectable_non_asian;
    if (retained_by_team) {
        if (candidate_asian && reserve_asian > 0u) {
            reserve_asian--;
        } else if (!candidate_asian && reserve_non_asian > 0u) {
            reserve_non_asian--;
        }
    }

    uint32_t reserved_after = kbo_effective_foreign_count_with_asian_quota(
        asian_after + reserve_asian,
        non_asian_after + reserve_non_asian);
    int32_t margin = kbo_retention_opportunity_score_margin(opportunity.best_score);
    int would_block = 0;
    const char* reason = "none";
    if (retained_by_team) {
        reason = "retained_candidate";
    } else if (already_in_org) {
        reason = "already_in_org";
    } else if (!allowed) {
        reason = "quota_blocked";
    } else if (opportunity.best_player_id == 0u) {
        reason = "no_retained_candidate";
    } else if (reserved_after <= effective_limit) {
        reason = "slot_available_after_reserve";
    } else if (kbo_retention_candidate_score_clears_best(candidate_score, opportunity.best_score)) {
        reason = "candidate_clears_retained_score";
    } else {
        reason = "retained_opportunity_cost";
        would_block = 1;
    }

    static volatile LONG opportunity_log_count = 0;
    LONG slot = InterlockedIncrement(&opportunity_log_count);
    int verbose = kbo_retention_opportunity_probe_logs_enabled();
    if (would_block || retained_by_team || verbose || slot <= 300) {
        if (verbose || slot <= 800) {
            kbo_log_runtimef(
                "foreign ai controller retention gate team=%u player=%u retained=%d candidate_score=%d candidate_pos=%u candidate_asian=%u allowed=%d would_block=%d reason=%s effective_before=%u effective_after=%u limit=%u reserved_after=%u count_asian=%u count_non_asian=%u pending_asian=%u pending_non_asian=%u candidate_pending=%d already_in_org=%d best_retained=%u best_score=%d best_asian=%u active_rights=%u protectable=%u protectable_asian=%u protectable_non_asian=%u margin=%d today=%u",
                team_id,
                candidate_id,
                retained_by_team,
                candidate_score,
                (uint32_t)candidate_pos_group,
                (uint32_t)candidate_asian,
                allowed,
                would_block,
                reason,
                effective_before,
                effective_after,
                effective_limit,
                reserved_after,
                asian_count,
                non_asian_count,
                pending_asian_count,
                pending_non_asian_count,
                candidate_pending,
                already_in_org,
                opportunity.best_player_id,
                opportunity.best_score,
                (uint32_t)opportunity.best_asian,
                opportunity.active_rights,
                opportunity.protectable_rights,
                opportunity.protectable_asian,
                opportunity.protectable_non_asian,
                margin,
                today);
        }
    }
    return would_block;
}
