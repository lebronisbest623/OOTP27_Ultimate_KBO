#include "independent_acquisition_seller_queue.h"

void kbo_independent_acquisition_seller_clear_later_requests(
    KboIndependentAcquisitionQueuedRequest* queue,
    int request_count,
    int start_index,
    uint32_t player_id,
    uint32_t seller_team_id)
{
    if (queue == NULL || request_count <= 0 || start_index < 0) {
        return;
    }

    for (int i = start_index; i < request_count; i++) {
        if (queue[i].player_id == player_id
                && queue[i].seller_team_id == seller_team_id) {
            queue[i].player_id = 0u;
        }
    }
}

int kbo_independent_acquisition_seller_daily_index(
    uint32_t seller_team_id,
    uint32_t* seller_ids,
    int* seller_transfer_counts,
    int* seller_count,
    int max_sellers)
{
    if (seller_team_id == 0u
            || seller_ids == NULL
            || seller_transfer_counts == NULL
            || seller_count == NULL
            || *seller_count < 0
            || max_sellers <= 0) {
        return -1;
    }

    for (int i = 0; i < *seller_count; i++) {
        if (seller_ids[i] == seller_team_id) {
            return i;
        }
    }
    if (*seller_count >= max_sellers) {
        return -1;
    }

    int index = *seller_count;
    seller_ids[index] = seller_team_id;
    seller_transfer_counts[index] = 0;
    (*seller_count)++;
    return index;
}
