#include "../foreign_injury_scanner_internal.h"
#include "../lifecycle/foreign_injury_existing_replacements_internal.h"

uint32_t kbo_foreign_injury_resolve_replacement_for_record(
    const KboForeignInjuryReplacement* rec,
    const KboForeignInjuryReplacement* snapshot,
    int snapshot_count)
{
    if (rec == NULL || rec->team_id == 0u || rec->injured_player_id == 0u) {
        return 0u;
    }
    if (rec->replacement_player_id != 0u) {
        uint8_t* replacement = kbo_find_player_by_id(rec->replacement_player_id, NULL, NULL);
        return kbo_foreign_injury_replacement_player_attached_to_record(rec, replacement)
            ? rec->replacement_player_id
            : 0u;
    }

    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        return 0u;
    }

    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }

        uint8_t* player = (uint8_t*)player_ptr;
        uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        if (player_id == 0u
                || player_id == rec->injured_player_id
                || !kbo_player_is_foreign_for_kbo_rights(player)
                || !kbo_foreign_replacement_player_seed_matches_loaded(player, NULL)
                || !kbo_foreign_injury_player_matches_team(player, rec->team_id)
                || !kbo_foreign_injury_candidate_matches_slot(player, rec->slot_type)
                || kbo_foreign_injury_replacement_player_reserved_snapshot(
                    player_id, rec, snapshot, snapshot_count)) {
            continue;
        }

        if (kbo_foreign_injury_runtime_injury_present_from_memory(player)) {
            continue;
        }
        return player_id;
    }

    return 0u;
}
