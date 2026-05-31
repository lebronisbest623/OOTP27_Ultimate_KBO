#include "../../foreign_injury_scanner_player_loop_internal.h"

int kbo_foreign_injury_closed_record_should_repair_locked(
    uint32_t injured_player_id,
    const KboForeignInjuryLiveMemory* live_injury,
    uint32_t today,
    int inactive_roster_present,
    int roster_hold_flags_present)
{
    if (injured_player_id == 0u) {
        return 0;
    }
    for (int i = 0; i < g_kbo_foreign_injury_replacement_count; i++) {
        KboForeignInjuryReplacement* rec = &g_kbo_foreign_injury_replacements[i];
        if (rec->injured_player_id != injured_player_id
                || !kbo_foreign_injury_closed_record_can_repair_on_date(
                    rec,
                    live_injury,
                    today,
                    inactive_roster_present,
                    roster_hold_flags_present)) {
            continue;
        }
        uint8_t* replacement = kbo_find_player_by_id(rec->replacement_player_id, NULL, NULL);
        if (kbo_foreign_injury_replacement_player_attached_to_record(rec, replacement)
                || kbo_foreign_injury_replacement_player_can_restore_to_record(rec, replacement)) {
            return 1;
        }
    }
    return 0;
}
