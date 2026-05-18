#include "../internal/foreign_waiver_rights_internal.h"
#include "../../../team/assignment/org_query/team_org_assignment_query.h"

/* Foreign reserve-right memory synchronization. */

static int kbo_sync_active_foreign_waiver_right_to_memory_with_league(
    uint8_t* player,
    uint32_t player_id,
    uint32_t holder_team_id,
    uint32_t league_id,
    uint32_t today_yyyymmdd)
{
    if (player == NULL || player_id == 0u || holder_team_id == 0u || today_yyyymmdd == 0u
            || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    uint8_t* holder_team = find_kbo_team_by_numeric_id_any_league(holder_team_id, 1);
    if (holder_team == NULL || !memory_range_readable(holder_team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return 0;
    }
    if (league_id == 0u) {
        league_id = *(uint32_t*)(holder_team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
    }
    if (league_id == 0u) {
        return 0;
    }

    uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    int changed = 0;
    if (current_team_id != 0u) {
        if (!kbo_player_current_assignment_matches_team_or_affiliate(player, holder_team_id)) {
            return 0;
        }

        uint8_t contract_level = player[OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET];
        if (contract_level != 1u) {
            uint32_t before_active = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
            uint32_t before_original = *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET);
            uint32_t before_default = memory_range_readable(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET, sizeof(uint32_t))
                ? *(uint32_t*)(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET)
                : 0u;
            uint32_t before_league = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
            uint32_t before_draft = *(uint32_t*)(player + OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET);
            uint8_t before_restricted = player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET];
            uint8_t before_secondary = player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET];
            uint8_t before_dfa = player[OOTP27_PLAYER_DFA_FLAG_OFFSET];

            uint8_t* current_team = find_kbo_team_by_numeric_id_any_league(current_team_id, 1);
            int removed_current = current_team != NULL
                ? kbo_remove_player_id_from_known_team_roster_arrays(current_team, player_id)
                : 0;
            int removed_holder = current_team_id != holder_team_id
                ? kbo_remove_player_id_from_known_team_roster_arrays(holder_team, player_id)
                : 0;

            *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET) = 0u;
            *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET) = 0u;
            if (*(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET) == holder_team_id) {
                *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET) = 0u;
            }
            if (*(uint32_t*)(player + OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET) == league_id) {
                *(uint32_t*)(player + OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET) = 0u;
            }
            if (*(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET) == 0u) {
                *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET) = holder_team_id;
            }
            if (*(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_LEAGUE_ID_OFFSET) == 0u) {
                *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_LEAGUE_ID_OFFSET) = league_id;
            }
            if (memory_range_readable(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET, sizeof(uint32_t))
                    && before_default != 0u
                    && before_default != holder_team_id) {
                *(uint32_t*)(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET) = 0u;
            }
            player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET] = 1u;
            player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET] = 1u;
            player[OOTP27_PLAYER_DFA_FLAG_OFFSET] = 0u;
            int added_restricted = kbo_add_player_id_to_team_fixed_array(
                holder_team,
                OOTP27_TEAM_RESTRICTED_PLAYER_IDS_OFFSET,
                player_id);

            static LONG unsigned_sync_log_count = 0;
            LONG unsigned_slot = InterlockedIncrement(&unsigned_sync_log_count);
            if (unsigned_slot <= 200) {
                kbo_log_runtimef(
                    "foreign reserve rights: repaired unsigned holder assignment player=%u holder_team=%u league=%u today=%u before_current=%u before_active=%u before_original=%u before_default=%u before_league=%u before_draft=%u before_level=%u before_restricted=%u before_secondary=%u before_dfa=%u after_current=%u after_active=%u after_original=%u after_default=%u after_league=%u after_draft=%u after_level=%u after_restricted=%u after_secondary=%u after_dfa=%u removed_current=%d removed_holder=%d added_restricted=%d",
                    player_id,
                    holder_team_id,
                    league_id,
                    today_yyyymmdd,
                    current_team_id,
                    before_active,
                    before_original,
                    before_default,
                    before_league,
                    before_draft,
                    (uint32_t)contract_level,
                    (uint32_t)before_restricted,
                    (uint32_t)before_secondary,
                    (uint32_t)before_dfa,
                    *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET),
                    *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET),
                    *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET),
                    memory_range_readable(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET, sizeof(uint32_t))
                        ? *(uint32_t*)(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET)
                        : 0u,
                    *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET),
                    *(uint32_t*)(player + OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET),
                    (uint32_t)player[OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET],
                    (uint32_t)player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET],
                    (uint32_t)player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET],
                    (uint32_t)player[OOTP27_PLAYER_DFA_FLAG_OFFSET],
                    removed_current,
                    removed_holder,
                    added_restricted);
            }
            return 1;
        }

        if (*(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET) != holder_team_id) {
            *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET) = holder_team_id;
            changed = 1;
        }
        if (*(uint32_t*)(player + OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET) != league_id) {
            *(uint32_t*)(player + OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET) = league_id;
            changed = 1;
        }
        if (player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET] != 0u) {
            player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET] = 0u;
            changed = 1;
        }
        if (player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET] != 0u) {
            player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET] = 0u;
            changed = 1;
        }
        if (kbo_remove_player_id_from_team_fixed_array(holder_team, OOTP27_TEAM_RESTRICTED_PLAYER_IDS_OFFSET, player_id) > 0) {
            changed = 1;
        }

        if (changed) {
            static LONG signed_sync_log_count = 0;
            LONG slot = InterlockedIncrement(&signed_sync_log_count);
            if (slot <= 200) {
                kbo_log_runtimef(
                    "foreign reserve rights: kept signed holder unrestricted player=%u holder_team=%u league=%u today=%u current=%u",
                    player_id,
                    holder_team_id,
                    league_id,
                    today_yyyymmdd,
                    current_team_id);
            }
        }
        return 1;
    }

    if (player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET] != 1u) {
        player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET] = 1u;
        changed = 1;
    }
    if (player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET] != 1u) {
        player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET] = 1u;
        changed = 1;
    }
    if (*(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET) == holder_team_id) {
        *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET) = 0u;
        changed = 1;
    }
    if (*(uint32_t*)(player + OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET) == league_id) {
        *(uint32_t*)(player + OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET) = 0u;
        changed = 1;
    }
    kbo_add_player_id_to_team_fixed_array(holder_team, OOTP27_TEAM_RESTRICTED_PLAYER_IDS_OFFSET, player_id);

    if (changed) {
        static LONG sync_log_count = 0;
        LONG slot = InterlockedIncrement(&sync_log_count);
        if (slot <= 200) {
            kbo_log_runtimef(
                "foreign reserve rights: synced active right to memory player=%u holder_team=%u league=%u today=%u current=%u",
                player_id,
                holder_team_id,
                league_id,
                today_yyyymmdd,
                current_team_id);
        }
    }
    return 1;
}

