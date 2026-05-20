#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../bootstrap/abi/hook_entrypoints.h"
#include "../../../bootstrap/profiling/perf_probe.h"
#include "../../../bootstrap/profiling/profiler.h"
#include "../../../core/dates/core_text_date.h"
#include "../../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../core/logging/core_log.h"
#include "../../../core/sync/lock.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../team/lookup/team_lookup.h"
#include "../../../team/assignment/org_query/team_org_assignment_query.h"
#include "../../common/dates/foreign_waiver_date.h"
#include "../../common/player_eval/foreign_waiver_player_eval.h"
#include "../../common/policy/foreign_waiver_policy.h"
#include "../../injury/api/foreign_injury.h"
#include "../../replacement_seed/api/foreign_replacement_seed.h"
#include "../counts/foreign_quota_counts.h"
#include "foreign_quota_pending_offer_summary_cache.h"

/* Custom foreign-player signing policy. Included from native/KBOFix.c. */

typedef struct KboCustomForeignPendingOffer {
    uint32_t team_id;
    uint32_t player_id;
    uint32_t date_yyyymmdd;
    uint8_t asian_quota_candidate;
} KboCustomForeignPendingOffer;

enum {
    KBO_CUSTOM_FOREIGN_PENDING_OFFER_MAX = 1024,
    KBO_CUSTOM_FOREIGN_PENDING_TEAM_GENERATION_CACHE_SIZE = 1024
};

KboCustomForeignPendingOffer g_kbo_custom_foreign_pending_offers[KBO_CUSTOM_FOREIGN_PENDING_OFFER_MAX];
KboLock g_kbo_custom_foreign_pending_offer_lock = KBO_LOCK_INIT;
int g_kbo_custom_foreign_pending_offer_count = 0;
volatile LONG g_kbo_custom_foreign_pending_offer_generation = 0;
static DWORD g_kbo_custom_foreign_pending_offer_last_prune_tick = 0u;
static uint32_t g_kbo_custom_foreign_pending_offer_last_prune_date = 0u;
static uint32_t
    g_kbo_custom_foreign_pending_team_generation_ids[KBO_CUSTOM_FOREIGN_PENDING_TEAM_GENERATION_CACHE_SIZE];
static LONG
    g_kbo_custom_foreign_pending_team_generations[KBO_CUSTOM_FOREIGN_PENDING_TEAM_GENERATION_CACHE_SIZE];

void kbo_custom_foreign_pending_offer_lock(void)
{
    kbo_lock_enter(&g_kbo_custom_foreign_pending_offer_lock);
}

void kbo_custom_foreign_pending_offer_unlock(void)
{
    kbo_lock_leave(&g_kbo_custom_foreign_pending_offer_lock);
}

static uint32_t kbo_custom_foreign_pending_team_generation_slot(uint32_t team_id)
{
    uint32_t h = team_id * 2654435761u;
    h ^= h >> 16;
    return h & (KBO_CUSTOM_FOREIGN_PENDING_TEAM_GENERATION_CACHE_SIZE - 1u);
}

static void kbo_custom_foreign_pending_offer_bump_team_generation(uint32_t team_id)
{
    if (team_id == 0u) {
        InterlockedIncrement(&g_kbo_custom_foreign_pending_offer_generation);
        return;
    }
    uint32_t slot = kbo_custom_foreign_pending_team_generation_slot(team_id);
    if (g_kbo_custom_foreign_pending_team_generation_ids[slot] != 0u
            && g_kbo_custom_foreign_pending_team_generation_ids[slot] != team_id) {
        InterlockedIncrement(&g_kbo_custom_foreign_pending_offer_generation);
    }
    g_kbo_custom_foreign_pending_team_generation_ids[slot] = team_id;
    InterlockedIncrement(&g_kbo_custom_foreign_pending_team_generations[slot]);
}

LONG kbo_custom_foreign_pending_offer_generation_for_team(uint32_t team_id)
{
    LONG global_generation = InterlockedCompareExchange(
        &g_kbo_custom_foreign_pending_offer_generation,
        0,
        0);
    if (team_id == 0u) {
        return global_generation;
    }

    uint32_t slot = kbo_custom_foreign_pending_team_generation_slot(team_id);
    if (g_kbo_custom_foreign_pending_team_generation_ids[slot] != team_id) {
        return global_generation;
    }
    LONG team_generation = InterlockedCompareExchange(
        &g_kbo_custom_foreign_pending_team_generations[slot],
        0,
        0);
    return global_generation ^ (LONG)((uint32_t)team_generation * 2246822519u);
}

