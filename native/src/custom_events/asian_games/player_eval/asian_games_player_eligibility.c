#include "asian_games_player_eligibility.h"

#include <stddef.h>

#include "../../../bootstrap/abi/ootp_offsets.h"

int kbo_asian_games_player_status_allows_selection(uint8_t* player)
{
    if (player == NULL) {
        return 0;
    }
    return player[OOTP27_PLAYER_MILITARY_ACTIVE_OFFSET] == 0u
        && player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET] == 0u
        && player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET] == 0u
        && player[OOTP27_PLAYER_INJURY_ACTIVE_OFFSET] == 0u
        && player[OOTP27_PLAYER_DFA_FLAG_OFFSET] == 0u
        && player[OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET] == 0u;
}

uint8_t kbo_asian_games_player_military_unserved(uint8_t* player)
{
    return player != NULL && player[OOTP27_PLAYER_MILITARY_EXEMPT_OFFSET] == 0u
        ? 1u
        : 0u;
}
