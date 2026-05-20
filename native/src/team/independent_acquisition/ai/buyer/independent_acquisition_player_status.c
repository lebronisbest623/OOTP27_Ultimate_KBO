#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../independent_acquisition_ai_internal.h"

#include "../../../../bootstrap/abi/ootp_offsets.h"
#include "../../../../runtime_memory/runtime_memory.h"

int kbo_independent_acquisition_player_status_ok(uint8_t* player)
{
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }
    return *(uint8_t*)(player + OOTP27_PLAYER_RETIRED_FLAG_OFFSET) == 0u
        && *(uint8_t*)(player + OOTP27_PLAYER_DFA_FLAG_OFFSET) == 0u
        && *(uint8_t*)(player + OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET) == 0u
        && *(uint8_t*)(player + OOTP27_PLAYER_INJURY_ACTIVE_OFFSET) == 0u;
}