int kbo_sync_active_foreign_waiver_right_to_memory(
    uint8_t* player,
    uint32_t player_id,
    uint32_t holder_team_id,
    uint32_t today_yyyymmdd)
{
    if (player == NULL || player_id == 0u || holder_team_id == 0u || today_yyyymmdd == 0u
            || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    uint32_t league_id = 0u;
    kbo_lock_enter(&g_kbo_foreign_waiver_rights_lock);
    for (int i = 0; i < g_kbo_foreign_waiver_rights_count; i++) {
        KboForeignWaiverRetention* rec = &g_kbo_foreign_waiver_rights[i];
        if (rec->player_id == player_id
                && rec->team_id == holder_team_id
                && kbo_is_foreign_waiver_right_active(rec, today_yyyymmdd)) {
            league_id = rec->league_id;
            break;
        }
    }
    kbo_lock_leave(&g_kbo_foreign_waiver_rights_lock);

    return kbo_sync_active_foreign_waiver_right_to_memory_with_league(
        player,
        player_id,
        holder_team_id,
        league_id,
        today_yyyymmdd);
}

void kbo_sync_active_foreign_waiver_rights_to_memory(
    const char* source,
    uint32_t today_yyyymmdd)
{
    if (today_yyyymmdd == 0u) {
        return;
    }

    kbo_ensure_foreign_waiver_rights_loaded_for_lookup();
    KboForeignWaiverRetention records[KBO_FOREIGN_WAIVER_RIGHTS_MAX];
    int count = 0;

    kbo_lock_enter(&g_kbo_foreign_waiver_rights_lock);
    for (int i = 0; i < g_kbo_foreign_waiver_rights_count && count < KBO_FOREIGN_WAIVER_RIGHTS_MAX; i++) {
        KboForeignWaiverRetention* rec = &g_kbo_foreign_waiver_rights[i];
        if (kbo_is_foreign_waiver_right_active(rec, today_yyyymmdd)) {
            records[count++] = *rec;
        }
    }
    kbo_lock_leave(&g_kbo_foreign_waiver_rights_lock);

    int checked = 0;
    int synced = 0;
    for (int i = 0; i < count; i++) {
        uint8_t* player = kbo_find_player_by_id(records[i].player_id, NULL, NULL);
        if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
            continue;
        }
        checked++;
        if (kbo_sync_active_foreign_waiver_right_to_memory_with_league(
                player,
                records[i].player_id,
                records[i].team_id,
                records[i].league_id,
                today_yyyymmdd)) {
            synced++;
        }
    }

    static LONG batch_log_count = 0;
    LONG slot = InterlockedIncrement(&batch_log_count);
    if (slot <= 120) {
        kbo_log_runtimef(
            "foreign reserve rights: memory sync batch source=%s today=%u rights=%d checked=%d synced=%d",
            source != NULL ? source : "",
            today_yyyymmdd,
            count,
            checked,
            synced);
    }
}

