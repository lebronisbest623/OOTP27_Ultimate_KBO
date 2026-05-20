#ifndef KBOFIX_SRC_FOREIGN_QUOTA_OFFERS_FOREIGN_QUOTA_PENDING_OFFER_SUMMARY_CACHE_H_
#define KBOFIX_SRC_FOREIGN_QUOTA_OFFERS_FOREIGN_QUOTA_PENDING_OFFER_SUMMARY_CACHE_H_

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>

enum {
    KBO_CUSTOM_FOREIGN_PENDING_SUMMARY_PLAYER_MAX = 128
};

typedef struct KboCustomForeignPendingOfferSummaryCacheEntry {
    uint32_t team_id;
    uint32_t today;
    LONG generation;
    uint32_t asian_pending;
    uint32_t non_asian_pending;
    uint32_t player_ids[KBO_CUSTOM_FOREIGN_PENDING_SUMMARY_PLAYER_MAX];
    uint16_t player_count;
    uint8_t overflow;
    uint8_t valid;
} KboCustomForeignPendingOfferSummaryCacheEntry;

int kbo_custom_foreign_pending_summary_cache_get(
    uint32_t team_id,
    uint32_t today,
    uint32_t candidate_id,
    uint32_t* out_asian_pending,
    uint32_t* out_non_asian_pending,
    int* out_candidate_pending);
void kbo_custom_foreign_pending_summary_cache_store(
    const KboCustomForeignPendingOfferSummaryCacheEntry* summary);

#endif
