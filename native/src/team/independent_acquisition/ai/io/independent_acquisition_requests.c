#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../independent_acquisition_ai_internal.h"

#include "../../../../bootstrap/profiling/profiler.h"
#include "../../../../core/logging/core_log.h"
#include "../../../../foreign/common/player_eval/foreign_waiver_player_eval.h"
#include "../../../../foreign/injury/api/foreign_injury_labels.h"
#include "sql/independent_acquisition_sql_store.h"

static const char* kbo_independent_acquisition_candidate_slot_label(
    const KboIndependentAcquisitionCandidate* candidate)
{
    if (candidate == NULL || candidate->player_ptr == 0u) {
        return "-";
    }
    uint8_t* player = (uint8_t*)candidate->player_ptr;
    if (!kbo_player_is_foreign_for_kbo_rights(player)) {
        return "Domestic";
    }
    if (candidate->slot_type != 0u) {
        return kbo_foreign_injury_slot_label(candidate->slot_type);
    }
    return candidate->asian_quota || kbo_player_is_asian_quota_slot_candidate(player)
        ? kbo_foreign_injury_slot_label(2u)
        : kbo_foreign_injury_slot_label(1u);
}

int kbo_independent_acquisition_request_exists(
    uint32_t season,
    uint32_t buyer_team_id,
    uint32_t seller_team_id,
    uint32_t player_id)
{
    if (season == 0u || buyer_team_id == 0u || seller_team_id == 0u || player_id == 0u) {
        return 0;
    }
    KBO_PROFILE_BEGIN(profile_independent_request_exists);
    int exists = kbo_independent_acquisition_sql_request_exists(
        season,
        buyer_team_id,
        seller_team_id,
        player_id);
    KBO_PROFILE_END(
        profile_independent_request_exists,
        exists
            ? "independent_acquisition.request.exists.hit"
            : "independent_acquisition.request.exists.miss");
    return exists;
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
    KBO_PROFILE_BEGIN(profile_independent_append_request);
    if (today == 0u || candidate == NULL || buyer == NULL || seller == NULL) {
        KBO_PROFILE_END(
            profile_independent_append_request,
            "independent_acquisition.request.append.invalid");
        return 0;
    }
    if (kbo_independent_acquisition_request_exists(
            today / 10000u,
            buyer->team_id,
            candidate->seller_team_id,
            candidate->player_id)) {
        KBO_PROFILE_END(
            profile_independent_append_request,
            "independent_acquisition.request.append.duplicate");
        return 0;
    }

    int32_t cash_cost = kbo_independent_acquisition_cash_cost_for_player((uint8_t*)candidate->player_ptr);
    int appended = kbo_independent_acquisition_sql_append_request(
        today,
        candidate,
        buyer,
        seller,
        cash_cost,
        kbo_independent_acquisition_candidate_slot_label(candidate),
        source);
    KBO_PROFILE_END(
        profile_independent_append_request,
        appended
            ? "independent_acquisition.request.append.ok"
            : "independent_acquisition.request.append.failed");
    return appended;
}

int kbo_independent_acquisition_load_requests(
    uint32_t season,
    KboIndependentAcquisitionQueuedRequest* out,
    int max_count)
{
    if (season == 0u || out == NULL || max_count <= 0) {
        return 0;
    }

    KBO_PROFILE_BEGIN(profile_independent_load_requests);
    int count = kbo_independent_acquisition_sql_load_pending_requests(season, out, max_count);
    KBO_PROFILE_END(
        profile_independent_load_requests,
        count > 0
            ? "independent_acquisition.request.load.hit"
            : "independent_acquisition.request.load.empty");
    return count;
}
