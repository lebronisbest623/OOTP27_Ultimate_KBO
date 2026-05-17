#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>

#include "../../../core/dates/core_current_date.h"
#include "../../../core/dates/tick/current_date_tick_capture.h"
#include "../../../foreign/common/dates/foreign_waiver_date.h"
#include "military_fa_policy.h"

static volatile LONG g_kbo_military_fa_block_player_id = 0;
static volatile LONG g_kbo_military_fa_block_requester_team_id = 0;
static volatile LONG g_kbo_military_fa_block_date = 0;
static volatile LONG64 g_kbo_military_fa_block_tick = 0;

uint32_t kbo_military_policy_current_yyyymmdd(void)
{
    uint32_t today = 0u;
    if (!kbo_get_foreign_waiver_current_yyyymmdd(&today)) {
        kbo_current_date_tick_latest_published_date(&today);
    }
    return today;
}

void kbo_record_recent_military_fa_block(uint32_t player_id, uint32_t requester_team_id, uint32_t today)
{
    if (player_id == 0u || requester_team_id == 0u || today == 0u) {
        return;
    }

    InterlockedExchange(&g_kbo_military_fa_block_player_id, (LONG)player_id);
    InterlockedExchange(&g_kbo_military_fa_block_requester_team_id, (LONG)requester_team_id);
    InterlockedExchange(&g_kbo_military_fa_block_date, (LONG)today);
    InterlockedExchange64(&g_kbo_military_fa_block_tick, (LONG64)GetTickCount64());
}
