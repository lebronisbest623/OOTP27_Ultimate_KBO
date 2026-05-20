#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../independent_acquisition_ai_internal.h"
#include "sql/independent_acquisition_sql_store.h"

int kbo_independent_acquisition_decision_exists(
    uint32_t season,
    uint32_t seller_team_id,
    uint32_t player_id)
{
    if (season == 0u || seller_team_id == 0u || player_id == 0u) {
        return 0;
    }
    return kbo_independent_acquisition_sql_decision_exists(season, seller_team_id, player_id);
}

int kbo_independent_acquisition_load_decision_keys(
    uint32_t season,
    KboIndependentAcquisitionDecisionKey* out,
    int max_count)
{
    if (season == 0u || out == NULL || max_count <= 0) {
        return -1;
    }

    return kbo_independent_acquisition_sql_load_decision_keys(season, out, max_count);
}

int kbo_independent_acquisition_transferred_count(
    uint32_t season,
    uint32_t seller_team_id)
{
    if (season == 0u || seller_team_id == 0u) {
        return 0;
    }
    return kbo_independent_acquisition_sql_transferred_count(season, seller_team_id, 1);
}

int kbo_independent_acquisition_buyer_transferred_count(
    uint32_t season,
    uint32_t buyer_team_id)
{
    if (season == 0u || buyer_team_id == 0u) {
        return 0;
    }
    return kbo_independent_acquisition_sql_transferred_count(season, buyer_team_id, 0);
}

uint32_t kbo_independent_acquisition_last_transfer_date(
    uint32_t season,
    uint32_t seller_team_id)
{
    if (season == 0u || seller_team_id == 0u) {
        return 0u;
    }
    return kbo_independent_acquisition_sql_last_transfer_date(season, seller_team_id);
}