int kbo_custom_foreign_pending_offer_is_stale(uint32_t offer_date, uint32_t today)
{
    if (offer_date == 0u || today == 0u) {
        return 0;
    }
    uint32_t offer_serial = kbo_date_serial(offer_date / 10000u, (offer_date / 100u) % 100u, offer_date % 100u);
    uint32_t today_serial = kbo_date_serial(today / 10000u, (today / 100u) % 100u, today % 100u);
    return offer_serial == 0u
        || today_serial == 0u
        || offer_serial > today_serial
        || today_serial - offer_serial > (uint32_t)kbo_foreign_player_policy()->pending_offer_ttl_days;
}

int kbo_custom_foreign_pending_offer_player_now_in_org(uint32_t team_id, uint32_t player_id)
{
    uint8_t* player = kbo_find_player_by_id(player_id, NULL, NULL);
    return player != NULL
        && memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)
        && kbo_player_current_assignment_matches_team_or_affiliate(player, team_id);
}

static void kbo_custom_foreign_prune_pending_offers_locked(uint32_t today)
{
    int old_count = g_kbo_custom_foreign_pending_offer_count;
    int write_index = 0;
    uint32_t changed_team_ids[32] = {0u};
    int changed_team_count = 0;
    for (int i = 0; i < g_kbo_custom_foreign_pending_offer_count; i++) {
        KboCustomForeignPendingOffer rec = g_kbo_custom_foreign_pending_offers[i];
        if (rec.team_id == 0u || rec.player_id == 0u
                || kbo_custom_foreign_pending_offer_is_stale(rec.date_yyyymmdd, today)
                || kbo_custom_foreign_pending_offer_player_now_in_org(rec.team_id, rec.player_id)) {
            if (rec.team_id != 0u && changed_team_count < (int)(sizeof(changed_team_ids) / sizeof(changed_team_ids[0]))) {
                int known = 0;
                for (int j = 0; j < changed_team_count; j++) {
                    if (changed_team_ids[j] == rec.team_id) {
                        known = 1;
                        break;
                    }
                }
                if (!known) {
                    changed_team_ids[changed_team_count++] = rec.team_id;
                }
            }
            continue;
        }
        g_kbo_custom_foreign_pending_offers[write_index++] = rec;
    }
    g_kbo_custom_foreign_pending_offer_count = write_index;
    if (write_index != old_count) {
        if (changed_team_count == 0
                || changed_team_count >= (int)(sizeof(changed_team_ids) / sizeof(changed_team_ids[0]))) {
            InterlockedIncrement(&g_kbo_custom_foreign_pending_offer_generation);
        } else {
            for (int i = 0; i < changed_team_count; i++) {
                kbo_custom_foreign_pending_offer_bump_team_generation(changed_team_ids[i]);
            }
        }
    }
    g_kbo_custom_foreign_pending_offer_last_prune_tick = GetTickCount();
    g_kbo_custom_foreign_pending_offer_last_prune_date = today;
}

void kbo_custom_foreign_count_pending_offers(
    uint32_t team_id,
    uint32_t today,
    uint32_t candidate_id,
    uint32_t* out_asian_pending,
    uint32_t* out_non_asian_pending,
    int* out_candidate_pending)
{
    if (out_asian_pending != NULL) { *out_asian_pending = 0u; }
    if (out_non_asian_pending != NULL) { *out_non_asian_pending = 0u; }
    if (out_candidate_pending != NULL) { *out_candidate_pending = 0; }
    if (team_id == 0u) {
        return;
    }

    KBO_PROFILE_BEGIN(profile_foreign_pending_offers);
    if (kbo_custom_foreign_pending_summary_cache_get(
            team_id,
            today,
            candidate_id,
            out_asian_pending,
            out_non_asian_pending,
            out_candidate_pending)) {
        KBO_PROFILE_END(profile_foreign_pending_offers, "foreign_policy.pending_offers.cache_hit");
        return;
    }

    kbo_custom_foreign_pending_offer_lock();

    DWORD now = GetTickCount();
    if (g_kbo_custom_foreign_pending_offer_last_prune_date != today
            || now - g_kbo_custom_foreign_pending_offer_last_prune_tick > 1000u) {
        kbo_custom_foreign_prune_pending_offers_locked(today);
    }

    uint32_t asian_pending = 0u;
    uint32_t non_asian_pending = 0u;
    int candidate_pending = 0;
    KboCustomForeignPendingOfferSummaryCacheEntry summary;
    memset(&summary, 0, sizeof(summary));
    summary.team_id = team_id;
    summary.today = today;
    summary.generation = kbo_custom_foreign_pending_offer_generation_for_team(team_id);
    for (int i = 0; i < g_kbo_custom_foreign_pending_offer_count; i++) {
        KboCustomForeignPendingOffer rec = g_kbo_custom_foreign_pending_offers[i];
        if (rec.team_id != team_id) {
            continue;
        }
        if (rec.player_id == candidate_id) {
            candidate_pending = 1;
        }
        if (rec.asian_quota_candidate) {
            asian_pending++;
        } else {
            non_asian_pending++;
        }
        if (summary.player_count < KBO_CUSTOM_FOREIGN_PENDING_SUMMARY_PLAYER_MAX) {
            summary.player_ids[summary.player_count++] = rec.player_id;
        } else {
            summary.overflow = 1u;
        }
    }

    kbo_custom_foreign_pending_offer_unlock();
    summary.asian_pending = asian_pending;
    summary.non_asian_pending = non_asian_pending;
    kbo_custom_foreign_pending_summary_cache_store(&summary);
    KBO_PROFILE_END(profile_foreign_pending_offers, "foreign_policy.pending_offers.count");

    if (out_asian_pending != NULL) { *out_asian_pending = asian_pending; }
    if (out_non_asian_pending != NULL) { *out_non_asian_pending = non_asian_pending; }
    if (out_candidate_pending != NULL) { *out_candidate_pending = candidate_pending; }
}

