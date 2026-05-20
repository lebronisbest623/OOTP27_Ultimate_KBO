#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../independent_acquisition_ai_internal.h"

#include "../../../../core/logging/core_log.h"
#include "../../../../foreign/injury/api/foreign_injury_labels.h"
#include "sql/independent_acquisition_sql_store.h"

int kbo_independent_acquisition_request_exists(
    uint32_t season,
    uint32_t buyer_team_id,
    uint32_t seller_team_id,
    uint32_t player_id)
{
    if (season == 0u || buyer_team_id == 0u || seller_team_id == 0u || player_id == 0u) {
        return 0;
    }
    return kbo_independent_acquisition_sql_request_exists(
        season,
        buyer_team_id,
        seller_team_id,
        player_id);
}

int kbo_independent_acquisition_cancel_request(
    uint32_t season,
    uint32_t buyer_team_id,
    uint32_t seller_team_id,
    uint32_t player_id,
    const char* source)
{
    if (season == 0u || buyer_team_id == 0u || seller_team_id == 0u || player_id == 0u) {
        return 0;
    }
    if (kbo_independent_acquisition_decision_exists(season, seller_team_id, player_id)) {
        return 0;
    }

    int removed = kbo_independent_acquisition_sql_cancel_request(
        season,
        buyer_team_id,
        seller_team_id,
        player_id);
    if (removed) {
        kbo_log_runtimef(
            "independent acquisition request cancelled source=%s season=%u buyer=%u seller=%u player=%u store=sqlite",
            source != NULL ? source : "",
            season,
            buyer_team_id,
            seller_team_id,
            player_id);
    }
    return removed;
}

int kbo_independent_acquisition_append_request(
    uint32_t today,
    const KboIndependentAcquisitionCandidate* candidate,
    const KboIndependentAcquisitionBuyerState* buyer,
    const KboIndependentFuturesTeamLeague* seller,
    const char* source)
{
    if (today == 0u || candidate == NULL || buyer == NULL || seller == NULL) {
        return 0;
    }
    if (kbo_independent_acquisition_request_exists(
            today / 10000u,
            buyer->team_id,
            candidate->seller_team_id,
            candidate->player_id)) {
        return 0;
    }

    int32_t cash_cost = kbo_independent_acquisition_cash_cost_for_player((uint8_t*)candidate->player_ptr);
    return kbo_independent_acquisition_sql_append_request(
        today,
        candidate,
        buyer,
        seller,
        cash_cost,
        candidate->slot_type != 0u ? kbo_foreign_injury_slot_label(candidate->slot_type) : "none",
        source);
}

int kbo_independent_acquisition_load_requests(
    uint32_t season,
    KboIndependentAcquisitionQueuedRequest* out,
    int max_count)
{
    if (season == 0u || out == NULL || max_count <= 0) {
        return 0;
    }

    return kbo_independent_acquisition_sql_load_pending_requests(season, out, max_count);
}
