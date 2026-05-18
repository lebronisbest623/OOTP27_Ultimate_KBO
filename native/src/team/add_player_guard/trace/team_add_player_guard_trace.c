#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/logging/core_log.h"
#include "../../../foreign/common/player_eval/foreign_waiver_player_eval.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../lookup/team_lookup.h"
#include "../internal/team_add_player_guard_internal.h"

void kbo_log_foreign_team_add_trace(
    uint32_t caller_rva,
    const char* result_label,
    uint8_t result,
    uintptr_t team_ptr,
    uintptr_t player_ptr,
    uintptr_t arg3,
    uintptr_t arg4,
    uintptr_t arg5,
    uintptr_t arg6,
    uintptr_t arg7,
    uintptr_t arg8,
    uint32_t before_current_team_id,
    uint32_t before_active_team_id,
    uint32_t before_original_team_id)
{
    static volatile LONG trace_log_count = 0;
    if (InterlockedCompareExchange(&trace_log_count, 0, 0) > 800) {
        return;
    }

    if (team_ptr == 0
            || player_ptr == 0
            || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)
            || !kbo_player_pointer_plausible(player_ptr)
            || !memory_range_readable((void*)player_ptr, OOTP27_PLAYER_SCAN_BYTES)) {
        return;
    }

    uint8_t* team = (uint8_t*)team_ptr;
    uint8_t* player = (uint8_t*)player_ptr;
    if (!kbo_player_is_foreign_for_kbo_rights(player)) {
        return;
    }

    LONG slot = InterlockedIncrement(&trace_log_count);
    if (slot > 800) {
        if (slot == 801) {
            kbo_log_runtime_line("foreign team_add caller trace suppressed after 800 entries");
        }
        return;
    }

    uint32_t team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET);
    uint32_t league_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    uint32_t nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
    uint32_t after_current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t after_active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    uint32_t after_original_team_id = 0u;
    if (memory_range_readable(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET, sizeof(uint32_t))) {
        after_original_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET);
    }

    kbo_log_runtimef(
        "foreign team_add caller trace #%ld caller_rva=0x%x result=%s/%u team=%u league=%u player=%u nation=%u before_current=%u before_active=%u before_original=%u after_current=%u after_active=%u after_original=%u secondary=%u dfa=%u contract_level=%u pos_group=%u pos_role=%u overall=%u talent=%u ratings=%u args=%llu,%llu,%llu,%llu,%llu,%llu",
        slot,
        caller_rva,
        result_label != NULL ? result_label : "",
        (uint32_t)result,
        team_id,
        league_id,
        player_id,
        nation_id,
        before_current_team_id,
        before_active_team_id,
        before_original_team_id,
        after_current_team_id,
        after_active_team_id,
        after_original_team_id,
        (uint32_t)player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET],
        (uint32_t)player[OOTP27_PLAYER_DFA_FLAG_OFFSET],
        (uint32_t)player[OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET],
        (uint32_t)player[OOTP27_PLAYER_POSITION_GROUP_OFFSET],
        (uint32_t)player[OOTP27_PLAYER_POSITION_ROLE_OFFSET],
        (uint32_t)(*(uint16_t*)(player + OOTP27_PLAYER_OVERALL_VALUE_OFFSET)),
        (uint32_t)(*(uint16_t*)(player + OOTP27_PLAYER_TALENT_VALUE_OFFSET)),
        (uint32_t)(*(uint16_t*)(player + OOTP27_PLAYER_RATINGS_VALUE_OFFSET)),
        (unsigned long long)arg3,
        (unsigned long long)arg4,
        (unsigned long long)arg5,
        (unsigned long long)arg6,
        (unsigned long long)arg7,
        (unsigned long long)arg8);
}
