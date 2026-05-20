#include "foreign_quota_pending_offer_summary_cache.h"
#include "../internal/foreign_quota_internal.h"

enum {
    KBO_CUSTOM_FOREIGN_PENDING_SUMMARY_CACHE_SIZE = 128
};

static KboCustomForeignPendingOfferSummaryCacheEntry
    g_kbo_custom_foreign_pending_summary_cache[KBO_CUSTOM_FOREIGN_PENDING_SUMMARY_CACHE_SIZE];
static KboLock g_kbo_custom_foreign_pending_summary_cache_lock = KBO_LOCK_INIT;

static void kbo_custom_foreign_pending_summary_cache_lock(void)
{
    kbo_lock_enter(&g_kbo_custom_foreign_pending_summary_cache_lock);
}

static void kbo_custom_foreign_pending_summary_cache_unlock(void)
{
    kbo_lock_leave(&g_kbo_custom_foreign_pending_summary_cache_lock);
}

static uint32_t kbo_custom_foreign_pending_summary_cache_slot(uint32_t team_id, uint32_t today)
{
    uint32_t h = team_id * 2654435761u;
    h ^= today * 2246822519u;
    h ^= h >> 16;
    return h & (KBO_CUSTOM_FOREIGN_PENDING_SUMMARY_CACHE_SIZE - 1u);
}

static int kbo_custom_foreign_pending_summary_has_player(
    const KboCustomForeignPendingOfferSummaryCacheEntry* entry,
    uint32_t candidate_id)
{
    if (entry == NULL || candidate_id == 0u) {
        return 0;
    }
    for (uint16_t i = 0; i < entry->player_count; i++) {
        if (entry->player_ids[i] == candidate_id) {
            return 1;
        }
    }
    return 0;
}

int kbo_custom_foreign_pending_summary_cache_get(
    uint32_t team_id,
    uint32_t today,
    uint32_t candidate_id,
    uint32_t* out_asian_pending,
    uint32_t* out_non_asian_pending,
    int* out_candidate_pending)
{
    LONG generation = kbo_custom_foreign_pending_offer_generation_for_team(team_id);
    uint32_t slot = kbo_custom_foreign_pending_summary_cache_slot(team_id, today);
    kbo_custom_foreign_pending_summary_cache_lock();
    KboCustomForeignPendingOfferSummaryCacheEntry cached =
        g_kbo_custom_foreign_pending_summary_cache[slot];
    kbo_custom_foreign_pending_summary_cache_unlock();

    if (!cached.valid
            || cached.team_id != team_id
            || cached.today != today
            || cached.generation != generation) {
        return 0;
    }

    int candidate_pending = kbo_custom_foreign_pending_summary_has_player(&cached, candidate_id);
    if (cached.overflow && candidate_id != 0u && !candidate_pending) {
        return 0;
    }

    if (out_asian_pending != NULL) { *out_asian_pending = cached.asian_pending; }
    if (out_non_asian_pending != NULL) { *out_non_asian_pending = cached.non_asian_pending; }
    if (out_candidate_pending != NULL) { *out_candidate_pending = candidate_pending; }
    return 1;
}

void kbo_custom_foreign_pending_summary_cache_store(
    const KboCustomForeignPendingOfferSummaryCacheEntry* summary)
{
    if (summary == NULL || summary->team_id == 0u) {
        return;
    }
    uint32_t slot = kbo_custom_foreign_pending_summary_cache_slot(summary->team_id, summary->today);
    kbo_custom_foreign_pending_summary_cache_lock();
    g_kbo_custom_foreign_pending_summary_cache[slot] = *summary;
    g_kbo_custom_foreign_pending_summary_cache[slot].valid = 1u;
    kbo_custom_foreign_pending_summary_cache_unlock();
}
