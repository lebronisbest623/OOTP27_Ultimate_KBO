#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "domestic_fa_orphan_rescue.h"
#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../core/core_flags/api/flags_api.h"
#include "../../core/logging/core_log.h"
#include "../../core/sync/lock.h"
#include "../../fa_market_classification/policy/fa_market_policy.h"
#include "../../foreign/common/dates/foreign_waiver_date.h"
#include "../../foreign/common/player_eval/foreign_waiver_player_eval.h"
#include "../../foreign/signability/foreign_policy/wrappers/candidate_array/foreign_signability_ai_fa_candidate_array.h"
#include "../../runtime_memory/runtime_memory.h"
#include "../../core/core_flags/keys/runtime_flag_keys.generated.h"

static KboDomesticFaOrphanRescueCachedCandidate
    g_kbo_domestic_fa_orphan_rescue_cache[KBO_DOMESTIC_FA_ORPHAN_RESCUE_CACHE_MAX];
static int g_kbo_domestic_fa_orphan_rescue_cache_count = 0;
static uint32_t g_kbo_domestic_fa_orphan_rescue_cache_date = 0u;
static KboLock g_kbo_domestic_fa_orphan_rescue_cache_lock = KBO_LOCK_INIT;

#define KBO_DOMESTIC_FA_ORPHAN_RESCUE_FORCE_PER_CALL_MAX 2

static void kbo_domestic_fa_orphan_rescue_lock(void)
{
    kbo_lock_enter(&g_kbo_domestic_fa_orphan_rescue_cache_lock);
}

static void kbo_domestic_fa_orphan_rescue_unlock(void)
{
    kbo_lock_leave(&g_kbo_domestic_fa_orphan_rescue_cache_lock);
}

int kbo_domestic_fa_orphan_rescue_enabled(void)
{
    return read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_ENABLE_KBO_DOMESTIC_FA_ORPHAN_RESCUE_FILE)
        || read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_ENABLE_KBO_DOMESTIC_FA_ORPHAN_RESCUE_DRY_RUN_FILE);
}

int kbo_domestic_fa_orphan_rescue_dry_run(void)
{
    return !read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_ENABLE_KBO_DOMESTIC_FA_ORPHAN_RESCUE_FILE)
        && read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_ENABLE_KBO_DOMESTIC_FA_ORPHAN_RESCUE_DRY_RUN_FILE);
}

static int kbo_domestic_fa_orphan_rescue_candidate_eligible(
    const KboDomesticFaInvestigationCandidate* candidate)
{
    if (candidate == NULL) {
        return 0;
    }
    const KboFaMarketClassification* row = &candidate->row;
    const KboFaMarketPolicy* policy = kbo_fa_market_policy();
    if (!kbo_domestic_fa_case_is_official_or_probable(row->case_label)) {
        return 0;
    }
    if (candidate->value_score < policy->investigation_unexplained_value_score_min) {
        return 0;
    }
    if (candidate->market_days < (uint32_t)policy->investigation_market_days_very_long_min) {
        return 0;
    }
    if (row->fa_demand >= policy->investigation_very_high_demand_min) {
        return 0;
    }
    return row->player_id != 0u
        && row->nation_id == OOTP27_KBO_KOREA_NATION_ID
        && !row->foreign_player
        && row->current_team_id == 0u
        && row->active_team_id == 0u
        && row->draft_league_id == 0u
        && row->retired_flag == 0u;
}

void kbo_domestic_fa_orphan_rescue_update_cache(
    uint32_t today,
    const KboDomesticFaInvestigationCandidate* candidates,
    int candidate_count)
{
    if (!kbo_domestic_fa_orphan_rescue_enabled() || today == 0u || candidates == NULL || candidate_count <= 0) {
        return;
    }

    KboDomesticFaOrphanRescueCachedCandidate next[KBO_DOMESTIC_FA_ORPHAN_RESCUE_CACHE_MAX];
    memset(next, 0, sizeof(next));
    int next_count = 0;
    for (int i = 0; i < candidate_count && next_count < KBO_DOMESTIC_FA_ORPHAN_RESCUE_CACHE_MAX; i++) {
        const KboDomesticFaInvestigationCandidate* candidate = &candidates[i];
        if (!kbo_domestic_fa_orphan_rescue_candidate_eligible(candidate)) {
            continue;
        }
        KboDomesticFaOrphanRescueCachedCandidate* out = &next[next_count++];
        out->player_id = candidate->row.player_id;
        out->today = today;
        out->market_days = candidate->market_days;
        out->age = candidate->row.age;
        out->value_score = candidate->value_score;
        out->fa_demand = candidate->row.fa_demand;
        snprintf(out->grade, sizeof(out->grade), "%s", candidate->row.grade);
        snprintf(out->case_label, sizeof(out->case_label), "%s", candidate->row.case_label);
    }

    kbo_domestic_fa_orphan_rescue_lock();
    memset(g_kbo_domestic_fa_orphan_rescue_cache, 0, sizeof(g_kbo_domestic_fa_orphan_rescue_cache));
    if (next_count > 0) {
        memcpy(
            g_kbo_domestic_fa_orphan_rescue_cache,
            next,
            (size_t)next_count * sizeof(next[0]));
    }
    g_kbo_domestic_fa_orphan_rescue_cache_count = next_count;
    g_kbo_domestic_fa_orphan_rescue_cache_date = today;
    kbo_domestic_fa_orphan_rescue_unlock();

    static LONG update_log_count = 0;
    LONG slot = InterlockedIncrement(&update_log_count);
    if (slot <= 100) {
        kbo_log_runtimef(
            "domestic FA orphan rescue cache updated date=%u candidates=%d mode=%s",
            today,
            next_count,
            kbo_domestic_fa_orphan_rescue_dry_run() ? "dry_run" : "insert");
    }
}

