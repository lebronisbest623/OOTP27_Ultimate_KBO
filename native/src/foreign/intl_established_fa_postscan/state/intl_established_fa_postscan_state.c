#include "../internal/intl_established_fa_postscan_internal.h"

KboIntlEstablishedFaPostscanState g_kbo_intl_established_fa_postscan = {0};
volatile LONG g_kbo_intl_established_fa_postscan_worker_started = 0;
uintptr_t g_kbo_intl_established_fa_observed_players[KBO_INTL_ESTABLISHED_FA_OBSERVED_PLAYER_MAX] = {0};
volatile LONG g_kbo_intl_established_fa_observed_player_count = 0;

const char* kbo_intl_established_fa_quality_policy_label(
    int asian_quota,
    uint8_t position_group,
    uint8_t position_role)
{
    if (asian_quota) {
        if (position_group == 1u) {
            if (kbo_intl_established_fa_pitcher_role_is_starter(position_role)) {
                return "asian_starter_cap";
            }
            if (kbo_intl_established_fa_pitcher_role_is_bullpen(position_role)) {
                return "asian_bullpen_cap";
            }
            return "asian_pitcher_cap";
        }
        if (kbo_intl_established_fa_position_is_catcher(position_group, position_role)) {
            return "asian_catcher_cap";
        }
        return "asian_hitter_cap";
    }

    if (position_group == 1u) {
        if (kbo_intl_established_fa_pitcher_role_is_starter(position_role)) {
            return "non_asian_starter_cap";
        }
        if (kbo_intl_established_fa_pitcher_role_is_bullpen(position_role)) {
            return "non_asian_bullpen_cap";
        }
        return "non_asian_pitcher_cap";
    }
    if (kbo_intl_established_fa_position_is_catcher(position_group, position_role)) {
        return "non_asian_catcher_cap";
    }
    return "non_asian_hitter_cap";
}
