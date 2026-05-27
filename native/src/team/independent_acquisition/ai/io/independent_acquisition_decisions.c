#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../independent_acquisition_ai_internal.h"
#include "../../../../bootstrap/profiling/profiler.h"
#include "sql/independent_acquisition_sql_store.h"

int kbo_independent_acquisition_decision_exists(
    uint32_t season,
    uint32_t seller_team_id,
    uint32_t player_id)
{
    if (season == 0u || seller_team_id == 0u || player_id == 0u) {
        return 0;
    }
    KBO_PROFILE_BEGIN(profile_independent_decision_exists);
    int exists = kbo_independent_acquisition_sql_decision_exists(season, seller_team_id, player_id);
    KBO_PROFILE_END(
        profile_independent_decision_exists,
        exists
            ? "independent_acquisition.decision.exists.hit"
            : "independent_acquisition.decision.exists.miss");
    return exists;
}

int kbo_independent_acquisition_load_decision_keys(
    uint32_t season,
    KboIndependentAcquisitionDecisionKey* out,
    int max_count)
{
    if (season == 0u || out == NULL || max_count <= 0) {
        return -1;
    }

    KBO_PROFILE_BEGIN(profile_independent_load_decision_keys);
    int count = kbo_independent_acquisition_sql_load_decision_keys(season, out, max_count);
    KBO_PROFILE_END(
        profile_independent_load_decision_keys,
        count > 0
            ? "independent_acquisition.decision.load_keys.hit"
            : "independent_acquisition.decision.load_keys.empty");
    return count;
}

int kbo_independent_acquisition_load_seller_transfer_summaries(
    uint32_t season,
    KboIndependentAcquisitionTransferSummary* out,
    int max_count)
{
    if (season == 0u || out == NULL || max_count <= 0) {
        return -1;
    }

    KBO_PROFILE_BEGIN(profile_independent_seller_transfer_summaries);
    int count = kbo_independent_acquisition_sql_load_seller_transfer_summaries(
        season,
        out,
        max_count);
    KBO_PROFILE_END(
        profile_independent_seller_transfer_summaries,
        count >= 0
            ? "independent_acquisition.decision.transfer_summary.seller"
            : "independent_acquisition.decision.transfer_summary.seller_failed");
    return count;
}

int kbo_independent_acquisition_load_buyer_transfer_summaries(
    uint32_t season,
    KboIndependentAcquisitionTransferSummary* out,
    int max_count)
{
    if (season == 0u || out == NULL || max_count <= 0) {
        return -1;
    }

    KBO_PROFILE_BEGIN(profile_independent_buyer_transfer_summaries);
    int count = kbo_independent_acquisition_sql_load_buyer_transfer_summaries(
        season,
        out,
        max_count);
    KBO_PROFILE_END(
        profile_independent_buyer_transfer_summaries,
        count >= 0
            ? "independent_acquisition.decision.transfer_summary.buyer"
            : "independent_acquisition.decision.transfer_summary.buyer_failed");
    return count;
}

int kbo_independent_acquisition_transferred_count(
    uint32_t season,
    uint32_t seller_team_id)
{
    if (season == 0u || seller_team_id == 0u) {
        return 0;
    }
    KBO_PROFILE_BEGIN(profile_independent_seller_transferred_count);
    int count = kbo_independent_acquisition_sql_transferred_count(season, seller_team_id, 1);
    KBO_PROFILE_END(
        profile_independent_seller_transferred_count,
        "independent_acquisition.decision.transferred_count.seller");
    return count;
}

int kbo_independent_acquisition_buyer_transferred_count(
    uint32_t season,
    uint32_t buyer_team_id)
{
    if (season == 0u || buyer_team_id == 0u) {
        return 0;
    }
    KBO_PROFILE_BEGIN(profile_independent_buyer_transferred_count);
    int count = kbo_independent_acquisition_sql_transferred_count(season, buyer_team_id, 0);
    KBO_PROFILE_END(
        profile_independent_buyer_transferred_count,
        "independent_acquisition.decision.transferred_count.buyer");
    return count;
}

uint32_t kbo_independent_acquisition_last_transfer_date(
    uint32_t season,
    uint32_t seller_team_id)
{
    if (season == 0u || seller_team_id == 0u) {
        return 0u;
    }
    KBO_PROFILE_BEGIN(profile_independent_last_transfer_date);
    uint32_t date = kbo_independent_acquisition_sql_last_transfer_date(season, seller_team_id);
    KBO_PROFILE_END(
        profile_independent_last_transfer_date,
        date != 0u
            ? "independent_acquisition.decision.last_transfer_date.hit"
            : "independent_acquisition.decision.last_transfer_date.miss");
    return date;
}
