#include "independent_acquisition_ui_offer_rows.h"

#include <stdio.h>

#include "../../../../../../foreign/injury/api/foreign_injury_labels.h"

int kbo_independent_acquisition_ui_decision_key_exists(
    const KboIndependentAcquisitionDecisionKey* keys,
    int key_count,
    int fallback_to_cached_file,
    uint32_t season,
    uint32_t seller_team_id,
    uint32_t player_id)
{
    if (season == 0u || seller_team_id == 0u || player_id == 0u) {
        return 0;
    }
    if (keys != NULL && key_count > 0) {
        for (int i = 0; i < key_count; i++) {
            if (keys[i].season == season
                    && keys[i].seller_team_id == seller_team_id
                    && keys[i].player_id == player_id) {
                return 1;
            }
        }
    }
    return fallback_to_cached_file
        ? kbo_independent_acquisition_decision_exists(season, seller_team_id, player_id)
        : 0;
}

void kbo_independent_acquisition_ui_slot_label(
    uint8_t slot_type,
    int foreign_player,
    int asian_quota,
    char* out,
    size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    if (!foreign_player) {
        snprintf(out, out_size, "국내");
        return;
    }
    if (slot_type != 0u) {
        snprintf(out, out_size, "%s", kbo_foreign_injury_slot_label(slot_type));
        return;
    }
    snprintf(out, out_size, "%s", asian_quota ? "아시아" : "외국인");
}

void kbo_independent_acquisition_ui_insert_offer_row(
    KboIndependentAcquisitionUiOfferRow* rows,
    int* count,
    int max_rows,
    const KboIndependentAcquisitionUiOfferRow* row)
{
    if (rows == NULL || count == NULL || max_rows <= 0 || row == NULL) {
        return;
    }
    if (*count < max_rows) {
        rows[*count] = *row;
        (*count)++;
        return;
    }

    int min_index = 0;
    for (int i = 1; i < max_rows; i++) {
        if (rows[i].request_score < rows[min_index].request_score) {
            min_index = i;
        }
    }
    if (row->request_score > rows[min_index].request_score) {
        rows[min_index] = *row;
    }
}

int kbo_independent_acquisition_ui_offer_row_cmp(const void* a, const void* b)
{
    const KboIndependentAcquisitionUiOfferRow* left = (const KboIndependentAcquisitionUiOfferRow*)a;
    const KboIndependentAcquisitionUiOfferRow* right = (const KboIndependentAcquisitionUiOfferRow*)b;
    if (left->already_decided != right->already_decided) {
        return (int)left->already_decided - (int)right->already_decided;
    }
    if (left->already_requested != right->already_requested) {
        return (int)left->already_requested - (int)right->already_requested;
    }
    if (left->request_score < right->request_score) {
        return 1;
    }
    if (left->request_score > right->request_score) {
        return -1;
    }
    return 0;
}

int kbo_independent_acquisition_ui_request_row_exists(
    const KboIndependentAcquisitionQueuedRequest* requests,
    int request_count,
    uint32_t buyer_team_id,
    uint32_t seller_team_id,
    uint32_t player_id)
{
    if (requests == NULL
            || request_count <= 0
            || buyer_team_id == 0u
            || seller_team_id == 0u
            || player_id == 0u) {
        return 0;
    }
    for (int i = 0; i < request_count; i++) {
        if (requests[i].buyer_team_id == buyer_team_id
                && requests[i].seller_team_id == seller_team_id
                && requests[i].player_id == player_id) {
            return 1;
        }
    }
    return 0;
}
