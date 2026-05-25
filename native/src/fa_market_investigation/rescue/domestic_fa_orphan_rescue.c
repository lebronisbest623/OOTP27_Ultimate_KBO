#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "domestic_fa_orphan_rescue.h"
#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../core/core_flags/api/flags_api.h"
#include "../../core/logging/core_log.h"
#include "../../core/sync/lock.h"
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

#define KBO_DOMESTIC_FA_ORPHAN_RESCUE_CACHE_MAX_AGE_DAYS 30u

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
    const KboFaMarketPolicy* policy = kbo_fa_market_policy();
    int next_count = 0;
    for (int i = 0; i < candidate_count && next_count < KBO_DOMESTIC_FA_ORPHAN_RESCUE_CACHE_MAX; i++) {
        const KboDomesticFaInvestigationCandidate* candidate = &candidates[i];
        if (!kbo_domestic_fa_orphan_rescue_candidate_eligible(candidate, policy)) {
            continue;
        }
        KboDomesticFaOrphanRescueCachedCandidate* out = &next[next_count++];
        out->player_id = candidate->row.player_id;
        out->today = today;
        out->market_days = candidate->market_days;
        out->original_team_id = candidate->row.original_team_id;
        out->current_team_id = candidate->row.current_team_id;
        out->active_team_id = candidate->row.active_team_id;
        out->nation_id = candidate->row.nation_id;
        out->fa_filing_season = candidate->row.fa_filing_season;
        out->age = candidate->row.age;
        out->value_score = candidate->value_score;
        out->fa_demand = candidate->row.fa_demand;
        out->fa_grade_salary = candidate->row.fa_grade_salary;
        snprintf(out->grade, sizeof(out->grade), "%s", candidate->row.grade);
        snprintf(out->case_label, sizeof(out->case_label), "%s", candidate->row.case_label);
    }
    if (next_count > 1) {
        qsort(
            next,
            (size_t)next_count,
            sizeof(next[0]),
            kbo_domestic_fa_orphan_rescue_compare_cached_desc);
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
            "domestic FA orphan rescue cache updated date=%u candidates=%d mode=%s threshold_days=%u",
            today,
            next_count,
            kbo_domestic_fa_orphan_rescue_dry_run() ? "dry_run" : "insert",
            kbo_domestic_fa_orphan_rescue_market_days_min(policy));
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
    uint32_t cache_age = kbo_domestic_fa_date_gap_days(
        g_kbo_domestic_fa_orphan_rescue_cache_date,
        today);
    if (g_kbo_domestic_fa_orphan_rescue_cache_date == today
            || (g_kbo_domestic_fa_orphan_rescue_cache_date != 0u
                && cache_age > 0u
                && cache_age <= KBO_DOMESTIC_FA_ORPHAN_RESCUE_CACHE_MAX_AGE_DAYS)) {
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

int kbo_domestic_fa_orphan_rescue_player_can_enter_market(
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
    return 1;
}

void kbo_domestic_fa_orphan_rescue_record_candidate_evidence(
    const KboDomesticFaOrphanRescueCachedCandidate* candidate,
    uint32_t requester_team_id,
    int32_t before_index,
    int32_t after_index,
    uint32_t today,
    int dry_run)
{
    (void)candidate;
    (void)requester_team_id;
    (void)before_index;
    (void)after_index;
    (void)today;
    (void)dry_run;
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
    int start_index = kbo_domestic_fa_orphan_rescue_team_start_index(
        requester_team_id,
        candidate_count);
    const KboFaMarketPolicy* policy = kbo_fa_market_policy();
    for (int pass = 0; pass < 2 && forced < KBO_DOMESTIC_FA_ORPHAN_RESCUE_FORCE_PER_CALL_MAX; pass++) {
        int original_team_pass = pass == 0;
        for (int attempt = 0; attempt < candidate_count && forced < KBO_DOMESTIC_FA_ORPHAN_RESCUE_FORCE_PER_CALL_MAX; attempt++) {
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
            if (!dry_run && insert_index == before_index) {
                continue;
            }
            kbo_domestic_fa_orphan_rescue_record_candidate_evidence(
                candidate,
                requester_team_id,
                before_index,
                insert_index,
                today,
                dry_run);

            static LONG force_log_count = 0;
            LONG slot = InterlockedIncrement(&force_log_count);
            if (slot <= 300) {
                kbo_log_runtimef(
                    "domestic FA orphan rescue candidate %s player=%u requester_team=%u team_fit=%s index=%d next=%d today=%u score=%d demand=%d market_days=%u age=%u grade=%s case=%s",
                    dry_run ? "dry_run" : "forced",
                    candidate->player_id,
                    requester_team_id,
                    original_team_fit ? "original" : "cross",
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
    }

    return insert_index;
}
