/* All-Star shared typedefs, state, and layout selection. */

#include "../allstar_league_context/allstar_league_context.h"

KboAllstarTeamRow g_allstar_team_rows[OOTP27_KBO_MAX_ALLSTAR_TEAM_ROWS];
int g_allstar_team_row_count = 0;
LONG g_allstar_team_rules_loaded = 0;
LONG g_allstar_candidate_seed_log_count = 0;
LONG g_allstar_voting_prepare_log_count = 0;
LONG g_allstar_team_name_log_count = 0;
volatile LONG g_allstar_native_event_generation_in_progress = 0;
volatile LONG g_allstar_native_event_generation_done = 0;
volatile uintptr_t g_allstar_schedule_import_league_ptr = 0;
volatile uintptr_t g_allstar_make_events_ptr = 0;

KboAllstarLayout kbo_get_allstar_layout(void)
{
    OotpBuildInfo info = read_ootp_build_info();
    if (kbo_ootp_build_uses_steam_2026_05_04_layout(info)) {
        KboAllstarLayout may_layout = {
            0x242u, 0x45f0u, 0x45f1u, 0x45f8u, 0x45fcu,
            0x4a38u, 0x4a44u, 0x4cc8u, 0x4cc0u
        };
        return may_layout;
    }

    KboAllstarLayout april_layout = {
        0x242u, 0x45e8u, 0x45e9u, 0x45f0u, 0x45f4u,
        0x4a30u, 0x4a3cu, 0x4cc0u, 0x4cc8u
    };
    return april_layout;
}
