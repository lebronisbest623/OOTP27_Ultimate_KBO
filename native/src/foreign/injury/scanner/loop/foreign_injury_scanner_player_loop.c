#include "foreign_injury_scanner_player_loop.h"
#include "foreign_injury_scanner_player_loop_internal.h"

#include "../../../common/policy/foreign_player_policy.h"
#include "../../../../bootstrap/profiling/profiler.h"
#include "../../../../team/add_player_guard/team_add_player_guard_ai_roster.h"

KboForeignInjuryScannerPlayerLoopResult kbo_foreign_injury_scan_player_loop(
    uintptr_t player_vector,
    int32_t player_count,
    uint32_t configured_league_id,
    uint32_t today,
    uint32_t live_date,
    int live_injury_fields_available,
    int process_existing_replacements,
    int captured_live_date,
    const char* source)
{
    KboForeignInjuryScannerPlayerLoopResult result = {0};
    KBO_PROFILE_BEGIN(profile_foreign_injury_player_loop);
    KboForeignInjuryScannerPlayerLoopContext context = {
        configured_league_id,
        today,
        live_date,
        live_injury_fields_available,
        process_existing_replacements,
        captured_live_date,
        source
    };
    for (int32_t i = 0; i < player_count; i++) {
        KboForeignInjuryScannerPlayerLoopResult player_result =
            kbo_foreign_injury_scan_player_for_replacement(
                *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t))),
                &context);
        result.scanned += player_result.scanned;
        result.opened += player_result.opened;
    }
    KBO_PROFILE_END(profile_foreign_injury_player_loop, "foreign_injury.scan.player_loop");
    return result;
}
