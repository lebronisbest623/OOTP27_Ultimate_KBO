#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>

#include "../../../../bootstrap/abi/ootp_offsets.h"
#include "../../../../core/dates/tick/current_date_tick_capture.h"
#include "../../../../core/logging/core_log.h"
#include "../../../../foreign/common/dates/foreign_waiver_date.h"
#include "../../../../foreign/common/player_eval/foreign_waiver_player_eval.h"
#include "../../../../foreign/retention_guard/foreign_retention_guard.h"
#include "../../../../foreign/rights/query/foreign_waiver_rights_query.h"
#include "../../../../runtime_memory/runtime_memory.h"
#include "../../../lookup/team_lookup.h"
#include "team_add_player_guard_foreign_retention_trace.h"

void kbo_team_add_log_foreign_retention_result(
    uint32_t caller_rva,
    uint8_t result,
    uintptr_t team_ptr,
    uintptr_t player_ptr,
    uint32_t before_current_team_id,
    uint32_t before_active_team_id,
    uint32_t before_original_team_id)
{
    if (caller_rva != 0x8515f3u
            || team_ptr == 0
            || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)
            || !kbo_player_pointer_plausible(player_ptr)) {
        return;
    }

    uint8_t* team = (uint8_t*)team_ptr;
    uint8_t* player = (uint8_t*)player_ptr;
    if (!memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)
            || !kbo_player_is_foreign_for_kbo_rights(player)) {
        return;
    }

    uint32_t today = 0u;
    if (!kbo_current_date_tick_latest_published_date(&today) || today == 0u) {
        return;
    }

    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    uint32_t holder_team_id = 0u;
    if (!kbo_find_active_foreign_waiver_holder(player_id, today, &holder_team_id)
            || holder_team_id == 0u) {
        return;
    }

    uint32_t team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET);
    uint32_t after_current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t after_active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    uint32_t after_original_team_id = 0u;
    if (memory_range_readable(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET, sizeof(uint32_t))) {
        after_original_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET);
    }
    uint32_t contract_status = memory_range_readable(player + OOTP27_PLAYER_CONTRACT_STATUS_OFFSET, sizeof(uint32_t))
        ? *(uint32_t*)(player + OOTP27_PLAYER_CONTRACT_STATUS_OFFSET)
        : 0u;
    uint32_t contract_start_year = memory_range_readable(player + OOTP27_PLAYER_CONTRACT_START_YEAR_OFFSET, sizeof(uint32_t))
        ? *(uint32_t*)(player + OOTP27_PLAYER_CONTRACT_START_YEAR_OFFSET)
        : 0u;
    int32_t salary_y1 = memory_range_readable(player + OOTP27_PLAYER_CONTRACT_SALARY_Y1_OFFSET, sizeof(int32_t))
        ? *(int32_t*)(player + OOTP27_PLAYER_CONTRACT_SALARY_Y1_OFFSET)
        : 0;
    int32_t contract_years[OOTP27_PLAYER_CONTRACT_SALARY_YEARS] = {0};
    if (memory_range_readable(
            player + OOTP27_PLAYER_CONTRACT_SALARY_Y1_OFFSET,
            OOTP27_PLAYER_CONTRACT_SALARY_YEARS * sizeof(int32_t))) {
        for (uint32_t year = 0u; year < OOTP27_PLAYER_CONTRACT_SALARY_YEARS; year++) {
            contract_years[year] = *(int32_t*)(
                player + OOTP27_PLAYER_CONTRACT_SALARY_Y1_OFFSET + (year * sizeof(int32_t)));
        }
    }
    int score = kbo_foreign_waiver_value_score(player);
    const char* label = team_id == holder_team_id
        ? "holder_team_add"
        : "non_holder_team_add";
    if (result != 0u
            && team_id == holder_team_id
            && before_current_team_id == 0u
            && before_active_team_id == 0u
            && after_current_team_id == team_id
            && after_active_team_id == team_id) {
        uint32_t league_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
        kbo_foreign_retention_guard_record_team_add(
            player_id,
            team_id,
            league_id,
            today,
            contract_status,
            contract_start_year,
            contract_years,
            OOTP27_PLAYER_CONTRACT_SALARY_YEARS);
    }

    static volatile LONG retention_result_log_count = 0;
    LONG slot = InterlockedIncrement(&retention_result_log_count);
    if (slot <= 200) {
        kbo_log_runtimef(
            "foreign retention re-signing: %s result=%u team=%u holder_team=%u player=%u today=%u before_current=%u before_active=%u before_original=%u after_current=%u after_active=%u after_original=%u contract_level=%u contract_status=%u contract_start_year=%u salary_y1=%d score=%d caller_rva=0x%x",
            label,
            result,
            team_id,
            holder_team_id,
            player_id,
            today,
            before_current_team_id,
            before_active_team_id,
            before_original_team_id,
            after_current_team_id,
            after_active_team_id,
            after_original_team_id,
            (uint32_t)player[OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET],
            contract_status,
            contract_start_year,
            salary_y1,
            score,
            caller_rva);
    }
}