void kbo_record_custom_foreign_pending_offer(uint32_t team_id, uint8_t* candidate, uint32_t today)
{
    if (team_id == 0u
            || candidate == NULL
            || !memory_range_readable(candidate, OOTP27_PLAYER_SCAN_BYTES)
            || !kbo_player_is_foreign_for_kbo_rights(candidate)
            || kbo_player_current_assignment_matches_team_or_affiliate(candidate, team_id)) {
        return;
    }

    uint32_t player_id = *(uint32_t*)(candidate + OOTP27_PLAYER_ID_OFFSET);
    if (player_id == 0u) {
        return;
    }

    uint8_t asian = kbo_player_is_asian_quota_candidate(candidate) ? 1u : 0u;
    kbo_custom_foreign_pending_offer_lock();

    int empty_slot = -1;
    for (int i = 0; i < g_kbo_custom_foreign_pending_offer_count; i++) {
        KboCustomForeignPendingOffer* rec = &g_kbo_custom_foreign_pending_offers[i];
        if (rec->team_id == team_id && rec->player_id == player_id) {
            rec->date_yyyymmdd = today;
            rec->asian_quota_candidate = asian;
            g_kbo_custom_foreign_pending_offer_last_prune_tick = 0u;
            kbo_custom_foreign_pending_offer_bump_team_generation(team_id);
            kbo_custom_foreign_pending_offer_unlock();
            return;
        }
    }
    if (g_kbo_custom_foreign_pending_offer_count < KBO_CUSTOM_FOREIGN_PENDING_OFFER_MAX) {
        empty_slot = g_kbo_custom_foreign_pending_offer_count++;
    }
    if (empty_slot >= 0) {
        g_kbo_custom_foreign_pending_offers[empty_slot] = (KboCustomForeignPendingOffer){
            .team_id = team_id,
            .player_id = player_id,
            .date_yyyymmdd = today,
            .asian_quota_candidate = asian
        };
        g_kbo_custom_foreign_pending_offer_last_prune_tick = 0u;
        kbo_custom_foreign_pending_offer_bump_team_generation(team_id);
    }

    kbo_custom_foreign_pending_offer_unlock();

    static volatile LONG pending_offer_log_count = 0;
    LONG slot = InterlockedIncrement(&pending_offer_log_count);
    if (slot <= 200) {
        kbo_log_runtimef(
            "custom foreign policy pending offer recorded team=%u player=%u asian=%u today=%u",
            team_id,
            player_id,
            (uint32_t)asian,
            today);
    }
}

void kbo_cancel_custom_foreign_pending_offer(uint32_t team_id, uint32_t player_id)
{
    if (team_id == 0u || player_id == 0u) {
        return;
    }

    kbo_custom_foreign_pending_offer_lock();
    int write_index = 0;
    int removed = 0;
    for (int i = 0; i < g_kbo_custom_foreign_pending_offer_count; i++) {
        KboCustomForeignPendingOffer rec = g_kbo_custom_foreign_pending_offers[i];
        if (rec.team_id == team_id && rec.player_id == player_id) {
            removed++;
            continue;
        }
        g_kbo_custom_foreign_pending_offers[write_index++] = rec;
    }
    g_kbo_custom_foreign_pending_offer_count = write_index;
    if (removed > 0) {
        g_kbo_custom_foreign_pending_offer_last_prune_tick = 0u;
        kbo_custom_foreign_pending_offer_bump_team_generation(team_id);
    }
    kbo_custom_foreign_pending_offer_unlock();

    if (removed > 0) {
        kbo_log_runtimef(
            "custom foreign policy pending offer cancelled team=%u player=%u removed=%d",
            team_id,
            player_id,
            removed);
    }
}