int kbo_domestic_fa_orphan_rescue_collect_cached(
    uint32_t today,
    KboDomesticFaOrphanRescueCachedCandidate* out_candidates,
    int max_candidates)
{
    if (!kbo_domestic_fa_orphan_rescue_enabled()
            || today == 0u
            || out_candidates == NULL
            || max_candidates <= 0) {
        return 0;
    }

    kbo_domestic_fa_orphan_rescue_lock();
    int count = 0;
    if (g_kbo_domestic_fa_orphan_rescue_cache_date == today) {
        int limit = g_kbo_domestic_fa_orphan_rescue_cache_count;
        if (limit > max_candidates) {
            limit = max_candidates;
        }
        for (int i = 0; i < limit; i++) {
            out_candidates[count++] = g_kbo_domestic_fa_orphan_rescue_cache[i];
        }
    }
    kbo_domestic_fa_orphan_rescue_unlock();
    return count;
}

static int kbo_domestic_fa_orphan_rescue_player_can_enter_market(
    uint8_t* player,
    uint32_t expected_player_id)
{
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }
    if (*(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET) != expected_player_id) {
        return 0;
    }
    if (*(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET) != OOTP27_KBO_KOREA_NATION_ID) {
        return 0;
    }
    if (player[OOTP27_PLAYER_RETIRED_FLAG_OFFSET] != 0u) {
        return 0;
    }
    if (*(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET) != 0u
            || *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET) != 0u
            || *(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET) != 0u) {
        return 0;
    }
    return *(uint32_t*)(player + OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET) == 0u;
}

int32_t kbo_domestic_fa_orphan_rescue_force_market_candidates(
    uintptr_t frame_ptr,
    uint32_t requester_team_id,
    uintptr_t candidate_array,
    int32_t insert_index,
    uint32_t today)
{
    if (frame_ptr == 0
            || requester_team_id == 0u
            || candidate_array == 0
            || insert_index < 0
            || !kbo_domestic_fa_orphan_rescue_enabled()) {
        return insert_index;
    }
    if (today == 0u && !kbo_get_foreign_waiver_current_yyyymmdd(&today)) {
        return insert_index;
    }

    KboDomesticFaOrphanRescueCachedCandidate candidates[KBO_DOMESTIC_FA_ORPHAN_RESCUE_CACHE_MAX];
    int candidate_count = kbo_domestic_fa_orphan_rescue_collect_cached(
        today,
        candidates,
        KBO_DOMESTIC_FA_ORPHAN_RESCUE_CACHE_MAX);
    if (candidate_count <= 0) {
        return insert_index;
    }

    int dry_run = kbo_domestic_fa_orphan_rescue_dry_run();
    int forced = 0;
    for (int i = 0; i < candidate_count && forced < KBO_DOMESTIC_FA_ORPHAN_RESCUE_FORCE_PER_CALL_MAX; i++) {
        const KboDomesticFaOrphanRescueCachedCandidate* candidate = &candidates[i];
        uint8_t* player = kbo_find_player_by_id(candidate->player_id, NULL, NULL);
        if (!kbo_domestic_fa_orphan_rescue_player_can_enter_market(player, candidate->player_id)) {
            continue;
        }

        uintptr_t player_ptr = (uintptr_t)player;
        if (kbo_ai_fa_status_candidate_array_contains(candidate_array, insert_index, player_ptr)) {
            continue;
        }

        int32_t before_index = insert_index;
        if (!dry_run) {
            insert_index = kbo_ai_fa_status_insert_candidate_ptr(
                frame_ptr,
                candidate_array,
                insert_index,
                player_ptr);
        }

        static LONG force_log_count = 0;
        LONG slot = InterlockedIncrement(&force_log_count);
        if (slot <= 300) {
            kbo_log_runtimef(
                "domestic FA orphan rescue candidate %s player=%u requester_team=%u index=%d next=%d today=%u score=%d demand=%d market_days=%u age=%u grade=%s case=%s",
                dry_run ? "dry_run" : "forced",
                candidate->player_id,
                requester_team_id,
                before_index,
                insert_index,
                today,
                candidate->value_score,
                candidate->fa_demand,
                candidate->market_days,
                (uint32_t)candidate->age,
                candidate->grade,
                candidate->case_label);
        }
        forced++;
    }

    return insert_index;
}
