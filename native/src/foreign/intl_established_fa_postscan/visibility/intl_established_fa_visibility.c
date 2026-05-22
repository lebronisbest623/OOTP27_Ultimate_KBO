#include "../internal/intl_established_fa_postscan_internal.h"

int kbo_intl_established_fa_postscan_candidate_matches(
    const KboIntlEstablishedFaPostscanState* batch,
    int32_t index,
    int32_t player_count,
    uint8_t* player)
{
    if (batch == NULL || player == NULL) {
        return 0;
    }

    if (kbo_intl_established_fa_postscan_player_was_observed(player)) {
        return 1;
    }

    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    if (batch->before_max_player_id != 0u) {
        return player_id > batch->before_max_player_id;
    }

    if (batch->before_count > 0 && batch->before_count < player_count) {
        return index >= batch->before_count;
    }

    if (batch->expected_count > 0 && player_count > batch->expected_count) {
        int32_t fallback_start = player_count - batch->expected_count - 8;
        if (fallback_start < 0) {
            fallback_start = 0;
        }
        return index >= fallback_start;
    }

    return 1;
}

