#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "independent_acquisition_ui_offer_cache.h"

#include <string.h>

#define KBO_INDEPENDENT_ACQUISITION_UI_OFFER_CACHE_TTL_MS 30000ULL

typedef struct KboIndependentAcquisitionUiOfferCache {
    LONG generation;
    ULONGLONG built_tick;
    uint32_t buyer_team_id;
    int32_t foreign_cash_cost;
    int32_t domestic_cash_cost;
    int32_t seller_transfer_limit;
    int max_rows;
    int count;
    KboIndependentAcquisitionUiContext context;
    KboIndependentAcquisitionUiOfferRow rows[KBO_INDEPENDENT_ACQUISITION_UI_MAX_OFFERS];
} KboIndependentAcquisitionUiOfferCache;

static volatile LONG g_kbo_independent_acquisition_ui_offer_cache_generation = 1;
static KboIndependentAcquisitionUiOfferCache g_kbo_independent_acquisition_ui_offer_cache;

void kbo_independent_acquisition_ui_invalidate_offer_cache(void)
{
    InterlockedIncrement(&g_kbo_independent_acquisition_ui_offer_cache_generation);
    g_kbo_independent_acquisition_ui_offer_cache.built_tick = 0u;
}

static int kbo_independent_acquisition_ui_offer_cache_context_matches(
    const KboIndependentAcquisitionUiContext* left,
    const KboIndependentAcquisitionUiContext* right)
{
    return left != NULL
        && right != NULL
        && left->today == right->today
        && left->season == right->season
        && left->buyer_team_id == right->buyer_team_id
        && left->open_date == right->open_date
        && left->close_date == right->close_date
        && left->window_open == right->window_open
        && left->policy_enabled == right->policy_enabled
        && left->buyer_valid == right->buyer_valid
        && left->seller_count == right->seller_count
        && left->seed_rows == right->seed_rows
        && left->unresolved_seed_rows == right->unresolved_seed_rows
        && left->buyer_active_count == right->buyer_active_count
        && left->buyer_effective_foreign_count == right->buyer_effective_foreign_count
        && left->buyer_cash == right->buyer_cash;
}

int kbo_independent_acquisition_ui_offer_cache_try_copy(
    uint32_t buyer_team_id,
    const KboIndependentAcquisitionUiContext* context,
    int32_t foreign_cash_cost,
    int32_t domestic_cash_cost,
    int32_t seller_transfer_limit,
    KboIndependentAcquisitionUiOfferRow* out_rows,
    int max_rows,
    KboIndependentAcquisitionUiContext* out_context,
    int* out_count)
{
    if (out_count != NULL) {
        *out_count = 0;
    }
    if (context == NULL || out_rows == NULL || max_rows <= 0) {
        return 0;
    }

    LONG generation = InterlockedCompareExchange(
        &g_kbo_independent_acquisition_ui_offer_cache_generation,
        0,
        0);
    const KboIndependentAcquisitionUiOfferCache* cache =
        &g_kbo_independent_acquisition_ui_offer_cache;
    ULONGLONG now = GetTickCount64();
    if (cache->generation != generation
            || cache->built_tick == 0u
            || now - cache->built_tick > KBO_INDEPENDENT_ACQUISITION_UI_OFFER_CACHE_TTL_MS
            || cache->buyer_team_id != buyer_team_id
            || cache->foreign_cash_cost != foreign_cash_cost
            || cache->domestic_cash_cost != domestic_cash_cost
            || cache->seller_transfer_limit != seller_transfer_limit
            || cache->max_rows < max_rows
            || !kbo_independent_acquisition_ui_offer_cache_context_matches(&cache->context, context)) {
        return 0;
    }

    int copy_count = cache->count < max_rows ? cache->count : max_rows;
    if (copy_count > 0) {
        memcpy(out_rows, cache->rows, sizeof(out_rows[0]) * (size_t)copy_count);
    }
    if (out_context != NULL) {
        *out_context = cache->context;
    }
    if (out_count != NULL) {
        *out_count = copy_count;
    }
    return 1;
}

void kbo_independent_acquisition_ui_offer_cache_store(
    uint32_t buyer_team_id,
    const KboIndependentAcquisitionUiContext* context,
    int32_t foreign_cash_cost,
    int32_t domestic_cash_cost,
    int32_t seller_transfer_limit,
    const KboIndependentAcquisitionUiOfferRow* rows,
    int count,
    int max_rows)
{
    if (context == NULL
            || rows == NULL
            || count < 0
            || max_rows <= 0
            || max_rows > KBO_INDEPENDENT_ACQUISITION_UI_MAX_OFFERS) {
        return;
    }

    KboIndependentAcquisitionUiOfferCache* cache =
        &g_kbo_independent_acquisition_ui_offer_cache;
    cache->built_tick = 0u;
    cache->generation = InterlockedCompareExchange(
        &g_kbo_independent_acquisition_ui_offer_cache_generation,
        0,
        0);
    cache->buyer_team_id = buyer_team_id;
    cache->foreign_cash_cost = foreign_cash_cost;
    cache->domestic_cash_cost = domestic_cash_cost;
    cache->seller_transfer_limit = seller_transfer_limit;
    cache->max_rows = max_rows;
    cache->count = count;
    cache->context = *context;
    if (count > 0) {
        memcpy(cache->rows, rows, sizeof(rows[0]) * (size_t)count);
    }
    cache->built_tick = GetTickCount64();
}
