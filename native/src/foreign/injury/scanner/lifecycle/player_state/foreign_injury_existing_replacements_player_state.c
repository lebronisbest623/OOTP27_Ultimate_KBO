#include "foreign_injury_existing_replacements_player_state.h"

int kbo_foreign_injury_replacement_unavailable_by_long_injury(
    const KboForeignInjuryReplacement* rec,
    uint32_t today)
{
    if (rec == NULL || rec->replacement_player_id == 0u) {
        return 0;
    }
    uint32_t team_id = 0u;
    uint32_t league_id = 0u;
    uint8_t* replacement = kbo_find_player_by_id(rec->replacement_player_id, &team_id, &league_id);
    if (replacement == NULL || !memory_range_readable(replacement, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    KboForeignInjuryLiveMemory live_injury;
    memset(&live_injury, 0, sizeof(live_injury));
    int min_days = kbo_foreign_player_policy()->injury_replacement_min_days;
    if (kbo_foreign_injury_read_live_memory(replacement, &live_injury)
            && kbo_foreign_injury_live_memory_has_long_term_basis(&live_injury, min_days)) {
        return 1;
    }

    (void)team_id;
    (void)league_id;
    (void)today;
    return 0;
}

int kbo_foreign_injury_runtime_injury_present(uint8_t* player)
{
    return kbo_foreign_injury_runtime_injury_present_from_memory(player);
}

int kbo_foreign_injury_roster_hold_flags_present(uint8_t* player)
{
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }
    return player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET] != 0u
        || player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET] != 0u
        || player[OOTP27_PLAYER_DFA_FLAG_OFFSET] != 0u;
}
